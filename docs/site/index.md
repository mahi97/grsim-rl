# grsim-rl

**A reinforcement learning platform for RoboCup Small Size League built on grSim.**

grsim-rl extends the widely-used [grSim](https://github.com/RoboCup-SSL/grSim) simulator with a structured RL training interface. It exposes Gymnasium and PettingZoo environments backed by a headless C++ physics engine (ODE), so agents can train at thousands of steps per second without a GUI.

## Key features

- **10 built-in scenarios** ranging from single-robot shooting to full 6v6 matches, each with tuned reward functions and termination conditions.
- **Gymnasium and PettingZoo APIs** for single-agent and multi-agent training out of the box.
- **Hierarchical action spaces** -- wheel velocities, body-frame velocities, skill-level commands, or coach-level plays.
- **Rule-aware event detection** aligned with the official SSL game controller, so agents learn within the rules.
- **Pure-Python 2D renderer** using matplotlib for debugging and visualization, with no C++ dependency.
- **Deterministic stepping** with configurable physics delta time for reproducible experiments.

## Architecture

```
+---------------------+
|   pygrsim (Python)  |   Gymnasium / PettingZoo wrappers
+---------+-----------+
          |  pybind11
+---------+-----------+
|  grsim_scenarios    |   10 scenarios, skill compiler, action hierarchy
+---------+-----------+
|    grsim_ref        |   Event detectors, reward compiler
+---------+-----------+
|   grsim_core        |   Headless ODE physics engine, SimConfig, WorldState
+---------------------+
```

## Quick links

- [Quick Start](quickstart.md) -- install and run your first environment in under five minutes.
- [Environment API](api/environments.md) -- observation spaces, action spaces, and the `render()` method.
- [Scenarios](api/scenarios.md) -- descriptions and parameters for all 10 scenarios.
- [Events](api/events.md) -- game event types and detector reference.
- [Contributing](contributing.md) -- how to add scenarios, detectors, or fix bugs.

## License

grsim-rl is released under the GPL-3.0 license, consistent with the upstream grSim project.
