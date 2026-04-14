# Contributing

Contributions to grsim-rl are welcome. This page covers the development workflow, conventions, and how to add new scenarios or event detectors.

## Development setup

```bash
# Clone the repository
git clone https://github.com/grsim-rl/grsim-rl.git
cd grsim-rl

# Build the C++ core
mkdir build && cd build
cmake .. -DBUILD_CORE_ONLY=ON
cmake --build . -j$(nproc)
cd ..

# Install the Python package in editable mode with dev dependencies
cd python
uv pip install -e ".[all,dev,docs]"
cd ..
```

## Running tests

```bash
# C++ tests (from build directory)
cd build && ctest --output-on-failure

# Python tests
pytest tests/ -v
```

## Code style

### C++

- C++17 for new code, C++11 compatibility where modifying original grSim sources.
- Namespaces: `grsim_core::`, `grsim_ref::`, `grsim_scenarios::`.
- `snake_case` for files and functions, `CamelCase` for classes.
- No Qt types in `grsim_core` headers.
- Centralize constants in `SimConfig` rather than using magic numbers.

### Python

- Follow PEP 8.
- Type hints on all public function signatures.
- Docstrings in NumPy style.

## Branch workflow

All development happens on the `feature/rl-refactor-core` branch. Do not push directly to `main`.

1. Create a feature branch off `feature/rl-refactor-core`.
2. Make your changes, write tests, and verify they pass.
3. Open a pull request targeting `feature/rl-refactor-core`.
4. Address review feedback.
5. Squash-merge when approved.

## Adding a new scenario

1. **C++ side:** Create a new class inheriting from `grsim_scenarios::Scenario` in `include/grsim_scenarios/scenario.h` (declaration) and `src/grsim_scenarios/` (implementation). Implement all pure virtual methods:
    - `name()`, `description()` -- metadata.
    - `blueRobotCount()`, `yellowRobotCount()` -- team sizes.
    - `maxEpisodeTime()` -- timeout in seconds.
    - `generateInitialState(rng)` -- set up robot and ball positions.
    - `checkTermination(state, events, elapsed)` -- define when the episode ends.
    - `rewardCompiler()` -- configure reward weights.

2. **Register it:** Add `REGISTER_SCENARIO("my_scenario", MyScenario);` at the bottom of your `.cpp` file.

3. **Python side:** Add the scenario to the `SCENARIOS` dict in `python/pygrsim/envs.py` and register it in `python/pygrsim/__init__.py` with `gym.register()`.

4. **Tests:** Add a test case in `tests/test_pygrsim.py` that instantiates, resets, and steps through your scenario.

5. **Docs:** Add a section in `docs/site/api/scenarios.md`.

## Adding a new event detector

1. Create a class inheriting from `grsim_ref::EventDetector`.
2. Implement `detect()`, `reset()`, `name()`, and `ruleRef()`.
3. Add an entry to `GameEventType` in `include/grsim_ref/events.h`.
4. Register the detector in `EventDetectorRegistry::createDefault()`.
5. Document the event in `docs/site/api/events.md`.

## Building the documentation

```bash
cd docs
mkdocs serve     # live preview at http://127.0.0.1:8000
mkdocs build     # generate static site in docs/site_build/
```

## Reporting issues

Use the GitHub issue templates:

- **Bug report** -- for crashes, incorrect behavior, or test failures.
- **Feature request** -- for new scenarios, detectors, or API changes.
- **Upstream change** -- for changes in the upstream grSim repository that affect this fork.

## License

By contributing, you agree that your contributions will be licensed under GPL-3.0, consistent with the rest of the project.
