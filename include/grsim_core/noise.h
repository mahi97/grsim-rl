#ifndef GRSIM_CORE_NOISE_H
#define GRSIM_CORE_NOISE_H

#include "grsim_core/config.h"
#include "grsim_core/world_state.h"
#include <random>

namespace grsim_core {

/**
 * Applies observation noise to WorldState, mirroring grSim's noise model.
 *
 * grSim adds Gaussian noise to vision-reported positions and angles,
 * and randomly drops ("vanishes") detections to simulate real SSL-vision
 * behaviour.  This class reproduces that model so RL agents train
 * against realistic observations.
 */
class ObservationNoise {
public:
    /**
     * @param config  Noise parameters (deviations, vanishing probabilities).
     * @param rng     Mersenne Twister engine — caller owns lifetime.
     */
    ObservationNoise(const NoiseConfig& config, std::mt19937& rng);

    /** Update noise parameters at runtime (e.g. curriculum). */
    void setConfig(const NoiseConfig& config);
    const NoiseConfig& config() const { return cfg_; }

    /**
     * Add Gaussian noise to every position and orientation in @p state.
     *
     * Ball:   x += N(0, deviation_x),  y += N(0, deviation_y)
     * Robot:  x += N(0, deviation_x),  y += N(0, deviation_y),
     *         orientation += N(0, deviation_angle)
     *
     * Velocity fields are left untouched (they are derived quantities
     * in real SSL-vision and already noisy).
     */
    void apply(WorldState& state);

    /**
     * Randomly remove detections to simulate SSL-vision vanishing.
     *
     * For the ball, each call has probability ball_vanishing of setting
     * the ball position to NaN (caller must handle NaN as "not seen").
     *
     * For robots, each robot has probability blue_vanishing / yellow_vanishing
     * of having its ``present`` flag cleared for this frame.
     */
    void applyVanishing(WorldState& state);

    /**
     * Convenience: apply both Gaussian noise and vanishing in one call.
     */
    void applyAll(WorldState& state);

private:
    NoiseConfig cfg_;
    std::mt19937& rng_;

    // Cached distributions — recreated when config changes
    std::normal_distribution<double> dist_x_;
    std::normal_distribution<double> dist_y_;
    std::normal_distribution<double> dist_angle_;
    std::uniform_real_distribution<double> uniform_;  // [0, 1)

    void rebuildDistributions();

    void addNoiseToRobot(RobotState& robot);
    void addNoiseToBall(BallState& ball);
};

} // namespace grsim_core

#endif // GRSIM_CORE_NOISE_H
