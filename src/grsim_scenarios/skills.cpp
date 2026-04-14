/**
 * Skill implementations for the hierarchical action interface.
 * Each skill compiles to body-velocity RobotAction.
 */

#include "grsim_scenarios/skills.h"
#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace grsim_scenarios {

using namespace grsim_core;

// Helper: find a robot in state
static const RobotState* findRobot(const WorldState& state, int team, int id) {
    const auto& robots = (team == 0) ? state.blue_robots : state.yellow_robots;
    for (const auto& r : robots) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

static double normalizeAngle(double a) {
    while (a > M_PI) a -= 2 * M_PI;
    while (a < -M_PI) a += 2 * M_PI;
    return a;
}

static double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

// ============================================================
// GoToPose
// ============================================================

SkillResult GoToPoseSkill::execute(const WorldState& state, int team, int robot_id, double /*dt*/) {
    SkillResult result;
    result.trace = "go_to_pose → body_velocity";

    const auto* r = findRobot(state, team, robot_id);
    if (!r) { result.complete = true; return result; }

    double dx = target_x_ - r->x;
    double dy = target_y_ - r->y;
    double dist = std::sqrt(dx * dx + dy * dy);
    double angle_to_target = std::atan2(dy, dx);
    double angle_error = normalizeAngle(target_ori_ - r->orientation);

    if (dist < pos_tolerance_ && std::abs(angle_error) < ori_tolerance_) {
        result.complete = true;
        return result;
    }

    // Convert global velocity to local frame
    double cos_a = std::cos(-r->orientation);
    double sin_a = std::sin(-r->orientation);
    double gvx = clamp(kp_pos_ * dx, -2.5, 2.5);
    double gvy = clamp(kp_pos_ * dy, -2.5, 2.5);
    double lvx = gvx * cos_a - gvy * sin_a;
    double lvy = gvx * sin_a + gvy * cos_a;
    double vw = clamp(kp_ori_ * angle_error, -10.0, 10.0);

    result.action.type = RobotAction::BODY_VELOCITY;
    result.action.values[0] = lvx;
    result.action.values[1] = lvy;
    result.action.values[2] = vw;
    return result;
}

// ============================================================
// FacePoint
// ============================================================

SkillResult FacePointSkill::execute(const WorldState& state, int team, int robot_id, double /*dt*/) {
    SkillResult result;
    result.trace = "face_point → body_velocity(angular)";

    const auto* r = findRobot(state, team, robot_id);
    if (!r) { result.complete = true; return result; }

    double dx = target_x_ - r->x;
    double dy = target_y_ - r->y;
    double target_angle = std::atan2(dy, dx);
    double error = normalizeAngle(target_angle - r->orientation);

    if (std::abs(error) < 0.03) {
        result.complete = true;
        return result;
    }

    result.action.type = RobotAction::BODY_VELOCITY;
    result.action.values[2] = clamp(5.0 * error, -10.0, 10.0);
    return result;
}

// ============================================================
// KickToPoint / ChipToPoint
// ============================================================

SkillResult KickToPointSkill::execute(const WorldState& state, int team, int robot_id, double dt) {
    SkillResult result;
    result.trace = chip_ ? "chip_to_point → face_point → kick" : "kick_to_point → face_point → kick";

    const auto* r = findRobot(state, team, robot_id);
    if (!r) { result.complete = true; return result; }

    // First: face the target
    double dx = target_x_ - r->x;
    double dy = target_y_ - r->y;
    double target_angle = std::atan2(dy, dx);
    double error = normalizeAngle(target_angle - r->orientation);

    if (std::abs(error) > 0.1) {
        // Still turning
        result.action.type = RobotAction::BODY_VELOCITY;
        result.action.values[2] = clamp(5.0 * error, -10.0, 10.0);
        return result;
    }

    // Facing target: kick
    if (r->touching_ball) {
        result.action.kick_speed = kick_speed_;
        result.action.kick_angle = chip_ ? 45.0 : 0.0;
        result.complete = true;
    } else {
        // Move towards ball
        double bx = state.ball.x - r->x;
        double by = state.ball.y - r->y;
        double cos_a = std::cos(-r->orientation);
        double sin_a = std::sin(-r->orientation);
        result.action.type = RobotAction::BODY_VELOCITY;
        result.action.values[0] = clamp(3.0 * (bx * cos_a - by * sin_a), -2.0, 2.0);
        result.action.values[1] = clamp(3.0 * (bx * sin_a + by * cos_a), -2.0, 2.0);
        result.action.values[2] = clamp(5.0 * error, -10.0, 10.0);
    }
    return result;
}

// ============================================================
// ReceiveBall
// ============================================================

SkillResult ReceiveBallSkill::execute(const WorldState& state, int team, int robot_id, double /*dt*/) {
    SkillResult result;
    result.trace = "receive_ball → intercept + dribbler";

    const auto* r = findRobot(state, team, robot_id);
    if (!r) { result.complete = true; return result; }

    // Predict ball position and move to intercept
    double bvx = state.ball.vx, bvy = state.ball.vy;
    double speed = std::sqrt(bvx * bvx + bvy * bvy);

    double target_x, target_y;
    if (speed > 0.1) {
        // Intercept point: project ball forward
        double t = 0.5; // predict 0.5s ahead
        target_x = state.ball.x + bvx * t;
        target_y = state.ball.y + bvy * t;
    } else {
        target_x = state.ball.x;
        target_y = state.ball.y;
    }

    double dx = target_x - r->x;
    double dy = target_y - r->y;
    double cos_a = std::cos(-r->orientation);
    double sin_a = std::sin(-r->orientation);

    result.action.type = RobotAction::BODY_VELOCITY;
    result.action.values[0] = clamp(3.0 * (dx * cos_a - dy * sin_a), -2.5, 2.5);
    result.action.values[1] = clamp(3.0 * (dx * sin_a + dy * cos_a), -2.5, 2.5);
    result.action.dribbler = true;

    // Face ball
    double ball_angle = std::atan2(state.ball.y - r->y, state.ball.x - r->x);
    double err = normalizeAngle(ball_angle - r->orientation);
    result.action.values[2] = clamp(5.0 * err, -10.0, 10.0);

    if (r->touching_ball) result.complete = true;
    return result;
}

// ============================================================
// InterceptBall
// ============================================================

SkillResult InterceptBallSkill::execute(const WorldState& state, int team, int robot_id, double /*dt*/) {
    SkillResult result;
    result.trace = "intercept_ball → go_to_predicted_ball";

    const auto* r = findRobot(state, team, robot_id);
    if (!r) { result.complete = true; return result; }

    // Simple intercept: move to ball
    double dx = state.ball.x - r->x;
    double dy = state.ball.y - r->y;
    double dist = std::sqrt(dx * dx + dy * dy);

    double cos_a = std::cos(-r->orientation);
    double sin_a = std::sin(-r->orientation);
    result.action.type = RobotAction::BODY_VELOCITY;
    result.action.values[0] = clamp(3.0 * (dx * cos_a - dy * sin_a), -3.0, 3.0);
    result.action.values[1] = clamp(3.0 * (dx * sin_a + dy * cos_a), -3.0, 3.0);

    double ball_angle = std::atan2(dy, dx);
    double err = normalizeAngle(ball_angle - r->orientation);
    result.action.values[2] = clamp(5.0 * err, -10.0, 10.0);

    if (dist < 0.15) result.complete = true;
    return result;
}

// ============================================================
// DribbleToPoint
// ============================================================

SkillResult DribbleToPointSkill::execute(const WorldState& state, int team, int robot_id, double /*dt*/) {
    SkillResult result;
    result.trace = "dribble_to_point → go_to_pose + dribbler";

    const auto* r = findRobot(state, team, robot_id);
    if (!r) { result.complete = true; return result; }

    double dx = target_x_ - r->x;
    double dy = target_y_ - r->y;
    double dist = std::sqrt(dx * dx + dy * dy);
    double target_angle = std::atan2(dy, dx);
    double error = normalizeAngle(target_angle - r->orientation);

    if (dist < 0.1) {
        result.complete = true;
        return result;
    }

    double cos_a = std::cos(-r->orientation);
    double sin_a = std::sin(-r->orientation);
    double speed = std::min(1.5, dist * 2.0); // Slower for dribbling
    result.action.type = RobotAction::BODY_VELOCITY;
    result.action.values[0] = speed; // Forward only while dribbling
    result.action.values[2] = clamp(5.0 * error, -8.0, 8.0);
    result.action.dribbler = true;
    return result;
}

// ============================================================
// MarkRobot
// ============================================================

SkillResult MarkRobotSkill::execute(const WorldState& state, int team, int robot_id, double dt) {
    SkillResult result;
    result.trace = "mark_robot → go_to_pose(between ball and target)";

    const auto* self = findRobot(state, team, robot_id);
    const auto* target = findRobot(state, mark_team_, mark_id_);
    if (!self || !target) { result.complete = true; return result; }

    // Position between ball and target robot
    double mx = (state.ball.x + target->x) / 2.0;
    double my = (state.ball.y + target->y) / 2.0;
    double face_angle = std::atan2(target->y - mx, target->x - mx);

    GoToPoseSkill gtp(mx, my, face_angle, 0.2, 0.2);
    return gtp.execute(state, team, robot_id, dt);
}

// ============================================================
// BlockLane
// ============================================================

SkillResult BlockLaneSkill::execute(const WorldState& state, int team, int robot_id, double dt) {
    SkillResult result;
    result.trace = "block_lane → go_to_pose(midpoint of lane)";

    double mx = (from_x_ + to_x_) / 2.0;
    double my = (from_y_ + to_y_) / 2.0;
    double lane_angle = std::atan2(to_y_ - from_y_, to_x_ - from_x_);
    // Face perpendicular to the lane
    double face_angle = lane_angle + M_PI / 2.0;

    GoToPoseSkill gtp(mx, my, face_angle, 0.2, 0.3);
    return gtp.execute(state, team, robot_id, dt);
}

// ============================================================
// PlaceBall
// ============================================================

SkillResult PlaceBallSkill::execute(const WorldState& state, int team, int robot_id, double dt) {
    SkillResult result;
    result.trace = "place_ball → dribble_to_point → release";

    const auto* r = findRobot(state, team, robot_id);
    if (!r) { result.complete = true; return result; }

    double dist_to_target = std::sqrt((state.ball.x - target_x_) * (state.ball.x - target_x_) +
                                       (state.ball.y - target_y_) * (state.ball.y - target_y_));

    if (dist_to_target < 0.15) {
        // Release ball
        result.action.dribbler = false;
        result.complete = true;
        return result;
    }

    // Dribble ball to target
    DribbleToPointSkill dtp(target_x_, target_y_);
    return dtp.execute(state, team, robot_id, dt);
}

} // namespace grsim_scenarios
