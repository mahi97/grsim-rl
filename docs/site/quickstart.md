# Quick Start

This guide gets you from zero to a running RL environment in under five minutes.

## Prerequisites

- Python 3.8 or later
- [uv](https://github.com/astral-sh/uv) (recommended) or pip

For the full C++ physics engine you also need CMake 3.16+, a C++17 compiler, and the ODE library. However, the Python environments work in mock mode without the native build, which is enough for API exploration and integration testing.

## Installation

### Python package only (mock physics)

```bash
cd python
uv pip install -e ".[all]"
```

This installs `pygrsim` with gymnasium, pettingzoo, stable-baselines3, and matplotlib support.

### Full build with native physics

```bash
# Build the C++ core
mkdir build && cd build
cmake .. -DBUILD_CORE_ONLY=ON
cmake --build . -j$(nproc)

# Install the Python package
cd ../python
uv pip install -e ".[all]"
```

## Your first environment

### Using the pygrsim API directly

```python
import pygrsim

env = pygrsim.make("empty_field_shot")
obs, info = env.reset(seed=42)

for step in range(500):
    action = env.action_space.sample()
    obs, reward, done, truncated, info = env.step(action)
    if done or truncated:
        obs, info = env.reset()

env.close()
```

### Using gymnasium.make

All 10 scenarios are registered as Gymnasium environments under the `grsim/` namespace:

```python
import gymnasium as gym

env = gym.make("grsim/EmptyFieldShot-v0", render_mode="rgb_array")
obs, info = env.reset()

frame = env.render()  # returns an HxWx3 numpy array
print(f"Frame shape: {frame.shape}")

env.close()
```

### Rendering

The built-in renderer draws the SSL field, robots, and ball using matplotlib. Two modes are supported:

| Mode        | Behavior                                               |
|-------------|--------------------------------------------------------|
| `human`     | Opens an interactive matplotlib window, updated live.  |
| `rgb_array` | Returns a numpy `uint8` array (H x W x 3) per frame.  |

```python
# Human mode -- opens a window
env = pygrsim.make("1v1_dribble", render_mode="human")
obs, _ = env.reset()
for _ in range(200):
    obs, *_ = env.step(env.action_space.sample())
    env.render()
env.close()
```

### Training with Stable Baselines3

```python
from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv
import pygrsim

env = DummyVecEnv([lambda: pygrsim.make("empty_field_shot")])
model = PPO("MlpPolicy", env, verbose=1)
model.learn(total_timesteps=100_000)
model.save("ppo_ssl_shot")
```

## Available environments

List all registered scenario names:

```python
import pygrsim
print(pygrsim.list_scenarios())
```

Or check the Gymnasium registry:

```python
import gymnasium as gym
ssl_envs = [eid for eid in gym.envs.registry if eid.startswith("grsim/")]
print(ssl_envs)
```

See the [Scenarios](api/scenarios.md) page for full details on each environment.
