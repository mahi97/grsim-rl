"""
Gymnasium and PettingZoo environment wrappers for grsim-rl.

These are pure-Python environment definitions. The actual physics stepping
will be provided by the C++ pygrsim_native module (via pybind11) when built.

For development/prototyping, a mock engine is provided that returns
zero observations and accepts any actions.
"""

import numpy as np
from typing import Optional, Tuple, Dict, Any, List

try:
    import gymnasium as gym
    from gymnasium import spaces
    HAS_GYMNASIUM = True
except ImportError:
    HAS_GYMNASIUM = False

try:
    from pettingzoo import ParallelEnv
    HAS_PETTINGZOO = True
except ImportError:
    HAS_PETTINGZOO = False


# ============================================================
# Scenario definitions (mirrors C++ scenario registry)
# ============================================================

SCENARIOS = {
    "empty_field_shot": {
        "description": "Single robot shoots on empty goal",
        "blue_robots": 1, "yellow_robots": 0,
        "max_time": 10.0, "field": "divA",
    },
    "1v1_dribble": {
        "description": "1v1 dribble past defender to goal",
        "blue_robots": 1, "yellow_robots": 1,
        "max_time": 15.0, "field": "divA",
    },
    "2v1_attack": {
        "description": "2 attackers vs 1 defender",
        "blue_robots": 2, "yellow_robots": 1,
        "max_time": 15.0, "field": "divA",
    },
    "goalkeeper_save": {
        "description": "Goalkeeper saves incoming shots",
        "blue_robots": 0, "yellow_robots": 1,
        "max_time": 5.0, "field": "divA",
    },
    "ball_placement": {
        "description": "Place ball at target position",
        "blue_robots": 1, "yellow_robots": 0,
        "max_time": 30.0, "field": "divA",
    },
    "kickoff_attack": {
        "description": "Execute kickoff and attack",
        "blue_robots": 6, "yellow_robots": 6,
        "max_time": 20.0, "field": "divA",
    },
    "free_kick_attack": {
        "description": "Execute direct free kick",
        "blue_robots": 6, "yellow_robots": 6,
        "max_time": 15.0, "field": "divA",
    },
    "3v3_possession": {
        "description": "3v3 keep possession game",
        "blue_robots": 3, "yellow_robots": 3,
        "max_time": 30.0, "field": "divA",
    },
    "3v2_counterattack": {
        "description": "3v2 fast counterattack",
        "blue_robots": 3, "yellow_robots": 2,
        "max_time": 15.0, "field": "divA",
    },
    "mini_game_full": {
        "description": "Full 6v6 mini game (Division B)",
        "blue_robots": 6, "yellow_robots": 6,
        "max_time": 120.0, "field": "divB",
    },
}


def list_scenarios() -> List[str]:
    """List all available scenario names."""
    return list(SCENARIOS.keys())


def make(scenario_name: str, **kwargs):
    """Create a Gymnasium environment for a scenario."""
    if scenario_name not in SCENARIOS:
        raise ValueError(f"Unknown scenario: {scenario_name}. Available: {list_scenarios()}")
    return SSLSingleAgentEnv(scenario_name=scenario_name, **kwargs)


# ============================================================
# Observation/Action space helpers
# ============================================================

def _obs_dim(n_blue: int, n_yellow: int) -> int:
    """Ball(6) + per-robot(7) for all robots."""
    return 6 + (n_blue + n_yellow) * 7


def _make_obs_space(n_blue: int, n_yellow: int):
    dim = _obs_dim(n_blue, n_yellow)
    return spaces.Box(low=-20.0, high=20.0, shape=(dim,), dtype=np.float32)


def _make_action_space(n_robots: int, action_level: str = "body"):
    """
    Action space per team:
    - wheel: 4 wheel speeds per robot
    - body: vx, vy, vw, kick_speed, kick_angle, dribbler per robot
    - skill: skill_id + 4 params per robot
    """
    if action_level == "wheel":
        dim = n_robots * 4
    elif action_level == "body":
        dim = n_robots * 6  # vx, vy, vw, kick_speed, kick_angle, dribbler
    elif action_level == "skill":
        dim = n_robots * 5  # skill_id + 4 params
    else:
        dim = n_robots * 6

    return spaces.Box(low=-1.0, high=1.0, shape=(dim,), dtype=np.float32)


# ============================================================
# Single-Agent Gymnasium Environment
# ============================================================

if HAS_GYMNASIUM:
    class SSLSingleAgentEnv(gym.Env):
        """
        Gymnasium environment for SSL RL training.

        Controls the blue team. Yellow team is either scripted, idle, or
        controlled by a separate policy (depending on scenario).

        Observation: flattened [ball_state, blue_robots, yellow_robots]
        Action: per-robot controls for the blue team
        """

        metadata = {"render_modes": ["human", "rgb_array"]}

        def __init__(
            self,
            scenario_name: str = "empty_field_shot",
            action_level: str = "body",
            dt: float = 0.016,
            render_mode: Optional[str] = None,
            seed: Optional[int] = None,
        ):
            super().__init__()

            self.scenario_name = scenario_name
            self.scenario = SCENARIOS[scenario_name]
            self.action_level = action_level
            self.dt = dt
            self.render_mode = render_mode

            n_blue = self.scenario["blue_robots"]
            n_yellow = self.scenario["yellow_robots"]

            self.observation_space = _make_obs_space(n_blue, n_yellow)
            self.action_space = _make_action_space(n_blue, action_level)

            self._engine = None  # Will hold C++ SimulationEngine when available
            self._elapsed = 0.0
            self._prev_obs = None

            # Try to import native engine
            try:
                import pygrsim_native
                self._native = pygrsim_native
            except ImportError:
                self._native = None

        def reset(
            self,
            seed: Optional[int] = None,
            options: Optional[Dict] = None,
        ) -> Tuple[np.ndarray, Dict[str, Any]]:
            super().reset(seed=seed)

            self._elapsed = 0.0

            if self._native:
                s = self._native.reset(self.scenario_name, seed or 0)
                obs = np.array(s, dtype=np.float32)
            else:
                obs = np.zeros(self.observation_space.shape, dtype=np.float32)

            self._prev_obs = obs
            return obs, {"scenario": self.scenario_name}

        def step(self, action: np.ndarray) -> Tuple[np.ndarray, float, bool, bool, Dict]:
            self._elapsed += self.dt

            if self._native:
                result = self._native.step(action.tolist(), self.dt)
                obs = np.array(result["observation"], dtype=np.float32)
                reward = result["reward"]
                done = result["done"]
                truncated = result["truncated"]
                info = result.get("info", {})
            else:
                # Mock: return zeros
                obs = np.zeros(self.observation_space.shape, dtype=np.float32)
                reward = 0.0
                done = False
                truncated = self._elapsed >= self.scenario["max_time"]
                info = {}

            info["elapsed_time"] = self._elapsed
            self._prev_obs = obs
            return obs, reward, done, truncated, info

        def render(self):
            if self.render_mode == "rgb_array":
                # Return a placeholder frame
                return np.zeros((480, 640, 3), dtype=np.uint8)
            return None

        def close(self):
            pass

else:
    class SSLSingleAgentEnv:
        def __init__(self, *args, **kwargs):
            raise ImportError("gymnasium is required. Install with: pip install gymnasium")


# ============================================================
# Multi-Agent PettingZoo Environment
# ============================================================

if HAS_PETTINGZOO:
    class SSLMultiAgentEnv(ParallelEnv):
        """
        PettingZoo Parallel environment for multi-agent SSL RL.

        Each agent controls one robot. Agents are named:
        "blue_0", "blue_1", ..., "yellow_0", "yellow_1", ...
        """

        metadata = {"render_modes": ["human"], "name": "ssl_v0"}

        def __init__(
            self,
            scenario_name: str = "3v3_possession",
            action_level: str = "body",
            dt: float = 0.016,
            render_mode: Optional[str] = None,
        ):
            self.scenario_name = scenario_name
            self.scenario = SCENARIOS[scenario_name]
            self.action_level = action_level
            self.dt = dt
            self.render_mode = render_mode
            self._elapsed = 0.0

            n_blue = self.scenario["blue_robots"]
            n_yellow = self.scenario["yellow_robots"]

            self.possible_agents = (
                [f"blue_{i}" for i in range(n_blue)] +
                [f"yellow_{i}" for i in range(n_yellow)]
            )
            self.agents = list(self.possible_agents)

            # Per-robot observation: ball(6) + self(7) + teammates + opponents
            self._obs_dim = _obs_dim(n_blue, n_yellow)
            # Per-robot action: vx, vy, vw, kick_speed, kick_angle, dribbler
            self._act_dim = 6 if action_level == "body" else 4

        def observation_space(self, agent):
            return spaces.Box(low=-20.0, high=20.0, shape=(self._obs_dim,), dtype=np.float32)

        def action_space(self, agent):
            return spaces.Box(low=-1.0, high=1.0, shape=(self._act_dim,), dtype=np.float32)

        def reset(self, seed=None, options=None):
            self.agents = list(self.possible_agents)
            self._elapsed = 0.0
            obs = {agent: np.zeros(self._obs_dim, dtype=np.float32) for agent in self.agents}
            infos = {agent: {} for agent in self.agents}
            return obs, infos

        def step(self, actions):
            self._elapsed += self.dt
            obs = {agent: np.zeros(self._obs_dim, dtype=np.float32) for agent in self.agents}
            rewards = {agent: 0.0 for agent in self.agents}
            terminations = {agent: False for agent in self.agents}
            truncations = {agent: self._elapsed >= self.scenario["max_time"] for agent in self.agents}
            infos = {agent: {"elapsed_time": self._elapsed} for agent in self.agents}

            if all(truncations.values()):
                self.agents = []

            return obs, rewards, terminations, truncations, infos

        def render(self):
            return None

        def close(self):
            pass

else:
    class SSLMultiAgentEnv:
        def __init__(self, *args, **kwargs):
            raise ImportError("pettingzoo is required. Install with: pip install pettingzoo")
