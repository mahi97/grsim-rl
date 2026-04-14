"""
pygrsim — Python bindings for grsim-rl.

Provides Gymnasium and PettingZoo environments for RoboCup SSL RL research.

Example usage:
    import pygrsim
    env = pygrsim.make("empty_field_shot")
    obs, info = env.reset()
    for _ in range(1000):
        action = env.action_space.sample()
        obs, reward, done, truncated, info = env.step(action)
        if done or truncated:
            obs, info = env.reset()
"""

__version__ = "0.1.0"

from pygrsim.envs import SSLSingleAgentEnv, SSLMultiAgentEnv
from pygrsim.envs import make, list_scenarios
