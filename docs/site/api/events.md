# Events and Detectors

grsim-rl includes a modular event detection system that mirrors the [SSL game controller](https://github.com/RoboCup-SSL/ssl-game-controller) game events. Events drive both rule enforcement and reward shaping.

## Architecture

```
WorldState (current + previous)
        |
        v
+-------------------+
| EventDetector (1) |----> GameEvent[]
| EventDetector (2) |----> GameEvent[]
|       ...         |
| EventDetector (N) |----> GameEvent[]
+-------------------+
        |
        v
  EventDetectorRegistry.detectAll()
        |
        v
  RewardCompiler  --->  RewardSignal (scalar + breakdown)
```

Each `EventDetector` inspects two consecutive `WorldState` snapshots and emits zero or more `GameEvent` structs. The `EventDetectorRegistry` runs all detectors every step and aggregates the results. The `RewardCompiler` then converts events into a scalar reward signal.

## GameEventType reference

Events are grouped by the SSL rule section they correspond to.

### Ball leaving field (Law 9)

| Event | Description |
|-------|-------------|
| `BALL_LEFT_FIELD_TOUCH_LINE` | Ball crossed the touch line (sideline). |
| `BALL_LEFT_FIELD_GOAL_LINE` | Ball crossed the goal line but not into the goal. |
| `AIMLESS_KICK` | Ball left the field from a kick without touching an opponent. |

### Goals (Law 10)

| Event | Description |
|-------|-------------|
| `POSSIBLE_GOAL` | Ball fully crossed the goal line between the posts. Awaiting validation. |
| `GOAL` | Confirmed valid goal after checking for violations. |
| `INVALID_GOAL` | Goal invalidated (e.g., attacker was inside the defense area). |

### Ball speed (Law 12)

| Event | Description |
|-------|-------------|
| `BOT_KICKED_BALL_TOO_FAST` | Ball speed exceeded 6.5 m/s immediately after a kick. |

### Defense area (Law 12)

| Event | Description |
|-------|-------------|
| `ATTACKER_TOO_CLOSE_TO_DEFENSE_AREA` | Attacker within the minimum distance of the opponent's defense area. |
| `ATTACKER_IN_DEFENSE_AREA` | Attacker entered the opponent's defense area. |
| `ATTACKER_TOUCHED_BALL_IN_DEFENSE_AREA` | Attacker touched the ball while inside the defense area. |
| `BOT_IN_DEFENSE_AREA` | Non-goalkeeper inside its own defense area (partial violation). |

### Dribbling (Law 12)

| Event | Description |
|-------|-------------|
| `BOT_DRIBBLED_BALL_TOO_FAR` | A robot dribbled the ball more than 1 meter. |

### Double touch (Law 12)

| Event | Description |
|-------|-------------|
| `DOUBLE_TOUCH` | The kicker touched the ball again before any other robot touched it (after a set piece). |

### Crashing (Law 12)

| Event | Description |
|-------|-------------|
| `BOT_CRASH_UNIQUE` | Collision between robots with speed difference > 1.5 m/s, one robot clearly responsible. |
| `BOT_CRASH_DRAWN` | Collision with speed difference > 1.5 m/s, fault shared between both robots. |

### Ball placement (Law 8)

| Event | Description |
|-------|-------------|
| `BALL_PLACEMENT_SUCCEEDED` | Ball was successfully placed at the target position. |
| `BALL_PLACEMENT_FAILED` | Ball placement attempt failed (timeout or incorrect position). |
| `BALL_PLACEMENT_INTERFERENCE` | Opponent interfered with ball placement. |

### Set pieces

| Event | Description |
|-------|-------------|
| `KICKOFF_IN_PLAY` | Ball moved from the center spot after a kickoff. |
| `FREE_KICK_IN_PLAY` | Ball moved at least 0.05 m after a free kick. |

### Stop violations

| Event | Description |
|-------|-------------|
| `BOT_TOO_FAST_IN_STOP` | Robot moving faster than 1.5 m/s during a STOP command. |
| `DEFENDER_TOO_CLOSE_TO_KICK_POINT` | Defender too close to the ball during an opponent's set piece. |

### Game flow

| Event | Description |
|-------|-------------|
| `NO_PROGRESS_IN_GAME` | No significant ball movement for an extended period. |
| `TOO_MANY_ROBOTS` | More robots on the field than allowed for the current game state. |

### Keeper violations (Law 12)

| Event | Description |
|-------|-------------|
| `KEEPER_HELD_BALL` | Goalkeeper held the ball longer than allowed (5 s Div A, 10 s Div B). |

### Pushing (Law 12)

| Event | Description |
|-------|-------------|
| `BOT_PUSHED_BOT` | A robot pushed another robot. |

### Internal episode events

| Event | Description |
|-------|-------------|
| `EPISODE_TIMEOUT` | Episode time limit reached. |
| `EPISODE_RESET` | Episode was explicitly reset. |

## GameEvent struct

Each detected event carries attribution and evidence:

| Field | Type | Description |
|-------|------|-------------|
| `type` | `GameEventType` | The event enum value. |
| `timestamp` | `double` | Simulation time when the event occurred. |
| `by_team` | `int` | Responsible team: 0 = blue, 1 = yellow, -1 = none. |
| `by_bot` | `int` | Responsible robot ID, or -1. |
| `victim_team` | `int` | Victim team, or -1. |
| `victim_bot` | `int` | Victim robot ID, or -1. |
| `x`, `y` | `double` | Position where the event occurred. |
| `ball_speed` | `double` | Ball speed (for `BOT_KICKED_BALL_TOO_FAST`). |
| `dribble_distance` | `double` | Cumulative dribble distance (for `BOT_DRIBBLED_BALL_TOO_FAR`). |
| `crash_speed_diff` | `double` | Speed difference at collision (for crash events). |
| `distance` | `double` | Generic distance measurement. |
| `confidence` | `double` | Detection confidence, 0.0 to 1.0. |
| `rule_ref` | `string` | Rule reference, e.g., "Law 12, Section 2". |

## EventDetector base class

Custom detectors inherit from `grsim_ref::EventDetector`:

```cpp
class EventDetector {
public:
    virtual ~EventDetector() = default;

    // Process two consecutive states and emit events
    virtual std::vector<GameEvent> detect(
        const grsim_core::WorldState& current,
        const grsim_core::WorldState& previous,
        double dt
    ) = 0;

    // Reset internal state (called on episode reset)
    virtual void reset() = 0;

    // Metadata
    virtual std::string name() const = 0;
    virtual std::string ruleRef() const = 0;
};
```

## EventDetectorRegistry

The registry holds all active detectors and provides batch detection:

```cpp
EventDetectorRegistry registry = EventDetectorRegistry::createDefault(config);

// Every step:
std::vector<GameEvent> events = registry.detectAll(current_state, previous_state, dt);

// On episode reset:
registry.resetAll();
```

`createDefault()` registers all built-in detectors appropriate for the given `SimConfig`.

## Reward compilation

The `RewardCompiler` converts events into a scalar reward. Each scenario configures its own compiler with weights for different event types. See the [Scenarios](scenarios.md) page for per-scenario reward descriptions.
