#ifndef GRSIM_REF_EVENTS_H
#define GRSIM_REF_EVENTS_H

#include "grsim_core/config.h"
#include "grsim_core/world_state.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace grsim_ref {

// Canonical event types aligned with ssl-game-controller game events
// Each maps to a rule section in ssl-rules
enum class GameEventType {
    NONE = 0,

    // Ball leaving field (ssl-rules: Law 9)
    BALL_LEFT_FIELD_TOUCH_LINE,    // Ball crossed touch line
    BALL_LEFT_FIELD_GOAL_LINE,     // Ball crossed goal line (not goal)
    AIMLESS_KICK,                  // Ball left field from kick without touching opponent

    // Goals (ssl-rules: Law 10)
    POSSIBLE_GOAL,                 // Ball fully crossed goal line between posts
    GOAL,                          // Confirmed goal
    INVALID_GOAL,                  // Goal invalidated (e.g., attacker in defense area)

    // Ball speed (ssl-rules: Law 12 - non-stopping foul)
    BOT_KICKED_BALL_TOO_FAST,      // Ball speed > 6.5 m/s after kick

    // Defense area (ssl-rules: Law 12)
    ATTACKER_TOO_CLOSE_TO_DEFENSE_AREA,
    ATTACKER_IN_DEFENSE_AREA,
    ATTACKER_TOUCHED_BALL_IN_DEFENSE_AREA,
    BOT_IN_DEFENSE_AREA,           // Defender in own defense area (partial)

    // Dribbling (ssl-rules: Law 12)
    BOT_DRIBBLED_BALL_TOO_FAR,     // Dribbled > 1m

    // Double touch (ssl-rules: Law 12)
    DOUBLE_TOUCH,                  // Kicker touched ball again before another robot

    // Crashing (ssl-rules: Law 12)
    BOT_CRASH_UNIQUE,              // Crash with speed diff > 1.5 m/s, one responsible
    BOT_CRASH_DRAWN,               // Crash with speed diff > 1.5 m/s, shared fault

    // Ball placement (ssl-rules: Law 8)
    BALL_PLACEMENT_SUCCEEDED,
    BALL_PLACEMENT_FAILED,
    BALL_PLACEMENT_INTERFERENCE,

    // Set piece events
    KICKOFF_IN_PLAY,               // Ball moved from center after kickoff
    FREE_KICK_IN_PLAY,             // Ball moved 0.05m after free kick

    // Stop violations
    BOT_TOO_FAST_IN_STOP,          // Robot > 1.5 m/s during STOP
    DEFENDER_TOO_CLOSE_TO_KICK_POINT,

    // Game flow
    NO_PROGRESS_IN_GAME,           // No significant ball movement for extended period
    TOO_MANY_ROBOTS,               // More robots on field than allowed

    // Keeper violations (ssl-rules: Law 12)
    KEEPER_HELD_BALL,              // Keeper held ball too long (5s Div A, 10s Div B)

    // Pushing (ssl-rules: Law 12)
    BOT_PUSHED_BOT,

    // Episode control (internal, not official)
    EPISODE_TIMEOUT,
    EPISODE_RESET,
};

// Attribution and evidence for a game event
struct GameEvent {
    GameEventType type = GameEventType::NONE;
    double timestamp = 0.0;

    // Attribution
    int by_team = -1;       // 0 = blue, 1 = yellow, -1 = none
    int by_bot = -1;        // Robot ID, -1 if N/A
    int victim_team = -1;
    int victim_bot = -1;

    // Position
    double x = 0.0, y = 0.0;

    // Evidence / details
    double ball_speed = 0.0;         // For ball-too-fast
    double dribble_distance = 0.0;   // For dribbled-too-far
    double crash_speed_diff = 0.0;   // For crash events
    double distance = 0.0;           // Generic distance measurement

    // Confidence (0.0 to 1.0)
    double confidence = 1.0;

    // Rule reference
    std::string rule_ref;            // e.g., "Law 12, Section 2"

    // Human-readable description
    std::string description() const;
};

// Base class for event detectors — one per rule family
class EventDetector {
public:
    virtual ~EventDetector() = default;

    // Process a new world state and emit any detected events
    virtual std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& previous,
        double dt
    ) = 0;

    // Reset detector state (e.g., on episode reset)
    virtual void reset() = 0;

    // Name and rule reference for this detector
    virtual std::string name() const = 0;
    virtual std::string ruleRef() const = 0;
};

// Registry of all detectors
class EventDetectorRegistry {
public:
    EventDetectorRegistry() = default;
    EventDetectorRegistry(EventDetectorRegistry&&) = default;
    EventDetectorRegistry& operator=(EventDetectorRegistry&&) = default;
    EventDetectorRegistry(const EventDetectorRegistry&) = delete;
    EventDetectorRegistry& operator=(const EventDetectorRegistry&) = delete;

    void addDetector(std::unique_ptr<EventDetector> detector);
    std::vector<GameEvent> detectAll(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& previous,
        double dt
    );
    void resetAll();
    const std::vector<std::unique_ptr<EventDetector>>& detectors() const { return detectors_; }

    // Create registry with all built-in detectors
    static EventDetectorRegistry createDefault(const grsim_core::SimConfig& config);

private:
    std::vector<std::unique_ptr<EventDetector>> detectors_;
};

} // namespace grsim_ref

#endif // GRSIM_REF_EVENTS_H
