[![Build Status](https://github.com/RoboCup-SSL/grSim/workflows/Build/badge.svg)](https://github.com/RoboCup-SSL/grSim/actions?query=workflow%3ABuild+branch%3Amaster) [![CodeFactor](https://www.codefactor.io/repository/github/robocup-ssl/grsim/badge/master)](https://www.codefactor.io/repository/github/robocup-ssl/grsim/overview/master)

grsim-rl
=======================

A dual-use platform for the [RoboCup Small Size League](https://ssl.robocup.org/):
**real-time simulator** + **RL/ML training environment**.

Built on [grSim](https://github.com/RoboCup-SSL/grSim), the official SSL simulator.

![grSim on Ubuntu](docs/img/screenshot01.jpg?raw=true "grSim on Ubuntu")

## Key Features

- **Gymnasium & PettingZoo** environments for single- and multi-agent RL
- **Headless simulation engine** (no Qt/GUI required for training)
- **10 built-in scenarios**: empty-field shot, 1v1 dribble, 2v1 attack, goalkeeper save, ball placement, kickoff, free kick, 3v3 possession, 3v2 counterattack, 6v6 mini-game
- **Hierarchical action spaces**: wheel velocity, body velocity, skill commands, coach commands
- **Canonical event detection** aligned with official SSL rules
- **Reward compiler** with predefined profiles (scoring, possession, defense, placement)
- **Full backward compatibility** with existing grSim workflows and protocols
- **Upstream change watcher** for tracking SSL rule and protocol updates

## Quick Start (Python with uv)

```bash
# Install uv if you don't have it
# https://docs.astral.sh/uv/getting-started/installation/

# Install Python dependencies
uv pip install --system gymnasium numpy pytest

# Install pygrsim (editable, from project root)
uv pip install --system --no-deps -e python/

# Or manually: add python/ to your Python path
# python -c "import site; open(site.getsitepackages()[1]+'/pygrsim.pth','w').write('C:/path/to/grsim-rl/python')"
```

```python
import pygrsim

# List available scenarios
print(pygrsim.list_scenarios())

# Create an environment
env = pygrsim.make("empty_field_shot")
obs, info = env.reset(seed=42)

for _ in range(1000):
    action = env.action_space.sample()
    obs, reward, done, truncated, info = env.step(action)
    if done or truncated:
        obs, info = env.reset()

# Also works with gymnasium.make
import gymnasium as gym
env = gym.make("grsim/EmptyFieldShot-v0")
```

## Building C++ (for native physics)

```bash
# Prerequisites: CMake, MSVC/GCC, vcpkg with ODE, pybind11
uv pip install --system pybind11

# Configure and build
cd core_standalone
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_MANIFEST_MODE=OFF -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# Run smoke test
./Release/test_core.exe

# Copy native module to Python package
cp Release/pygrsim_native.*.pyd ../python/pygrsim/
cp Release/ode_double.dll ../python/pygrsim/   # Windows only
```

## Running Tests

```bash
python -m pytest tests/ -v
```

## Running Benchmarks

```bash
python benchmarks/bench_env_overhead.py
python benchmarks/bench_reproducibility.py
```

## Documentation

- [Install instructions](INSTALL.md)
- [Repository audit](docs/audit/repo_audit.md)
- [Architecture and refactor plan](docs/audit/refactor_plan.md)
- [SSL stack research](docs/research/official_stack.md)
- [Paper outline](docs/design/paper_outline.md)
- [Authors](AUTHORS.md)
- [Changelog](CHANGELOG.md)
- License: [GNU General Public License (GPLv3)](LICENSE.md)
- [Citation](CITATION.cff)

System Requirements
-----------------------

grSim will likely run on a modern dual-core PC with a decent graphics card. A typical configuration is:

- Dual Core CPU (2.0 Ghz+)
- 1GB of RAM
- 256MB nVidia or ATI graphics card

Note that it may run on lower-end equipment though good performance is not guaranteed.


Software Requirements
---------------------

grSim compiles on Linux (tested on Ubuntu and Arch Linux variants only) and Mac OS. It depends on the following libraries:

- [CMake](https://cmake.org/) version 3.5+
- [pkg-config](https://freedesktop.org/wiki/Software/pkg-config/)
- [OpenGL](https://www.opengl.org)
- [Qt5 Development Libraries](https://www.qt.io)
- [Open Dynamics Engine (ODE)](http://www.ode.org)
- [VarTypes Library](https://github.com/jpfeltracco/vartypes) forked from [Szi's Vartypes](https://github.com/szi/vartypes)
- [Google Protobuf](https://github.com/google/protobuf)
- [Boost development libraries](http://www.boost.org/) (needed by VarTypes)

Please consult the [install instructions](INSTALL.md) for more details.

Usage
-----

Receiving data from the grSim is similar to receiving data from the [SSL-Vision](https://github.com/RoboCup-SSL/ssl-vision) using [Google Protobuf](https://github.com/google/protobuf) library.
Sending data to the simulator is also possible using Google Protobuf. Sample clients are included in [clients](./clients) folder. There are two clients available, *qt-based* and *Java-based*. The native client is compiled during the grSim compilation. To compile the Java client, please consult the corresponding `README` file.

Qt [example project](https://github.com/robocin/ssl-client) to receive and send data to the simulator.

Star History
------
[![Star History Chart](https://api.star-history.com/svg?repos=robocup-ssl/grsim&type=Date)](https://star-history.com/#robocup-ssl/grsim&Date)

Citing
------

If you use this in your research, please cite the original paper:
```
@inproceedings{Monajjemi2011grSimR,
  title={grSim - RoboCup Small Size Robot Soccer Simulator},
  author={Valiallah Monajjemi and A. Koochakzadeh and S. S. Ghidary},
  booktitle={RoboCup},
  year={2011}
}
```

If you wish to cite this repo with it's modifications specifically, please cite:

```
@misc{grsim2021,
  author = {Mohammad Mahdi Rahimi and Jan Segre and Valiallah Monajjemi and A. Koochakzadeh and Sepehr MohaimenianPour and Nicolai Ommer and  Avatar
Kazunori Kimura and Jeremy Feltracco and Kenta Sato and Atousa Ahsani},
  title = {GRSIM},
  year = {2021},
  publisher = {GitHub},
  note = {GitHub repository},
  howpublished = {\url{https://github.com/RoboCup-SSL/grSim/}}
}
```
