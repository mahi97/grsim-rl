"""Tests for the pygrsim Python environment."""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))

import numpy as np
import pytest


def test_list_scenarios():
    from pygrsim import list_scenarios
    scenarios = list_scenarios()
    assert len(scenarios) == 10
    assert "empty_field_shot" in scenarios
    assert "mini_game_full" in scenarios


def test_make_env():
    from pygrsim import make
    env = make("empty_field_shot")
    assert env is not None
    env.close()


def test_env_reset():
    from pygrsim import make
    env = make("empty_field_shot")
    obs, info = env.reset(seed=42)
    assert obs is not None
    assert obs.shape == env.observation_space.shape
    assert "scenario" in info
    env.close()


def test_env_step():
    from pygrsim import make
    env = make("empty_field_shot")
    obs, info = env.reset(seed=42)
    action = env.action_space.sample()
    obs, reward, done, truncated, info = env.step(action)
    assert obs.shape == env.observation_space.shape
    assert isinstance(reward, float)
    assert isinstance(done, bool)
    assert isinstance(truncated, bool)
    env.close()


def test_env_multiple_steps():
    from pygrsim import make
    env = make("1v1_dribble")
    obs, _ = env.reset(seed=0)
    for _ in range(100):
        action = env.action_space.sample()
        obs, reward, done, truncated, info = env.step(action)
        if done or truncated:
            obs, _ = env.reset()
    env.close()


def test_all_scenarios_instantiate():
    from pygrsim import make, list_scenarios
    for name in list_scenarios():
        env = make(name)
        obs, info = env.reset(seed=42)
        assert obs is not None, f"Failed to reset {name}"
        action = env.action_space.sample()
        obs, r, d, t, i = env.step(action)
        assert obs is not None, f"Failed to step {name}"
        env.close()


def test_observation_dimensions():
    from pygrsim import make
    from pygrsim.envs import SCENARIOS, _obs_dim

    for name, spec in SCENARIOS.items():
        expected_dim = _obs_dim(spec["blue_robots"], spec["yellow_robots"])
        env = make(name)
        assert env.observation_space.shape[0] == expected_dim, \
            f"{name}: expected obs dim {expected_dim}, got {env.observation_space.shape[0]}"
        env.close()


def test_multi_agent_env():
    """Test PettingZoo multi-agent environment if available."""
    try:
        from pygrsim.envs import SSLMultiAgentEnv
        env = SSLMultiAgentEnv(scenario_name="3v3_possession")
        obs, infos = env.reset()
        assert len(env.agents) == 6  # 3 blue + 3 yellow

        actions = {agent: env.action_space(agent).sample() for agent in env.agents}
        obs, rewards, terms, truncs, infos = env.step(actions)
        assert len(obs) == 6
        env.close()
    except ImportError:
        pytest.skip("PettingZoo not installed")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
