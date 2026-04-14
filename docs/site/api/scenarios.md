# Scenarios

grsim-rl ships with 10 built-in scenarios that cover a progression from single-robot skills to full team play. Each scenario defines:

- **Initial state** -- positions of robots and the ball (may be randomized).
- **Termination conditions** -- when the episode ends (success, failure, or timeout).
- **Reward function** -- composed from event detectors and shaping terms via the reward compiler.
- **Supported action levels** -- which abstraction layers are available.

## Scenario table

| Gymnasium ID | Scenario key | Blue | Yellow | Time limit | Description |
|---|---|---|---|---|---|
| `grsim/EmptyFieldShot-v0` | `empty_field_shot` | 1 | 0 | 10 s | Shoot on an empty goal |
| `grsim/OneVsOneDribble-v0` | `1v1_dribble` | 1 | 1 | 15 s | Dribble past a defender and score |
| `grsim/TwoVsOneAttack-v0` | `2v1_attack` | 2 | 1 | 15 s | 2v1 attack with passing |
| `grsim/GoalkeeperSave-v0` | `goalkeeper_save` | 0 | 1 | 5 s | Save incoming shots as goalkeeper |
| `grsim/BallPlacement-v0` | `ball_placement` | 1 | 0 | 30 s | Navigate ball to a target position |
| `grsim/KickoffAttack-v0` | `kickoff_attack` | 6 | 6 | 20 s | Execute a kickoff and attack |
| `grsim/FreeKickAttack-v0` | `free_kick_attack` | 6 | 6 | 15 s | Execute a direct free kick |
| `grsim/ThreeVsThreePossession-v0` | `3v3_possession` | 3 | 3 | 30 s | Keep possession in 3v3 |
| `grsim/ThreeVsTwoCounterattack-v0` | `3v2_counterattack` | 3 | 2 | 15 s | Fast 3v2 counter |
| `grsim/MiniGameFull-v0` | `mini_game_full` | 6 | 6 | 120 s | Full 6v6 on Division B field |

## Detailed descriptions

### EmptyFieldShot

A single blue robot starts near the center of the field with the ball. The goal is undefended. The agent must kick the ball into the goal as quickly and accurately as possible.

**Reward signals:** positive reward for goal scored, shaping reward based on ball-to-goal distance reduction, small time penalty.

**Termination:** goal scored (success), ball leaves field (failure), or timeout.

**Action levels:** wheel, body, skill.

### OneVsOneDribble

One blue attacker faces one yellow defender between the attacker and the goal. The attacker starts with possession and must dribble past the defender to score.

**Reward signals:** goal scored, ball-to-goal distance shaping, dribble distance bonus, penalty for losing possession.

**Termination:** goal scored, defender gains possession, ball out of play, or timeout.

**Action levels:** wheel, body, skill.

### TwoVsOneAttack

Two blue attackers against one yellow defender. Tests passing, movement off the ball, and coordination. One attacker starts with the ball.

**Reward signals:** goal scored, pass completion bonus, ball-to-goal shaping, coordination bonus for supporting runs.

**Termination:** goal scored, defender clears ball, ball out of play, or timeout.

**Action levels:** wheel, body, skill.

### GoalkeeperSave

One yellow goalkeeper defends against scripted shots from various positions. The goalkeeper must position itself to block incoming balls.

**Reward signals:** positive for each save, negative for each goal conceded, shaping based on goalkeeper-to-ball-trajectory distance.

**Termination:** fixed number of shots completed, or timeout.

**Action levels:** wheel, body, skill.

### BallPlacement

A single blue robot must move the ball from its current position to a randomly chosen target location on the field. This mirrors the official SSL ball placement rule.

**Reward signals:** distance-to-target reduction shaping, large bonus when ball is within 0.15 m of target and nearly stationary, penalty for excessive time.

**Termination:** ball placed successfully, or timeout (30 s).

**Action levels:** wheel, body, skill.

### KickoffAttack

Full 6v6 setup in kickoff formation. The blue team executes the kickoff and attempts to score. Yellow team runs a static defensive formation.

**Reward signals:** goal scored, ball progression toward goal, successful passes, rule violation penalties.

**Termination:** goal scored, yellow gains possession for extended period, rule violation, or timeout.

**Action levels:** body, skill, coach.

### FreeKickAttack

Full 6v6 with the blue team taking a direct free kick from a random position in the attacking half. Yellow sets up a wall and defensive shape.

**Reward signals:** goal scored, shot quality (speed and placement), set piece execution bonus.

**Termination:** goal scored, ball out of play, yellow possession, or timeout.

**Action levels:** body, skill, coach.

### ThreeVsThreePossession

3v3 half-field possession game. The blue team starts with the ball and must maintain possession while the yellow team presses to win it back. No goals -- pure possession.

**Reward signals:** time in possession, successful passes, penalty for losing the ball, bonus for consecutive passes.

**Termination:** blue team loses possession for more than 3 seconds, or timeout.

**Action levels:** body, skill, coach.

### ThreeVsTwoCounterattack

3v2 fast break. Three blue attackers against two yellow defenders, starting from the halfway line. Tests rapid decision-making and finishing under numerical advantage.

**Reward signals:** goal scored, speed of attack bonus, pass completion, shot quality.

**Termination:** goal scored, yellow clears ball past halfway, ball out of play, or timeout.

**Action levels:** body, skill, coach.

### MiniGameFull

Full 6v6 match on the Division B field (9 m x 6 m). Both teams play with all rules enforced. This is the most complex scenario and is intended for evaluating trained team policies.

**Reward signals:** goals scored/conceded, possession time, rule violations, ball progression.

**Termination:** timeout only (120 s). No early termination -- the episode always runs to completion.

**Action levels:** body, skill, coach.

!!! note "Division B field"
    The MiniGameFull scenario uses Division B field dimensions (9 m x 6 m) rather than the Division A field (12 m x 9 m) used by all other scenarios.

## Creating custom scenarios

On the C++ side, scenarios inherit from `grsim_scenarios::Scenario` and are registered with the `REGISTER_SCENARIO` macro. See `include/grsim_scenarios/scenario.h` for the base class interface.

On the Python side, you can define custom kwargs to override default parameters:

```python
import pygrsim
env = pygrsim.make("empty_field_shot", dt=0.008, action_level="wheel")
```
