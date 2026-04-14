#!/usr/bin/env python3
"""
Benchmark: Environment overhead measurement.

Measures steps/second for each scenario in mock mode and with native module.
"""

import sys
import os
import time
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))

import pygrsim


def bench_scenario(scenario_name: str, num_steps: int = 5000) -> dict:
    """Benchmark a single scenario."""
    env = pygrsim.make(scenario_name)
    obs, _ = env.reset(seed=42)

    start = time.perf_counter()
    total_reward = 0.0
    episodes = 0

    for step in range(num_steps):
        action = env.action_space.sample()
        obs, reward, done, truncated, info = env.step(action)
        total_reward += reward
        if done or truncated:
            obs, _ = env.reset()
            episodes += 1

    elapsed = time.perf_counter() - start
    env.close()

    return {
        "scenario": scenario_name,
        "steps": num_steps,
        "elapsed_s": elapsed,
        "steps_per_sec": num_steps / elapsed,
        "episodes": episodes,
        "avg_reward": total_reward / max(episodes, 1),
    }


def bench_vec_env(scenario_name: str, num_envs: int = 4, num_steps: int = 2000) -> dict:
    """Benchmark vectorized environment."""
    vec_env = pygrsim.SSLSyncVecEnv(scenario_name=scenario_name, num_envs=num_envs)
    obs = vec_env.reset(seed=42)

    start = time.perf_counter()
    total_steps = 0

    for step in range(num_steps):
        actions = np.random.uniform(-1, 1, (num_envs,) + vec_env.action_space.shape).astype(np.float32)
        obs, rewards, dones, infos = vec_env.step(actions)
        total_steps += num_envs

    elapsed = time.perf_counter() - start
    vec_env.close()

    return {
        "scenario": scenario_name,
        "num_envs": num_envs,
        "total_steps": total_steps,
        "elapsed_s": elapsed,
        "steps_per_sec": total_steps / elapsed,
    }


def main():
    print("=" * 70)
    print("grsim-rl Environment Throughput Benchmark")
    print("=" * 70)

    # Check if native module is available
    try:
        import pygrsim_native
        mode = "NATIVE"
    except ImportError:
        mode = "MOCK"

    print(f"Mode: {mode}")
    print()

    # Single environment benchmarks
    print(f"{'Scenario':<28} {'Steps/sec':>10} {'Episodes':>8} {'Avg Reward':>10}")
    print("-" * 60)

    for name in pygrsim.list_scenarios():
        result = bench_scenario(name, num_steps=3000)
        print(f"{result['scenario']:<28} {result['steps_per_sec']:>10.0f} "
              f"{result['episodes']:>8d} {result['avg_reward']:>10.3f}")

    print()

    # Vectorized benchmarks
    print("Vectorized Environment Benchmarks")
    print(f"{'Scenario':<28} {'Envs':>5} {'Total Steps/sec':>15}")
    print("-" * 55)

    for num_envs in [1, 4, 8, 16]:
        result = bench_vec_env("empty_field_shot", num_envs=num_envs, num_steps=1000)
        print(f"{'empty_field_shot':<28} {result['num_envs']:>5d} {result['steps_per_sec']:>15.0f}")

    print()
    print("Done.")


if __name__ == "__main__":
    main()
