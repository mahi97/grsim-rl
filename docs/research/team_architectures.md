# Team Architecture Research Dossier

## Architecture Patterns

### STP (Skill-Tactic-Play) — Universal Pattern
All major SSL teams use some form of STP hierarchy:

**Skill**: Low-level robot capabilities (go_to_point, kick, dribble, intercept)
**Tactic**: Per-robot behavioral units combining skills (Attacker, Defender, Goalie)
**Play**: Team-level coordination selecting tactics for all robots (Offense, Defense, Kickoff)

### Implementation Variants

| Team | Language | Play Selection | Tactic Execution | Skills |
|------|----------|---------------|-------------------|--------|
| ER-Force | Lua scripts | Entrypoint functions | Hierarchical Lua | Spline trajectories |
| TIGERs Sumatra | Java | STP factory | FSM-based | Internal simulator |
| UBC Thunderbots | C++ (Bazel) | FSM + Factory | FSM (Boost SML) | Priority-based |
| Immortals | C++ | Strategy Maker GUI | Scripted | GPU path planning |

## Key Findings per Team

### ER-Force (robotics-erlangen/framework)
- Lua-based strategy with entrypoint mechanism for pluggable strategies
- 100Hz strategy loop
- Direct SSL-Simulation-Protocol support
- World state, visualization, debug tree, plot aggregation
- Modular robot generation specs (multiple hardware revisions)

### TIGERs Sumatra
- Java STP with internal replay capabilities
- Supports both internal simulator and external grSim
- Config-driven simulator selection (sim.xml)
- Multi-instance setup for competitive AI testing

### TIGERs AutoReferee
- **Critical reference for grsim_ref layer**
- ~20+ specialized detectors in modular architecture:
  - Ball: BallLeftField, BallSpeeding, BallPlacementSucceeded
  - Attacker: AttackerToDefenseArea, AttackerTouchInDefenseArea, DoubleTouch, Dribbling
  - Defender: BotInDefenseArea, DefenderToKickPointDistance
  - Collision: BotCollision, PushingDetector
  - PenaltyKickFailed
- Modes: Off, Passive (detect only), Active (send to game-controller)

### UBC Thunderbots
- C++ STP with explicit FSM (Boost SML):
  - ~20 plays (Offense, Defense, KickoffFriendly, BallPlacement...)
  - ~15 tactics (Attacker, Defender, Goalie, Chip, Kick, Move...)
  - Priority-based tactic assignment via TacticCoroutine
  - Inter-play shared state for coordination

### Immortals Robotics
- GPU-accelerated path planning (ERRT + NOK-RRT)
- Dynamic Safety Search (DSS) for multi-agent paths
- Visual Strategy Maker tool for play authoring
- Newton Dynamics physics engine (not ODE)

## Implications for grsim-rl

### Hierarchical Action Design
Our four action layers map cleanly onto the STP pattern:
1. **Wheel control** → Below skills (motor level)
2. **Body velocity** → Skill input (movement primitives)
3. **Skill control** → Skills (go_to_pose, kick_to_point, etc.)
4. **Coach control** → Play level (formations, set pieces)

### Skill Set (derived from team implementations)
Common skills across all teams:
- go_to_pose, face_point, kick_to_point, chip_to_point
- receive_ball, intercept_ball, dribble_to_point
- mark_robot, block_lane, place_ball

### Event Detection (derived from TIGERs AutoReferee)
Our canonical event detector should mirror the AutoReferee's modular detector architecture:
- One detector per rule family
- Each detector maps to rule section
- Attribution fields: team, robot, timestamp, position, confidence

### Neutral Action Schema
Key insight: **Do not force one team architecture internally.**
- Our action hierarchy compiles high → low
- STP users map plays → coach actions
- FSM users map states → skill actions
- RL users can train at any level
- Behavior tree users compose skills naturally
