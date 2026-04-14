"""
pygrsim -- Python bindings for grsim-rl.

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

Environments can also be created via gymnasium.make():
    import gymnasium as gym
    env = gym.make("grsim/EmptyFieldShot-v0")
"""

__version__ = "0.1.0"

from pygrsim.envs import SSLSingleAgentEnv, SSLMultiAgentEnv
from pygrsim.envs import make, list_scenarios
from pygrsim.vec_env import SSLVecEnv, SSLSyncVecEnv
from pygrsim.coach_env import SSLCoachEnv
from pygrsim.gc_client import GCClient, GameState, RefereeCommand, RefereeState

# ============================================================
# Gymnasium environment registration
# ============================================================
# Register all 10 scenarios so they can be instantiated with
# gymnasium.make("grsim/<Name>-v0").

try:
    import gymnasium as gym

    _REGISTRATIONS = [
        {
            "id": "grsim/EmptyFieldShot-v0",
            "kwargs": {"scenario_name": "empty_field_shot"},
            "max_episode_steps": 625,    # 10s at 16ms steps
        },
        {
            "id": "grsim/OneVsOneDribble-v0",
            "kwargs": {"scenario_name": "1v1_dribble"},
            "max_episode_steps": 937,    # 15s
        },
        {
            "id": "grsim/TwoVsOneAttack-v0",
            "kwargs": {"scenario_name": "2v1_attack"},
            "max_episode_steps": 937,    # 15s
        },
        {
            "id": "grsim/GoalkeeperSave-v0",
            "kwargs": {"scenario_name": "goalkeeper_save"},
            "max_episode_steps": 312,    # 5s
        },
        {
            "id": "grsim/BallPlacement-v0",
            "kwargs": {"scenario_name": "ball_placement"},
            "max_episode_steps": 1875,   # 30s
        },
        {
            "id": "grsim/KickoffAttack-v0",
            "kwargs": {"scenario_name": "kickoff_attack"},
            "max_episode_steps": 1250,   # 20s
        },
        {
            "id": "grsim/FreeKickAttack-v0",
            "kwargs": {"scenario_name": "free_kick_attack"},
            "max_episode_steps": 937,    # 15s
        },
        {
            "id": "grsim/ThreeVsThreePossession-v0",
            "kwargs": {"scenario_name": "3v3_possession"},
            "max_episode_steps": 1875,   # 30s
        },
        {
            "id": "grsim/ThreeVsTwoCounterattack-v0",
            "kwargs": {"scenario_name": "3v2_counterattack"},
            "max_episode_steps": 937,    # 15s
        },
        {
            "id": "grsim/MiniGameFull-v0",
            "kwargs": {"scenario_name": "mini_game_full"},
            "max_episode_steps": 7500,   # 120s
        },
    ]

    for _reg in _REGISTRATIONS:
        gym.register(
            id=_reg["id"],
            entry_point="pygrsim.envs:SSLSingleAgentEnv",
            max_episode_steps=_reg["max_episode_steps"],
            kwargs=_reg["kwargs"],
        )

except ImportError:
    pass  # gymnasium not installed; registration is optional
