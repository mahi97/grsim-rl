#!/usr/bin/env python3
"""
Multi-agent RL example using pygrsim with PettingZoo.

Demonstrates 3v3 possession training with independent learners.
"""

import sys
sys.path.insert(0, "../python")

from pygrsim.envs import SSLMultiAgentEnv
import numpy as np


def random_multi_agent():
    """Run random agents in the multi-agent environment."""
    env = SSLMultiAgentEnv(scenario_name="3v3_possession")
    obs, infos = env.reset()

    total_rewards = {agent: 0.0 for agent in env.possible_agents}
    steps = 0

    while env.agents and steps < 500:
        actions = {
            agent: env.action_space(agent).sample()
            for agent in env.agents
        }
        obs, rewards, terminations, truncations, infos = env.step(actions)

        for agent in env.possible_agents:
            if agent in rewards:
                total_rewards[agent] += rewards[agent]
        steps += 1

    print(f"3v3 Possession: {steps} steps")
    for agent, reward in total_rewards.items():
        print(f"  {agent}: reward = {reward:.3f}")

    env.close()


def self_play_example():
    """Demonstrate self-play setup for competitive training."""
    env = SSLMultiAgentEnv(scenario_name="mini_game_full")
    obs, infos = env.reset()

    print(f"Mini Game Full Play:")
    print(f"  Agents: {env.possible_agents}")
    print(f"  Obs dim: {env._obs_dim}")
    print(f"  Act dim: {env._act_dim}")

    # Run a few steps
    for step in range(10):
        actions = {agent: env.action_space(agent).sample() for agent in env.agents}
        obs, rewards, terms, truncs, infos = env.step(actions)

    env.close()
    print("  Self-play framework ready for training integration.")


if __name__ == "__main__":
    print("=== Multi-Agent Random Policy ===")
    random_multi_agent()
    print()
    print("=== Self-Play Setup ===")
    self_play_example()
