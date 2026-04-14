#include "grsim_core/noise.h"
#include <cmath>
#include <limits>

namespace grsim_core {

// ============================================================
// Construction
// ============================================================

ObservationNoise::ObservationNoise(const NoiseConfig& config, std::mt19937& rng)
    : cfg_(config)
    , rng_(rng)
    , uniform_(0.0, 1.0)
{
    rebuildDistributions();
}

void ObservationNoise::setConfig(const NoiseConfig& config) {
    cfg_ = config;
    rebuildDistributions();
}

void ObservationNoise::rebuildDistributions() {
    // std::normal_distribution with stddev = 0 is valid (always returns mean)
    dist_x_     = std::normal_distribution<double>(0.0, cfg_.deviation_x);
    dist_y_     = std::normal_distribution<double>(0.0, cfg_.deviation_y);
    dist_angle_ = std::normal_distribution<double>(0.0, cfg_.deviation_angle);
}

// ============================================================
// Gaussian noise
// ============================================================

void ObservationNoise::addNoiseToBall(BallState& ball) {
    ball.x += dist_x_(rng_);
    ball.y += dist_y_(rng_);
    // Ball z noise uses the average of x/y deviation (grSim does not
    // model z noise explicitly, but we mirror the magnitude).
    double dev_z = (cfg_.deviation_x + cfg_.deviation_y) * 0.5;
    if (dev_z > 0.0) {
        std::normal_distribution<double> dz(0.0, dev_z);
        ball.z += dz(rng_);
        // Clamp z to non-negative (ball cannot be underground)
        if (ball.z < 0.0) ball.z = 0.0;
    }
}

void ObservationNoise::addNoiseToRobot(RobotState& robot) {
    robot.x += dist_x_(rng_);
    robot.y += dist_y_(rng_);
    robot.orientation += dist_angle_(rng_);

    // Normalise orientation to [-pi, pi]
    while (robot.orientation >  M_PI) robot.orientation -= 2.0 * M_PI;
    while (robot.orientation < -M_PI) robot.orientation += 2.0 * M_PI;
}

void ObservationNoise::apply(WorldState& state) {
    if (!cfg_.enabled) return;

    addNoiseToBall(state.ball);

    for (auto& r : state.blue_robots) {
        if (r.present) addNoiseToRobot(r);
    }
    for (auto& r : state.yellow_robots) {
        if (r.present) addNoiseToRobot(r);
    }
}

// ============================================================
// Vanishing (random detection drops)
// ============================================================

void ObservationNoise::applyVanishing(WorldState& state) {
    if (!cfg_.enabled || !cfg_.vanishing_enabled) return;

    // Ball vanishing: mark position as NaN so downstream knows it is unseen
    if (cfg_.ball_vanishing > 0.0 && uniform_(rng_) < cfg_.ball_vanishing) {
        double nan = std::numeric_limits<double>::quiet_NaN();
        state.ball.x  = nan;
        state.ball.y  = nan;
        state.ball.z  = nan;
        state.ball.vx = nan;
        state.ball.vy = nan;
        state.ball.vz = nan;
    }

    // Blue robot vanishing
    if (cfg_.blue_vanishing > 0.0) {
        for (auto& r : state.blue_robots) {
            if (r.present && uniform_(rng_) < cfg_.blue_vanishing) {
                r.present = false;
            }
        }
    }

    // Yellow robot vanishing
    if (cfg_.yellow_vanishing > 0.0) {
        for (auto& r : state.yellow_robots) {
            if (r.present && uniform_(rng_) < cfg_.yellow_vanishing) {
                r.present = false;
            }
        }
    }
}

// ============================================================
// Combined
// ============================================================

void ObservationNoise::applyAll(WorldState& state) {
    apply(state);
    applyVanishing(state);
}

} // namespace grsim_core
