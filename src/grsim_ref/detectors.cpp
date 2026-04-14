/**
 * Built-in event detectors for grsim_ref.
 *
 * Each detector covers one rule family from the SSL rules.
 * Detectors are modular and independently testable.
 */

#include "grsim_ref/events.h"
#include "grsim_core/config.h"
#include <cmath>
#include <algorithm>

namespace grsim_ref {

// ============================================================
// Helper functions
// ============================================================

static double distance2D(double x1, double y1, double x2, double y2) {
    return std::sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

static double ballSpeed(const grsim_core::BallState& b) {
    return std::sqrt(b.vx * b.vx + b.vy * b.vy + b.vz * b.vz);
}

static bool inDefenseArea(const grsim_core::SimConfig& cfg, int defense_team, double x, double y) {
    double half_l = cfg.field.field_length / 2.0;
    double pen_d = cfg.field.penalty_area_depth;
    double pen_hw = cfg.field.penalty_area_width / 2.0;
    if (defense_team == 0) {
        return x < -(half_l - pen_d) && x > -half_l && std::abs(y) < pen_hw;
    } else {
        return x > (half_l - pen_d) && x < half_l && std::abs(y) < pen_hw;
    }
}

// ============================================================
// Ball Left Field Detector
// Rule: Law 9 — Ball In and Out of Play
// ============================================================

class BallLeftFieldDetector : public EventDetector {
    grsim_core::SimConfig cfg_;
    bool ball_was_in_field_ = true;
public:
    explicit BallLeftFieldDetector(const grsim_core::SimConfig& cfg) : cfg_(cfg) {}

    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& previous,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;
        double half_l = cfg_.field.field_length / 2.0;
        double half_w = cfg_.field.field_width / 2.0;
        double bx = current.ball.x, by = current.ball.y;

        bool in_field = std::abs(bx) <= half_l && std::abs(by) <= half_w;

        if (ball_was_in_field_ && !in_field) {
            GameEvent e;
            e.timestamp = current.sim_time;
            e.x = bx;
            e.y = by;
            e.rule_ref = "Law 9 — Ball In and Out of Play";

            if (std::abs(by) > half_w) {
                e.type = GameEventType::BALL_LEFT_FIELD_TOUCH_LINE;
            } else if (std::abs(bx) > half_l) {
                // Check if it's a goal
                double goal_hw = cfg_.field.goal_width / 2.0;
                if (std::abs(by) < goal_hw) {
                    e.type = GameEventType::POSSIBLE_GOAL;
                    e.rule_ref = "Law 10 — Scoring";
                } else {
                    e.type = GameEventType::BALL_LEFT_FIELD_GOAL_LINE;
                }
            }
            events.push_back(e);
        }
        ball_was_in_field_ = in_field;
        return events;
    }

    void reset() override { ball_was_in_field_ = true; }
    std::string name() const override { return "BallLeftField"; }
    std::string ruleRef() const override { return "Law 9"; }
};

// ============================================================
// Ball Speed Detector
// Rule: Law 12 — Ball Speed > 6.5 m/s (non-stopping foul)
// ============================================================

class BallSpeedDetector : public EventDetector {
    static constexpr double MAX_BALL_SPEED = 6.5; // m/s
    double last_kick_speed_ = 0.0;
    int last_kick_team_ = -1;
    int last_kick_bot_ = -1;
public:
    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& previous,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;
        double speed = ballSpeed(current.ball);
        double prev_speed = ballSpeed(previous.ball);

        // Detect speed increase (kick event)
        if (speed > prev_speed + 0.5) {
            last_kick_speed_ = speed;
            // Find nearest robot to ball as likely kicker
            double min_dist = 1e6;
            auto checkTeam = [&](const std::vector<grsim_core::RobotState>& robots, int team) {
                for (const auto& r : robots) {
                    double d = distance2D(r.x, r.y, current.ball.x, current.ball.y);
                    if (d < min_dist) {
                        min_dist = d;
                        last_kick_team_ = team;
                        last_kick_bot_ = r.id;
                    }
                }
            };
            checkTeam(current.blue_robots, 0);
            checkTeam(current.yellow_robots, 1);
        }

        if (speed > MAX_BALL_SPEED && prev_speed <= MAX_BALL_SPEED) {
            GameEvent e;
            e.type = GameEventType::BOT_KICKED_BALL_TOO_FAST;
            e.timestamp = current.sim_time;
            e.ball_speed = speed;
            e.by_team = last_kick_team_;
            e.by_bot = last_kick_bot_;
            e.x = current.ball.x;
            e.y = current.ball.y;
            e.rule_ref = "Law 12 — Ball Speed";
            events.push_back(e);
        }
        return events;
    }

    void reset() override {
        last_kick_speed_ = 0;
        last_kick_team_ = -1;
        last_kick_bot_ = -1;
    }
    std::string name() const override { return "BallSpeed"; }
    std::string ruleRef() const override { return "Law 12, Ball Speed"; }
};

// ============================================================
// Defense Area Detector
// Rule: Law 12 — Attacker/Defender in Defense Area
// ============================================================

class DefenseAreaDetector : public EventDetector {
    grsim_core::SimConfig cfg_;
public:
    explicit DefenseAreaDetector(const grsim_core::SimConfig& cfg) : cfg_(cfg) {}

    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& /*previous*/,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;

        auto check = [&](const std::vector<grsim_core::RobotState>& robots, int team) {
            int opponent = 1 - team;
            for (const auto& r : robots) {
                if (!r.present) continue;
                // Attacker in opponent defense area
                if (inDefenseArea(cfg_, opponent, r.x, r.y)) {
                    GameEvent e;
                    e.type = GameEventType::ATTACKER_IN_DEFENSE_AREA;
                    e.timestamp = current.sim_time;
                    e.by_team = team;
                    e.by_bot = r.id;
                    e.x = r.x;
                    e.y = r.y;
                    e.rule_ref = "Law 12 — Defense Area";
                    events.push_back(e);
                }
            }
        };

        check(current.blue_robots, 0);
        check(current.yellow_robots, 1);
        return events;
    }

    void reset() override {}
    std::string name() const override { return "DefenseArea"; }
    std::string ruleRef() const override { return "Law 12, Defense Area"; }
};

// ============================================================
// Dribbling Detector
// Rule: Law 12 — Dribbling > 1m
// ============================================================

class DribblingDetector : public EventDetector {
    static constexpr double MAX_DRIBBLE_DISTANCE = 1.0; // meters
    int dribbling_team_ = -1;
    int dribbling_bot_ = -1;
    double dribble_start_x_ = 0, dribble_start_y_ = 0;
    bool is_dribbling_ = false;
public:
    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& /*previous*/,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;

        // Find robot touching ball
        int touching_team = -1, touching_bot = -1;
        auto findTouching = [&](const std::vector<grsim_core::RobotState>& robots, int team) {
            for (const auto& r : robots) {
                if (r.touching_ball) {
                    touching_team = team;
                    touching_bot = r.id;
                }
            }
        };
        findTouching(current.blue_robots, 0);
        findTouching(current.yellow_robots, 1);

        if (touching_team >= 0) {
            if (!is_dribbling_ || touching_team != dribbling_team_ || touching_bot != dribbling_bot_) {
                // New dribble session
                is_dribbling_ = true;
                dribbling_team_ = touching_team;
                dribbling_bot_ = touching_bot;
                dribble_start_x_ = current.ball.x;
                dribble_start_y_ = current.ball.y;
            } else {
                // Continuing dribble — check distance
                double dist = distance2D(current.ball.x, current.ball.y,
                                        dribble_start_x_, dribble_start_y_);
                if (dist > MAX_DRIBBLE_DISTANCE) {
                    GameEvent e;
                    e.type = GameEventType::BOT_DRIBBLED_BALL_TOO_FAR;
                    e.timestamp = current.sim_time;
                    e.by_team = dribbling_team_;
                    e.by_bot = dribbling_bot_;
                    e.dribble_distance = dist;
                    e.x = current.ball.x;
                    e.y = current.ball.y;
                    e.rule_ref = "Law 12 — Dribbling";
                    events.push_back(e);
                    // Reset to avoid spamming
                    is_dribbling_ = false;
                }
            }
        } else {
            is_dribbling_ = false;
        }

        return events;
    }

    void reset() override {
        is_dribbling_ = false;
        dribbling_team_ = -1;
        dribbling_bot_ = -1;
    }
    std::string name() const override { return "Dribbling"; }
    std::string ruleRef() const override { return "Law 12, Dribbling"; }
};

// ============================================================
// Goal Detector
// Rule: Law 10 — Scoring
// ============================================================

class GoalDetector : public EventDetector {
    grsim_core::SimConfig cfg_;
    bool ball_was_in_goal_[2] = {false, false};
public:
    explicit GoalDetector(const grsim_core::SimConfig& cfg) : cfg_(cfg) {}

    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& /*previous*/,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;
        double half_l = cfg_.field.field_length / 2.0;
        double goal_hw = cfg_.field.goal_width / 2.0;
        double goal_d = cfg_.field.goal_depth;

        for (int goal_team = 0; goal_team < 2; goal_team++) {
            double goal_x = (goal_team == 0) ? -half_l : half_l;
            double sign = (goal_team == 0) ? -1.0 : 1.0;

            bool in_goal = (sign * current.ball.x > half_l) &&
                          (sign * current.ball.x < half_l + goal_d) &&
                          (std::abs(current.ball.y) < goal_hw);

            if (in_goal && !ball_was_in_goal_[goal_team]) {
                GameEvent e;
                // Scoring team is the opponent of the goal's team
                int scoring_team = 1 - goal_team;
                e.type = GameEventType::GOAL;
                e.timestamp = current.sim_time;
                e.by_team = scoring_team;
                e.x = current.ball.x;
                e.y = current.ball.y;
                e.rule_ref = "Law 10 — Scoring";
                events.push_back(e);
            }
            ball_was_in_goal_[goal_team] = in_goal;
        }
        return events;
    }

    void reset() override {
        ball_was_in_goal_[0] = ball_was_in_goal_[1] = false;
    }
    std::string name() const override { return "Goal"; }
    std::string ruleRef() const override { return "Law 10"; }
};

// ============================================================
// Stop Speed Detector
// Rule: Law 5 — STOP state robots must be < 1.5 m/s
// (requires external game state input; this detects speed only)
// ============================================================

class RobotSpeedDetector : public EventDetector {
    static constexpr double MAX_STOP_SPEED = 1.5; // m/s
public:
    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& /*previous*/,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;

        auto check = [&](const std::vector<grsim_core::RobotState>& robots, int team) {
            for (const auto& r : robots) {
                if (!r.present) continue;
                double speed = std::sqrt(r.vx * r.vx + r.vy * r.vy);
                if (speed > MAX_STOP_SPEED) {
                    GameEvent e;
                    e.type = GameEventType::BOT_TOO_FAST_IN_STOP;
                    e.timestamp = current.sim_time;
                    e.by_team = team;
                    e.by_bot = r.id;
                    e.ball_speed = speed; // reusing field for robot speed
                    e.x = r.x;
                    e.y = r.y;
                    e.rule_ref = "Law 5 — STOP state";
                    e.confidence = 0.5; // Lower confidence — needs game state context
                    events.push_back(e);
                }
            }
        };

        check(current.blue_robots, 0);
        check(current.yellow_robots, 1);
        return events;
    }

    void reset() override {}
    std::string name() const override { return "RobotSpeed"; }
    std::string ruleRef() const override { return "Law 5, STOP"; }
};

// ============================================================
// Double Touch Detector
// Rule: Law 12 — Kicker must not touch ball again before another robot
// ============================================================

class DoubleTouchDetector : public EventDetector {
    int last_toucher_team_ = -1;
    int last_toucher_id_ = -1;
    int kicker_team_ = -1;
    int kicker_id_ = -1;
    bool awaiting_second_touch_ = false;
public:
    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& previous,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;

        // Find who is touching the ball now
        int touching_team = -1, touching_id = -1;
        auto findTouching = [&](const std::vector<grsim_core::RobotState>& robots, int team) {
            for (const auto& r : robots) {
                if (r.touching_ball) { touching_team = team; touching_id = r.id; }
            }
        };
        findTouching(current.blue_robots, 0);
        findTouching(current.yellow_robots, 1);

        if (touching_team >= 0) {
            if (last_toucher_team_ < 0) {
                // First touch — this is the kicker for set-piece tracking
                kicker_team_ = touching_team;
                kicker_id_ = touching_id;
                awaiting_second_touch_ = true;
            } else if (touching_team != last_toucher_team_ || touching_id != last_toucher_id_) {
                // Different robot touched — check for double touch
                if (awaiting_second_touch_ &&
                    touching_team == kicker_team_ && touching_id == kicker_id_ &&
                    last_toucher_team_ == kicker_team_ && last_toucher_id_ == kicker_id_) {
                    // Same robot touched twice without another robot in between
                    GameEvent e;
                    e.type = GameEventType::DOUBLE_TOUCH;
                    e.timestamp = current.sim_time;
                    e.by_team = kicker_team_;
                    e.by_bot = kicker_id_;
                    e.x = current.ball.x;
                    e.y = current.ball.y;
                    e.rule_ref = "Law 12 — Double Touch";
                    events.push_back(e);
                    awaiting_second_touch_ = false;
                } else {
                    // Another robot touched — reset double-touch tracking
                    awaiting_second_touch_ = false;
                }
            }
            last_toucher_team_ = touching_team;
            last_toucher_id_ = touching_id;
        }

        return events;
    }

    void reset() override {
        last_toucher_team_ = last_toucher_id_ = -1;
        kicker_team_ = kicker_id_ = -1;
        awaiting_second_touch_ = false;
    }
    std::string name() const override { return "DoubleTouch"; }
    std::string ruleRef() const override { return "Law 12, Double Touch"; }
};

// ============================================================
// Crash Detector
// Rule: Law 12 — Robot crash with speed diff > 1.5 m/s
// ============================================================

class CrashDetector : public EventDetector {
    static constexpr double CRASH_SPEED_THRESHOLD = 1.5; // m/s
    grsim_core::SimConfig cfg_;
public:
    explicit CrashDetector(const grsim_core::SimConfig& cfg) : cfg_(cfg) {}

    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& /*previous*/,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;

        // Collect all present robots
        std::vector<const grsim_core::RobotState*> all_robots;
        for (const auto& r : current.blue_robots) if (r.present) all_robots.push_back(&r);
        for (const auto& r : current.yellow_robots) if (r.present) all_robots.push_back(&r);

        // Check pairwise proximity + speed difference
        for (size_t i = 0; i < all_robots.size(); i++) {
            for (size_t j = i + 1; j < all_robots.size(); j++) {
                const auto* a = all_robots[i];
                const auto* b = all_robots[j];
                if (a->team == b->team) continue; // Same-team crashes not penalized normally

                double dx = a->x - b->x, dy = a->y - b->y;
                double dist = std::sqrt(dx * dx + dy * dy);
                double robot_diameter = cfg_.blue_robot.radius * 2.0;

                if (dist < robot_diameter * 1.2) {
                    // Close enough to be a collision
                    double dvx = a->vx - b->vx, dvy = a->vy - b->vy;
                    double speed_diff = std::sqrt(dvx * dvx + dvy * dvy);

                    if (speed_diff > CRASH_SPEED_THRESHOLD) {
                        double speed_a = std::sqrt(a->vx * a->vx + a->vy * a->vy);
                        double speed_b = std::sqrt(b->vx * b->vx + b->vy * b->vy);

                        GameEvent e;
                        e.timestamp = current.sim_time;
                        e.crash_speed_diff = speed_diff;
                        e.x = (a->x + b->x) / 2.0;
                        e.y = (a->y + b->y) / 2.0;

                        if (std::abs(speed_a - speed_b) < 0.3) {
                            e.type = GameEventType::BOT_CRASH_DRAWN;
                            e.rule_ref = "Law 12 — Crashing (drawn)";
                        } else {
                            e.type = GameEventType::BOT_CRASH_UNIQUE;
                            if (speed_a > speed_b) {
                                e.by_team = a->team; e.by_bot = a->id;
                                e.victim_team = b->team; e.victim_bot = b->id;
                            } else {
                                e.by_team = b->team; e.by_bot = b->id;
                                e.victim_team = a->team; e.victim_bot = a->id;
                            }
                            e.rule_ref = "Law 12 — Crashing (unique)";
                        }
                        events.push_back(e);
                    }
                }
            }
        }
        return events;
    }

    void reset() override {}
    std::string name() const override { return "Crash"; }
    std::string ruleRef() const override { return "Law 12, Crashing"; }
};

// ============================================================
// Ball Placement Detector
// Rule: Law 8 — Ball placement success/failure
// ============================================================

class BallPlacementDetector : public EventDetector {
    static constexpr double PLACEMENT_RADIUS = 0.15; // meters
    static constexpr double PLACEMENT_MAX_TIME = 30.0; // seconds
    static constexpr double BALL_SPEED_THRESHOLD = 0.05; // m/s

    bool placement_active_ = false;
    double placement_start_time_ = 0.0;
    double target_x_ = 0.0, target_y_ = 0.0;
    int placing_team_ = -1;

public:
    void startPlacement(int team, double target_x, double target_y, double current_time) {
        placement_active_ = true;
        placing_team_ = team;
        target_x_ = target_x;
        target_y_ = target_y;
        placement_start_time_ = current_time;
    }

    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& /*previous*/,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;
        if (!placement_active_) return events;

        double dx = current.ball.x - target_x_;
        double dy = current.ball.y - target_y_;
        double dist = std::sqrt(dx * dx + dy * dy);
        double speed = std::sqrt(current.ball.vx * current.ball.vx + current.ball.vy * current.ball.vy);
        double elapsed = current.sim_time - placement_start_time_;

        if (dist < PLACEMENT_RADIUS && speed < BALL_SPEED_THRESHOLD) {
            GameEvent e;
            e.type = GameEventType::BALL_PLACEMENT_SUCCEEDED;
            e.timestamp = current.sim_time;
            e.by_team = placing_team_;
            e.x = current.ball.x;
            e.y = current.ball.y;
            e.distance = dist;
            e.rule_ref = "Law 8 — Ball Placement";
            events.push_back(e);
            placement_active_ = false;
        } else if (elapsed > PLACEMENT_MAX_TIME) {
            GameEvent e;
            e.type = GameEventType::BALL_PLACEMENT_FAILED;
            e.timestamp = current.sim_time;
            e.by_team = placing_team_;
            e.x = current.ball.x;
            e.y = current.ball.y;
            e.distance = dist;
            e.rule_ref = "Law 8 — Ball Placement (timeout)";
            events.push_back(e);
            placement_active_ = false;
        }

        return events;
    }

    void reset() override { placement_active_ = false; placing_team_ = -1; }
    std::string name() const override { return "BallPlacement"; }
    std::string ruleRef() const override { return "Law 8, Ball Placement"; }
};

// ============================================================
// No Progress Detector
// Rule: Law 5 — No significant ball movement for extended period
// ============================================================

class NoProgressDetector : public EventDetector {
    static constexpr double PROGRESS_TIMEOUT = 10.0; // seconds
    static constexpr double PROGRESS_DISTANCE = 0.05; // meters

    double last_progress_time_ = 0.0;
    double last_progress_x_ = 0.0, last_progress_y_ = 0.0;
    bool initialized_ = false;

public:
    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& /*previous*/,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;

        if (!initialized_) {
            last_progress_x_ = current.ball.x;
            last_progress_y_ = current.ball.y;
            last_progress_time_ = current.sim_time;
            initialized_ = true;
            return events;
        }

        double dx = current.ball.x - last_progress_x_;
        double dy = current.ball.y - last_progress_y_;
        double dist = std::sqrt(dx * dx + dy * dy);

        if (dist > PROGRESS_DISTANCE) {
            last_progress_x_ = current.ball.x;
            last_progress_y_ = current.ball.y;
            last_progress_time_ = current.sim_time;
        } else if (current.sim_time - last_progress_time_ > PROGRESS_TIMEOUT) {
            GameEvent e;
            e.type = GameEventType::NO_PROGRESS_IN_GAME;
            e.timestamp = current.sim_time;
            e.x = current.ball.x;
            e.y = current.ball.y;
            e.rule_ref = "Law 5 — No Progress";
            events.push_back(e);
            // Reset to avoid re-firing every frame
            last_progress_time_ = current.sim_time;
        }

        return events;
    }

    void reset() override { initialized_ = false; }
    std::string name() const override { return "NoProgress"; }
    std::string ruleRef() const override { return "Law 5, No Progress"; }
};

// ============================================================
// Too Many Robots Detector
// Rule: Law 3 — Number of robots
// ============================================================

class TooManyRobotsDetector : public EventDetector {
    grsim_core::SimConfig cfg_;
public:
    explicit TooManyRobotsDetector(const grsim_core::SimConfig& cfg) : cfg_(cfg) {}

    std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& /*previous*/,
        double /*dt*/
    ) override {
        std::vector<GameEvent> events;
        int max_robots = cfg_.sim.robots_per_team;
        double half_l = cfg_.field.field_length / 2.0;
        double half_w = cfg_.field.field_width / 2.0;

        auto countInField = [&](const std::vector<grsim_core::RobotState>& robots) {
            int count = 0;
            for (const auto& r : robots) {
                if (r.present && std::abs(r.x) <= half_l + 0.5 && std::abs(r.y) <= half_w + 0.5)
                    count++;
            }
            return count;
        };

        int blue_count = countInField(current.blue_robots);
        int yellow_count = countInField(current.yellow_robots);

        if (blue_count > max_robots) {
            GameEvent e;
            e.type = GameEventType::TOO_MANY_ROBOTS;
            e.timestamp = current.sim_time;
            e.by_team = 0;
            e.rule_ref = "Law 3 — Number of Robots";
            events.push_back(e);
        }
        if (yellow_count > max_robots) {
            GameEvent e;
            e.type = GameEventType::TOO_MANY_ROBOTS;
            e.timestamp = current.sim_time;
            e.by_team = 1;
            e.rule_ref = "Law 3 — Number of Robots";
            events.push_back(e);
        }
        return events;
    }

    void reset() override {}
    std::string name() const override { return "TooManyRobots"; }
    std::string ruleRef() const override { return "Law 3"; }
};

// ============================================================
// Event description helper
// ============================================================

std::string GameEvent::description() const {
    switch (type) {
        case GameEventType::BALL_LEFT_FIELD_TOUCH_LINE:
            return "Ball left field via touch line";
        case GameEventType::BALL_LEFT_FIELD_GOAL_LINE:
            return "Ball left field via goal line";
        case GameEventType::POSSIBLE_GOAL:
            return "Possible goal";
        case GameEventType::GOAL:
            return "Goal scored by team " + std::to_string(by_team);
        case GameEventType::BOT_KICKED_BALL_TOO_FAST:
            return "Ball too fast: " + std::to_string(ball_speed) + " m/s";
        case GameEventType::ATTACKER_IN_DEFENSE_AREA:
            return "Attacker in defense area";
        case GameEventType::BOT_DRIBBLED_BALL_TOO_FAR:
            return "Dribbled too far: " + std::to_string(dribble_distance) + "m";
        case GameEventType::BOT_TOO_FAST_IN_STOP:
            return "Robot too fast in stop";
        case GameEventType::DOUBLE_TOUCH:
            return "Double touch by team " + std::to_string(by_team) + " robot " + std::to_string(by_bot);
        case GameEventType::BOT_CRASH_UNIQUE:
            return "Crash (speed diff " + std::to_string(crash_speed_diff) + " m/s)";
        case GameEventType::BOT_CRASH_DRAWN:
            return "Crash drawn (speed diff " + std::to_string(crash_speed_diff) + " m/s)";
        case GameEventType::BALL_PLACEMENT_SUCCEEDED:
            return "Ball placement succeeded (dist " + std::to_string(distance) + "m)";
        case GameEventType::BALL_PLACEMENT_FAILED:
            return "Ball placement failed (dist " + std::to_string(distance) + "m)";
        case GameEventType::NO_PROGRESS_IN_GAME:
            return "No progress in game";
        case GameEventType::TOO_MANY_ROBOTS:
            return "Too many robots for team " + std::to_string(by_team);
        default:
            return "Event type " + std::to_string(static_cast<int>(type));
    }
}

// ============================================================
// Registry
// ============================================================

void EventDetectorRegistry::addDetector(std::unique_ptr<EventDetector> detector) {
    detectors_.push_back(std::move(detector));
}

std::vector<GameEvent> EventDetectorRegistry::detectAll(
    const grsim_core::WorldState& current,
    const grsim_core::WorldState& previous,
    double dt
) {
    std::vector<GameEvent> all_events;
    for (auto& d : detectors_) {
        auto events = d->detect(current, previous, dt);
        all_events.insert(all_events.end(), events.begin(), events.end());
    }
    return all_events;
}

void EventDetectorRegistry::resetAll() {
    for (auto& d : detectors_) {
        d->reset();
    }
}

EventDetectorRegistry EventDetectorRegistry::createDefault(const grsim_core::SimConfig& config) {
    EventDetectorRegistry registry;
    registry.addDetector(std::make_unique<BallLeftFieldDetector>(config));
    registry.addDetector(std::make_unique<BallSpeedDetector>());
    registry.addDetector(std::make_unique<DefenseAreaDetector>(config));
    registry.addDetector(std::make_unique<DribblingDetector>());
    registry.addDetector(std::make_unique<GoalDetector>(config));
    registry.addDetector(std::make_unique<RobotSpeedDetector>());
    registry.addDetector(std::make_unique<DoubleTouchDetector>());
    registry.addDetector(std::make_unique<CrashDetector>(config));
    registry.addDetector(std::make_unique<BallPlacementDetector>());
    registry.addDetector(std::make_unique<NoProgressDetector>());
    registry.addDetector(std::make_unique<TooManyRobotsDetector>(config));
    return registry;
}

} // namespace grsim_ref
