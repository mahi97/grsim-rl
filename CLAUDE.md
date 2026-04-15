# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

grsim-rl is a dual-use fork of RoboCup-SSL/grSim:
1. Preserves the real-time SSL simulator with Qt GUI
2. Adds an RL/ML training platform with Gymnasium/PettingZoo APIs

The C++ side has two build targets: the original grSim GUI app and a headless `core_standalone` used by the Python RL environments via pybind11. The Python package (`pygrsim`) works in mock mode (zero observations) without the native module, or with real ODE physics when the native module is built and copied into `python/pygrsim/`.

## Build Commands

### Full grSim (Qt GUI + RL libs)
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake ..
cmake --build . --config Release
```

### Headless core only (for RL / native Python module)
```bash
cd core_standalone && mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_MANIFEST_MODE=OFF -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
# Smoke test:
./Release/test_core.exe
# Deploy native module to Python package:
cp Release/pygrsim_native.*.pyd ../python/pygrsim/
cp Release/ode_double.dll ../python/pygrsim/   # Windows only
```

### Python environment
```bash
uv pip install --system gymnasium numpy pytest
uv pip install --system --no-deps -e python/
# Verify:
python -c "import pygrsim; print(pygrsim.list_scenarios())"
```

### Tests
```bash
python -m pytest tests/ -v              # all tests
python -m pytest tests/test_pygrsim.py -v  # env tests only
python -m pytest tests/test_vec_env.py -v  # vec env + coach + registration tests
python -m pytest tests/test_pygrsim.py::test_env_step -v  # single test
```

### Benchmarks
```bash
python benchmarks/bench_env_overhead.py
python benchmarks/bench_reproducibility.py
```

## Architecture

### C++ Layer Stack (bottom-up)

**grsim_core** (`src/grsim_core/`, `include/grsim_core/`) — Headless ODE physics engine. No Qt dependency. Key types:
- `SimulationEngine` — owns ODE world, steps physics, produces `WorldState`/`FlatObservation`. Pimpl pattern hides ODE types.
- `SimConfig` with nested `FieldConfig`, `BallConfig`, `RobotConfig`, `SimulationParams`, `NoiseConfig` — pure data, factory methods `defaults()`, `divisionA()`, `divisionB()`
- `WorldState` / `FlatObservation` — state representation. Flat obs layout: ball(6) + per-robot(7) for all robots.
- `Actions` / `RobotAction` — three action types: WHEEL_VELOCITY, BODY_VELOCITY, GLOBAL_VELOCITY

**grsim_ref** (`src/grsim_ref/`, `include/grsim_ref/`) — Event detection and reward compilation, kept separate by design:
- `EventDetector` (abstract) — one subclass per SSL rule family. `EventDetectorRegistry::createDefault()` wires them all up.
- `GameEvent` — attributed events with positions, confidence, rule references
- `RewardCompiler` — consumes events + state, produces `RewardSignal`. Predefined profiles: `scoringProfile()`, `possessionProfile()`, `placementProfile()`, `defensiveProfile()`, `fullMatchProfile()`

**grsim_scenarios** (`src/grsim_scenarios/`, `include/grsim_scenarios/`) — 10 scenarios + skill system:
- `Scenario` (abstract) — defines initial state, termination, reward config. Registered via `REGISTER_SCENARIO` macro.
- `Skill` hierarchy — `GoToPoseSkill`, `KickToPointSkill`, `DribbleToPointSkill`, etc. Each compiles down to `RobotAction` via `SkillCompiler`.
- `ActionLevel` enum: WHEEL -> BODY -> SKILL -> COACH (each compiles downward)

**pygrsim_native** (`src/pygrsim_native/bindings.cpp`) — pybind11 module exposing engine, events, rewards to Python.

### Python Layer (`python/pygrsim/`)

- `envs.py` — `SSLSingleAgentEnv` (Gymnasium), `SSLMultiAgentEnv` (PettingZoo Parallel). The single-agent env controls blue team; uses native engine if available, else mock zeros.
- `coach_env.py` — `SSLCoachEnv` wraps `SSLSingleAgentEnv` with a 5-dim action space (formation, pressing, attack pattern, transition, aggressiveness) that compiles to per-robot body velocities.
- `vec_env.py` — `SSLVecEnv` (multiprocessing) and `SSLSyncVecEnv` (single-process) for batched training. SB3-compatible.
- `gc_client.py` — Game Controller UDP client for SSL protocol integration.
- `renderer.py` — Matplotlib field visualization.
- `__init__.py` — Registers all 10 scenarios with `gymnasium.register()` under the `grsim/` namespace (e.g., `grsim/EmptyFieldShot-v0`).

### Original grSim Code

The top-level `CMakeLists.txt` builds the original Qt GUI app (`src/main.cpp`, `src/sslworld.cpp`, `src/robot.cpp`, etc.) plus the RL libs when `BUILD_RL_LIBS=ON` (default). The original physics wrappers live in `src/physics/p*.cpp` — the new `grsim_core` does NOT use these; it talks to ODE directly to avoid CGraphics coupling.

### Two Build Entry Points

1. **Root `CMakeLists.txt`** — builds grSim GUI + optionally RL libs + optionally Python bindings
2. **`core_standalone/CMakeLists.txt`** — builds only the three RL libs + smoke test + Python bindings. This is the primary build for RL development.

## Key Design Decisions

- ODE used directly in grsim_core (not through original PWorld wrappers) to avoid CGraphics coupling
- Global `_w` pointer eliminated; collision callbacks use `void* data` parameter
- Config extracted from Qt VarTreeView into pure `SimConfig` struct
- Event detection separated from reward shaping so official rule logic stays clean
- Hierarchical actions: wheel -> body -> skill -> coach (all compile downward)
- Scenarios are self-contained: initial state, termination, reward config
- Python envs degrade gracefully: work in mock mode without native module

## Coding Conventions

- C++17 for new code, C++11 compatibility for modified original code
- `grsim_core::`, `grsim_ref::`, `grsim_scenarios::` namespaces
- snake_case for files and functions, CamelCase for classes
- No Qt types in grsim_core headers
- Centralize config values — no rule-specific magic numbers scattered in code

## Feature Branch

All RL refactor work is on `feature/rl-refactor-core`. Do not merge to main without review.
