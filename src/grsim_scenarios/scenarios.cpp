/**
 * Built-in scenario implementations.
 * Each scenario defines initial state, termination, and reward.
 */

#include "grsim_scenarios/scenario.h"
#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace grsim_scenarios {

using namespace grsim_core;
using namespace grsim_ref;

// ============================================================
// Scenario Registry
// ============================================================

ScenarioRegistry& ScenarioRegistry::instance() {
    static ScenarioRegistry reg;
    return reg;
}

void ScenarioRegistry::registerScenario(const std::string& name,
                                         std::function<std::unique_ptr<Scenario>()> factory) {
    factories_[name] = std::move(factory);
}

std::unique_ptr<Scenario> ScenarioRegistry::create(const std::string& name) const {
    auto it = factories_.find(name);
    if (it != factories_.end()) return it->second();
    return nullptr;
}

std::vector<std::string> ScenarioRegistry::listScenarios() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : factories_) names.push_back(name);
    return names;
}

// Default sim config
SimConfig Scenario::simConfig() const {
    return SimConfig::divisionA();
}

// ============================================================
// Helper: make a robot state
// ============================================================

static RobotState makeRobot(int id, int team, double x, double y, double ori = 0.0) {
    RobotState r;
    r.id = id; r.team = team;
    r.x = x; r.y = y; r.orientation = ori;
    r.present = true;
    return r;
}

static bool hasGoalEvent(const std::vector<GameEvent>& events, int team) {
    for (const auto& e : events) {
        if (e.type == GameEventType::GOAL && e.by_team == team) return true;
    }
    return false;
}

static bool hasBallLeftField(const std::vector<GameEvent>& events) {
    for (const auto& e : events) {
        if (e.type == GameEventType::BALL_LEFT_FIELD_TOUCH_LINE ||
            e.type == GameEventType::BALL_LEFT_FIELD_GOAL_LINE) return true;
    }
    return false;
}

// ============================================================
// Empty Field Shot
// ============================================================

WorldState EmptyFieldShotScenario::generateInitialState(std::mt19937& rng) const {
    WorldState state;
    std::uniform_real_distribution<double> y_dist(-2.0, 2.0);
    double ball_x = 3.0;
    double ball_y = y_dist(rng);

    state.ball = {ball_x, ball_y, 0.0215, 0, 0, 0};
    state.blue_robots.push_back(makeRobot(0, 0, ball_x - 0.15, ball_y, 0.0));
    return state;
}

TerminationStatus EmptyFieldShotScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    if (hasGoalEvent(events, 0)) { ts.done = true; ts.success = true; ts.reason = "goal"; }
    else if (hasBallLeftField(events)) { ts.done = true; ts.reason = "ball_out"; }
    else if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler EmptyFieldShotScenario::rewardCompiler() const {
    return RewardCompiler::scoringProfile();
}

// ============================================================
// 1v1 Dribble
// ============================================================

WorldState OneVsOneDribbleScenario::generateInitialState(std::mt19937& rng) const {
    WorldState state;
    std::uniform_real_distribution<double> y_dist(-1.5, 1.5);
    double ball_y = y_dist(rng);

    state.ball = {0.0, ball_y, 0.0215, 0, 0, 0};
    state.blue_robots.push_back(makeRobot(0, 0, -0.15, ball_y, 0.0));
    state.yellow_robots.push_back(makeRobot(0, 1, 3.0, 0.0, M_PI));
    return state;
}

TerminationStatus OneVsOneDribbleScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    if (hasGoalEvent(events, 0)) { ts.done = true; ts.success = true; ts.reason = "goal"; }
    else if (hasBallLeftField(events)) { ts.done = true; ts.reason = "ball_out"; }
    else if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler OneVsOneDribbleScenario::rewardCompiler() const {
    RewardCompiler rc;
    rc.addTerm(RewardCompiler::goalScoredReward(10.0));
    rc.addTerm(RewardCompiler::ballProgressReward(0.2));
    rc.addTerm(RewardCompiler::ballPossessionReward(0.1));
    rc.addTerm(RewardCompiler::foulPenalty(-1.0));
    rc.addTerm(RewardCompiler::timestepPenalty(-0.001));
    return rc;
}

// ============================================================
// 2v1 Attack
// ============================================================

WorldState TwoVsOneAttackScenario::generateInitialState(std::mt19937& rng) const {
    WorldState state;
    std::uniform_real_distribution<double> y_dist(-1.0, 1.0);
    double y_off = y_dist(rng);

    state.ball = {1.0, y_off, 0.0215, 0, 0, 0};
    state.blue_robots.push_back(makeRobot(0, 0, 0.85, y_off, 0.0));
    state.blue_robots.push_back(makeRobot(1, 0, 0.5, y_off + 1.5, 0.0));
    state.yellow_robots.push_back(makeRobot(0, 1, 3.5, 0.0, M_PI));
    return state;
}

TerminationStatus TwoVsOneAttackScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    if (hasGoalEvent(events, 0)) { ts.done = true; ts.success = true; ts.reason = "goal"; }
    else if (hasBallLeftField(events)) { ts.done = true; ts.reason = "ball_out"; }
    else if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler TwoVsOneAttackScenario::rewardCompiler() const {
    return RewardCompiler::scoringProfile();
}

// ============================================================
// Goalkeeper Save
// ============================================================

WorldState GoalkeeperSaveScenario::generateInitialState(std::mt19937& rng) const {
    WorldState state;
    std::uniform_real_distribution<double> ball_y(-0.8, 0.8);
    std::uniform_real_distribution<double> speed_dist(3.0, 6.0);

    double by = ball_y(rng);
    double speed = speed_dist(rng);

    // Ball approaching yellow goal from midfield
    state.ball = {3.0, by, 0.0215, speed, 0.0, 0.0};
    // Goalkeeper in front of yellow goal
    state.yellow_robots.push_back(makeRobot(0, 1, 5.8, 0.0, M_PI));
    return state;
}

TerminationStatus GoalkeeperSaveScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    // Success: ball stopped or moving away from goal
    if (state.ball.vx < -0.1 && state.ball.x < 4.0) {
        ts.done = true; ts.success = true; ts.reason = "saved";
    }
    // Failure: goal scored
    for (const auto& e : events) {
        if (e.type == GameEventType::GOAL) { ts.done = true; ts.reason = "goal_conceded"; }
    }
    if (hasBallLeftField(events)) { ts.done = true; ts.reason = "ball_out"; }
    if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler GoalkeeperSaveScenario::rewardCompiler() const {
    return RewardCompiler::defensiveProfile();
}

// ============================================================
// Ball Placement
// ============================================================

WorldState BallPlacementScenario::generateInitialState(std::mt19937& rng) const {
    WorldState state;
    std::uniform_real_distribution<double> x_dist(-3.0, 3.0);
    std::uniform_real_distribution<double> y_dist(-2.0, 2.0);

    double bx = x_dist(rng), by = y_dist(rng);

    state.ball = {bx, by, 0.0215, 0, 0, 0};
    state.blue_robots.push_back(makeRobot(0, 0, bx - 0.3, by, 0.0));
    return state;
}

TerminationStatus BallPlacementScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    // Target: center of field (0, 0) — within 0.15m
    double dist = std::sqrt(state.ball.x * state.ball.x + state.ball.y * state.ball.y);
    double speed = std::sqrt(state.ball.vx * state.ball.vx + state.ball.vy * state.ball.vy);
    if (dist < 0.15 && speed < 0.1) {
        ts.done = true; ts.success = true; ts.reason = "placed";
    }
    if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler BallPlacementScenario::rewardCompiler() const {
    return RewardCompiler::placementProfile();
}

// ============================================================
// Kickoff Attack
// ============================================================

WorldState KickoffAttackScenario::generateInitialState(std::mt19937& /*rng*/) const {
    WorldState state;
    state.ball = {0.0, 0.0, 0.0215, 0, 0, 0};

    // Blue team: kickoff formation (left side)
    state.blue_robots.push_back(makeRobot(0, 0, -0.3, 0.0, 0.0));    // kicker
    state.blue_robots.push_back(makeRobot(1, 0, -1.5, 1.5, 0.0));
    state.blue_robots.push_back(makeRobot(2, 0, -1.5, -1.5, 0.0));
    state.blue_robots.push_back(makeRobot(3, 0, -3.0, 0.0, 0.0));
    state.blue_robots.push_back(makeRobot(4, 0, -4.5, 2.0, 0.0));
    state.blue_robots.push_back(makeRobot(5, 0, -5.5, 0.0, 0.0));    // GK

    // Yellow team: defensive formation (right side)
    state.yellow_robots.push_back(makeRobot(0, 1, 1.5, 0.0, M_PI));
    state.yellow_robots.push_back(makeRobot(1, 1, 3.0, 1.5, M_PI));
    state.yellow_robots.push_back(makeRobot(2, 1, 3.0, -1.5, M_PI));
    state.yellow_robots.push_back(makeRobot(3, 1, 4.0, 0.0, M_PI));
    state.yellow_robots.push_back(makeRobot(4, 1, 4.5, 2.0, M_PI));
    state.yellow_robots.push_back(makeRobot(5, 1, 5.5, 0.0, M_PI));  // GK

    return state;
}

TerminationStatus KickoffAttackScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    if (hasGoalEvent(events, 0)) { ts.done = true; ts.success = true; ts.reason = "goal"; }
    else if (hasGoalEvent(events, 1)) { ts.done = true; ts.reason = "conceded"; }
    else if (hasBallLeftField(events)) { ts.done = true; ts.reason = "ball_out"; }
    else if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler KickoffAttackScenario::rewardCompiler() const {
    return RewardCompiler::scoringProfile();
}

// ============================================================
// Free Kick Attack
// ============================================================

WorldState FreeKickAttackScenario::generateInitialState(std::mt19937& rng) const {
    WorldState state;
    std::uniform_real_distribution<double> x_dist(2.0, 4.0);
    std::uniform_real_distribution<double> y_dist(-2.0, 2.0);

    double bx = x_dist(rng), by = y_dist(rng);
    state.ball = {bx, by, 0.0215, 0, 0, 0};

    state.blue_robots.push_back(makeRobot(0, 0, bx - 0.15, by, 0.0));
    state.blue_robots.push_back(makeRobot(1, 0, bx - 1.0, by + 1.0, 0.0));
    state.blue_robots.push_back(makeRobot(2, 0, bx - 1.0, by - 1.0, 0.0));
    state.blue_robots.push_back(makeRobot(3, 0, -2.0, 0.0, 0.0));
    state.blue_robots.push_back(makeRobot(4, 0, -4.0, 1.5, 0.0));
    state.blue_robots.push_back(makeRobot(5, 0, -5.5, 0.0, 0.0));

    // Defensive wall + goalkeeper
    state.yellow_robots.push_back(makeRobot(0, 1, bx + 0.7, by + 0.1, M_PI));
    state.yellow_robots.push_back(makeRobot(1, 1, bx + 0.7, by - 0.1, M_PI));
    state.yellow_robots.push_back(makeRobot(2, 1, 4.0, 0.0, M_PI));
    state.yellow_robots.push_back(makeRobot(3, 1, 4.5, 1.5, M_PI));
    state.yellow_robots.push_back(makeRobot(4, 1, 4.5, -1.5, M_PI));
    state.yellow_robots.push_back(makeRobot(5, 1, 5.5, 0.0, M_PI));

    return state;
}

TerminationStatus FreeKickAttackScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    if (hasGoalEvent(events, 0)) { ts.done = true; ts.success = true; ts.reason = "goal"; }
    else if (hasBallLeftField(events)) { ts.done = true; ts.reason = "ball_out"; }
    else if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler FreeKickAttackScenario::rewardCompiler() const {
    return RewardCompiler::scoringProfile();
}

// ============================================================
// 3v3 Possession
// ============================================================

WorldState ThreeVsThreePossessionScenario::generateInitialState(std::mt19937& rng) const {
    WorldState state;
    std::uniform_real_distribution<double> offset(-0.5, 0.5);

    state.ball = {0.0 + offset(rng), 0.0 + offset(rng), 0.0215, 0, 0, 0};

    state.blue_robots.push_back(makeRobot(0, 0, -1.0, 0.0, 0.0));
    state.blue_robots.push_back(makeRobot(1, 0, -0.5, 1.5, 0.0));
    state.blue_robots.push_back(makeRobot(2, 0, -0.5, -1.5, 0.0));

    state.yellow_robots.push_back(makeRobot(0, 1, 1.0, 0.0, M_PI));
    state.yellow_robots.push_back(makeRobot(1, 1, 0.5, 1.5, M_PI));
    state.yellow_robots.push_back(makeRobot(2, 1, 0.5, -1.5, M_PI));

    return state;
}

TerminationStatus ThreeVsThreePossessionScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    if (hasBallLeftField(events)) { ts.done = true; ts.reason = "ball_out"; }
    else if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler ThreeVsThreePossessionScenario::rewardCompiler() const {
    return RewardCompiler::possessionProfile();
}

// ============================================================
// 3v2 Counterattack
// ============================================================

WorldState ThreeVsTwoCounterattackScenario::generateInitialState(std::mt19937& rng) const {
    WorldState state;
    std::uniform_real_distribution<double> y_dist(-1.0, 1.0);
    double y = y_dist(rng);

    state.ball = {-1.0, y, 0.0215, 0, 0, 0};

    state.blue_robots.push_back(makeRobot(0, 0, -1.15, y, 0.0));
    state.blue_robots.push_back(makeRobot(1, 0, -2.0, 2.0, 0.3));
    state.blue_robots.push_back(makeRobot(2, 0, -2.0, -2.0, -0.3));

    state.yellow_robots.push_back(makeRobot(0, 1, 3.0, 0.5, M_PI));
    state.yellow_robots.push_back(makeRobot(1, 1, 5.5, 0.0, M_PI));

    return state;
}

TerminationStatus ThreeVsTwoCounterattackScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    if (hasGoalEvent(events, 0)) { ts.done = true; ts.success = true; ts.reason = "goal"; }
    else if (hasBallLeftField(events)) { ts.done = true; ts.reason = "ball_out"; }
    else if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler ThreeVsTwoCounterattackScenario::rewardCompiler() const {
    return RewardCompiler::scoringProfile();
}

// ============================================================
// Mini Game Full Play
// ============================================================

WorldState MiniGameFullPlayScenario::generateInitialState(std::mt19937& /*rng*/) const {
    WorldState state;
    state.ball = {0.0, 0.0, 0.0215, 0, 0, 0};

    // 6v6 kickoff formation for Division B field (9m × 6m)
    double spacing = 1.0;
    for (int i = 0; i < 6; i++) {
        double y = (i - 2.5) * spacing;
        double x = -2.0 - (i % 2) * 1.5;
        if (i == 5) { x = -4.0; y = 0.0; } // GK
        state.blue_robots.push_back(makeRobot(i, 0, x, y, 0.0));
    }
    for (int i = 0; i < 6; i++) {
        double y = (i - 2.5) * spacing;
        double x = 2.0 + (i % 2) * 1.5;
        if (i == 5) { x = 4.0; y = 0.0; } // GK
        state.yellow_robots.push_back(makeRobot(i, 1, x, y, M_PI));
    }

    return state;
}

TerminationStatus MiniGameFullPlayScenario::checkTermination(
    const WorldState& state, const std::vector<GameEvent>& events, double elapsed
) const {
    TerminationStatus ts;
    // Full game: ends on timeout or after N goals
    if (elapsed > maxEpisodeTime()) { ts.truncated = true; ts.reason = "timeout"; }
    return ts;
}

RewardCompiler MiniGameFullPlayScenario::rewardCompiler() const {
    return RewardCompiler::fullMatchProfile();
}

// ============================================================
// Auto-registration
// ============================================================

static struct ScenarioRegistrar {
    ScenarioRegistrar() {
        auto& reg = ScenarioRegistry::instance();
        reg.registerScenario("empty_field_shot", []() { return std::make_unique<EmptyFieldShotScenario>(); });
        reg.registerScenario("1v1_dribble", []() { return std::make_unique<OneVsOneDribbleScenario>(); });
        reg.registerScenario("2v1_attack", []() { return std::make_unique<TwoVsOneAttackScenario>(); });
        reg.registerScenario("goalkeeper_save", []() { return std::make_unique<GoalkeeperSaveScenario>(); });
        reg.registerScenario("ball_placement", []() { return std::make_unique<BallPlacementScenario>(); });
        reg.registerScenario("kickoff_attack", []() { return std::make_unique<KickoffAttackScenario>(); });
        reg.registerScenario("free_kick_attack", []() { return std::make_unique<FreeKickAttackScenario>(); });
        reg.registerScenario("3v3_possession", []() { return std::make_unique<ThreeVsThreePossessionScenario>(); });
        reg.registerScenario("3v2_counterattack", []() { return std::make_unique<ThreeVsTwoCounterattackScenario>(); });
        reg.registerScenario("mini_game_full", []() { return std::make_unique<MiniGameFullPlayScenario>(); });
    }
} _registrar;

} // namespace grsim_scenarios
