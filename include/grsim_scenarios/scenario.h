#ifndef GRSIM_SCENARIOS_SCENARIO_H
#define GRSIM_SCENARIOS_SCENARIO_H

#include "grsim_core/engine.h"
#include "grsim_core/world_state.h"
#include "grsim_ref/events.h"
#include "grsim_ref/reward_compiler.h"

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <random>
#include <map>

namespace grsim_scenarios {

// Action abstraction levels
enum class ActionLevel {
    WHEEL = 0,      // Raw wheel velocities
    BODY = 1,       // Body-frame velocity (vx, vy, vw)
    SKILL = 2,      // Skill commands (go_to_pose, kick_to_point, etc.)
    COACH = 3,      // Coach-level commands (formations, plays)
};

// Scenario termination status
struct TerminationStatus {
    bool done = false;        // Episode complete (success or failure)
    bool success = false;     // True if success criteria met
    bool truncated = false;   // True if time limit reached
    std::string reason;
};

// Scenario step result
struct ScenarioStepResult {
    grsim_core::WorldState state;
    grsim_ref::RewardSignal reward;
    TerminationStatus termination;
    std::vector<grsim_ref::GameEvent> events;
    std::map<std::string, double> info;  // Extra metrics
};

// Base scenario definition
class Scenario {
public:
    virtual ~Scenario() = default;

    // Metadata
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual std::vector<ActionLevel> supportedActionLevels() const = 0;

    // Configuration
    virtual grsim_core::SimConfig simConfig() const;
    virtual int blueRobotCount() const = 0;
    virtual int yellowRobotCount() const = 0;
    virtual double maxEpisodeTime() const = 0;  // seconds, 0 = unlimited

    // Initial state generation (may use RNG for curriculum)
    virtual grsim_core::WorldState generateInitialState(std::mt19937& rng) const = 0;

    // Termination criteria
    virtual TerminationStatus checkTermination(
        const grsim_core::WorldState& state,
        const std::vector<grsim_ref::GameEvent>& events,
        double elapsed_time
    ) const = 0;

    // Reward configuration
    virtual grsim_ref::RewardCompiler rewardCompiler() const = 0;

    // Event logging policy: which events to log for this scenario
    virtual bool shouldLogEvent(const grsim_ref::GameEvent& event) const {
        return true; // Log all events by default
    }

    // Reset policy
    virtual bool shouldResetOnEvent(const grsim_ref::GameEvent& event) const {
        return false; // Don't auto-reset by default
    }
};

// Scenario registry
class ScenarioRegistry {
public:
    static ScenarioRegistry& instance();

    void registerScenario(const std::string& name, std::function<std::unique_ptr<Scenario>()> factory);
    std::unique_ptr<Scenario> create(const std::string& name) const;
    std::vector<std::string> listScenarios() const;

private:
    std::map<std::string, std::function<std::unique_ptr<Scenario>()>> factories_;
};

// Macro for registering scenarios
#define REGISTER_SCENARIO(name, cls) \
    static bool _reg_##cls = []() { \
        ScenarioRegistry::instance().registerScenario(name, []() { return std::make_unique<cls>(); }); \
        return true; \
    }()

// ============================================================
// Built-in scenarios
// ============================================================

class EmptyFieldShotScenario : public Scenario {
public:
    std::string name() const override { return "empty_field_shot"; }
    std::string description() const override { return "Single robot shoots on empty goal"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::WHEEL, ActionLevel::BODY, ActionLevel::SKILL}; }
    int blueRobotCount() const override { return 1; }
    int yellowRobotCount() const override { return 0; }
    double maxEpisodeTime() const override { return 10.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

class OneVsOneDribbleScenario : public Scenario {
public:
    std::string name() const override { return "1v1_dribble"; }
    std::string description() const override { return "1v1 dribble past defender to goal"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::WHEEL, ActionLevel::BODY, ActionLevel::SKILL}; }
    int blueRobotCount() const override { return 1; }
    int yellowRobotCount() const override { return 1; }
    double maxEpisodeTime() const override { return 15.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

class TwoVsOneAttackScenario : public Scenario {
public:
    std::string name() const override { return "2v1_attack"; }
    std::string description() const override { return "2 attackers vs 1 defender"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::WHEEL, ActionLevel::BODY, ActionLevel::SKILL}; }
    int blueRobotCount() const override { return 2; }
    int yellowRobotCount() const override { return 1; }
    double maxEpisodeTime() const override { return 15.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

class GoalkeeperSaveScenario : public Scenario {
public:
    std::string name() const override { return "goalkeeper_save"; }
    std::string description() const override { return "Goalkeeper saves incoming shots"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::WHEEL, ActionLevel::BODY, ActionLevel::SKILL}; }
    int blueRobotCount() const override { return 0; }
    int yellowRobotCount() const override { return 1; }
    double maxEpisodeTime() const override { return 5.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

class BallPlacementScenario : public Scenario {
public:
    std::string name() const override { return "ball_placement"; }
    std::string description() const override { return "Place ball at target position"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::WHEEL, ActionLevel::BODY, ActionLevel::SKILL}; }
    int blueRobotCount() const override { return 1; }
    int yellowRobotCount() const override { return 0; }
    double maxEpisodeTime() const override { return 30.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

class KickoffAttackScenario : public Scenario {
public:
    std::string name() const override { return "kickoff_attack"; }
    std::string description() const override { return "Execute kickoff and attack"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::BODY, ActionLevel::SKILL, ActionLevel::COACH}; }
    int blueRobotCount() const override { return 6; }
    int yellowRobotCount() const override { return 6; }
    double maxEpisodeTime() const override { return 20.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

class FreeKickAttackScenario : public Scenario {
public:
    std::string name() const override { return "free_kick_attack"; }
    std::string description() const override { return "Execute direct free kick"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::BODY, ActionLevel::SKILL, ActionLevel::COACH}; }
    int blueRobotCount() const override { return 6; }
    int yellowRobotCount() const override { return 6; }
    double maxEpisodeTime() const override { return 15.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

class ThreeVsThreePossessionScenario : public Scenario {
public:
    std::string name() const override { return "3v3_possession"; }
    std::string description() const override { return "3v3 keep possession game"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::BODY, ActionLevel::SKILL, ActionLevel::COACH}; }
    int blueRobotCount() const override { return 3; }
    int yellowRobotCount() const override { return 3; }
    double maxEpisodeTime() const override { return 30.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

class ThreeVsTwoCounterattackScenario : public Scenario {
public:
    std::string name() const override { return "3v2_counterattack"; }
    std::string description() const override { return "3v2 fast counterattack"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::BODY, ActionLevel::SKILL, ActionLevel::COACH}; }
    int blueRobotCount() const override { return 3; }
    int yellowRobotCount() const override { return 2; }
    double maxEpisodeTime() const override { return 15.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

class MiniGameFullPlayScenario : public Scenario {
public:
    std::string name() const override { return "mini_game_full"; }
    std::string description() const override { return "Full 6v6 mini game (Division B field)"; }
    std::vector<ActionLevel> supportedActionLevels() const override { return {ActionLevel::BODY, ActionLevel::SKILL, ActionLevel::COACH}; }
    grsim_core::SimConfig simConfig() const override { return grsim_core::SimConfig::divisionB(); }
    int blueRobotCount() const override { return 6; }
    int yellowRobotCount() const override { return 6; }
    double maxEpisodeTime() const override { return 120.0; }
    grsim_core::WorldState generateInitialState(std::mt19937& rng) const override;
    TerminationStatus checkTermination(const grsim_core::WorldState& state, const std::vector<grsim_ref::GameEvent>& events, double elapsed) const override;
    grsim_ref::RewardCompiler rewardCompiler() const override;
};

} // namespace grsim_scenarios

#endif // GRSIM_SCENARIOS_SCENARIO_H
