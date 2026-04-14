# CLAUDE.md — grsim-rl project guide

## Project
grsim-rl is a dual-use fork of RoboCup-SSL/grSim:
1. Preserves real-time simulator for the SSL community
2. Adds RL/ML training platform with Gymnasium/PettingZoo APIs

## Repository Layout
```
grsim-rl/
├── src/                    # Original grSim C++ sources
│   ├── grsim_core/         # NEW: headless simulation engine (no Qt)
│   ├── grsim_ref/          # NEW: event detection and reward compilation
│   └── grsim_scenarios/    # NEW: scenario system and skill compiler
├── include/
│   ├── grsim_core/         # engine.h, config.h, world_state.h
│   ├── grsim_ref/          # events.h, reward_compiler.h
│   └── grsim_scenarios/    # scenario.h, skills.h
├── python/pygrsim/         # Python RL environments
├── tools/upstream_watch/   # Upstream change monitoring
├── docs/
│   ├── audit/              # Repo audit and refactor plan
│   ├── research/           # SSL stack research dossier
│   └── design/             # Paper outline, architecture docs
├── examples/               # Training and evaluation scripts
├── benchmarks/             # Performance benchmarks
└── tests/                  # Test suite
```

## Build
```bash
mkdir build && cd build
cmake .. -DBUILD_CORE_ONLY=ON  # For headless core only
cmake ..                       # For full grSim + core
make -j$(nproc)
```

## Python Environment
```bash
cd python && pip install -e ".[all]"
python -c "import pygrsim; env = pygrsim.make('empty_field_shot')"
```

## Architecture Layers
1. **grsim_core** — Pure ODE physics, no Qt. SimulationEngine API.
2. **grsim_ref** — Event detection (modular detectors) + reward compiler.
3. **grsim_scenarios** — 10 scenarios + skill-level actions.
4. **pygrsim** — Gymnasium single-agent + PettingZoo multi-agent.

## Key Design Decisions
- ODE used directly in grsim_core (not through original PWorld wrappers) to avoid CGraphics coupling
- Global `_w` pointer eliminated; collision callbacks use `void* data` parameter
- Config extracted from Qt VarTreeView into pure SimConfig struct
- Event detection separated from reward shaping for modularity
- Hierarchical actions: wheel → body → skill → coach (all compile downward)
- Scenarios are self-contained: initial state, termination, reward

## Coding Conventions
- C++17 for new code, C++11 compatibility for modified original code
- `grsim_core::` namespace for core, `grsim_ref::` for referee, `grsim_scenarios::` for scenarios
- snake_case for files and functions, CamelCase for classes
- No Qt types in grsim_core headers
- No rule-specific magic numbers scattered in code — centralize in config

## Feature Branch
All work is on `feature/rl-refactor-core`. Do not merge to main without review.
