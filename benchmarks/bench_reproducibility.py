#!/usr/bin/env python3
"""
Benchmark: Reproducibility verification.

Verifies that identical seeds produce identical trajectories.
Measures cross-seed variance for key metrics.
"""

import sys
import os
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))

import pygrsim


def verify_determinism(scenario_name: str, seed: int = 42, num_steps: int = 200) -> bool:
    """Run same scenario twice with same seed, verify identical observations."""
    trajectories = []

    for trial in range(2):
        env = pygrsim.make(scenario_name)
        obs, _ = env.reset(seed=seed)
        traj = [obs.copy()]

        for step in range(num_steps):
            # Use deterministic actions based on step number
            np.random.seed(step + seed)
            action = env.action_space.sample()
            obs, reward, done, truncated, info = env.step(action)
            traj.append(obs.copy())
            if done or truncated:
                break

        trajectories.append(np.array(traj))
        env.close()

    # Compare
    if trajectories[0].shape != trajectories[1].shape:
        return False

    max_diff = np.max(np.abs(trajectories[0] - trajectories[1]))
    return max_diff < 1e-10


def cross_seed_variance(scenario_name: str, num_seeds: int = 10, num_steps: int = 500) -> dict:
    """Measure variance in episode length and reward across seeds."""
    episode_lengths = []
    total_rewards = []

    for seed in range(num_seeds):
        env = pygrsim.make(scenario_name)
        obs, _ = env.reset(seed=seed)
        total_reward = 0.0
        steps = 0

        for step in range(num_steps):
            action = env.action_space.sample()
            obs, reward, done, truncated, info = env.step(action)
            total_reward += reward
            steps += 1
            if done or truncated:
                break

        episode_lengths.append(steps)
        total_rewards.append(total_reward)
        env.close()

    return {
        "scenario": scenario_name,
        "num_seeds": num_seeds,
        "length_mean": np.mean(episode_lengths),
        "length_std": np.std(episode_lengths),
        "reward_mean": np.mean(total_rewards),
        "reward_std": np.std(total_rewards),
    }


def main():
    print("=" * 70)
    print("grsim-rl Reproducibility Benchmark")
    print("=" * 70)
    print()

    # Determinism check
    print("Determinism Verification (same seed → same trajectory)")
    print(f"{'Scenario':<28} {'Result':>10}")
    print("-" * 40)

    for name in pygrsim.list_scenarios():
        ok = verify_determinism(name, seed=42, num_steps=100)
        status = "PASS" if ok else "FAIL"
        print(f"{name:<28} {status:>10}")

    print()

    # Cross-seed variance
    print("Cross-Seed Variance (random agent, 10 seeds)")
    print(f"{'Scenario':<28} {'Len μ':>7} {'Len σ':>7} {'Rew μ':>9} {'Rew σ':>9}")
    print("-" * 62)

    for name in ["empty_field_shot", "1v1_dribble", "3v3_possession", "mini_game_full"]:
        result = cross_seed_variance(name, num_seeds=10, num_steps=300)
        print(f"{result['scenario']:<28} {result['length_mean']:>7.1f} {result['length_std']:>7.1f} "
              f"{result['reward_mean']:>9.3f} {result['reward_std']:>9.3f}")

    print()
    print("Done.")


if __name__ == "__main__":
    main()
