#!/usr/bin/env python3
"""
Basic RL training example using pygrsim with Stable Baselines3.

This script trains a PPO agent on the empty_field_shot scenario.
"""

import sys
sys.path.insert(0, "../python")

import pygrsim
import numpy as np

def train_ppo():
    """Train a PPO agent on the empty field shot scenario."""
    try:
        from stable_baselines3 import PPO
        from stable_baselines3.common.vec_env import DummyVecEnv
    except ImportError:
        print("stable-baselines3 required. Install with: pip install stable-baselines3")
        return

    env = pygrsim.make("empty_field_shot")
    env = DummyVecEnv([lambda: env])

    model = PPO("MlpPolicy", env, verbose=1,
                learning_rate=3e-4,
                n_steps=2048,
                batch_size=64,
                n_epochs=10,
                gamma=0.99,
                gae_lambda=0.95)

    print("Training PPO on empty_field_shot scenario...")
    model.learn(total_timesteps=100_000)
    model.save("ppo_empty_field_shot")
    print("Training complete. Model saved to ppo_empty_field_shot.zip")


def evaluate():
    """Evaluate a random agent on all scenarios."""
    for scenario_name in pygrsim.list_scenarios():
        env = pygrsim.make(scenario_name)
        obs, info = env.reset(seed=42)

        total_reward = 0.0
        steps = 0
        done = False

        while not done and steps < 1000:
            action = env.action_space.sample()
            obs, reward, done, truncated, info = env.step(action)
            total_reward += reward
            steps += 1
            if truncated:
                break

        print(f"{scenario_name:25s} | steps={steps:4d} | reward={total_reward:8.3f}")
        env.close()


if __name__ == "__main__":
    print("=== Evaluating random agent on all scenarios ===")
    evaluate()
    print()
    print("=== Training PPO (if stable-baselines3 available) ===")
    train_ppo()
