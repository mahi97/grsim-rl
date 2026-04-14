# Official SSL Stack Research Dossier

## ssl-rules — Rule Semantics

### Field Dimensions
- **Division A**: 12m × 9m play area (13.4m × 10.4m total), max 11 robots
- **Division B**: 9m × 6m play area (10.4m × 7.4m total), max 6 robots
- Center circle: 1m diameter; Defense area: 3.6m × 1.8m (Div A), 2m × 1m (Div B)
- Penalty mark: 8m (Div A) or 6m (Div B) from goal center

### Robot Constraints
- Max radius: 0.09m within 0.18m × 0.15m cylinder
- Max linear kick speed: 6.5 m/s
- Chip/dribbling constrained to 1m max linear distance

### Game States
- **HALT**: No movement allowed
- **STOP**: Robots <1.5 m/s, ≥0.5m from ball
- **NORMAL_START**: Active play
- **Stages**: Pre-half, halves, half-time, extra time, penalty shootout

### Set Pieces
- **Kickoff**: Ball at center, both teams positioned
- **Free Kick**: From foul location, 0.05m ball movement = in-play
- **Penalty Kick**: From penalty mark
- **Ball Placement**: Within 0.15m radius in ≤30s, 2s grace period

### Key Fouls (Relevant to Event Detection)
- **Stopping Fouls**: Double touch, defense area entry, dribble >1m, pushing, ball holding, tipping over
- **Non-Stopping Fouls**: Ball speed >6.5 m/s, crash (speed diff >1.5 m/s), attacker in defense area
- **Placement Fouls**: Defender too close (0.2m + 2s grace), bot too fast in stop (>1.5 m/s + 2s grace)

## ssl-game-controller — CI Integration

### CI Mode (TCP port 10009)
- **CiInput**: timestamp (ns), tracker packet, API inputs, geometry
- **CiOutput**: referee message with game state and commands
- Enables synchronous, frame-accurate integration without multicast
- Can define simulation time/speed directly

### Game Events (~40 types)
Ball events, fouls (double touch, crashing, dribbling, defense area), placement outcomes, goals, no-progress

### Commands
HALT, STOP, NORMAL_START, FORCE_START, PREPARE_KICKOFF_*, PREPARE_PENALTY_*, PREPARE_DIRECT_FREEKICK_*, BALL_PLACEMENT_*, TIMEOUT_*

## ssl-simulation-protocol — Simulator Control

### Teleport Ball
- Position (x, y, z), velocity (vx, vy, vz)
- `teleport_safely`: Avoid collisions by moving robots
- `roll`: Auto-adjust spin; `by_force`: Force-based persistent movement

### Teleport Robot
- Position, orientation, velocity, `present` flag (add/remove from field)

### Synchronous Stepping (`ssl_simulation_synchronous.proto`)
- Request: `sim_step` (seconds) + optional commands + optional robot control
- Response: detection frames for all cameras + robot feedback
- Enables frame-by-frame automated testing

### Robot Control
- Modes: wheel velocity, local velocity (forward/left/angular), global velocity
- Kick: speed (m/s) + angle (degrees); Dribbler: speed (rpm)

## ssl-autoref-tests — Test Scenarios

### Format
- 16 test folders named after rule violations
- `.log` files: SSL game log format (binary)
- `.json` files: `DesiredEvent` with expected `GameEvent` + stop_after_event flag

### Covered Scenarios
AIMLESS_KICK, BOT_KICKED_BALL_TOO_FAST, DEFENDER_IN_DEFENSE_AREA, DOUBLE_TOUCH, CRASHING (drawn/unique), DRIBBLING, PLACEMENT (succeeded/interference), POSSIBLE_GOAL, BOT_CRASH_BOUNCING

## technical-challenge-rules — Benchmark Challenges

### 2025 Obstacle Avoidance (Division B)
- Single robot navigating obstacle field
- 30s trials, scored by remaining time
- Obstacles: defense area, placement zone, moving robots, virtual walls
- Modified rules: stricter collision, no grace periods

### Team-vs-Team Passing Challenge
- Multi-robot coordination benchmark
- Annual progression of difficulty

## Integration Points for grsim-rl

1. **CI Mode**: TCP 10009 for synchronous game-controller integration
2. **Sync Stepping**: Frame-accurate testing via synchronous protocol
3. **State Reset**: Teleport API for scenario initialization
4. **Rule Validation**: Test against autoref-test scenarios
5. **Benchmarks**: Technical challenges as standardized benchmarks
