"""
Coach-level Gymnasium environment.

Exposes strategic decisions (formation, pressing, attack pattern) as the
action space instead of per-robot velocity commands. The coach compiler
translates these into per-robot skill commands each tick.

This enables RL at the team-strategy level rather than individual robot control.
"""

import numpy as np
from typing import Optional, Tuple, Dict, Any
from enum import IntEnum

try:
    import gymnasium as gym
    from gymnasium import spaces
except ImportError:
    raise ImportError("gymnasium required: uv pip install gymnasium")

from pygrsim.envs import SSLSingleAgentEnv, SCENARIOS, _obs_dim


class Formation(IntEnum):
    DEFAULT = 0
    DEFENSIVE_4_1 = 1
    BALANCED_3_2 = 2
    ATTACKING_2_3 = 3
    WIDE_2_2_1 = 4
    KICKOFF = 5
    PENALTY_ATTACK = 6
    PENALTY_DEFEND = 7


class SetPiece(IntEnum):
    NONE = 0
    KICKOFF_SHORT_PASS = 1
    KICKOFF_LONG_PASS = 2
    FREEKICK_DIRECT_SHOT = 3
    FREEKICK_PASS_AND_SHOOT = 4
    FREEKICK_CHIP_TO_CORNER = 5
    CORNER_KICK_NEAR_POST = 6
    CORNER_KICK_FAR_POST = 7
    GOALKICK_SHORT = 8
    GOALKICK_LONG = 9


class PressingLevel(IntEnum):
    PASSIVE = 0
    MODERATE = 1
    HIGH_PRESS = 2


class AttackPattern(IntEnum):
    NONE = 0
    DIRECT_ATTACK = 1
    POSSESSION_BUILDUP = 2
    WING_PLAY = 3
    COUNTER_ATTACK = 4


class TransitionMode(IntEnum):
    BALANCED = 0
    ATTACK_PRIORITY = 1
    DEFENSE_PRIORITY = 2


# Formation position templates (x_ratio, y_ratio) relative to field
FORMATION_POSITIONS = {
    Formation.DEFAULT: [
        (-0.9, 0.0), (-0.5, 0.3), (-0.5, 0.0), (-0.5, -0.3), (-0.1, 0.2), (0.1, 0.0)
    ],
    Formation.DEFENSIVE_4_1: [
        (-0.9, 0.0), (-0.6, 0.5), (-0.6, 0.15), (-0.6, -0.15), (-0.6, -0.5), (-0.2, 0.0)
    ],
    Formation.BALANCED_3_2: [
        (-0.9, 0.0), (-0.5, 0.4), (-0.5, 0.0), (-0.5, -0.4), (-0.1, 0.25), (-0.1, -0.25)
    ],
    Formation.ATTACKING_2_3: [
        (-0.9, 0.0), (-0.5, 0.3), (-0.5, -0.3), (0.1, 0.4), (0.1, 0.0), (0.1, -0.4)
    ],
    Formation.WIDE_2_2_1: [
        (-0.9, 0.0), (-0.5, 0.4), (-0.5, -0.4), (-0.1, 0.6), (-0.1, -0.6), (0.2, 0.0)
    ],
    Formation.KICKOFF: [
        (-0.9, 0.0), (-0.5, 0.3), (-0.5, -0.3), (-0.2, 0.4), (-0.2, -0.4), (-0.05, 0.0)
    ],
}


def _coach_action_to_body_actions(
    coach_action: np.ndarray,
    obs: np.ndarray,
    num_robots: int,
    field_length: float = 12.0,
    field_width: float = 9.0,
) -> np.ndarray:
    """
    Convert a coach-level action to per-robot body velocity actions.

    Coach action space (Discrete + continuous):
      [0]: formation index (0-7, discretized from continuous)
      [1]: pressing level (0-2, discretized)
      [2]: attack pattern (0-4, discretized)
      [3]: transition mode (0-2, discretized)
      [4]: team aggressiveness (continuous 0-1)

    Returns: (num_robots * 6,) body velocity action array
    """
    # Decode coach decisions
    formation_idx = int(np.clip(np.round((coach_action[0] + 1) * 3.5), 0, 7))
    pressing = int(np.clip(np.round((coach_action[1] + 1) * 1.0), 0, 2))
    attack_pat = int(np.clip(np.round((coach_action[2] + 1) * 2.0), 0, 4))
    aggressiveness = (coach_action[4] + 1.0) / 2.0  # Map [-1,1] to [0,1]

    # Get formation positions
    formation = Formation(min(formation_idx, len(Formation) - 1))
    positions = FORMATION_POSITIONS.get(formation, FORMATION_POSITIONS[Formation.DEFAULT])

    # Extract ball position from observation
    ball_x, ball_y = obs[0], obs[1]

    # Convert formation positions to field coordinates
    half_l = field_length / 2.0
    half_w = field_width / 2.0

    body_actions = np.zeros(num_robots * 6, dtype=np.float32)

    for i in range(min(num_robots, len(positions))):
        px = positions[i][0] * half_l
        py = positions[i][1] * half_w

        # Offset towards ball based on aggressiveness
        px += (ball_x - px) * aggressiveness * 0.3
        py += (ball_y - py) * aggressiveness * 0.2

        # Extract robot position from observation (after ball: 6 + i*7)
        robot_base = 6 + i * 7
        if robot_base + 2 < len(obs):
            rx, ry = obs[robot_base], obs[robot_base + 1]
        else:
            rx, ry = 0.0, 0.0

        # Simple proportional control to target
        dx = px - rx
        dy = py - ry
        dist = np.sqrt(dx * dx + dy * dy)

        # Scale velocity (normalized to [-1, 1])
        speed = min(dist * 2.0, 1.0)
        if dist > 0.01:
            vx = dx / dist * speed
            vy = dy / dist * speed
        else:
            vx, vy = 0.0, 0.0

        # Face towards ball
        angle_to_ball = np.arctan2(ball_y - ry, ball_x - rx)
        if robot_base + 2 < len(obs):
            current_angle = obs[robot_base + 2]
        else:
            current_angle = 0.0
        angle_err = angle_to_ball - current_angle
        # Normalize angle error
        while angle_err > np.pi: angle_err -= 2 * np.pi
        while angle_err < -np.pi: angle_err += 2 * np.pi
        vw = np.clip(angle_err * 2.0, -1.0, 1.0)

        # Closest robot to ball gets kick/dribble commands
        ball_dist = np.sqrt((ball_x - rx) ** 2 + (ball_y - ry) ** 2)
        kick_speed = 0.0
        dribbler = 0.0
        if ball_dist < 0.3:
            dribbler = 1.0
            if pressing >= 2 and ball_dist < 0.15:  # High press → shoot
                kick_speed = aggressiveness

        base = i * 6
        body_actions[base + 0] = np.clip(vx, -1, 1)
        body_actions[base + 1] = np.clip(vy, -1, 1)
        body_actions[base + 2] = np.clip(vw, -1, 1)
        body_actions[base + 3] = kick_speed
        body_actions[base + 4] = 0.0  # chip angle
        body_actions[base + 5] = dribbler

    return body_actions


class SSLCoachEnv(gym.Env):
    """
    Coach-level Gymnasium environment.

    Action space: Box(5) — [formation, pressing, attack, transition, aggressiveness]
    Observation: same as SSLSingleAgentEnv (full state)

    The coach action is compiled to per-robot body velocities each step.
    """

    metadata = {"render_modes": ["human", "rgb_array"]}

    def __init__(
        self,
        scenario_name: str = "kickoff_attack",
        dt: float = 0.016,
        render_mode: Optional[str] = None,
    ):
        super().__init__()

        self.scenario = SCENARIOS[scenario_name]
        self.inner_env = SSLSingleAgentEnv(
            scenario_name=scenario_name,
            action_level="body",
            dt=dt,
            render_mode=render_mode,
        )

        self.observation_space = self.inner_env.observation_space

        # Coach action: formation, pressing, attack, transition, aggressiveness
        self.action_space = spaces.Box(low=-1.0, high=1.0, shape=(5,), dtype=np.float32)

        self._num_robots = self.scenario["blue_robots"]
        self._last_obs = None

    def reset(self, seed=None, options=None):
        obs, info = self.inner_env.reset(seed=seed, options=options)
        self._last_obs = obs
        info["action_level"] = "coach"
        return obs, info

    def step(self, action: np.ndarray):
        body_actions = _coach_action_to_body_actions(
            action, self._last_obs, self._num_robots
        )
        obs, reward, done, truncated, info = self.inner_env.step(body_actions)
        self._last_obs = obs
        info["coach_action"] = action.tolist()
        return obs, reward, done, truncated, info

    def render(self):
        return self.inner_env.render()

    def close(self):
        self.inner_env.close()
