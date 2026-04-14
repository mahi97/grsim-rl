"""
Pure-Python 2D renderer for the SSL field using matplotlib.

Draws the field, robots (with IDs and orientation), and ball.
Supports 'human' mode (interactive window) and 'rgb_array' mode (numpy array).

No C++ dependency -- works with just numpy and matplotlib.
"""

import numpy as np
from typing import Optional, Dict, Any

try:
    import matplotlib
    import matplotlib.pyplot as plt
    import matplotlib.patches as patches
    from matplotlib.patches import FancyArrowPatch
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False


# Official Division A field dimensions (meters) from SimConfig defaults
FIELD_LENGTH = 12.0
FIELD_WIDTH = 9.0
CENTER_CIRCLE_RADIUS = 0.5
PENALTY_AREA_DEPTH = 1.8
PENALTY_AREA_WIDTH = 3.6
GOAL_WIDTH = 1.8
GOAL_DEPTH = 0.18
ROBOT_RADIUS = 0.09
BALL_RADIUS = 0.0215
MARGIN = 0.7  # visual margin around the field


def _require_matplotlib():
    if not HAS_MATPLOTLIB:
        raise ImportError(
            "matplotlib is required for rendering. "
            "Install with: uv pip install matplotlib"
        )


class FieldRenderer:
    """
    2D renderer for the RoboCup SSL field.

    Parameters
    ----------
    field_length : float
        Length of the field in meters (default 12.0 for Div A).
    field_width : float
        Width of the field in meters (default 9.0 for Div A).
    render_mode : str
        One of 'human' (interactive window) or 'rgb_array' (returns numpy array).
    dpi : int
        Resolution for the rendered image.
    figsize : tuple
        Figure size in inches (width, height).
    """

    def __init__(
        self,
        field_length: float = FIELD_LENGTH,
        field_width: float = FIELD_WIDTH,
        render_mode: str = "rgb_array",
        dpi: int = 100,
        figsize: tuple = (10, 7),
    ):
        _require_matplotlib()

        self.field_length = field_length
        self.field_width = field_width
        self.render_mode = render_mode
        self.dpi = dpi
        self.figsize = figsize

        # Derived dimensions (scale penalty area proportionally for non-default fields)
        scale = field_length / FIELD_LENGTH
        self.center_circle_radius = CENTER_CIRCLE_RADIUS * scale
        self.penalty_area_depth = PENALTY_AREA_DEPTH * scale
        self.penalty_area_width = PENALTY_AREA_WIDTH * scale
        self.goal_width = GOAL_WIDTH * scale
        self.goal_depth = GOAL_DEPTH * scale

        self._fig = None
        self._ax = None

        if render_mode == "human":
            matplotlib.use("TkAgg")
            plt.ion()

    def _setup_figure(self):
        """Create or clear the matplotlib figure and axis."""
        if self._fig is None or not plt.fignum_exists(self._fig.number):
            self._fig, self._ax = plt.subplots(
                1, 1, figsize=self.figsize, dpi=self.dpi
            )
        else:
            self._ax.clear()

    def _draw_field(self):
        """Draw field lines, penalty areas, goals, and center circle."""
        ax = self._ax
        half_l = self.field_length / 2
        half_w = self.field_width / 2

        # Green pitch background
        ax.set_facecolor("#2d8a4e")
        ax.set_xlim(-half_l - MARGIN, half_l + MARGIN)
        ax.set_ylim(-half_w - MARGIN, half_w + MARGIN)
        ax.set_aspect("equal")
        ax.set_xlabel("X (m)")
        ax.set_ylabel("Y (m)")

        lw = 1.5  # line width for field markings
        line_color = "white"

        # Field boundary
        ax.plot(
            [-half_l, half_l, half_l, -half_l, -half_l],
            [-half_w, -half_w, half_w, half_w, -half_w],
            color=line_color, linewidth=lw,
        )

        # Center line
        ax.plot([0, 0], [-half_w, half_w], color=line_color, linewidth=lw)

        # Center circle
        center_circle = plt.Circle(
            (0, 0), self.center_circle_radius,
            fill=False, color=line_color, linewidth=lw,
        )
        ax.add_patch(center_circle)

        # Center dot
        ax.plot(0, 0, "o", color=line_color, markersize=3)

        # Penalty areas (left and right)
        pa_half_w = self.penalty_area_width / 2
        pa_d = self.penalty_area_depth

        # Left penalty area (blue goal end, x = -half_l)
        ax.plot(
            [-half_l, -half_l + pa_d, -half_l + pa_d, -half_l],
            [-pa_half_w, -pa_half_w, pa_half_w, pa_half_w],
            color=line_color, linewidth=lw,
        )

        # Right penalty area (yellow goal end, x = +half_l)
        ax.plot(
            [half_l, half_l - pa_d, half_l - pa_d, half_l],
            [-pa_half_w, -pa_half_w, pa_half_w, pa_half_w],
            color=line_color, linewidth=lw,
        )

        # Goals
        g_half_w = self.goal_width / 2
        g_d = self.goal_depth

        # Left goal (at x = -half_l, extends outward to -half_l - g_d)
        ax.plot(
            [-half_l, -half_l - g_d, -half_l - g_d, -half_l],
            [-g_half_w, -g_half_w, g_half_w, g_half_w],
            color="#4488ff", linewidth=2.5,
        )

        # Right goal (at x = +half_l, extends outward to +half_l + g_d)
        ax.plot(
            [half_l, half_l + g_d, half_l + g_d, half_l],
            [-g_half_w, -g_half_w, g_half_w, g_half_w],
            color="#ffcc00", linewidth=2.5,
        )

    def _draw_robot(self, x: float, y: float, theta: float, robot_id: int,
                    team: str):
        """Draw a single robot as a filled circle with ID and orientation arrow."""
        ax = self._ax
        r = ROBOT_RADIUS * 6  # visual scaling for visibility

        if team == "blue":
            face_color = "#2266dd"
            edge_color = "#1144aa"
            text_color = "white"
        else:
            face_color = "#ddcc22"
            edge_color = "#aa9900"
            text_color = "black"

        # Robot body
        circle = plt.Circle(
            (x, y), r,
            facecolor=face_color, edgecolor=edge_color, linewidth=1.5,
            zorder=10,
        )
        ax.add_patch(circle)

        # Orientation arrow
        arrow_len = r * 1.3
        dx = arrow_len * np.cos(theta)
        dy = arrow_len * np.sin(theta)
        ax.annotate(
            "",
            xy=(x + dx, y + dy),
            xytext=(x, y),
            arrowprops=dict(
                arrowstyle="->",
                color=edge_color,
                lw=2.0,
            ),
            zorder=11,
        )

        # Robot ID label
        ax.text(
            x, y, str(robot_id),
            ha="center", va="center",
            fontsize=7, fontweight="bold",
            color=text_color,
            zorder=12,
        )

    def _draw_ball(self, x: float, y: float):
        """Draw the ball as an orange circle."""
        ax = self._ax
        ball = plt.Circle(
            (x, y), BALL_RADIUS * 8,  # visual scaling
            facecolor="#ff6600", edgecolor="#cc4400", linewidth=1.2,
            zorder=15,
        )
        ax.add_patch(ball)

    def render_state(self, world_state: Dict[str, Any]) -> Optional[np.ndarray]:
        """
        Render the current world state.

        Parameters
        ----------
        world_state : dict
            Dictionary with keys:
            - 'ball': dict with 'x', 'y' (and optionally 'vx', 'vy', 'vz', 'z')
            - 'blue_robots': list of dicts, each with 'id', 'x', 'y', 'theta'
            - 'yellow_robots': list of dicts, each with 'id', 'x', 'y', 'theta'

        Returns
        -------
        np.ndarray or None
            If render_mode is 'rgb_array', returns HxWx3 uint8 numpy array.
            If render_mode is 'human', displays/updates the window and returns None.
        """
        self._setup_figure()
        self._draw_field()

        # Draw ball
        ball = world_state.get("ball", {})
        bx = ball.get("x", 0.0)
        by = ball.get("y", 0.0)
        self._draw_ball(bx, by)

        # Draw blue robots
        for robot in world_state.get("blue_robots", []):
            self._draw_robot(
                robot.get("x", 0.0),
                robot.get("y", 0.0),
                robot.get("theta", 0.0),
                robot.get("id", 0),
                "blue",
            )

        # Draw yellow robots
        for robot in world_state.get("yellow_robots", []):
            self._draw_robot(
                robot.get("x", 0.0),
                robot.get("y", 0.0),
                robot.get("theta", 0.0),
                robot.get("id", 0),
                "yellow",
            )

        self._ax.set_title("grsim-rl", fontsize=10, color="white")

        if self.render_mode == "rgb_array":
            self._fig.canvas.draw()
            buf = self._fig.canvas.buffer_rgba()
            img = np.asarray(buf)[:, :, :3].copy()  # drop alpha channel
            return img
        elif self.render_mode == "human":
            self._fig.canvas.draw_idle()
            self._fig.canvas.flush_events()
            plt.pause(0.001)
            return None
        else:
            return None

    def close(self):
        """Close the rendering window and free resources."""
        if self._fig is not None:
            plt.close(self._fig)
            self._fig = None
            self._ax = None


def obs_to_world_state(obs: np.ndarray, n_blue: int, n_yellow: int) -> Dict[str, Any]:
    """
    Convert a flat observation vector back into a world_state dict for rendering.

    The observation layout is:
        [ball_x, ball_y, ball_z, ball_vx, ball_vy, ball_vz,
         blue_0_x, blue_0_y, blue_0_theta, blue_0_vx, blue_0_vy, blue_0_vw, blue_0_infrared,
         blue_1_x, ...
         yellow_0_x, ...]

    Each robot has 7 values: x, y, theta, vx, vy, vw, infrared.
    Ball has 6 values: x, y, z, vx, vy, vz.
    """
    state: Dict[str, Any] = {}

    # Ball: first 6 elements
    state["ball"] = {
        "x": float(obs[0]),
        "y": float(obs[1]),
        "z": float(obs[2]) if len(obs) > 2 else 0.0,
        "vx": float(obs[3]) if len(obs) > 3 else 0.0,
        "vy": float(obs[4]) if len(obs) > 4 else 0.0,
        "vz": float(obs[5]) if len(obs) > 5 else 0.0,
    }

    offset = 6

    # Blue robots
    blue_robots = []
    for i in range(n_blue):
        base = offset + i * 7
        if base + 7 <= len(obs):
            blue_robots.append({
                "id": i,
                "x": float(obs[base]),
                "y": float(obs[base + 1]),
                "theta": float(obs[base + 2]),
            })
    state["blue_robots"] = blue_robots

    # Yellow robots
    offset_yellow = offset + n_blue * 7
    yellow_robots = []
    for i in range(n_yellow):
        base = offset_yellow + i * 7
        if base + 7 <= len(obs):
            yellow_robots.append({
                "id": i,
                "x": float(obs[base]),
                "y": float(obs[base + 1]),
                "theta": float(obs[base + 2]),
            })
    state["yellow_robots"] = yellow_robots

    return state
