#ifndef GRSIM_CORE_WORLD_STATE_H
#define GRSIM_CORE_WORLD_STATE_H

#include <vector>
#include <cstdint>

namespace grsim_core {

struct BallState {
    double x = 0.0, y = 0.0, z = 0.0;
    double vx = 0.0, vy = 0.0, vz = 0.0;
    // Angular velocity (for future use)
    double wx = 0.0, wy = 0.0, wz = 0.0;
};

struct RobotState {
    int id = 0;
    int team = 0;  // 0 = blue, 1 = yellow
    bool present = true;
    double x = 0.0, y = 0.0;
    double orientation = 0.0;  // radians
    double vx = 0.0, vy = 0.0;
    double vw = 0.0;  // angular velocity
    double wheel_speeds[4] = {0, 0, 0, 0};
    bool dribbler_on = false;
    bool touching_ball = false;
    bool infrared = false;
};

struct WorldState {
    double sim_time = 0.0;
    int frame_num = 0;
    BallState ball;
    std::vector<RobotState> blue_robots;
    std::vector<RobotState> yellow_robots;

    // Convenience accessors
    const RobotState& robot(int team, int id) const;
    int totalRobots() const { return blue_robots.size() + yellow_robots.size(); }
};

// Flattened observation suitable for NumPy arrays
struct FlatObservation {
    // Ball: x, y, z, vx, vy, vz (6)
    // Per robot: x, y, orientation, vx, vy, vw, touching_ball (7)
    // Total: 6 + N_robots * 7
    std::vector<double> data;
    int ball_dim = 6;
    int robot_dim = 7;
    int num_blue = 0;
    int num_yellow = 0;

    static FlatObservation fromWorldState(const WorldState& state);
};

// Actions for a single robot
struct RobotAction {
    enum Type {
        WHEEL_VELOCITY = 0,   // 4 wheel speeds
        BODY_VELOCITY = 1,    // vx, vy, vw (local frame)
        GLOBAL_VELOCITY = 2,  // vx, vy, vw (global frame)
    };

    Type type = BODY_VELOCITY;
    double values[4] = {0, 0, 0, 0};  // meaning depends on type
    double kick_speed = 0.0;
    double kick_angle = 0.0;  // 0 = flat, >0 = chip (degrees)
    bool dribbler = false;
};

// Actions for an entire team
struct TeamActions {
    std::vector<RobotAction> robot_actions;
};

// Full action set for both teams
struct Actions {
    TeamActions blue;
    TeamActions yellow;
};

} // namespace grsim_core

#endif // GRSIM_CORE_WORLD_STATE_H
