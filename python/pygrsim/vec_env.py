"""
Vectorized environment manager for batched RL training.

Runs multiple SSLSingleAgentEnv instances in parallel using
multiprocessing for throughput. Compatible with SB3's VecEnv interface.
"""

import numpy as np
from typing import List, Optional, Tuple, Dict, Any
from multiprocessing import Process, Pipe
from multiprocessing.connection import Connection

try:
    import gymnasium as gym
    from gymnasium import spaces
except ImportError:
    raise ImportError("gymnasium required: uv pip install gymnasium")

from pygrsim.envs import SSLSingleAgentEnv, SCENARIOS, _obs_dim, _make_action_space


def _worker(conn: Connection, scenario_name: str, action_level: str, dt: float):
    """Worker process that runs a single environment."""
    env = SSLSingleAgentEnv(scenario_name=scenario_name, action_level=action_level, dt=dt)
    try:
        while True:
            cmd, data = conn.recv()
            if cmd == "step":
                obs, reward, done, truncated, info = env.step(data)
                if done or truncated:
                    final_obs = obs.copy()
                    info["terminal_observation"] = final_obs
                    obs, reset_info = env.reset()
                    info.update(reset_info)
                conn.send((obs, reward, done, truncated, info))
            elif cmd == "reset":
                obs, info = env.reset(seed=data)
                conn.send((obs, info))
            elif cmd == "get_spaces":
                conn.send((env.observation_space, env.action_space))
            elif cmd == "close":
                env.close()
                conn.close()
                break
    except EOFError:
        env.close()


class SSLVecEnv:
    """
    Vectorized SSL environment running N instances in parallel.

    Follows the auto-reset pattern: when an env is done, it automatically
    resets and the terminal observation is stored in info["terminal_observation"].

    Compatible with Stable Baselines3 VecEnv interface conventions.

    Usage:
        vec_env = SSLVecEnv("empty_field_shot", num_envs=8)
        obs = vec_env.reset()
        for _ in range(1000):
            actions = np.random.uniform(-1, 1, (8, action_dim))
            obs, rewards, dones, infos = vec_env.step(actions)
        vec_env.close()
    """

    def __init__(
        self,
        scenario_name: str = "empty_field_shot",
        num_envs: int = 4,
        action_level: str = "body",
        dt: float = 0.016,
    ):
        self.num_envs = num_envs
        self.scenario_name = scenario_name
        self.waiting = False

        # Launch worker processes
        self.parents: List[Connection] = []
        self.children: List[Connection] = []
        self.processes: List[Process] = []

        for i in range(num_envs):
            parent_conn, child_conn = Pipe()
            proc = Process(
                target=_worker,
                args=(child_conn, scenario_name, action_level, dt),
                daemon=True,
            )
            proc.start()
            child_conn.close()  # Parent doesn't need child end
            self.parents.append(parent_conn)
            self.processes.append(proc)

        # Get spaces from first worker
        self.parents[0].send(("get_spaces", None))
        self.observation_space, self.action_space = self.parents[0].recv()

        self.obs_shape = self.observation_space.shape
        self.act_shape = self.action_space.shape

    def reset(self, seed: Optional[int] = None) -> np.ndarray:
        """Reset all environments. Returns stacked observations (num_envs, obs_dim)."""
        for i, parent in enumerate(self.parents):
            env_seed = (seed + i) if seed is not None else None
            parent.send(("reset", env_seed))

        obs_list = []
        for parent in self.parents:
            obs, info = parent.recv()
            obs_list.append(obs)

        return np.stack(obs_list)

    def step(self, actions: np.ndarray) -> Tuple[np.ndarray, np.ndarray, np.ndarray, List[Dict]]:
        """
        Step all environments with batched actions.

        Args:
            actions: (num_envs, action_dim) array

        Returns:
            obs: (num_envs, obs_dim)
            rewards: (num_envs,)
            dones: (num_envs,) — True when episode ended (auto-reset already happened)
            infos: list of info dicts
        """
        assert actions.shape[0] == self.num_envs

        for i, parent in enumerate(self.parents):
            parent.send(("step", actions[i]))

        obs_list = []
        reward_list = []
        done_list = []
        info_list = []

        for parent in self.parents:
            obs, reward, done, truncated, info = parent.recv()
            obs_list.append(obs)
            reward_list.append(reward)
            done_list.append(done or truncated)
            info_list.append(info)

        return (
            np.stack(obs_list),
            np.array(reward_list, dtype=np.float64),
            np.array(done_list, dtype=bool),
            info_list,
        )

    def step_async(self, actions: np.ndarray):
        """Send actions to all envs without waiting for results."""
        for i, parent in enumerate(self.parents):
            parent.send(("step", actions[i]))
        self.waiting = True

    def step_wait(self) -> Tuple[np.ndarray, np.ndarray, np.ndarray, List[Dict]]:
        """Wait for results from step_async."""
        assert self.waiting
        obs_list, reward_list, done_list, info_list = [], [], [], []
        for parent in self.parents:
            obs, reward, done, truncated, info = parent.recv()
            obs_list.append(obs)
            reward_list.append(reward)
            done_list.append(done or truncated)
            info_list.append(info)
        self.waiting = False
        return (
            np.stack(obs_list),
            np.array(reward_list, dtype=np.float64),
            np.array(done_list, dtype=bool),
            info_list,
        )

    def close(self):
        """Shut down all worker processes."""
        for parent in self.parents:
            try:
                parent.send(("close", None))
            except BrokenPipeError:
                pass
        for proc in self.processes:
            proc.join(timeout=5)
            if proc.is_alive():
                proc.terminate()

    def __del__(self):
        self.close()

    @property
    def unwrapped(self):
        return self

    def __len__(self):
        return self.num_envs


class SSLSyncVecEnv:
    """
    Synchronous (single-process) vectorized environment.

    Faster for small num_envs or when native module makes step() cheap.
    No multiprocessing overhead.
    """

    def __init__(
        self,
        scenario_name: str = "empty_field_shot",
        num_envs: int = 4,
        action_level: str = "body",
        dt: float = 0.016,
    ):
        self.num_envs = num_envs
        self.envs = [
            SSLSingleAgentEnv(scenario_name=scenario_name, action_level=action_level, dt=dt)
            for _ in range(num_envs)
        ]
        self.observation_space = self.envs[0].observation_space
        self.action_space = self.envs[0].action_space

    def reset(self, seed: Optional[int] = None) -> np.ndarray:
        obs_list = []
        for i, env in enumerate(self.envs):
            env_seed = (seed + i) if seed is not None else None
            obs, _ = env.reset(seed=env_seed)
            obs_list.append(obs)
        return np.stack(obs_list)

    def step(self, actions: np.ndarray) -> Tuple[np.ndarray, np.ndarray, np.ndarray, List[Dict]]:
        obs_list, reward_list, done_list, info_list = [], [], [], []
        for i, env in enumerate(self.envs):
            obs, reward, done, truncated, info = env.step(actions[i])
            if done or truncated:
                info["terminal_observation"] = obs.copy()
                obs, reset_info = env.reset()
                info.update(reset_info)
            obs_list.append(obs)
            reward_list.append(reward)
            done_list.append(done or truncated)
            info_list.append(info)
        return (
            np.stack(obs_list),
            np.array(reward_list, dtype=np.float64),
            np.array(done_list, dtype=bool),
            info_list,
        )

    def close(self):
        for env in self.envs:
            env.close()

    def __len__(self):
        return self.num_envs
