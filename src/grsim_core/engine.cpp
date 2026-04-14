/**
 * grsim_core::SimulationEngine — headless, Qt-free simulation engine.
 *
 * This is the core RL-facing interface. It owns an ODE world with robots and a
 * ball, steps physics deterministically, and returns structured state. No
 * rendering, no network I/O, no Qt types.
 *
 * Design decisions:
 *   - ODE is used directly (no PWorld/PObject wrappers from original grSim)
 *     to avoid CGraphics coupling. We re-implement the physics setup from
 *     sslworld.cpp but stripped of all GUI concerns.
 *   - The global `_w` pointer problem is solved by passing `this` as the
 *     user-data in dSpaceCollide.
 *   - Random number generation uses std::mt19937 with explicit seed.
 *   - Ball friction model mirrors grSim's inline friction in SSLWorld::step().
 */

#include "grsim_core/engine.h"

#include <ode/ode.h>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace grsim_core {

// ---------- Internal robot representation ----------

struct InternalWheel {
    dBodyID body = nullptr;
    dGeomID geom = nullptr;
    dJointID hinge = nullptr;
    dJointID motor = nullptr;
    double speed = 0.0;
    double angle_rad = 0.0; // wheel rotation axis angle
};

struct InternalKicker {
    dBodyID body = nullptr;
    dGeomID geom = nullptr;
    dJointID hinge = nullptr;
    dJointID ball_joint = nullptr;
    bool holding_ball = false;
    int kick_countdown = 0;
    bool dribbler_on = false;
};

struct InternalRobot {
    int id = 0;
    int team = 0;
    bool present = true;

    dBodyID chassis_body = nullptr;
    dGeomID chassis_geom = nullptr;
    dBodyID dummy_body = nullptr;
    dGeomID dummy_geom = nullptr;
    dJointID dummy_joint = nullptr;
    dSpaceID space = nullptr;

    InternalWheel wheels[4];
    InternalKicker kicker;

    RobotConfig cfg;

    double acc_speedup_abs_max;
    double acc_speedup_ang_max;
    double acc_brake_abs_max;
    double acc_brake_ang_max;
    double vel_abs_max;
    double vel_ang_max;
};

// ---------- Engine implementation ----------

struct EngineImpl {
    SimConfig config;

    // ODE objects
    dWorldID world = nullptr;
    dSpaceID space = nullptr;
    dJointGroupID contact_group = nullptr;

    // Ball
    dBodyID ball_body = nullptr;
    dGeomID ball_geom = nullptr;

    // Ground
    dGeomID ground_geom = nullptr;

    // Walls
    static constexpr int WALL_COUNT = 10;
    dGeomID wall_geoms[WALL_COUNT] = {};
    dBodyID wall_bodies[WALL_COUNT] = {};

    // Robots
    std::vector<InternalRobot> robots;

    // Simulation state
    double sim_time = 0.0;
    int frame_num = 0;
    std::vector<SimEvent> pending_events;

    // RNG
    std::mt19937 rng;

    // Initialization
    void init();
    void createField();
    void createBall();
    InternalRobot createRobot(int team, int id, double x, double y, double dir_deg);
    void createRobotWheel(InternalRobot& robot, int wheel_idx);
    void createRobotKicker(InternalRobot& robot);
    void setupSurfaces();
    void destroy();

    // Stepping
    void applyActions(const Actions& actions);
    void applyRobotAction(InternalRobot& robot, const RobotAction& action);
    void applyBallFriction(double dt);
    void stepRobots();
    void detectEvents();

    // State extraction
    WorldState extractState() const;
    BallState extractBallState() const;
    RobotState extractRobotState(const InternalRobot& robot) const;

    // Helpers
    void teleportBallImpl(double x, double y, double z, double vx, double vy, double vz);
    void teleportRobotImpl(InternalRobot& robot, double x, double y, double orientation_rad);
    void setRobotVelocity(InternalRobot& robot, double vx, double vy, double vw);
    void setWheelSpeeds(InternalRobot& robot, double w0, double w1, double w2, double w3);
    void kickBall(InternalRobot& robot, double speed, double angle_deg);
    bool isKickerTouchingBall(const InternalRobot& robot) const;

    InternalRobot* findRobot(int team, int id);
    int robotIndex(int team, int id) const;

    double robotStartZ() const;

    // Collision
    static void nearCallback(void* data, dGeomID o1, dGeomID o2);
};

// ---------- ODE collision callback ----------

void EngineImpl::nearCallback(void* data, dGeomID o1, dGeomID o2) {
    auto* impl = static_cast<EngineImpl*>(data);

    const int MAX_CONTACTS = 10;
    dContact contacts[MAX_CONTACTS];
    int n = dCollide(o1, o2, MAX_CONTACTS, &contacts[0].geom, sizeof(dContact));

    for (int i = 0; i < n; i++) {
        contacts[i].surface.mode = dContactBounce | dContactApprox1 | dContactSoftCFM;
        contacts[i].surface.mu = 0.5;
        contacts[i].surface.bounce = impl->config.ball.bounce;
        contacts[i].surface.bounce_vel = impl->config.ball.bounce_vel;
        contacts[i].surface.soft_cfm = 0.002;

        dJointID c = dJointCreateContact(impl->world, impl->contact_group, &contacts[i]);
        dJointAttach(c, dGeomGetBody(contacts[i].geom.g1), dGeomGetBody(contacts[i].geom.g2));
    }
}

// ---------- Engine public API ----------

SimulationEngine::SimulationEngine(const SimConfig& config)
    : impl_(std::make_unique<EngineImpl>()) {
    impl_->config = config;
    impl_->init();
}

SimulationEngine::~SimulationEngine() {
    if (impl_) {
        impl_->destroy();
    }
}

SimulationEngine::SimulationEngine(SimulationEngine&&) noexcept = default;
SimulationEngine& SimulationEngine::operator=(SimulationEngine&&) noexcept = default;

StepResult SimulationEngine::step(const Actions& actions, double dt) {
    if (dt <= 0) dt = impl_->config.sim.delta_time;

    impl_->applyActions(actions);

    int substeps = impl_->config.sim.ball_collision_substeps;
    for (int i = 0; i < substeps; i++) {
        impl_->applyBallFriction(dt / substeps);
        dSpaceCollide(impl_->space, impl_.get(), &EngineImpl::nearCallback);
        dWorldStep(impl_->world, dt / substeps);
        dJointGroupEmpty(impl_->contact_group);
    }

    impl_->stepRobots();
    impl_->sim_time += dt;
    impl_->frame_num++;

    impl_->detectEvents();

    StepResult result;
    result.state = impl_->extractState();
    result.events = std::move(impl_->pending_events);
    impl_->pending_events.clear();
    result.ball_in_play = isBallInField();
    return result;
}

StepResult SimulationEngine::stepPhysics(double dt) {
    Actions empty;
    return step(empty, dt);
}

WorldState SimulationEngine::getState() const {
    return impl_->extractState();
}

FlatObservation SimulationEngine::observe() const {
    return FlatObservation::fromWorldState(impl_->extractState());
}

void SimulationEngine::reset(uint64_t seed) {
    impl_->rng.seed(seed);
    impl_->destroy();
    impl_->init();
}

void SimulationEngine::reset(uint64_t seed, const WorldState& initial_state) {
    reset(seed);
    restore(initial_state);
}

WorldState SimulationEngine::snapshot() const {
    return impl_->extractState();
}

void SimulationEngine::restore(const WorldState& state) {
    // Restore ball
    impl_->teleportBallImpl(state.ball.x, state.ball.y, state.ball.z,
                            state.ball.vx, state.ball.vy, state.ball.vz);

    // Restore blue robots
    for (const auto& rs : state.blue_robots) {
        auto* robot = impl_->findRobot(0, rs.id);
        if (!robot) continue;
        robot->present = rs.present;
        impl_->teleportRobotImpl(*robot, rs.x, rs.y, rs.orientation);
        dBodySetLinearVel(robot->chassis_body, rs.vx, rs.vy, 0);
        dBodySetAngularVel(robot->chassis_body, 0, 0, rs.vw);
    }

    // Restore yellow robots
    for (const auto& rs : state.yellow_robots) {
        auto* robot = impl_->findRobot(1, rs.id);
        if (!robot) continue;
        robot->present = rs.present;
        impl_->teleportRobotImpl(*robot, rs.x, rs.y, rs.orientation);
        dBodySetLinearVel(robot->chassis_body, rs.vx, rs.vy, 0);
        dBodySetAngularVel(robot->chassis_body, 0, 0, rs.vw);
    }

    impl_->sim_time = state.sim_time;
    impl_->frame_num = state.frame_num;
}

void SimulationEngine::setWheelSpeeds(int team, int robot_id, double w0, double w1, double w2, double w3) {
    auto* robot = impl_->findRobot(team, robot_id);
    if (robot) impl_->setWheelSpeeds(*robot, w0, w1, w2, w3);
}

void SimulationEngine::setBodyVelocity(int team, int robot_id, double vx, double vy, double vw) {
    auto* robot = impl_->findRobot(team, robot_id);
    if (robot) impl_->setRobotVelocity(*robot, vx, vy, vw);
}

void SimulationEngine::setGlobalVelocity(int team, int robot_id, double vx, double vy, double vw) {
    auto* robot = impl_->findRobot(team, robot_id);
    if (!robot) return;
    // Convert global to local velocity
    const dReal* rot = dBodyGetRotation(robot->chassis_body);
    double cos_a = rot[0]; // rotation matrix [0][0]
    double sin_a = rot[4]; // rotation matrix [1][0]
    double local_vx = vx * cos_a + vy * sin_a;
    double local_vy = -vx * sin_a + vy * cos_a;
    impl_->setRobotVelocity(*robot, local_vx, local_vy, vw);
}

void SimulationEngine::kick(int team, int robot_id, double speed, double angle_deg) {
    auto* robot = impl_->findRobot(team, robot_id);
    if (robot) impl_->kickBall(*robot, speed, angle_deg);
}

void SimulationEngine::setDribbler(int team, int robot_id, bool on) {
    auto* robot = impl_->findRobot(team, robot_id);
    if (robot) robot->kicker.dribbler_on = on;
}

void SimulationEngine::teleportBall(double x, double y, double z, double vx, double vy, double vz) {
    impl_->teleportBallImpl(x, y, z, vx, vy, vz);
}

void SimulationEngine::teleportRobot(int team, int robot_id, double x, double y, double orientation_rad, bool present) {
    auto* robot = impl_->findRobot(team, robot_id);
    if (!robot) return;
    robot->present = present;
    if (present) {
        impl_->teleportRobotImpl(*robot, x, y, orientation_rad);
    } else {
        // Move off-field
        impl_->teleportRobotImpl(*robot, 1e6 * robot_id, 1e6 * team, 0);
    }
}

void SimulationEngine::setRobotPresent(int team, int robot_id, bool present) {
    auto* robot = impl_->findRobot(team, robot_id);
    if (robot) {
        robot->present = present;
        if (!present) {
            impl_->teleportRobotImpl(*robot, 1e6 * robot_id, 1e6 * team, 0);
        }
    }
}

int SimulationEngine::robotsPerTeam() const {
    return impl_->config.sim.robots_per_team;
}

const SimConfig& SimulationEngine::config() const {
    return impl_->config;
}

double SimulationEngine::simTime() const {
    return impl_->sim_time;
}

int SimulationEngine::frameNumber() const {
    return impl_->frame_num;
}

bool SimulationEngine::isBallInField() const {
    auto bs = impl_->extractBallState();
    return isPositionInField(bs.x, bs.y);
}

bool SimulationEngine::isBallInGoal(int team) const {
    auto bs = impl_->extractBallState();
    double half_length = impl_->config.field.field_length / 2.0;
    double half_goal_w = impl_->config.field.goal_width / 2.0;
    double goal_depth = impl_->config.field.goal_depth;

    if (team == 0) {
        // Blue defends negative-x goal
        return bs.x < -half_length && bs.x > -(half_length + goal_depth) &&
               std::abs(bs.y) < half_goal_w;
    } else {
        // Yellow defends positive-x goal
        return bs.x > half_length && bs.x < (half_length + goal_depth) &&
               std::abs(bs.y) < half_goal_w;
    }
}

bool SimulationEngine::isRobotInDefenseArea(int team, int robot_id) const {
    auto* robot = impl_->findRobot(team, robot_id);
    if (!robot) return false;
    const dReal* pos = dBodyGetPosition(robot->chassis_body);
    // Check both defense areas
    return isPositionInDefenseArea(0, pos[0], pos[1]) ||
           isPositionInDefenseArea(1, pos[0], pos[1]);
}

bool SimulationEngine::isPositionInDefenseArea(int defense_team, double x, double y) const {
    double half_length = impl_->config.field.field_length / 2.0;
    double pen_depth = impl_->config.field.penalty_area_depth;
    double pen_half_w = impl_->config.field.penalty_area_width / 2.0;

    if (defense_team == 0) {
        // Blue defense area: negative-x side
        return x < -(half_length - pen_depth) && x > -half_length &&
               std::abs(y) < pen_half_w;
    } else {
        // Yellow defense area: positive-x side
        return x > (half_length - pen_depth) && x < half_length &&
               std::abs(y) < pen_half_w;
    }
}

bool SimulationEngine::isPositionInField(double x, double y) const {
    double half_length = impl_->config.field.field_length / 2.0;
    double half_width = impl_->config.field.field_width / 2.0;
    return std::abs(x) <= half_length && std::abs(y) <= half_width;
}

// ---------- EngineImpl internals ----------

void EngineImpl::init() {
    dInitODE();
    world = dWorldCreate();
    space = dHashSpaceCreate(nullptr);
    contact_group = dJointGroupCreate(0);
    dWorldSetGravity(world, 0, 0, -config.sim.gravity);

    sim_time = 0.0;
    frame_num = 0;
    pending_events.clear();

    createField();
    createBall();

    int n = config.sim.robots_per_team;

    // Default formation: line up outside field
    double spacing = 0.3;
    for (int i = 0; i < n; i++) {
        double y = (i - n / 2.0 + 0.5) * spacing;
        robots.push_back(createRobot(0, i, -(config.field.field_length / 2.0 + 0.5), y, 0));
    }
    for (int i = 0; i < n; i++) {
        double y = (i - n / 2.0 + 0.5) * spacing;
        robots.push_back(createRobot(1, i, (config.field.field_length / 2.0 + 0.5), y, 180));
    }
}

void EngineImpl::createField() {
    // Ground plane
    ground_geom = dCreatePlane(space, 0, 0, 1, 0);

    // Walls (simplified: 4 outer walls + 6 goal walls)
    double half_l = config.field.field_length / 2.0;
    double half_w = config.field.field_width / 2.0;
    double margin_x = config.field.margin_goal_line + config.field.referee_margin;
    double margin_y = config.field.margin_touch_line + config.field.referee_margin;
    double thick = config.field.wall_thickness;

    double outer_x = half_l + margin_x;
    double outer_y = half_w + margin_y;
    double wall_h = 0.4;

    auto makeWall = [&](int idx, double px, double py, double pz, double sx, double sy, double sz) {
        wall_geoms[idx] = dCreateBox(space, sx, sy, sz);
        dGeomSetPosition(wall_geoms[idx], px, py, pz);
    };

    // Outer walls
    makeWall(0, 0, outer_y + thick / 2, wall_h / 2, 2 * outer_x, thick, wall_h);
    makeWall(1, 0, -(outer_y + thick / 2), wall_h / 2, 2 * outer_x, thick, wall_h);
    makeWall(2, outer_x + thick / 2, 0, wall_h / 2, thick, 2 * outer_y, wall_h);
    makeWall(3, -(outer_x + thick / 2), 0, wall_h / 2, thick, 2 * outer_y, wall_h);

    // Goal walls (yellow side = positive x)
    double gd = config.field.goal_depth;
    double gw = config.field.goal_width;
    double gh = config.field.goal_height;
    double gt = config.field.goal_thickness;

    // Yellow goal (positive x)
    double gx = half_l + gd + gt / 2;
    makeWall(4, gx, 0, gh / 2, gt, gw, gh);                            // back
    makeWall(5, half_l + gd / 2, gw / 2 + gt / 2, gh / 2, gd + gt, gt, gh);  // side+
    makeWall(6, half_l + gd / 2, -(gw / 2 + gt / 2), gh / 2, gd + gt, gt, gh); // side-

    // Blue goal (negative x)
    makeWall(7, -(half_l + gd + gt / 2), 0, gh / 2, gt, gw, gh);
    makeWall(8, -(half_l + gd / 2), gw / 2 + gt / 2, gh / 2, gd + gt, gt, gh);
    makeWall(9, -(half_l + gd / 2), -(gw / 2 + gt / 2), gh / 2, gd + gt, gt, gh);
}

void EngineImpl::createBall() {
    double r = config.ball.radius;
    double m = config.ball.mass;

    ball_body = dBodyCreate(world);
    ball_geom = dCreateSphere(space, r);
    dGeomSetBody(ball_geom, ball_body);

    dMass mass;
    dMassSetSphere(&mass, m / (4.0 / 3.0 * M_PI * r * r * r), r);
    mass.mass = m;
    dBodySetMass(ball_body, &mass);
    dBodySetPosition(ball_body, 0, 0, r + 0.005);

    dBodySetLinearDamping(ball_body, config.ball.linear_damp);
    dBodySetAngularDamping(ball_body, config.ball.angular_damp);
}

double EngineImpl::robotStartZ() const {
    const auto& rc = config.blue_robot;
    return rc.height * 0.5 + rc.wheel_radius * 1.1 + rc.bottom_height;
}

InternalRobot EngineImpl::createRobot(int team, int id, double x, double y, double dir_deg) {
    InternalRobot robot;
    robot.id = id;
    robot.team = team;
    robot.cfg = (team == 0) ? config.blue_robot : config.yellow_robot;
    robot.acc_speedup_abs_max = robot.cfg.acc_speedup_absolute_max;
    robot.acc_speedup_ang_max = robot.cfg.acc_speedup_angular_max;
    robot.acc_brake_abs_max = robot.cfg.acc_brake_absolute_max;
    robot.acc_brake_ang_max = robot.cfg.acc_brake_angular_max;
    robot.vel_abs_max = robot.cfg.vel_absolute_max;
    robot.vel_ang_max = robot.cfg.vel_angular_max;

    double z = robotStartZ();

    // Chassis (cylinder)
    robot.chassis_body = dBodyCreate(world);
    robot.chassis_geom = dCreateCylinder(space, robot.cfg.radius, robot.cfg.height);
    dGeomSetBody(robot.chassis_geom, robot.chassis_body);

    dMass mass;
    dMassSetCylinder(&mass, 1.0, 3, robot.cfg.radius, robot.cfg.height);
    mass.mass = robot.cfg.body_mass * 0.99;
    dBodySetMass(robot.chassis_body, &mass);
    dBodySetPosition(robot.chassis_body, x, y, z);

    // Dummy sphere (for collision shape approximation)
    robot.dummy_body = dBodyCreate(world);
    robot.dummy_geom = dCreateSphere(space, robot.cfg.center_from_kicker);
    dGeomSetBody(robot.dummy_geom, robot.dummy_body);

    dMass dm;
    dMassSetSphere(&dm, 1.0, robot.cfg.center_from_kicker);
    dm.mass = robot.cfg.body_mass * 0.01;
    dBodySetMass(robot.dummy_body, &dm);
    dBodySetPosition(robot.dummy_body, x, y, z);

    robot.dummy_joint = dJointCreateFixed(world, nullptr);
    dJointAttach(robot.dummy_joint, robot.chassis_body, robot.dummy_body);
    dJointSetFixed(robot.dummy_joint);

    // Create wheels
    for (int w = 0; w < 4; w++) {
        createRobotWheel(robot, w);
    }

    // Create kicker
    createRobotKicker(robot);

    // Set initial direction
    double ang = dir_deg * M_PI / 180.0;
    dMatrix3 R;
    dRFromAxisAndAngle(R, 0, 0, 1, ang);
    dBodySetRotation(robot.chassis_body, R);

    return robot;
}

void EngineImpl::createRobotWheel(InternalRobot& robot, int idx) {
    double angles[4] = {
        robot.cfg.wheel1_angle,
        robot.cfg.wheel2_angle,
        robot.cfg.wheel3_angle,
        robot.cfg.wheel4_angle
    };

    double ang = angles[idx] * M_PI / 180.0;
    double rad = robot.cfg.radius - robot.cfg.wheel_thickness / 2.0;

    const dReal* cpos = dBodyGetPosition(robot.chassis_body);
    double cx = cpos[0] + rad * cos(ang);
    double cy = cpos[1] + rad * sin(ang);
    double cz = cpos[2] - robot.cfg.height * 0.5 + robot.cfg.wheel_radius - robot.cfg.bottom_height;

    auto& w = robot.wheels[idx];
    w.angle_rad = ang;

    w.body = dBodyCreate(world);
    w.geom = dCreateCylinder(space, robot.cfg.wheel_radius, robot.cfg.wheel_thickness);
    dGeomSetBody(w.geom, w.body);

    dMass mass;
    dMassSetCylinder(&mass, 1.0, 3, robot.cfg.wheel_radius, robot.cfg.wheel_thickness);
    mass.mass = robot.cfg.wheel_mass;
    dBodySetMass(w.body, &mass);
    dBodySetPosition(w.body, cx, cy, cz);

    // Hinge joint
    w.hinge = dJointCreateHinge(world, nullptr);
    dJointAttach(w.hinge, robot.chassis_body, w.body);
    dJointSetHingeAnchor(w.hinge, cx, cy, cz);
    dJointSetHingeAxis(w.hinge, cos(ang), sin(ang), 0);

    // Motor
    w.motor = dJointCreateAMotor(world, nullptr);
    dJointAttach(w.motor, robot.chassis_body, w.body);
    dJointSetAMotorNumAxes(w.motor, 1);
    dJointSetAMotorAxis(w.motor, 0, 1, cos(ang), sin(ang), 0);
    dJointSetAMotorParam(w.motor, dParamFMax, robot.cfg.wheel_motor_fmax);
    w.speed = 0;
}

void EngineImpl::createRobotKicker(InternalRobot& robot) {
    const dReal* cpos = dBodyGetPosition(robot.chassis_body);
    double kx = cpos[0] + robot.cfg.center_from_kicker + robot.cfg.kicker_thickness;
    double ky = cpos[1];
    double kz = cpos[2] - robot.cfg.height * 0.5 + robot.cfg.wheel_radius
                - robot.cfg.bottom_height + robot.cfg.kicker_z;

    robot.kicker.body = dBodyCreate(world);
    robot.kicker.geom = dCreateBox(space, robot.cfg.kicker_thickness,
                                   robot.cfg.kicker_width, robot.cfg.kicker_height);
    dGeomSetBody(robot.kicker.geom, robot.kicker.body);

    dMass mass;
    dMassSetBox(&mass, 1.0, robot.cfg.kicker_thickness, robot.cfg.kicker_width, robot.cfg.kicker_height);
    mass.mass = robot.cfg.kicker_mass;
    dBodySetMass(robot.kicker.body, &mass);
    dBodySetPosition(robot.kicker.body, kx, ky, kz);

    robot.kicker.hinge = dJointCreateHinge(world, nullptr);
    dJointAttach(robot.kicker.hinge, robot.chassis_body, robot.kicker.body);
    dJointSetHingeAnchor(robot.kicker.hinge, kx, ky, kz);
    dJointSetHingeAxis(robot.kicker.hinge, 0, -1, 0);
    dJointSetHingeParam(robot.kicker.hinge, dParamVel, 0);
    dJointSetHingeParam(robot.kicker.hinge, dParamLoStop, 0);
    dJointSetHingeParam(robot.kicker.hinge, dParamHiStop, 0);
}

void EngineImpl::destroy() {
    robots.clear();

    if (contact_group) {
        dJointGroupDestroy(contact_group);
        contact_group = nullptr;
    }
    if (space) {
        dSpaceDestroy(space);
        space = nullptr;
    }
    if (world) {
        dWorldDestroy(world);
        world = nullptr;
    }
    dCloseODE();

    ball_body = nullptr;
    ball_geom = nullptr;
    ground_geom = nullptr;
    std::memset(wall_geoms, 0, sizeof(wall_geoms));
}

void EngineImpl::applyActions(const Actions& actions) {
    for (size_t i = 0; i < actions.blue.robot_actions.size(); i++) {
        auto* r = findRobot(0, i);
        if (r && r->present) applyRobotAction(*r, actions.blue.robot_actions[i]);
    }
    for (size_t i = 0; i < actions.yellow.robot_actions.size(); i++) {
        auto* r = findRobot(1, i);
        if (r && r->present) applyRobotAction(*r, actions.yellow.robot_actions[i]);
    }
}

void EngineImpl::applyRobotAction(InternalRobot& robot, const RobotAction& action) {
    switch (action.type) {
        case RobotAction::WHEEL_VELOCITY:
            setWheelSpeeds(robot, action.values[0], action.values[1],
                          action.values[2], action.values[3]);
            break;
        case RobotAction::BODY_VELOCITY:
            setRobotVelocity(robot, action.values[0], action.values[1], action.values[2]);
            break;
        case RobotAction::GLOBAL_VELOCITY: {
            const dReal* rot = dBodyGetRotation(robot.chassis_body);
            double cos_a = rot[0];
            double sin_a = rot[4];
            double local_vx = action.values[0] * cos_a + action.values[1] * sin_a;
            double local_vy = -action.values[0] * sin_a + action.values[1] * cos_a;
            setRobotVelocity(robot, local_vx, local_vy, action.values[2]);
            break;
        }
    }

    robot.kicker.dribbler_on = action.dribbler;

    if (action.kick_speed > 0.001) {
        kickBall(robot, action.kick_speed, action.kick_angle);
    }
}

void EngineImpl::applyBallFriction(double dt) {
    const dReal* vel = dBodyGetLinearVel(ball_body);
    double speed = std::sqrt(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);

    if (speed > 0.01) {
        double fk = config.ball.friction * config.ball.mass * config.sim.gravity;
        double fx = -fk * vel[0] / speed;
        double fy = -fk * vel[1] / speed;
        double fz = -fk * vel[2] / speed;
        double tx = -fy * config.ball.radius;
        double ty = fx * config.ball.radius;
        dBodyAddTorque(ball_body, tx, ty, 0);
        dBodyAddForce(ball_body, fx, fy, fz);
    } else {
        dBodySetAngularVel(ball_body, 0, 0, 0);
        dBodySetLinearVel(ball_body, 0, 0, 0);
    }
}

void EngineImpl::stepRobots() {
    for (auto& robot : robots) {
        if (!robot.present) continue;
        for (int w = 0; w < 4; w++) {
            dJointSetAMotorParam(robot.wheels[w].motor, dParamVel, robot.wheels[w].speed);
            dJointSetAMotorParam(robot.wheels[w].motor, dParamFMax, robot.cfg.wheel_motor_fmax);
        }

        // Kicker countdown
        if (robot.kicker.kick_countdown > 0) {
            robot.kicker.kick_countdown--;
        }

        // Dribbler: hold ball if touching and dribbler is on
        if (robot.kicker.dribbler_on && isKickerTouchingBall(robot)) {
            if (!robot.kicker.holding_ball) {
                // Create a joint to hold ball
                dBodySetLinearVel(ball_body, 0, 0, 0);
                robot.kicker.ball_joint = dJointCreateHinge(world, nullptr);
                dJointAttach(robot.kicker.ball_joint, robot.kicker.body, ball_body);
                robot.kicker.holding_ball = true;
            }
        } else if (robot.kicker.holding_ball) {
            dJointDestroy(robot.kicker.ball_joint);
            robot.kicker.holding_ball = false;
        }
    }
}

void EngineImpl::detectEvents() {
    // Ball out of field
    auto bs = extractBallState();
    double half_l = config.field.field_length / 2.0;
    double half_w = config.field.field_width / 2.0;

    if (std::abs(bs.y) > half_w && std::abs(bs.x) <= half_l) {
        SimEvent e;
        e.type = EventType::BALL_OUT_TOUCHLINE;
        e.timestamp = sim_time;
        e.x = bs.x;
        e.y = bs.y;
        pending_events.push_back(e);
    }

    if (std::abs(bs.x) > half_l) {
        // Check if in goal
        double goal_half_w = config.field.goal_width / 2.0;
        if (std::abs(bs.y) < goal_half_w) {
            SimEvent e;
            e.type = bs.x > 0 ? EventType::GOAL_SCORED_BLUE : EventType::GOAL_SCORED_YELLOW;
            e.timestamp = sim_time;
            e.x = bs.x;
            e.y = bs.y;
            pending_events.push_back(e);
        } else {
            SimEvent e;
            e.type = EventType::BALL_OUT_GOALLINE;
            e.timestamp = sim_time;
            e.x = bs.x;
            e.y = bs.y;
            pending_events.push_back(e);
        }
    }
}

WorldState EngineImpl::extractState() const {
    WorldState state;
    state.sim_time = sim_time;
    state.frame_num = frame_num;
    state.ball = extractBallState();

    for (const auto& r : robots) {
        auto rs = extractRobotState(r);
        if (r.team == 0) state.blue_robots.push_back(rs);
        else state.yellow_robots.push_back(rs);
    }
    return state;
}

BallState EngineImpl::extractBallState() const {
    BallState bs;
    if (ball_body) {
        const dReal* pos = dBodyGetPosition(ball_body);
        const dReal* vel = dBodyGetLinearVel(ball_body);
        bs.x = pos[0]; bs.y = pos[1]; bs.z = pos[2];
        bs.vx = vel[0]; bs.vy = vel[1]; bs.vz = vel[2];
    }
    return bs;
}

RobotState EngineImpl::extractRobotState(const InternalRobot& robot) const {
    RobotState rs;
    rs.id = robot.id;
    rs.team = robot.team;
    rs.present = robot.present;

    if (robot.chassis_body) {
        const dReal* pos = dBodyGetPosition(robot.chassis_body);
        const dReal* vel = dBodyGetLinearVel(robot.chassis_body);
        const dReal* avel = dBodyGetAngularVel(robot.chassis_body);
        const dReal* rot = dBodyGetRotation(robot.chassis_body);

        rs.x = pos[0];
        rs.y = pos[1];
        rs.vx = vel[0];
        rs.vy = vel[1];
        rs.vw = avel[2];

        // Extract orientation from rotation matrix
        rs.orientation = std::atan2(rot[4], rot[0]); // atan2(R[1][0], R[0][0])

        for (int w = 0; w < 4; w++) {
            rs.wheel_speeds[w] = robot.wheels[w].speed;
        }
        rs.dribbler_on = robot.kicker.dribbler_on;
        rs.touching_ball = isKickerTouchingBall(robot);
    }
    return rs;
}

void EngineImpl::teleportBallImpl(double x, double y, double z, double vx, double vy, double vz) {
    if (z < config.ball.radius) z = config.ball.radius + 0.005;
    dBodySetPosition(ball_body, x, y, z);
    dBodySetLinearVel(ball_body, vx, vy, vz);
    dBodySetAngularVel(ball_body, 0, 0, 0);
}

void EngineImpl::teleportRobotImpl(InternalRobot& robot, double x, double y, double orientation_rad) {
    double z = robotStartZ();
    dBodySetPosition(robot.chassis_body, x, y, z);
    dBodySetPosition(robot.dummy_body, x, y, z);

    dMatrix3 R;
    dRFromAxisAndAngle(R, 0, 0, 1, orientation_rad);
    dBodySetRotation(robot.chassis_body, R);
    dBodySetRotation(robot.dummy_body, R);

    // Zero velocities
    dBodySetLinearVel(robot.chassis_body, 0, 0, 0);
    dBodySetAngularVel(robot.chassis_body, 0, 0, 0);
    dBodySetLinearVel(robot.dummy_body, 0, 0, 0);
    dBodySetAngularVel(robot.dummy_body, 0, 0, 0);

    for (int w = 0; w < 4; w++) {
        robot.wheels[w].speed = 0;
        dBodySetLinearVel(robot.wheels[w].body, 0, 0, 0);
        dBodySetAngularVel(robot.wheels[w].body, 0, 0, 0);
    }
    dBodySetLinearVel(robot.kicker.body, 0, 0, 0);
    dBodySetAngularVel(robot.kicker.body, 0, 0, 0);

    if (robot.kicker.holding_ball) {
        dJointDestroy(robot.kicker.ball_joint);
        robot.kicker.holding_ball = false;
    }
}

void EngineImpl::setRobotVelocity(InternalRobot& robot, double vx, double vy, double vw) {
    // Clamp to limits
    double v = std::sqrt(vx * vx + vy * vy);
    if (v > robot.vel_abs_max) {
        vx *= robot.vel_abs_max / v;
        vy *= robot.vel_abs_max / v;
    }
    if (std::abs(vw) > robot.vel_ang_max) {
        vw = std::copysign(robot.vel_ang_max, vw);
    }

    // Acceleration limiting (matches original grSim logic)
    const dReal* cvv = dBodyGetLinearVel(robot.chassis_body);
    double cv = std::sqrt(cvv[0] * cvv[0] + cvv[1] * cvv[1]);
    double a = (std::sqrt(vx * vx + vy * vy) - cv) / config.sim.delta_time / 2.0;
    double a_limit = (a > 0) ? robot.acc_speedup_abs_max : robot.acc_brake_abs_max;
    if (std::abs(a) > a_limit) {
        a = std::copysign(a_limit, a);
        double new_v = cv + a * config.sim.delta_time * 2.0;
        double target_v = std::sqrt(vx * vx + vy * vy);
        if (target_v > 0) {
            vx *= new_v / target_v;
            vy *= new_v / target_v;
        }
    }

    const dReal* cvvw = dBodyGetAngularVel(robot.chassis_body);
    double cvw = cvvw[2];
    double aw = (vw - cvw) / config.sim.delta_time / 2.0;
    double aw_limit = (aw > 0) ? robot.acc_speedup_ang_max : robot.acc_brake_ang_max;
    if (std::abs(aw) > aw_limit) {
        aw = std::copysign(aw_limit, aw);
        vw = cvw + aw * config.sim.delta_time * 2.0;
    }

    // Convert to wheel speeds (inverse kinematics)
    double angles[4] = {
        robot.cfg.wheel1_angle * M_PI / 180.0,
        robot.cfg.wheel2_angle * M_PI / 180.0,
        robot.cfg.wheel3_angle * M_PI / 180.0,
        robot.cfg.wheel4_angle * M_PI / 180.0
    };

    for (int w = 0; w < 4; w++) {
        robot.wheels[w].speed = (1.0 / robot.cfg.wheel_radius) *
            (robot.cfg.radius * vw - vx * std::sin(angles[w]) + vy * std::cos(angles[w]));
    }
}

void EngineImpl::setWheelSpeeds(InternalRobot& robot, double w0, double w1, double w2, double w3) {
    robot.wheels[0].speed = w0;
    robot.wheels[1].speed = w1;
    robot.wheels[2].speed = w2;
    robot.wheels[3].speed = w3;
}

void EngineImpl::kickBall(InternalRobot& robot, double speed, double angle_deg) {
    if (!isKickerTouchingBall(robot)) return;

    double angle_rad = angle_deg * M_PI / 180.0;
    double limit = (angle_deg > 0) ? robot.cfg.max_chip_kick_speed : robot.cfg.max_linear_kick_speed;
    if (speed > limit) speed = limit;

    // Unhhold ball first
    if (robot.kicker.holding_ball) {
        dJointDestroy(robot.kicker.ball_joint);
        robot.kicker.holding_ball = false;
    }

    // Get robot forward direction
    const dReal* rot = dBodyGetRotation(robot.chassis_body);
    double dx = rot[0]; // forward x
    double dy = rot[4]; // forward y
    double dlen = std::sqrt(dx * dx + dy * dy);
    if (dlen < 1e-6) return;

    double vx = dx * speed * std::cos(angle_rad) / dlen;
    double vy = dy * speed * std::cos(angle_rad) / dlen;
    double vz = speed * std::sin(angle_rad);

    // Apply damping from existing ball velocity (matches grSim)
    const dReal* bvel = dBodyGetLinearVel(ball_body);
    double vn = -(bvel[0] * dx + bvel[1] * dy) * robot.cfg.kicker_damp_factor;
    double vt = -(bvel[0] * dy - bvel[1] * dx);
    vx += vn * dx - vt * dy;
    vy += vn * dy + vt * dx;

    dBodySetLinearVel(ball_body, vx, vy, vz);

    robot.kicker.kick_countdown = 10;
    SimEvent e;
    e.type = (angle_deg > 0) ? EventType::BALL_CHIPPED : EventType::BALL_KICKED;
    e.timestamp = sim_time;
    e.team = robot.team;
    e.robot_id = robot.id;
    const dReal* bpos = dBodyGetPosition(ball_body);
    e.x = bpos[0];
    e.y = bpos[1];
    pending_events.push_back(e);
}

bool EngineImpl::isKickerTouchingBall(const InternalRobot& robot) const {
    if (!ball_body || !robot.kicker.body) return false;

    const dReal* bpos = dBodyGetPosition(ball_body);
    const dReal* kpos = dBodyGetPosition(robot.kicker.body);
    const dReal* rot = dBodyGetRotation(robot.chassis_body);

    double dx = rot[0]; // forward
    double dy = rot[4];

    double kx = kpos[0] + dx * robot.cfg.kicker_thickness * 0.5;
    double ky = kpos[1] + dy * robot.cfg.kicker_thickness * 0.5;

    double xx = std::abs((kx - bpos[0]) * dx + (ky - bpos[1]) * dy);
    double yy = std::abs(-(kx - bpos[0]) * dy + (ky - bpos[1]) * dx);
    double zz = std::abs(kpos[2] - bpos[2]);

    return xx < robot.cfg.kicker_thickness * 2.0 + config.ball.radius &&
           yy < robot.cfg.kicker_width * 0.5 &&
           zz < robot.cfg.kicker_height * 0.5;
}

InternalRobot* EngineImpl::findRobot(int team, int id) {
    for (auto& r : robots) {
        if (r.team == team && r.id == id) return &r;
    }
    return nullptr;
}

// const version for const methods
const InternalRobot* findRobotConst(const std::vector<InternalRobot>& robots, int team, int id) {
    for (const auto& r : robots) {
        if (r.team == team && r.id == id) return &r;
    }
    return nullptr;
}

int EngineImpl::robotIndex(int team, int id) const {
    for (size_t i = 0; i < robots.size(); i++) {
        if (robots[i].team == team && robots[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

} // namespace grsim_core
