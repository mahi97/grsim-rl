#!/usr/bin/env python3
"""
ssl-autoref-tests replay harness for grsim-rl event detectors.

Reads the ssl-autoref-tests JSON test definitions, catalogues all test
scenarios and their expected events, maps them against our detector
coverage, and generates a Markdown coverage report.

Usage:
    python replay.py [--tests-dir PATH] [--report FILE]

Defaults:
    --tests-dir  ../../.references/ssl-autoref-tests
    --report     coverage_report.md
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import dataclass, field
from enum import Enum, auto
from pathlib import Path
from typing import Any


# ── Our detector capabilities ────────────────────────────────────────────

# Map ssl-autoref-tests event type strings to our GameEventType enum names.
# Must stay in sync with grsim_ref/events.h GameEventType.
AUTOREF_TO_OUR_EVENT: dict[str, str] = {
    "AIMLESS_KICK": "AIMLESS_KICK",
    "ATTACKER_DOUBLE_TOUCHED_BALL": "DOUBLE_TOUCH",
    "ATTACKER_TOO_CLOSE_TO_DEFENSE_AREA": "ATTACKER_TOO_CLOSE_TO_DEFENSE_AREA",
    "ATTACKER_TOUCHED_BALL_IN_DEFENSE_AREA": "ATTACKER_TOUCHED_BALL_IN_DEFENSE_AREA",
    "BALL_LEFT_FIELD_GOAL_LINE": "BALL_LEFT_FIELD_GOAL_LINE",
    "BALL_LEFT_FIELD_TOUCH_LINE": "BALL_LEFT_FIELD_TOUCH_LINE",
    "BOT_CRASH_DRAWN": "BOT_CRASH_DRAWN",
    "BOT_CRASH_UNIQUE": "BOT_CRASH_UNIQUE",
    "BOT_DRIBBLED_BALL_TOO_FAR": "BOT_DRIBBLED_BALL_TOO_FAR",
    "BOT_INTERFERED_PLACEMENT": "BALL_PLACEMENT_INTERFERENCE",
    "BOT_KICKED_BALL_TOO_FAST": "BOT_KICKED_BALL_TOO_FAST",
    "BOT_TOO_FAST_IN_STOP": "BOT_TOO_FAST_IN_STOP",
    "DEFENDER_IN_DEFENSE_AREA": "BOT_IN_DEFENSE_AREA",
    "DEFENDER_TOO_CLOSE_TO_KICK_POINT": "DEFENDER_TOO_CLOSE_TO_KICK_POINT",
    "PLACEMENT_SUCCEEDED": "BALL_PLACEMENT_SUCCEEDED",
    "POSSIBLE_GOAL": "POSSIBLE_GOAL",
}

# Which event types we have detectors for (from detectors.cpp).
IMPLEMENTED_DETECTORS: set[str] = {
    # BallLeftFieldDetector
    "BALL_LEFT_FIELD_TOUCH_LINE",
    "BALL_LEFT_FIELD_GOAL_LINE",
    "POSSIBLE_GOAL",
    # GoalDetector
    "GOAL",
    # BallSpeedDetector
    "BOT_KICKED_BALL_TOO_FAST",
    # DefenseAreaDetector
    "ATTACKER_IN_DEFENSE_AREA",
    # DribblingDetector
    "BOT_DRIBBLED_BALL_TOO_FAR",
    # DoubleTouchDetector
    "DOUBLE_TOUCH",
    # CrashDetector
    "BOT_CRASH_UNIQUE",
    "BOT_CRASH_DRAWN",
    # BallPlacementDetector
    "BALL_PLACEMENT_SUCCEEDED",
    "BALL_PLACEMENT_FAILED",
    # RobotSpeedDetector
    "BOT_TOO_FAST_IN_STOP",
    # NoProgressDetector
    "NO_PROGRESS_IN_GAME",
    # TooManyRobotsDetector
    "TOO_MANY_ROBOTS",
    # AimlessKickDetector
    "AIMLESS_KICK",
    # KeeperHeldBallDetector
    "KEEPER_HELD_BALL",
    # PushingDetector
    "BOT_PUSHED_BOT",
}


# ── Data model ───────────────────────────────────────────────────────────

class DetectionStatus(Enum):
    COVERED = auto()          # We have a detector for this event type
    NOT_IMPLEMENTED = auto()  # No detector exists yet
    NO_EVENT_EXPECTED = auto()  # Negative test — no event should fire


@dataclass
class ExpectedEvent:
    """Parsed expected event from one ssl-autoref-tests JSON file."""
    event_type: str                 # e.g. "AIMLESS_KICK"
    by_team: str | None = None      # "BLUE" / "YELLOW" / None
    by_bot: int | None = None
    location: dict[str, float] | None = None
    raw: dict[str, Any] = field(default_factory=dict)


@dataclass
class TestScenario:
    """One test case (a .json + .log pair) from the test suite."""
    category: str           # Directory name, e.g. "AIMLESS_KICK"
    name: str               # File stem, e.g. "aimless"
    json_path: Path
    log_path: Path | None
    expected: ExpectedEvent | None   # None ⇒ negative test
    stop_after_event: bool = False
    our_event_type: str | None = None
    status: DetectionStatus = DetectionStatus.NOT_IMPLEMENTED


def parse_expected_event(data: dict[str, Any]) -> ExpectedEvent | None:
    """Extract the expected event from a deserialized JSON test file."""
    raw_event = data.get("expectedEvent") or data.get("expected_event")
    if raw_event is None:
        return None

    event_type = raw_event.get("type", "UNKNOWN")

    # The event detail key is the camelCase version of the type
    # e.g. "aimlessKick", "possibleGoal", etc.
    detail = {}
    for key, val in raw_event.items():
        if key == "type":
            continue
        if isinstance(val, dict):
            detail = val
            break

    by_team = detail.get("byTeam") or detail.get("by_team")
    by_bot = detail.get("byBot") or detail.get("by_bot")
    if by_bot is None:
        by_bot = detail.get("violator")
    location = detail.get("location")

    return ExpectedEvent(
        event_type=event_type,
        by_team=by_team,
        by_bot=by_bot,
        location=location,
        raw=raw_event,
    )


# ── Discovery ────────────────────────────────────────────────────────────

def discover_tests(tests_dir: Path) -> list[TestScenario]:
    """Walk the ssl-autoref-tests directory and build a list of scenarios."""
    scenarios: list[TestScenario] = []

    if not tests_dir.is_dir():
        print(f"ERROR: tests directory not found: {tests_dir}", file=sys.stderr)
        sys.exit(1)

    for category_dir in sorted(tests_dir.iterdir()):
        if not category_dir.is_dir():
            continue
        category = category_dir.name

        json_files = sorted(category_dir.glob("*.json"))
        for jf in json_files:
            stem = jf.stem
            log_path = jf.with_suffix(".log")
            if not log_path.exists():
                log_path = None

            with open(jf, "r", encoding="utf-8") as f:
                try:
                    data = json.load(f)
                except json.JSONDecodeError as exc:
                    print(f"WARNING: cannot parse {jf}: {exc}", file=sys.stderr)
                    continue

            expected = parse_expected_event(data)
            stop_after = data.get("stopAfterEvent", False) or data.get("stop_after_event", False)

            # Determine our event type and coverage status
            our_type = None
            if expected is not None:
                our_type = AUTOREF_TO_OUR_EVENT.get(expected.event_type)
                if our_type and our_type in IMPLEMENTED_DETECTORS:
                    status = DetectionStatus.COVERED
                else:
                    status = DetectionStatus.NOT_IMPLEMENTED
            else:
                # Negative test — no event expected
                status = DetectionStatus.NO_EVENT_EXPECTED
                # Negative tests still belong to a category that may be covered
                mapped = AUTOREF_TO_OUR_EVENT.get(category)
                if mapped and mapped in IMPLEMENTED_DETECTORS:
                    status = DetectionStatus.COVERED

            scenarios.append(TestScenario(
                category=category,
                name=stem,
                json_path=jf,
                log_path=log_path,
                expected=expected,
                stop_after_event=stop_after,
                our_event_type=our_type,
                status=status,
            ))

    return scenarios


# ── Replay framework ─────────────────────────────────────────────────────

@dataclass
class ReplayResult:
    """Result of replaying one test scenario through our detectors."""
    scenario: TestScenario
    detected: bool = False
    detected_event_type: str | None = None
    match_team: bool = False
    match_bot: bool = False
    match_location: bool = False  # within 0.5m tolerance
    error: str | None = None


def replay_scenario(scenario: TestScenario) -> ReplayResult:
    """
    Replay a single test scenario through our event detectors.

    Currently this performs a *static* analysis: it checks whether our
    detector registry covers the expected event type.  Full log replay
    requires the C++ engine (or its Python bindings) to step through the
    log frames.  This framework is structured so that adding actual frame
    replay only requires filling in ``_step_log_frames()``.

    Returns a ReplayResult summarising what we can / cannot detect.
    """
    result = ReplayResult(scenario=scenario)

    if scenario.expected is None:
        # Negative test — we would need to verify we do NOT fire an event.
        # Without log replay we mark it as structurally covered if the
        # category's detector exists.
        result.detected = scenario.status == DetectionStatus.COVERED
        return result

    if scenario.status == DetectionStatus.COVERED:
        result.detected = True
        result.detected_event_type = scenario.our_event_type
        # Static analysis cannot verify team/bot/location, but the
        # framework records fields for future log-based replay.
        result.match_team = True   # optimistic — real check needs log
        result.match_bot = True
        result.match_location = True
    elif scenario.status == DetectionStatus.NOT_IMPLEMENTED:
        result.detected = False
        result.error = (
            f"No detector for event type "
            f"{scenario.expected.event_type!r}"
        )
    return result


def _step_log_frames(log_path: Path) -> list[dict]:
    """
    Placeholder: parse an SSL log file and return a list of WorldState
    dicts that can be fed to the C++ EventDetectorRegistry (via pybind
    or the grsim-rl Python bindings).

    This is the integration point for full replay.  The log format is the
    standard ssl-vision log (protobuf-based).  Once grsim-rl exposes
    ``EventDetectorRegistry`` to Python we can:

        1. Parse each log frame into a WorldState.
        2. Call ``registry.detect_all(current, previous, dt)``.
        3. Collect emitted events and compare against expected.
    """
    # TODO: implement once Python bindings for EventDetectorRegistry land
    return []


# ── Report generation ────────────────────────────────────────────────────

def generate_report(
    scenarios: list[TestScenario],
    results: list[ReplayResult],
) -> str:
    """Generate a Markdown coverage report."""

    lines: list[str] = []
    lines.append("# ssl-autoref-tests Coverage Report")
    lines.append("")
    lines.append("Generated by `tools/autoref_replay/replay.py`.")
    lines.append("")

    # ── Summary stats
    total = len(scenarios)
    positive = [s for s in scenarios if s.expected is not None]
    negative = [s for s in scenarios if s.expected is None]
    covered = [s for s in scenarios if s.status == DetectionStatus.COVERED]
    not_impl = [s for s in scenarios if s.status == DetectionStatus.NOT_IMPLEMENTED]

    lines.append("## Summary")
    lines.append("")
    lines.append(f"| Metric | Count |")
    lines.append(f"|--------|-------|")
    lines.append(f"| Total test scenarios | {total} |")
    lines.append(f"| Positive tests (event expected) | {len(positive)} |")
    lines.append(f"| Negative tests (no event expected) | {len(negative)} |")
    lines.append(f"| Covered by our detectors | {len(covered)} |")
    lines.append(f"| Not yet implemented | {len(not_impl)} |")
    if total > 0:
        pct = len(covered) / total * 100
        lines.append(f"| Coverage | {pct:.1f}% |")
    lines.append("")

    # ── Per-category breakdown
    categories: dict[str, list[TestScenario]] = {}
    for s in scenarios:
        categories.setdefault(s.category, []).append(s)

    lines.append("## Per-Category Breakdown")
    lines.append("")
    lines.append("| Category | Tests | Covered | Our Event Type | Status |")
    lines.append("|----------|-------|---------|---------------|--------|")

    for cat in sorted(categories):
        cat_scenarios = categories[cat]
        cat_covered = sum(1 for s in cat_scenarios if s.status == DetectionStatus.COVERED)
        cat_total = len(cat_scenarios)

        # Representative event type
        our_types = {s.our_event_type for s in cat_scenarios if s.our_event_type}
        our_type_str = ", ".join(sorted(our_types)) if our_types else "-"

        if cat_covered == cat_total:
            status = "FULL"
        elif cat_covered > 0:
            status = "PARTIAL"
        else:
            status = "MISSING"

        icon = {"FULL": "PASS", "PARTIAL": "PARTIAL", "MISSING": "FAIL"}[status]
        lines.append(
            f"| {cat} | {cat_total} | {cat_covered}/{cat_total} "
            f"| {our_type_str} | {icon} |"
        )

    lines.append("")

    # ── Detailed per-scenario results
    lines.append("## Detailed Scenario Results")
    lines.append("")

    for cat in sorted(categories):
        lines.append(f"### {cat}")
        lines.append("")
        lines.append("| Test | Expected Event | Our Type | Detected | Notes |")
        lines.append("|------|---------------|----------|----------|-------|")

        for s in categories[cat]:
            exp_type = s.expected.event_type if s.expected else "(negative test)"
            our_type = s.our_event_type or "-"
            result = next((r for r in results if r.scenario is s), None)
            detected = "Yes" if (result and result.detected) else "No"
            notes = ""
            if result and result.error:
                notes = result.error
            elif s.expected is None:
                notes = "Negative test; should NOT fire event"
            lines.append(
                f"| {s.name} | {exp_type} | {our_type} | {detected} | {notes} |"
            )
        lines.append("")

    # ── Detector coverage matrix
    lines.append("## Detector Coverage Matrix")
    lines.append("")
    lines.append("Events our detectors can emit vs ssl-autoref-tests event types:")
    lines.append("")
    lines.append("| ssl-autoref-tests Type | Our Event Constant | Detector Exists |")
    lines.append("|----------------------|-------------------|-----------------|")

    for autoref_type in sorted(AUTOREF_TO_OUR_EVENT):
        our_type = AUTOREF_TO_OUR_EVENT[autoref_type]
        exists = "Yes" if our_type in IMPLEMENTED_DETECTORS else "No"
        lines.append(f"| {autoref_type} | {our_type} | {exists} |")

    lines.append("")

    # ── Missing coverage
    missing_autoref = [
        at for at, ot in AUTOREF_TO_OUR_EVENT.items()
        if ot not in IMPLEMENTED_DETECTORS
    ]
    if missing_autoref:
        lines.append("## Missing Detectors")
        lines.append("")
        lines.append(
            "The following ssl-autoref-tests event types have no "
            "corresponding detector in `detectors.cpp`:"
        )
        lines.append("")
        for at in sorted(missing_autoref):
            lines.append(f"- `{at}` -> needs `{AUTOREF_TO_OUR_EVENT[at]}`")
        lines.append("")

    # ── Next steps
    lines.append("## Integration Notes")
    lines.append("")
    lines.append(
        "This report is based on *static* analysis of detector coverage. "
        "Full frame-by-frame log replay requires:"
    )
    lines.append("")
    lines.append("1. Python bindings for `EventDetectorRegistry` (pybind11)")
    lines.append("2. An SSL log parser (protobuf `SSL_WrapperPacket` frames)")
    lines.append(
        "3. A WorldState adapter that converts vision frames to "
        "`grsim_core::WorldState`"
    )
    lines.append("")
    lines.append(
        "Once those are in place, `_step_log_frames()` in this script "
        "can be filled in to provide true detection accuracy metrics."
    )
    lines.append("")

    return "\n".join(lines)


# ── Main ─────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Replay ssl-autoref-tests through grsim-rl detectors"
    )
    script_dir = Path(__file__).resolve().parent
    default_tests = script_dir.parent.parent.parent / ".references" / "ssl-autoref-tests"

    parser.add_argument(
        "--tests-dir",
        type=Path,
        default=default_tests,
        help="Path to ssl-autoref-tests directory",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=script_dir / "coverage_report.md",
        help="Output path for the Markdown coverage report",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="Just list scenarios and exit (no report)",
    )

    args = parser.parse_args()

    # Discover
    scenarios = discover_tests(args.tests_dir)
    if not scenarios:
        print("No test scenarios found.", file=sys.stderr)
        sys.exit(1)

    print(f"Discovered {len(scenarios)} test scenarios in {args.tests_dir}")
    print()

    if args.list:
        for s in scenarios:
            exp = s.expected.event_type if s.expected else "(negative)"
            tag = s.status.name
            print(f"  [{tag:18s}] {s.category}/{s.name}: {exp}")
        sys.exit(0)

    # Replay (static analysis for now)
    results: list[ReplayResult] = []
    for s in scenarios:
        r = replay_scenario(s)
        results.append(r)

    # Console summary
    covered_count = sum(1 for r in results if r.detected)
    print(f"Coverage: {covered_count}/{len(results)} scenarios "
          f"({covered_count / len(results) * 100:.1f}%)")
    print()

    for r in results:
        icon = "PASS" if r.detected else "FAIL"
        exp = (r.scenario.expected.event_type
               if r.scenario.expected else "(negative)")
        print(f"  [{icon}] {r.scenario.category}/{r.scenario.name}: {exp}")
        if r.error:
            print(f"         -> {r.error}")

    # Write report
    report_md = generate_report(scenarios, results)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(report_md, encoding="utf-8")
    print()
    print(f"Report written to {args.report}")


if __name__ == "__main__":
    main()
