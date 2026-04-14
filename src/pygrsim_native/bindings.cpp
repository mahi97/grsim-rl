/**
 * pygrsim_native — pybind11 bindings for grsim_core, grsim_ref, grsim_scenarios.
 *
 * This module exposes the C++ SimulationEngine and related types to Python,
 * enabling the pygrsim Gymnasium/PettingZoo environments to run real physics
 * instead of mock mode.
 *
 * Build:
 *   cmake -DBUILD_PYTHON_BINDINGS=ON ..
 *   make pygrsim_native
 *
 * Usage from Python:
 *   import pygrsim_native as _n
 *   engine = _n.SimulationEngine(_n.SimConfig.defaults())
 *   engine.reset(42)
 *   result = engine.step(_n.Actions(), 0.016)
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/functional.h>

#include "grsim_core/engine.h"
#include "grsim_core/config.h"
#include "grsim_core/world_state.h"
#include "grsim_ref/events.h"
#include "grsim_ref/reward_compiler.h"
#include "grsim_scenarios/scenario.h"
#include "grsim_scenarios/skills.h"
#include "grsim_scenarios/coach.h"

namespace py = pybind11;
using namespace grsim_core;
using namespace grsim_ref;
using namespace grsim_scenarios;

// Helper: WorldState → numpy flat observation
static py::array_t<double> worldStateToNumpy(const WorldState& state) {
    auto obs = FlatObservation::fromWorldState(state);
    py::array_t<double> arr(obs.data.size());
    auto buf = arr.mutable_unchecked<1>();
    for (size_t i = 0; i < obs.data.size(); i++) {
        buf(i) = obs.data[i];
    }
    return arr;
}

// Helper: numpy action array → Actions struct for single-team body velocity
static Actions numpyToActions(py::array_t<double> action_arr, int team, int num_robots) {
    Actions actions;
    auto buf = action_arr.unchecked<1>();
    int stride = 6; // vx, vy, vw, kick_speed, kick_angle, dribbler

    auto& team_actions = (team == 0) ? actions.blue : actions.yellow;
    team_actions.robot_actions.resize(num_robots);

    for (int i = 0; i < num_robots; i++) {
        int base = i * stride;
        if (base + stride > buf.shape(0)) break;

        auto& ra = team_actions.robot_actions[i];
        ra.type = RobotAction::BODY_VELOCITY;
        ra.values[0] = buf(base + 0) * 3.0;   // vx scaled to m/s
        ra.values[1] = buf(base + 1) * 3.0;   // vy scaled to m/s
        ra.values[2] = buf(base + 2) * 10.0;  // vw scaled to rad/s
        ra.kick_speed = std::max(0.0, buf(base + 3)) * 6.5;  // kick speed scaled
        ra.kick_angle = std::max(0.0, buf(base + 4)) * 60.0; // chip angle degrees
        ra.dribbler = buf(base + 5) > 0.0;
    }

    return actions;
}

PYBIND11_MODULE(pygrsim_native, m) {
    m.doc() = "Native C++ bindings for grsim-rl simulation engine";

    // ================================================================
    // grsim_core::SimConfig and sub-structs
    // ================================================================

    py::class_<FieldConfig>(m, "FieldConfig")
        .def(py::init<>())
        .def_readwrite("field_length", &FieldConfig::field_length)
        .def_readwrite("field_width", &FieldConfig::field_width)
        .def_readwrite("goal_width", &FieldConfig::goal_width)
        .def_readwrite("goal_depth", &FieldConfig::goal_depth)
        .def_readwrite("goal_height", &FieldConfig::goal_height)
        .def_readwrite("penalty_area_depth", &FieldConfig::penalty_area_depth)
        .def_readwrite("penalty_area_width", &FieldConfig::penalty_area_width)
        .def_readwrite("center_circle_radius", &FieldConfig::center_circle_radius)
        .def_readwrite("wall_thickness", &FieldConfig::wall_thickness);

    py::class_<BallConfig>(m, "BallConfig")
        .def(py::init<>())
        .def_readwrite("radius", &BallConfig::radius)
        .def_readwrite("mass", &BallConfig::mass)
        .def_readwrite("friction", &BallConfig::friction)
        .def_readwrite("bounce", &BallConfig::bounce);

    py::class_<RobotConfig>(m, "RobotConfig")
        .def(py::init<>())
        .def_readwrite("radius", &RobotConfig::radius)
        .def_readwrite("height", &RobotConfig::height)
        .def_readwrite("body_mass", &RobotConfig::body_mass)
        .def_readwrite("max_linear_kick_speed", &RobotConfig::max_linear_kick_speed)
        .def_readwrite("vel_absolute_max", &RobotConfig::vel_absolute_max)
        .def_readwrite("vel_angular_max", &RobotConfig::vel_angular_max);

    py::class_<SimulationParams>(m, "SimulationParams")
        .def(py::init<>())
        .def_readwrite("delta_time", &SimulationParams::delta_time)
        .def_readwrite("gravity", &SimulationParams::gravity)
        .def_readwrite("robots_per_team", &SimulationParams::robots_per_team);

    py::class_<NoiseConfig>(m, "NoiseConfig")
        .def(py::init<>())
        .def_readwrite("enabled", &NoiseConfig::enabled)
        .def_readwrite("deviation_x", &NoiseConfig::deviation_x)
        .def_readwrite("deviation_y", &NoiseConfig::deviation_y)
        .def_readwrite("deviation_angle", &NoiseConfig::deviation_angle);

    py::class_<SimConfig>(m, "SimConfig")
        .def(py::init<>())
        .def_readwrite("field", &SimConfig::field)
        .def_readwrite("ball", &SimConfig::ball)
        .def_readwrite("blue_robot", &SimConfig::blue_robot)
        .def_readwrite("yellow_robot", &SimConfig::yellow_robot)
        .def_readwrite("sim", &SimConfig::sim)
        .def_readwrite("noise", &SimConfig::noise)
        .def_static("defaults", &SimConfig::defaults)
        .def_static("divisionA", &SimConfig::divisionA)
        .def_static("divisionB", &SimConfig::divisionB);

    // ================================================================
    // grsim_core::WorldState types
    // ================================================================

    py::class_<BallState>(m, "BallState")
        .def(py::init<>())
        .def_readwrite("x", &BallState::x)
        .def_readwrite("y", &BallState::y)
        .def_readwrite("z", &BallState::z)
        .def_readwrite("vx", &BallState::vx)
        .def_readwrite("vy", &BallState::vy)
        .def_readwrite("vz", &BallState::vz);

    py::class_<RobotState>(m, "RobotState")
        .def(py::init<>())
        .def_readwrite("id", &RobotState::id)
        .def_readwrite("team", &RobotState::team)
        .def_readwrite("present", &RobotState::present)
        .def_readwrite("x", &RobotState::x)
        .def_readwrite("y", &RobotState::y)
        .def_readwrite("orientation", &RobotState::orientation)
        .def_readwrite("vx", &RobotState::vx)
        .def_readwrite("vy", &RobotState::vy)
        .def_readwrite("vw", &RobotState::vw)
        .def_readwrite("dribbler_on", &RobotState::dribbler_on)
        .def_readwrite("touching_ball", &RobotState::touching_ball);

    py::class_<WorldState>(m, "WorldState")
        .def(py::init<>())
        .def_readwrite("sim_time", &WorldState::sim_time)
        .def_readwrite("frame_num", &WorldState::frame_num)
        .def_readwrite("ball", &WorldState::ball)
        .def_readwrite("blue_robots", &WorldState::blue_robots)
        .def_readwrite("yellow_robots", &WorldState::yellow_robots)
        .def("robot", &WorldState::robot, py::return_value_policy::reference)
        .def("totalRobots", &WorldState::totalRobots)
        .def("to_numpy", [](const WorldState& self) {
            return worldStateToNumpy(self);
        });

    // ================================================================
    // grsim_core::Actions
    // ================================================================

    py::enum_<RobotAction::Type>(m, "ActionType")
        .value("WHEEL_VELOCITY", RobotAction::WHEEL_VELOCITY)
        .value("BODY_VELOCITY", RobotAction::BODY_VELOCITY)
        .value("GLOBAL_VELOCITY", RobotAction::GLOBAL_VELOCITY);

    py::class_<RobotAction>(m, "RobotAction")
        .def(py::init<>())
        .def_readwrite("type", &RobotAction::type)
        .def_readwrite("kick_speed", &RobotAction::kick_speed)
        .def_readwrite("kick_angle", &RobotAction::kick_angle)
        .def_readwrite("dribbler", &RobotAction::dribbler)
        .def("set_values", [](RobotAction& self, double v0, double v1, double v2, double v3) {
            self.values[0] = v0; self.values[1] = v1;
            self.values[2] = v2; self.values[3] = v3;
        });

    py::class_<TeamActions>(m, "TeamActions")
        .def(py::init<>())
        .def_readwrite("robot_actions", &TeamActions::robot_actions);

    py::class_<Actions>(m, "Actions")
        .def(py::init<>())
        .def_readwrite("blue", &Actions::blue)
        .def_readwrite("yellow", &Actions::yellow);

    // ================================================================
    // grsim_core::SimulationEngine
    // ================================================================

    py::class_<SimEvent>(m, "SimEvent")
        .def(py::init<>())
        .def_readwrite("type", &SimEvent::type)
        .def_readwrite("timestamp", &SimEvent::timestamp)
        .def_readwrite("team", &SimEvent::team)
        .def_readwrite("robot_id", &SimEvent::robot_id)
        .def_readwrite("x", &SimEvent::x)
        .def_readwrite("y", &SimEvent::y);

    py::enum_<EventType>(m, "EventType")
        .value("NONE", EventType::NONE)
        .value("BALL_OUT_TOUCHLINE", EventType::BALL_OUT_TOUCHLINE)
        .value("BALL_OUT_GOALLINE", EventType::BALL_OUT_GOALLINE)
        .value("GOAL_SCORED_BLUE", EventType::GOAL_SCORED_BLUE)
        .value("GOAL_SCORED_YELLOW", EventType::GOAL_SCORED_YELLOW)
        .value("BALL_KICKED", EventType::BALL_KICKED)
        .value("BALL_CHIPPED", EventType::BALL_CHIPPED);

    py::class_<StepResult>(m, "StepResult")
        .def(py::init<>())
        .def_readwrite("state", &StepResult::state)
        .def_readwrite("events", &StepResult::events)
        .def_readwrite("ball_in_play", &StepResult::ball_in_play);

    py::class_<SimulationEngine>(m, "SimulationEngine")
        .def(py::init<const SimConfig&>())
        .def("step", &SimulationEngine::step,
             py::arg("actions"), py::arg("dt") = -1.0)
        .def("stepPhysics", &SimulationEngine::stepPhysics,
             py::arg("dt") = -1.0)
        .def("getState", &SimulationEngine::getState)
        .def("observe", [](const SimulationEngine& self) {
            return worldStateToNumpy(self.getState());
        })
        .def("reset", py::overload_cast<uint64_t>(&SimulationEngine::reset),
             py::arg("seed") = 0)
        .def("reset_with_state", py::overload_cast<uint64_t, const WorldState&>(&SimulationEngine::reset),
             py::arg("seed"), py::arg("initial_state"))
        .def("snapshot", &SimulationEngine::snapshot)
        .def("restore", &SimulationEngine::restore)
        .def("setWheelSpeeds", &SimulationEngine::setWheelSpeeds)
        .def("setBodyVelocity", &SimulationEngine::setBodyVelocity)
        .def("setGlobalVelocity", &SimulationEngine::setGlobalVelocity)
        .def("kick", &SimulationEngine::kick,
             py::arg("team"), py::arg("robot_id"),
             py::arg("speed"), py::arg("angle_deg") = 0.0)
        .def("setDribbler", &SimulationEngine::setDribbler)
        .def("teleportBall", &SimulationEngine::teleportBall,
             py::arg("x"), py::arg("y"), py::arg("z") = 0.0,
             py::arg("vx") = 0.0, py::arg("vy") = 0.0, py::arg("vz") = 0.0)
        .def("teleportRobot", &SimulationEngine::teleportRobot,
             py::arg("team"), py::arg("robot_id"),
             py::arg("x"), py::arg("y"), py::arg("orientation_rad"),
             py::arg("present") = true)
        .def("setRobotPresent", &SimulationEngine::setRobotPresent)
        .def("robotsPerTeam", &SimulationEngine::robotsPerTeam)
        .def("config", &SimulationEngine::config, py::return_value_policy::reference)
        .def("simTime", &SimulationEngine::simTime)
        .def("frameNumber", &SimulationEngine::frameNumber)
        .def("isBallInField", &SimulationEngine::isBallInField)
        .def("isBallInGoal", &SimulationEngine::isBallInGoal)
        .def("isPositionInDefenseArea", &SimulationEngine::isPositionInDefenseArea)
        .def("isPositionInField", &SimulationEngine::isPositionInField)
        // Convenience: step with numpy action array for a single team
        .def("step_numpy", [](SimulationEngine& self,
                               py::array_t<double> action_arr,
                               int team, int num_robots, double dt) {
            Actions actions = numpyToActions(action_arr, team, num_robots);
            return self.step(actions, dt);
        }, py::arg("action"), py::arg("team") = 0,
           py::arg("num_robots") = 1, py::arg("dt") = -1.0);

    // ================================================================
    // grsim_ref::GameEvent types
    // ================================================================

    py::enum_<GameEventType>(m, "GameEventType")
        .value("NONE", GameEventType::NONE)
        .value("BALL_LEFT_FIELD_TOUCH_LINE", GameEventType::BALL_LEFT_FIELD_TOUCH_LINE)
        .value("BALL_LEFT_FIELD_GOAL_LINE", GameEventType::BALL_LEFT_FIELD_GOAL_LINE)
        .value("GOAL", GameEventType::GOAL)
        .value("BOT_KICKED_BALL_TOO_FAST", GameEventType::BOT_KICKED_BALL_TOO_FAST)
        .value("ATTACKER_IN_DEFENSE_AREA", GameEventType::ATTACKER_IN_DEFENSE_AREA)
        .value("BOT_DRIBBLED_BALL_TOO_FAR", GameEventType::BOT_DRIBBLED_BALL_TOO_FAR)
        .value("DOUBLE_TOUCH", GameEventType::DOUBLE_TOUCH)
        .value("BOT_CRASH_UNIQUE", GameEventType::BOT_CRASH_UNIQUE)
        .value("BOT_CRASH_DRAWN", GameEventType::BOT_CRASH_DRAWN)
        .value("BALL_PLACEMENT_SUCCEEDED", GameEventType::BALL_PLACEMENT_SUCCEEDED)
        .value("BALL_PLACEMENT_FAILED", GameEventType::BALL_PLACEMENT_FAILED)
        .value("NO_PROGRESS_IN_GAME", GameEventType::NO_PROGRESS_IN_GAME)
        .value("TOO_MANY_ROBOTS", GameEventType::TOO_MANY_ROBOTS);

    py::class_<GameEvent>(m, "GameEvent")
        .def(py::init<>())
        .def_readwrite("type", &GameEvent::type)
        .def_readwrite("timestamp", &GameEvent::timestamp)
        .def_readwrite("by_team", &GameEvent::by_team)
        .def_readwrite("by_bot", &GameEvent::by_bot)
        .def_readwrite("x", &GameEvent::x)
        .def_readwrite("y", &GameEvent::y)
        .def_readwrite("ball_speed", &GameEvent::ball_speed)
        .def_readwrite("confidence", &GameEvent::confidence)
        .def_readwrite("rule_ref", &GameEvent::rule_ref)
        .def("description", &GameEvent::description);

    py::class_<EventDetectorRegistry>(m, "EventDetectorRegistry")
        .def_static("createDefault", [](const SimConfig& config) {
            auto reg = std::make_unique<EventDetectorRegistry>(
                EventDetectorRegistry::createDefault(config));
            return reg;
        })
        .def("detectAll", &EventDetectorRegistry::detectAll)
        .def("resetAll", &EventDetectorRegistry::resetAll);

    // ================================================================
    // grsim_ref::RewardCompiler
    // ================================================================

    py::class_<RewardSignal>(m, "RewardSignal")
        .def(py::init<>())
        .def_readwrite("reward", &RewardSignal::reward)
        .def_readwrite("done", &RewardSignal::done)
        .def_readwrite("truncated", &RewardSignal::truncated)
        .def_readwrite("components", &RewardSignal::components);

    py::class_<RewardCompiler>(m, "RewardCompiler")
        .def(py::init<>())
        .def("compute", &RewardCompiler::compute)
        .def_static("scoringProfile", &RewardCompiler::scoringProfile)
        .def_static("possessionProfile", &RewardCompiler::possessionProfile)
        .def_static("placementProfile", &RewardCompiler::placementProfile)
        .def_static("defensiveProfile", &RewardCompiler::defensiveProfile)
        .def_static("fullMatchProfile", &RewardCompiler::fullMatchProfile);

    // ================================================================
    // grsim_scenarios::Scenario registry
    // ================================================================

    py::class_<ScenarioRegistry>(m, "ScenarioRegistry")
        .def_static("instance", &ScenarioRegistry::instance,
                     py::return_value_policy::reference)
        .def("listScenarios", &ScenarioRegistry::listScenarios);

    // ================================================================
    // Coach-level types
    // ================================================================

    py::enum_<Formation>(m, "Formation")
        .value("DEFAULT", Formation::DEFAULT)
        .value("DEFENSIVE_4_1", Formation::DEFENSIVE_4_1)
        .value("BALANCED_3_2", Formation::BALANCED_3_2)
        .value("ATTACKING_2_3", Formation::ATTACKING_2_3)
        .value("WIDE_2_2_1", Formation::WIDE_2_2_1)
        .value("KICKOFF", Formation::KICKOFF);

    py::enum_<PressingLevel>(m, "PressingLevel")
        .value("PASSIVE", PressingLevel::PASSIVE)
        .value("MODERATE", PressingLevel::MODERATE)
        .value("HIGH_PRESS", PressingLevel::HIGH_PRESS);

    py::class_<CoachCommand>(m, "CoachCommand")
        .def(py::init<>())
        .def_readwrite("formation", &CoachCommand::formation)
        .def_readwrite("pressing", &CoachCommand::pressing)
        .def_readwrite("allow_chip_kicks", &CoachCommand::allow_chip_kicks);

    // ================================================================
    // Utility: high-level step for Gymnasium integration
    // ================================================================

    m.def("create_engine", [](const std::string& scenario_name) -> SimulationEngine {
        auto scenario = ScenarioRegistry::instance().create(scenario_name);
        if (!scenario) {
            return SimulationEngine(SimConfig::defaults());
        }
        return SimulationEngine(scenario->simConfig());
    }, py::arg("scenario_name") = "empty_field_shot");

    m.def("actions_from_numpy", &numpyToActions,
          py::arg("action_arr"), py::arg("team") = 0, py::arg("num_robots") = 1);
}
