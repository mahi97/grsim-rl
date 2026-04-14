"""Tests for vectorized environments."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))

import numpy as np
import pytest


def test_sync_vec_env_basic():
    from pygrsim.vec_env import SSLSyncVecEnv
    vec_env = SSLSyncVecEnv("empty_field_shot", num_envs=3)
    obs = vec_env.reset(seed=42)
    assert obs.shape == (3,) + vec_env.observation_space.shape

    actions = np.random.uniform(-1, 1, (3,) + vec_env.action_space.shape).astype(np.float32)
    obs, rewards, dones, infos = vec_env.step(actions)
    assert obs.shape[0] == 3
    assert rewards.shape == (3,)
    assert dones.shape == (3,)
    assert len(infos) == 3
    vec_env.close()


def test_sync_vec_env_auto_reset():
    from pygrsim.vec_env import SSLSyncVecEnv
    vec_env = SSLSyncVecEnv("empty_field_shot", num_envs=2)
    obs = vec_env.reset(seed=0)

    for _ in range(1000):
        actions = np.random.uniform(-1, 1, (2,) + vec_env.action_space.shape).astype(np.float32)
        obs, rewards, dones, infos = vec_env.step(actions)
        # Auto-reset should keep obs valid
        assert not np.any(np.isnan(obs))

    vec_env.close()


def test_coach_env_basic():
    from pygrsim.coach_env import SSLCoachEnv
    env = SSLCoachEnv("kickoff_attack")
    obs, info = env.reset(seed=42)
    assert obs is not None
    assert info["action_level"] == "coach"
    assert env.action_space.shape == (5,)

    action = env.action_space.sample()
    obs, reward, done, truncated, info = env.step(action)
    assert "coach_action" in info
    env.close()


def test_coach_env_action_compilation():
    from pygrsim.coach_env import _coach_action_to_body_actions
    obs = np.zeros(6 + 6 * 7, dtype=np.float32)  # Ball + 6 robots
    coach_action = np.array([0.0, 0.0, 0.0, 0.0, 0.5], dtype=np.float32)
    body = _coach_action_to_body_actions(coach_action, obs, 6)
    assert body.shape == (36,)  # 6 robots * 6 dims


def test_gymnasium_registration():
    import gymnasium as gym
    # These should be registered via pygrsim.__init__
    import pygrsim
    env = gym.make("grsim/EmptyFieldShot-v0")
    obs, info = env.reset()
    assert obs is not None
    env.close()


def test_gc_client_import():
    """Test that GCClient can be imported (doesn't need running GC)."""
    from pygrsim.gc_client import GCClient, GameState, RefereeCommand
    client = GCClient(host="localhost", port=10009)
    assert client is not None
    assert GameState.HALT is not None


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
