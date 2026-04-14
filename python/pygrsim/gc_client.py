"""
Game-Controller CI adapter for Python.

Connects to the ssl-game-controller CI mode on TCP port 10009.
The CI protocol is a synchronous request/response over TCP using
varint-delimited protobuf messages:

    Send:    CiInput  { timestamp_ns, tracker_packet }
    Receive: CiOutput { referee_message }

This module uses raw protobuf wire encoding to avoid requiring
compiled proto files. It is self-contained and needs only the
Python standard library.

Usage:
    from pygrsim.gc_client import GCClient, GameState

    client = GCClient()
    client.connect()

    # Each step: send world state, get referee command
    ref = client.send_state(world_state_dict)
    print(ref.state, ref.command, ref.blue_score, ref.yellow_score)

    client.disconnect()
"""

import enum
import socket
import struct
import time
import logging
from dataclasses import dataclass, field
from typing import Optional, Dict, Any, List, Tuple

logger = logging.getLogger(__name__)

# ============================================================
# Enums matching ssl-game-controller
# ============================================================


class RefereeCommand(enum.IntEnum):
    """Referee command values from the ssl-game-controller Referee message."""
    HALT = 0
    STOP = 1
    NORMAL_START = 2
    FORCE_START = 3
    PREPARE_KICKOFF_YELLOW = 4
    PREPARE_KICKOFF_BLUE = 5
    PREPARE_PENALTY_YELLOW = 6
    PREPARE_PENALTY_BLUE = 7
    DIRECT_FREE_YELLOW = 8
    DIRECT_FREE_BLUE = 9
    TIMEOUT_YELLOW = 12
    TIMEOUT_BLUE = 13
    BALL_PLACEMENT_YELLOW = 16
    BALL_PLACEMENT_BLUE = 17


class GameStage(enum.IntEnum):
    """Game stage values from the ssl-game-controller Referee message."""
    NORMAL_FIRST_HALF_PRE = 0
    NORMAL_FIRST_HALF = 1
    NORMAL_HALF_TIME = 2
    NORMAL_SECOND_HALF_PRE = 3
    NORMAL_SECOND_HALF = 4
    EXTRA_TIME_BREAK = 5
    EXTRA_FIRST_HALF_PRE = 6
    EXTRA_FIRST_HALF = 7
    EXTRA_HALF_TIME = 8
    EXTRA_SECOND_HALF_PRE = 9
    EXTRA_SECOND_HALF = 10
    PENALTY_SHOOTOUT_BREAK = 11
    PENALTY_SHOOTOUT = 12
    POST_GAME = 13


class GameState(enum.Enum):
    """Simplified game state derived from command + stage."""
    HALT = "halt"
    STOP = "stop"
    RUNNING = "running"
    KICKOFF = "kickoff"
    FREE_KICK = "free_kick"
    PENALTY = "penalty"
    BALL_PLACEMENT = "ball_placement"
    TIMEOUT = "timeout"
    POST_GAME = "post_game"


# ============================================================
# Referee state dataclass
# ============================================================


@dataclass
class RefereeState:
    """Parsed referee state from a CiOutput response."""
    command: RefereeCommand = RefereeCommand.HALT
    stage: GameStage = GameStage.NORMAL_FIRST_HALF_PRE
    state: GameState = GameState.HALT

    command_timestamp_us: int = 0
    command_counter: int = 0

    blue_score: int = 0
    yellow_score: int = 0

    blue_yellow_cards: int = 0
    yellow_yellow_cards: int = 0
    blue_red_cards: int = 0
    yellow_red_cards: int = 0

    placement_x: float = 0.0
    placement_y: float = 0.0
    has_placement_pos: bool = False

    raw_bytes: bytes = field(default=b"", repr=False)


# ============================================================
# Protobuf wire-format helpers (no external deps)
# ============================================================

# Wire types
_VARINT = 0
_FIXED64 = 1
_LENGTH_DELIMITED = 2
_FIXED32 = 5


def _encode_varint(value: int) -> bytes:
    """Encode an unsigned integer as a protobuf varint."""
    parts = []
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            parts.append(byte | 0x80)
        else:
            parts.append(byte)
            break
    return bytes(parts)


def _decode_varint(data: bytes, offset: int = 0) -> Tuple[int, int]:
    """Decode a varint from bytes. Returns (value, new_offset)."""
    result = 0
    shift = 0
    while offset < len(data):
        byte = data[offset]
        result |= (byte & 0x7F) << shift
        offset += 1
        if (byte & 0x80) == 0:
            return result, offset
        shift += 7
    raise ValueError("Truncated varint")


def _encode_tag(field_number: int, wire_type: int) -> bytes:
    return _encode_varint((field_number << 3) | wire_type)


def _encode_float(value: float) -> bytes:
    return struct.pack("<f", value)


def _encode_double(value: float) -> bytes:
    return struct.pack("<d", value)


def _encode_field_varint(field_number: int, value: int) -> bytes:
    return _encode_tag(field_number, _VARINT) + _encode_varint(value)


def _encode_field_float(field_number: int, value: float) -> bytes:
    return _encode_tag(field_number, _FIXED32) + _encode_float(value)


def _encode_field_double(field_number: int, value: float) -> bytes:
    return _encode_tag(field_number, _FIXED64) + _encode_double(value)


def _encode_field_bytes(field_number: int, data: bytes) -> bytes:
    return _encode_tag(field_number, _LENGTH_DELIMITED) + _encode_varint(len(data)) + data


def _encode_field_string(field_number: int, value: str) -> bytes:
    encoded = value.encode("utf-8")
    return _encode_field_bytes(field_number, encoded)


def _length_delimit(data: bytes) -> bytes:
    """Prepend varint length to data (for TCP framing)."""
    return _encode_varint(len(data)) + data


# ============================================================
# Protobuf decoder (minimal, field-level)
# ============================================================


def _iter_fields(data: bytes, offset: int = 0, end: Optional[int] = None):
    """
    Iterate over protobuf fields in a message.
    Yields (field_number, wire_type, value, new_offset) tuples.
    For varint: value is int.
    For fixed64: value is 8 raw bytes.
    For fixed32: value is 4 raw bytes.
    For length-delimited: value is bytes.
    """
    if end is None:
        end = len(data)
    while offset < end:
        tag, offset = _decode_varint(data, offset)
        field_number = tag >> 3
        wire_type = tag & 0x07

        if wire_type == _VARINT:
            value, offset = _decode_varint(data, offset)
            yield field_number, wire_type, value, offset
        elif wire_type == _FIXED64:
            value = data[offset:offset + 8]
            offset += 8
            yield field_number, wire_type, value, offset
        elif wire_type == _LENGTH_DELIMITED:
            length, offset = _decode_varint(data, offset)
            value = data[offset:offset + length]
            offset += length
            yield field_number, wire_type, value, offset
        elif wire_type == _FIXED32:
            value = data[offset:offset + 4]
            offset += 4
            yield field_number, wire_type, value, offset
        else:
            raise ValueError(f"Unknown wire type {wire_type} at offset {offset}")


def _decode_float(data: bytes) -> float:
    return struct.unpack("<f", data)[0]


def _decode_double(data: bytes) -> float:
    return struct.unpack("<d", data)[0]


# ============================================================
# World state to tracker packet conversion
# ============================================================


def world_state_to_tracker(world: Dict[str, Any], timestamp_ns: Optional[int] = None) -> bytes:
    """
    Convert a WorldState dict to a TrackerWrapperPacket protobuf.

    Expected dict format (matches grsim_core::WorldState / pygrsim observation):
        {
            "sim_time": float,       # seconds
            "frame_num": int,
            "ball": {"x": float, "y": float, "z": float,
                     "vx": float, "vy": float, "vz": float},
            "blue_robots": [
                {"id": int, "x": float, "y": float, "orientation": float,
                 "vx": float, "vy": float, "vw": float, "present": bool},
                ...
            ],
            "yellow_robots": [
                {"id": int, "x": float, "y": float, "orientation": float,
                 "vx": float, "vy": float, "vw": float, "present": bool},
                ...
            ],
        }

    Returns the serialized TrackerWrapperPacket bytes.
    """
    ball = world.get("ball", {})
    frame_num = int(world.get("frame_num", 0))
    sim_time = float(world.get("sim_time", 0.0))

    # Build TrackedBall: { Vector3 pos=1, Vector3 vel=2 }
    ball_pos = (
        _encode_field_float(1, ball.get("x", 0.0))
        + _encode_field_float(2, ball.get("y", 0.0))
        + _encode_field_float(3, ball.get("z", 0.0))
    )
    ball_vel = (
        _encode_field_float(1, ball.get("vx", 0.0))
        + _encode_field_float(2, ball.get("vy", 0.0))
        + _encode_field_float(3, ball.get("vz", 0.0))
    )
    tracked_ball = (
        _encode_field_bytes(1, ball_pos)
        + _encode_field_bytes(2, ball_vel)
    )

    # Build TrackedRobots
    robot_entries = b""
    for team_name, team_enum_val in [("blue_robots", 2), ("yellow_robots", 1)]:
        for robot in world.get(team_name, []):
            if not robot.get("present", True):
                continue

            # RobotId { uint32 id=1; Team team=2; }
            robot_id = (
                _encode_field_varint(1, int(robot.get("id", 0)))
                + _encode_field_varint(2, team_enum_val)
            )

            # pos: Vector3
            pos = (
                _encode_field_float(1, robot.get("x", 0.0))
                + _encode_field_float(2, robot.get("y", 0.0))
                + _encode_field_float(3, 0.0)  # z = 0 for ground robots
            )

            # vel: Vector2
            vel = (
                _encode_field_float(1, robot.get("vx", 0.0))
                + _encode_field_float(2, robot.get("vy", 0.0))
            )

            tracked_robot = (
                _encode_field_bytes(1, robot_id)      # robot_id
                + _encode_field_bytes(2, pos)          # pos
                + _encode_field_float(3, robot.get("orientation", 0.0))  # orientation
                + _encode_field_bytes(4, vel)          # vel
                + _encode_field_float(5, robot.get("vw", 0.0))  # vel_angular
            )
            robot_entries += _encode_field_bytes(4, tracked_robot)

    # TrackedFrame { uint32 frame_number=1, double timestamp=2,
    #                repeated TrackedBall balls=3, repeated TrackedRobot robots=4 }
    tracked_frame = (
        _encode_field_varint(1, frame_num)
        + _encode_field_double(2, sim_time)
        + _encode_field_bytes(3, tracked_ball)
        + robot_entries
    )

    # TrackerWrapperPacket { string uuid=1, string source_name=2, TrackedFrame frame=3 }
    tracker = (
        _encode_field_string(1, "grsim-rl")
        + _encode_field_string(2, "grsim-rl-ci")
        + _encode_field_bytes(3, tracked_frame)
    )

    return tracker


def build_ci_input(world: Dict[str, Any], timestamp_ns: Optional[int] = None) -> bytes:
    """
    Build a CiInput protobuf message from a world state dict.

    CiInput { uint64 timestamp=1, TrackerWrapperPacket tracker_packet=2 }
    """
    if timestamp_ns is None:
        sim_time = float(world.get("sim_time", 0.0))
        timestamp_ns = int(sim_time * 1e9)

    tracker = world_state_to_tracker(world, timestamp_ns)

    ci_input = (
        _encode_field_varint(1, timestamp_ns)
        + _encode_field_bytes(2, tracker)
    )
    return ci_input


# ============================================================
# CiOutput parser
# ============================================================


def _parse_team_info(data: bytes) -> Dict[str, int]:
    """Parse a TeamInfo submessage. Returns dict with score, yellow_cards, red_cards."""
    info = {"score": 0, "yellow_cards": 0, "red_cards": 0}
    for fn, wt, val, _ in _iter_fields(data):
        if wt != _VARINT:
            continue
        if fn == 2:
            info["score"] = val
        elif fn == 3:
            info["red_cards"] = val
        elif fn == 5:
            info["yellow_cards"] = val
    return info


def _parse_designated_position(data: bytes) -> Tuple[float, float]:
    """Parse Point { float x=1; float y=2; }"""
    x, y = 0.0, 0.0
    for fn, wt, val, _ in _iter_fields(data):
        if wt == _FIXED32:
            if fn == 1:
                x = _decode_float(val)
            elif fn == 2:
                y = _decode_float(val)
    return x, y


def parse_ci_output(data: bytes) -> RefereeState:
    """
    Parse a CiOutput protobuf message into a RefereeState.

    CiOutput { Referee referee_message = 1; }
    """
    ref_data = None

    for fn, wt, val, _ in _iter_fields(data):
        if fn == 1 and wt == _LENGTH_DELIMITED:
            ref_data = val
            break

    if ref_data is None:
        raise ValueError("CiOutput missing referee_message field")

    state = RefereeState(raw_bytes=data)

    for fn, wt, val, _ in _iter_fields(ref_data):
        if wt == _VARINT:
            if fn == 2:
                try:
                    state.stage = GameStage(val)
                except ValueError:
                    pass
            elif fn == 4:
                try:
                    state.command = RefereeCommand(val)
                except ValueError:
                    pass
            elif fn == 5:
                state.command_counter = val
            elif fn == 6:
                state.command_timestamp_us = val
        elif wt == _LENGTH_DELIMITED:
            if fn == 7:  # yellow TeamInfo
                info = _parse_team_info(val)
                state.yellow_score = info["score"]
                state.yellow_yellow_cards = info["yellow_cards"]
                state.yellow_red_cards = info["red_cards"]
            elif fn == 8:  # blue TeamInfo
                info = _parse_team_info(val)
                state.blue_score = info["score"]
                state.blue_yellow_cards = info["yellow_cards"]
                state.blue_red_cards = info["red_cards"]
            elif fn == 9:  # designated_position
                x, y = _parse_designated_position(val)
                state.placement_x = x
                state.placement_y = y
                state.has_placement_pos = True

    state.state = derive_game_state(state.command, state.stage)
    return state


def derive_game_state(command: RefereeCommand, stage: GameStage) -> GameState:
    """Map referee command + stage to simplified GameState."""
    if stage == GameStage.POST_GAME:
        return GameState.POST_GAME

    mapping = {
        RefereeCommand.HALT: GameState.HALT,
        RefereeCommand.STOP: GameState.STOP,
        RefereeCommand.NORMAL_START: GameState.RUNNING,
        RefereeCommand.FORCE_START: GameState.RUNNING,
        RefereeCommand.PREPARE_KICKOFF_YELLOW: GameState.KICKOFF,
        RefereeCommand.PREPARE_KICKOFF_BLUE: GameState.KICKOFF,
        RefereeCommand.PREPARE_PENALTY_YELLOW: GameState.PENALTY,
        RefereeCommand.PREPARE_PENALTY_BLUE: GameState.PENALTY,
        RefereeCommand.DIRECT_FREE_YELLOW: GameState.FREE_KICK,
        RefereeCommand.DIRECT_FREE_BLUE: GameState.FREE_KICK,
        RefereeCommand.TIMEOUT_YELLOW: GameState.TIMEOUT,
        RefereeCommand.TIMEOUT_BLUE: GameState.TIMEOUT,
        RefereeCommand.BALL_PLACEMENT_YELLOW: GameState.BALL_PLACEMENT,
        RefereeCommand.BALL_PLACEMENT_BLUE: GameState.BALL_PLACEMENT,
    }
    return mapping.get(command, GameState.HALT)


# ============================================================
# GCClient — main interface
# ============================================================


class GCClient:
    """
    Client for the ssl-game-controller CI mode.

    Connects via TCP to the game-controller and exchanges world state
    for referee commands each simulation tick.

    Example:
        client = GCClient(host="127.0.0.1", port=10009)
        client.connect()

        world = {
            "sim_time": 1.0, "frame_num": 60,
            "ball": {"x": 0.0, "y": 0.0, "z": 0.0, "vx": 0.0, "vy": 0.0, "vz": 0.0},
            "blue_robots": [{"id": 0, "x": -1.0, "y": 0.0, "orientation": 0.0,
                             "vx": 0.0, "vy": 0.0, "vw": 0.0}],
            "yellow_robots": [],
        }
        ref_state = client.send_state(world)
        print(ref_state.state)  # GameState.HALT, .RUNNING, etc.

        client.disconnect()

    Offline mode:
        client = GCClient(offline=True)
        client.connect()  # Always succeeds
        ref_state = client.send_state(world)  # Returns default HALT state
    """

    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 10009,
        connect_timeout: float = 3.0,
        read_timeout: float = 0.5,
        offline: bool = False,
    ):
        self.host = host
        self.port = port
        self.connect_timeout = connect_timeout
        self.read_timeout = read_timeout
        self.offline = offline

        self._socket: Optional[socket.socket] = None
        self._connected = False
        self._last_state = RefereeState()
        self._offline_state = RefereeState(
            command=RefereeCommand.HALT,
            stage=GameStage.NORMAL_FIRST_HALF,
            state=GameState.HALT,
        )
        self._recv_buf = b""

    @property
    def connected(self) -> bool:
        """True if connected to game-controller or in offline mode."""
        return self._connected or self.offline

    @property
    def last_state(self) -> RefereeState:
        """The most recent RefereeState from the last exchange."""
        return self._last_state

    def connect(self) -> bool:
        """
        Connect to the game-controller CI TCP port.
        In offline mode, always returns True without opening a socket.
        """
        if self.offline:
            self._connected = True
            logger.info("GCClient: offline mode, no connection needed")
            return True

        try:
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._socket.settimeout(self.connect_timeout)
            self._socket.connect((self.host, self.port))
            self._socket.settimeout(self.read_timeout)
            self._connected = True
            self._recv_buf = b""
            logger.info("GCClient: connected to %s:%d", self.host, self.port)
            return True
        except (socket.error, OSError) as e:
            logger.warning("GCClient: failed to connect to %s:%d — %s", self.host, self.port, e)
            self._socket = None
            self._connected = False
            return False

    def disconnect(self):
        """Disconnect from the game-controller."""
        if self._socket is not None:
            try:
                self._socket.close()
            except OSError:
                pass
            self._socket = None
        self._connected = False
        self._recv_buf = b""
        logger.info("GCClient: disconnected")

    def send_state(self, world: Dict[str, Any]) -> RefereeState:
        """
        Send world state to game-controller and receive referee state.

        Args:
            world: Dict matching the WorldState format (see world_state_to_tracker docs).

        Returns:
            RefereeState with the current command, score, cards, etc.

        Raises:
            ConnectionError: If not connected and not in offline mode.
            RuntimeError: If communication with GC fails.
        """
        if self.offline:
            self._last_state = self._offline_state
            return self._offline_state

        if not self._connected or self._socket is None:
            raise ConnectionError("Not connected to game-controller. Call connect() first.")

        # Build CiInput
        ci_input = build_ci_input(world)

        # Send with varint length prefix
        message = _length_delimit(ci_input)
        try:
            self._socket.sendall(message)
        except (socket.error, OSError) as e:
            self._connected = False
            raise RuntimeError(f"Failed to send CiInput: {e}") from e

        # Receive varint-delimited CiOutput
        try:
            response_data = self._recv_length_delimited()
        except (socket.error, OSError, ValueError) as e:
            self._connected = False
            raise RuntimeError(f"Failed to receive CiOutput: {e}") from e

        # Parse
        ref_state = parse_ci_output(response_data)
        self._last_state = ref_state
        return ref_state

    def get_command(self) -> RefereeCommand:
        """Get the current referee command from the last exchange."""
        return self._last_state.command

    def get_game_state(self) -> GameState:
        """Get the simplified game state from the last exchange."""
        return self._last_state.state

    def set_offline_state(self, state: RefereeState):
        """Set the state returned in offline mode (for testing)."""
        self._offline_state = state

    def _recv_length_delimited(self) -> bytes:
        """Receive a varint-length-delimited protobuf message from the socket."""
        # Read until we have at least the varint header
        while True:
            try:
                msg_len, header_end = _decode_varint(self._recv_buf, 0)
                break
            except (ValueError, IndexError):
                # Need more data for the varint
                chunk = self._socket.recv(4096)
                if not chunk:
                    raise ConnectionError("Connection closed by game-controller")
                self._recv_buf += chunk

        # Read until we have the full message
        total_needed = header_end + msg_len
        while len(self._recv_buf) < total_needed:
            chunk = self._socket.recv(4096)
            if not chunk:
                raise ConnectionError("Connection closed by game-controller")
            self._recv_buf += chunk

        # Extract the message, keep any remainder in the buffer
        payload = self._recv_buf[header_end:total_needed]
        self._recv_buf = self._recv_buf[total_needed:]
        return payload

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()
        return False

    def __del__(self):
        try:
            self.disconnect()
        except Exception:
            pass


# ============================================================
# Convenience: command-line test
# ============================================================


def _make_sample_world() -> Dict[str, Any]:
    """Create a sample world state for testing."""
    return {
        "sim_time": 0.0,
        "frame_num": 0,
        "ball": {"x": 0.0, "y": 0.0, "z": 0.0, "vx": 0.0, "vy": 0.0, "vz": 0.0},
        "blue_robots": [
            {"id": i, "x": -2.0 + i * 0.3, "y": 0.0, "orientation": 0.0,
             "vx": 0.0, "vy": 0.0, "vw": 0.0, "present": True}
            for i in range(6)
        ],
        "yellow_robots": [
            {"id": i, "x": 2.0 - i * 0.3, "y": 0.0, "orientation": 3.14,
             "vx": 0.0, "vy": 0.0, "vw": 0.0, "present": True}
            for i in range(6)
        ],
    }


if __name__ == "__main__":
    import argparse
    import sys

    parser = argparse.ArgumentParser(description="Test the GC CI client")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=10009)
    parser.add_argument("--offline", action="store_true", help="Run in offline/mock mode")
    parser.add_argument("--steps", type=int, default=10, help="Number of CI exchanges")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)

    client = GCClient(host=args.host, port=args.port, offline=args.offline)
    if not client.connect():
        print("Failed to connect to game-controller", file=sys.stderr)
        sys.exit(1)

    world = _make_sample_world()
    for step in range(args.steps):
        world["sim_time"] = step * 0.016
        world["frame_num"] = step

        try:
            ref = client.send_state(world)
            print(
                f"Step {step:4d}: state={ref.state.value:16s} "
                f"cmd={ref.command.name:24s} "
                f"score={ref.blue_score}-{ref.yellow_score}"
            )
        except (RuntimeError, ConnectionError) as e:
            print(f"Step {step}: error — {e}", file=sys.stderr)
            break

    client.disconnect()
    print("Done.")
