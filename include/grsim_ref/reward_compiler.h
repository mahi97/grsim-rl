#ifndef GRSIM_REF_REWARD_COMPILER_H
#define GRSIM_REF_REWARD_COMPILER_H

#include "grsim_ref/events.h"
#include "grsim_core/world_state.h"
#include <string>
#include <map>
#include <functional>

namespace grsim_ref {

// Reward signal for a single team
struct RewardSignal {
    double reward = 0.0;
    bool done = false;
    bool truncated = false;
    std::map<std::string, double> components;  // Breakdown for debugging
};

// Reward term: a named function that contributes to the total reward
struct RewardTerm {
    std::string name;
    double weight = 1.0;
    // Function: (current_state, previous_state, events, team) → reward component
    std::function<double(
        const grsim_core::WorldState&,
        const grsim_core::WorldState&,
        const std::vector<GameEvent>&,
        int team
    )> compute;
};

// Reward compiler: consumes events + state → task-specific rewards
// Separate from event detection to keep official rule logic clean
class RewardCompiler {
public:
    void addTerm(const RewardTerm& term);
    void clearTerms();

    RewardSignal compute(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& previous,
        const std::vector<GameEvent>& events,
        int team
    ) const;

    // Predefined reward term factories
    static RewardTerm goalScoredReward(double value = 10.0);
    static RewardTerm goalConcededPenalty(double value = -10.0);
    static RewardTerm ballProgressReward(double scale = 1.0);
    static RewardTerm ballPossessionReward(double value = 0.1);
    static RewardTerm foulPenalty(double value = -1.0);
    static RewardTerm ballSpeedPenalty(double value = -0.5);
    static RewardTerm defenseAreaViolationPenalty(double value = -0.5);
    static RewardTerm ballPlacementSuccessReward(double value = 5.0);
    static RewardTerm distanceToBallReward(double scale = 0.01);
    static RewardTerm timestepPenalty(double value = -0.001);

    // Predefined reward profiles
    static RewardCompiler scoringProfile();       // Optimize for goals
    static RewardCompiler possessionProfile();    // Optimize for ball control
    static RewardCompiler placementProfile();     // Optimize for ball placement
    static RewardCompiler defensiveProfile();     // Optimize for preventing goals
    static RewardCompiler fullMatchProfile();     // Balanced match play

private:
    std::vector<RewardTerm> terms_;
};

} // namespace grsim_ref

#endif // GRSIM_REF_REWARD_COMPILER_H
