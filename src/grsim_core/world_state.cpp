#include "grsim_core/world_state.h"
#include <stdexcept>
#include <string>

namespace grsim_core {

const RobotState& WorldState::robot(int team, int id) const {
    const auto& robots = (team == 0) ? blue_robots : yellow_robots;
    for (const auto& r : robots) {
        if (r.id == id) return r;
    }
    throw std::out_of_range("Robot not found: team=" + std::to_string(team) + " id=" + std::to_string(id));
}

FlatObservation FlatObservation::fromWorldState(const WorldState& state) {
    FlatObservation obs;
    obs.num_blue = state.blue_robots.size();
    obs.num_yellow = state.yellow_robots.size();
    int total = obs.ball_dim + (obs.num_blue + obs.num_yellow) * obs.robot_dim;
    obs.data.resize(total);

    int idx = 0;
    // Ball state
    obs.data[idx++] = state.ball.x;
    obs.data[idx++] = state.ball.y;
    obs.data[idx++] = state.ball.z;
    obs.data[idx++] = state.ball.vx;
    obs.data[idx++] = state.ball.vy;
    obs.data[idx++] = state.ball.vz;

    // Blue robots
    for (const auto& r : state.blue_robots) {
        obs.data[idx++] = r.x;
        obs.data[idx++] = r.y;
        obs.data[idx++] = r.orientation;
        obs.data[idx++] = r.vx;
        obs.data[idx++] = r.vy;
        obs.data[idx++] = r.vw;
        obs.data[idx++] = r.touching_ball ? 1.0 : 0.0;
    }

    // Yellow robots
    for (const auto& r : state.yellow_robots) {
        obs.data[idx++] = r.x;
        obs.data[idx++] = r.y;
        obs.data[idx++] = r.orientation;
        obs.data[idx++] = r.vx;
        obs.data[idx++] = r.vy;
        obs.data[idx++] = r.vw;
        obs.data[idx++] = r.touching_ball ? 1.0 : 0.0;
    }

    return obs;
}

} // namespace grsim_core
