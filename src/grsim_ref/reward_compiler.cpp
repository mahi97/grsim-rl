#include "grsim_ref/reward_compiler.h"
#include <cmath>
#include <algorithm>

namespace grsim_ref {

void RewardCompiler::addTerm(const RewardTerm& term) {
    terms_.push_back(term);
}

void RewardCompiler::clearTerms() {
    terms_.clear();
}

RewardSignal RewardCompiler::compute(
    const grsim_core::WorldState& current,
    const grsim_core::WorldState& previous,
    const std::vector<GameEvent>& events,
    int team
) const {
    RewardSignal signal;
    for (const auto& term : terms_) {
        double component = term.compute(current, previous, events, team) * term.weight;
        signal.components[term.name] = component;
        signal.reward += component;
    }

    // Check for terminal conditions
    for (const auto& e : events) {
        if (e.type == GameEventType::GOAL) {
            signal.done = true;
        }
        if (e.type == GameEventType::EPISODE_TIMEOUT || e.type == GameEventType::EPISODE_RESET) {
            signal.truncated = true;
        }
    }

    return signal;
}

// ---------- Predefined reward terms ----------

RewardTerm RewardCompiler::goalScoredReward(double value) {
    return {"goal_scored", value, [](const grsim_core::WorldState&, const grsim_core::WorldState&,
                                      const std::vector<GameEvent>& events, int team) -> double {
        for (const auto& e : events) {
            if (e.type == GameEventType::GOAL && e.by_team == team) return 1.0;
        }
        return 0.0;
    }};
}

RewardTerm RewardCompiler::goalConcededPenalty(double value) {
    return {"goal_conceded", value, [](const grsim_core::WorldState&, const grsim_core::WorldState&,
                                        const std::vector<GameEvent>& events, int team) -> double {
        for (const auto& e : events) {
            if (e.type == GameEventType::GOAL && e.by_team != team && e.by_team >= 0) return 1.0;
        }
        return 0.0;
    }};
}

RewardTerm RewardCompiler::ballProgressReward(double scale) {
    return {"ball_progress", scale, [](const grsim_core::WorldState& current,
                                        const grsim_core::WorldState& previous,
                                        const std::vector<GameEvent>&, int team) -> double {
        // Reward for ball moving towards opponent goal
        double target_x = (team == 0) ? 6.0 : -6.0; // Approximate goal x
        double prev_dist = std::abs(target_x - previous.ball.x);
        double curr_dist = std::abs(target_x - current.ball.x);
        return prev_dist - curr_dist; // Positive when moving towards goal
    }};
}

RewardTerm RewardCompiler::ballPossessionReward(double value) {
    return {"ball_possession", value, [](const grsim_core::WorldState& current,
                                          const grsim_core::WorldState&,
                                          const std::vector<GameEvent>&, int team) -> double {
        const auto& robots = (team == 0) ? current.blue_robots : current.yellow_robots;
        for (const auto& r : robots) {
            if (r.touching_ball) return 1.0;
        }
        return 0.0;
    }};
}

RewardTerm RewardCompiler::foulPenalty(double value) {
    return {"foul", value, [](const grsim_core::WorldState&, const grsim_core::WorldState&,
                               const std::vector<GameEvent>& events, int team) -> double {
        double count = 0;
        for (const auto& e : events) {
            if (e.by_team == team) {
                switch (e.type) {
                    case GameEventType::BOT_KICKED_BALL_TOO_FAST:
                    case GameEventType::ATTACKER_IN_DEFENSE_AREA:
                    case GameEventType::BOT_DRIBBLED_BALL_TOO_FAR:
                    case GameEventType::DOUBLE_TOUCH:
                    case GameEventType::BOT_CRASH_UNIQUE:
                        count += 1.0;
                        break;
                    default:
                        break;
                }
            }
        }
        return count;
    }};
}

RewardTerm RewardCompiler::ballSpeedPenalty(double value) {
    return {"ball_speed_penalty", value, [](const grsim_core::WorldState&, const grsim_core::WorldState&,
                                             const std::vector<GameEvent>& events, int team) -> double {
        for (const auto& e : events) {
            if (e.type == GameEventType::BOT_KICKED_BALL_TOO_FAST && e.by_team == team) return 1.0;
        }
        return 0.0;
    }};
}

RewardTerm RewardCompiler::defenseAreaViolationPenalty(double value) {
    return {"defense_area_violation", value, [](const grsim_core::WorldState&, const grsim_core::WorldState&,
                                                 const std::vector<GameEvent>& events, int team) -> double {
        for (const auto& e : events) {
            if (e.type == GameEventType::ATTACKER_IN_DEFENSE_AREA && e.by_team == team) return 1.0;
        }
        return 0.0;
    }};
}

RewardTerm RewardCompiler::ballPlacementSuccessReward(double value) {
    return {"ball_placement_success", value, [](const grsim_core::WorldState&, const grsim_core::WorldState&,
                                                 const std::vector<GameEvent>& events, int team) -> double {
        for (const auto& e : events) {
            if (e.type == GameEventType::BALL_PLACEMENT_SUCCEEDED && e.by_team == team) return 1.0;
        }
        return 0.0;
    }};
}

RewardTerm RewardCompiler::distanceToBallReward(double scale) {
    return {"distance_to_ball", scale, [](const grsim_core::WorldState& current,
                                           const grsim_core::WorldState&,
                                           const std::vector<GameEvent>&, int team) -> double {
        const auto& robots = (team == 0) ? current.blue_robots : current.yellow_robots;
        double min_dist = 1e6;
        for (const auto& r : robots) {
            if (!r.present) continue;
            double d = std::sqrt((r.x - current.ball.x) * (r.x - current.ball.x) +
                                (r.y - current.ball.y) * (r.y - current.ball.y));
            min_dist = std::min(min_dist, d);
        }
        return -min_dist; // Negative distance = reward for being close
    }};
}

RewardTerm RewardCompiler::timestepPenalty(double value) {
    return {"timestep", value, [](const grsim_core::WorldState&, const grsim_core::WorldState&,
                                   const std::vector<GameEvent>&, int) -> double {
        return 1.0; // Constant per-step penalty
    }};
}

// ---------- Predefined profiles ----------

RewardCompiler RewardCompiler::scoringProfile() {
    RewardCompiler rc;
    rc.addTerm(goalScoredReward(10.0));
    rc.addTerm(goalConcededPenalty(-10.0));
    rc.addTerm(ballProgressReward(0.1));
    rc.addTerm(foulPenalty(-1.0));
    rc.addTerm(timestepPenalty(-0.001));
    return rc;
}

RewardCompiler RewardCompiler::possessionProfile() {
    RewardCompiler rc;
    rc.addTerm(ballPossessionReward(0.5));
    rc.addTerm(distanceToBallReward(0.05));
    rc.addTerm(foulPenalty(-1.0));
    rc.addTerm(timestepPenalty(-0.001));
    return rc;
}

RewardCompiler RewardCompiler::placementProfile() {
    RewardCompiler rc;
    rc.addTerm(ballPlacementSuccessReward(10.0));
    rc.addTerm(foulPenalty(-1.0));
    rc.addTerm(timestepPenalty(-0.01));
    return rc;
}

RewardCompiler RewardCompiler::defensiveProfile() {
    RewardCompiler rc;
    rc.addTerm(goalConcededPenalty(-10.0));
    rc.addTerm(goalScoredReward(5.0));
    rc.addTerm(defenseAreaViolationPenalty(-2.0));
    rc.addTerm(timestepPenalty(-0.001));
    return rc;
}

RewardCompiler RewardCompiler::fullMatchProfile() {
    RewardCompiler rc;
    rc.addTerm(goalScoredReward(10.0));
    rc.addTerm(goalConcededPenalty(-10.0));
    rc.addTerm(ballProgressReward(0.05));
    rc.addTerm(ballPossessionReward(0.1));
    rc.addTerm(foulPenalty(-2.0));
    rc.addTerm(defenseAreaViolationPenalty(-1.0));
    rc.addTerm(ballSpeedPenalty(-0.5));
    rc.addTerm(timestepPenalty(-0.001));
    return rc;
}

} // namespace grsim_ref
