#!/usr/bin/env python3
"""
Upstream Change Watcher for grsim-rl.

Polls watched repositories for changes and generates impact reports.
Can be run manually or as a GitHub Actions workflow.

Usage:
    python watch.py --check           # Check for new changes
    python watch.py --report          # Generate impact report
    python watch.py --state-file state.json  # Use custom state file
"""

import argparse
import json
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


def load_watchlist(path: str = "watchlist.yaml") -> dict:
    """Load the watchlist configuration."""
    if HAS_YAML:
        with open(path) as f:
            return yaml.safe_load(f)
    else:
        # Fallback: hardcoded watchlist
        return {
            "repos": [
                {"name": "ssl-rules", "url": "https://github.com/RoboCup-SSL/ssl-rules",
                 "branch": "master", "priority": "critical",
                 "impact_areas": ["rules", "event_semantics"]},
                {"name": "ssl-game-controller", "url": "https://github.com/RoboCup-SSL/ssl-game-controller",
                 "branch": "master", "priority": "critical",
                 "impact_areas": ["protocols", "event_semantics"]},
                {"name": "ssl-simulation-protocol", "url": "https://github.com/RoboCup-SSL/ssl-simulation-protocol",
                 "branch": "master", "priority": "critical",
                 "impact_areas": ["protocols"]},
                {"name": "grSim", "url": "https://github.com/RoboCup-SSL/grSim",
                 "branch": "master", "priority": "high",
                 "impact_areas": ["compatibility"]},
            ]
        }


def load_state(state_file: str) -> dict:
    """Load previously seen commit SHAs."""
    if os.path.exists(state_file):
        with open(state_file) as f:
            return json.load(f)
    return {}


def save_state(state_file: str, state: dict):
    """Save current commit SHAs."""
    with open(state_file, "w") as f:
        json.dump(state, f, indent=2)


def get_remote_head(url: str, branch: str) -> Optional[str]:
    """Get the HEAD commit SHA of a remote branch."""
    try:
        result = subprocess.run(
            ["git", "ls-remote", url, f"refs/heads/{branch}"],
            capture_output=True, text=True, timeout=30
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip().split()[0]
    except (subprocess.TimeoutExpired, FileNotFoundError):
        pass
    return None


def get_commit_log(url: str, old_sha: str, new_sha: str, ref_dir: Optional[str] = None) -> str:
    """Get commit log between two SHAs if we have a local clone."""
    if ref_dir and os.path.isdir(ref_dir):
        try:
            result = subprocess.run(
                ["git", "-C", ref_dir, "log", "--oneline", f"{old_sha}..{new_sha}"],
                capture_output=True, text=True, timeout=30
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
    return f"Changes from {old_sha[:8]} to {new_sha[:8]}"


def classify_impact(repo_config: dict, changed_files: List[str] = None) -> List[str]:
    """Classify the impact of changes based on repo config."""
    return repo_config.get("impact_areas", ["unknown"])


def generate_report(changes: List[dict]) -> str:
    """Generate a markdown impact report."""
    if not changes:
        return "# Upstream Change Report\n\nNo new changes detected.\n"

    lines = [
        "# Upstream Change Report",
        f"\nGenerated: {datetime.now().isoformat()}\n",
    ]

    for change in changes:
        lines.append(f"## {change['name']}")
        lines.append(f"- **Priority**: {change['priority']}")
        lines.append(f"- **Previous SHA**: `{change['old_sha'][:12]}`")
        lines.append(f"- **Current SHA**: `{change['new_sha'][:12]}`")
        lines.append(f"- **Impact Areas**: {', '.join(change['impact_areas'])}")
        if change.get("log"):
            lines.append(f"\n### Commits\n```\n{change['log']}\n```")
        lines.append("")

    lines.append("## Action Items\n")
    for change in changes:
        for area in change["impact_areas"]:
            if area == "rules":
                lines.append(f"- [ ] Review rule changes in {change['name']} for event detector updates")
            elif area == "protocols":
                lines.append(f"- [ ] Check protocol changes in {change['name']} for proto file sync")
            elif area == "event_semantics":
                lines.append(f"- [ ] Review event semantic changes in {change['name']}")
            elif area == "scenarios":
                lines.append(f"- [ ] Check for new benchmark scenarios in {change['name']}")
            elif area == "compatibility":
                lines.append(f"- [ ] Verify compatibility with {change['name']} changes")

    return "\n".join(lines)


def check_changes(watchlist: dict, state: dict, references_dir: Optional[str] = None) -> List[dict]:
    """Check all watched repos for changes."""
    changes = []

    for repo in watchlist.get("repos", []):
        name = repo["name"]
        url = repo["url"]
        branch = repo.get("branch", "master")
        priority = repo.get("priority", "medium")

        current_sha = get_remote_head(url, branch)
        if not current_sha:
            print(f"  WARNING: Could not fetch HEAD for {name}", file=sys.stderr)
            continue

        previous_sha = state.get(name, {}).get("sha", "")

        if current_sha != previous_sha and previous_sha:
            ref_dir = os.path.join(references_dir, name) if references_dir else None
            log = get_commit_log(url, previous_sha, current_sha, ref_dir)

            changes.append({
                "name": name,
                "url": url,
                "priority": priority,
                "old_sha": previous_sha,
                "new_sha": current_sha,
                "impact_areas": classify_impact(repo),
                "log": log,
            })

        # Update state
        state[name] = {
            "sha": current_sha,
            "checked_at": datetime.now().isoformat(),
        }

    return changes


def main():
    parser = argparse.ArgumentParser(description="Upstream Change Watcher for grsim-rl")
    parser.add_argument("--check", action="store_true", help="Check for changes")
    parser.add_argument("--report", action="store_true", help="Generate report")
    parser.add_argument("--state-file", default="upstream_state.json", help="State file path")
    parser.add_argument("--watchlist", default="watchlist.yaml", help="Watchlist file path")
    parser.add_argument("--references-dir", default=None, help="Path to reference repo clones")
    parser.add_argument("--output", default=None, help="Output report file")
    args = parser.parse_args()

    # Default to script directory
    script_dir = Path(__file__).parent
    os.chdir(script_dir)

    watchlist = load_watchlist(args.watchlist)
    state = load_state(args.state_file)

    if args.check or args.report:
        changes = check_changes(watchlist, state, args.references_dir)
        save_state(args.state_file, state)

        if changes:
            print(f"Detected {len(changes)} upstream change(s):")
            for c in changes:
                print(f"  - {c['name']} ({c['priority']}): {c['old_sha'][:8]} → {c['new_sha'][:8]}")
        else:
            print("No new upstream changes detected.")

        if args.report:
            report = generate_report(changes)
            if args.output:
                with open(args.output, "w") as f:
                    f.write(report)
                print(f"Report written to {args.output}")
            else:
                print(report)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
