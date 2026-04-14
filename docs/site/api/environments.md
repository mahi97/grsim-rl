# Environment API Reference

grsim-rl provides two environment classes:

- **`SSLSingleAgentEnv`** -- a standard Gymnasium environment for single-agent (or single-team) control.
- **`SSLMultiAgentEnv`** -- a PettingZoo `ParallelEnv` where each robot is an independent agent.

Both wrap the same underlying physics engine and scenario system.

## SSLSingleAgentEnv

```python
from pygrsim.envs import SSLSingleAgentEnv
```

### Constructor

```python
SSLSingleAgentEnv(
    scenario_name: str = "empty_field_shot",
    action_level: str = "body",
    dt: float = 0.016,
    render_mode: str | None = None,
    seed: int | None = None,
)
```

| Parameter       | Type   | Default              | Description |
|-----------------|--------|----------------------|-------------|
| `scenario_name` | str    | `"empty_field_shot"` | One of the 10 registered scenario keys (see [Scenarios](scenarios.md)). |
| `action_level`  | str    | `"body"`             | Action abstraction: `"wheel"` (4 per robot), `"body"` (6 per robot), or `"skill"` (5 per robot). |
| `dt`            | float  | `0.016`              | Physics time step in seconds (~60 Hz). |
| `render_mode`   | str    | `None`               | `"human"` for a live window, `"rgb_array"` for numpy frames, or `None` to disable. |
| `seed`          | int    | `None`               | Optional RNG seed for reproducibility. |

### Observation space

A flat `Box` of shape `(obs_dim,)` with dtype `float32` and bounds `[-20, 20]`.

The observation vector is laid out as:

| Offset | Count | Description |
|--------|-------|-------------|
| 0      | 6     | Ball state: `x, y, z, vx, vy, vz` (meters, m/s) |
| 6      | 7 per robot | Blue robots: `x, y, theta, vx, vy, vw, infrared` |
| 6 + 7*N_blue | 7 per robot | Yellow robots: same layout |

Total dimension = `6 + 7 * (N_blue + N_yellow)`.

### Action space

A flat `Box` of shape `(act_dim,)` with dtype `float32` and bounds `[-1, 1]`.

The action layout depends on `action_level`:

| Level   | Dim per robot | Entries |
|---------|---------------|---------|
| `wheel` | 4 | Wheel speeds: `w1, w2, w3, w4` |
| `body`  | 6 | Body-frame: `vx, vy, vw, kick_speed, kick_angle, dribbler` |
| `skill` | 5 | Skill command: `skill_id, param0, param1, param2, param3` |

Actions are normalized to `[-1, 1]` and scaled internally to physical units.

### Methods

#### `reset(seed=None, options=None) -> (obs, info)`

Reset the environment to the scenario's initial state.

- **seed** -- optional integer seed for the episode RNG.
- **options** -- reserved for future use.
- Returns a tuple `(observation, info_dict)`. The `info` dict always contains the key `"scenario"`.

#### `step(action) -> (obs, reward, terminated, truncated, info)`

Advance the simulation by one time step.

- **action** -- numpy array matching `action_space`.
- **terminated** -- `True` when the scenario's success/failure condition is met.
- **truncated** -- `True` when the episode time limit is reached.
- **info** -- dict containing at least `"elapsed_time"`.

#### `render() -> np.ndarray | None`

Render the current state.

- In `rgb_array` mode, returns an `(H, W, 3)` uint8 numpy array.
- In `human` mode, updates the matplotlib window and returns `None`.
- If `render_mode` was not set, returns `None`.

#### `close()`

Release resources (closes the render window if open).

---

## SSLMultiAgentEnv

```python
from pygrsim.envs import SSLMultiAgentEnv
```

A PettingZoo `ParallelEnv` where each robot is a separate agent. Requires the `pettingzoo` package.

### Constructor

```python
SSLMultiAgentEnv(
    scenario_name: str = "3v3_possession",
    action_level: str = "body",
    dt: float = 0.016,
    render_mode: str | None = None,
)
```

### Agents

Agents are named `"blue_0"`, `"blue_1"`, ..., `"yellow_0"`, `"yellow_1"`, etc. The total count depends on the scenario.

### Observation and action spaces

Each agent receives the full global observation (same layout as `SSLSingleAgentEnv`) and controls its own robot with a per-robot action vector.

### Methods

Follows the standard PettingZoo `ParallelEnv` API:

- `reset(seed, options)` -- returns `(observations, infos)` dicts keyed by agent name.
- `step(actions)` -- takes a dict of actions, returns `(observations, rewards, terminations, truncations, infos)`.
- `render()`, `close()` -- same semantics as the single-agent environment.

---

## Factory functions

### `pygrsim.make(scenario_name, **kwargs)`

Shortcut that creates an `SSLSingleAgentEnv` for the given scenario. Accepts the same keyword arguments as the constructor.

```python
import pygrsim
env = pygrsim.make("2v1_attack", render_mode="rgb_array", action_level="wheel")
```

### `pygrsim.list_scenarios()`

Returns a list of all available scenario name strings.

---

## Gymnasium registration

All scenarios are also registered with Gymnasium and can be created with `gymnasium.make()`:

```python
import gymnasium as gym
env = gym.make("grsim/EmptyFieldShot-v0")
```

See [Scenarios](scenarios.md) for the full mapping of names to registered IDs.
