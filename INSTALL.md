# GrSim - INSTALL

## Overview

We developed grSim on Ubuntu OS. (Ubuntu 14.04+ tested and is recommended). It is  important that the graphics card driver is installed properly (the official Ubuntu packages for nVidia and AMD(ATI) graphics cards are available). grSim will compile and run in both 32 and 64 bits Linux and Mac OS, and in 64 bit Windows. 

GrSim is written in C++, in order to compile it, you will need a working toolchain and a c++ compiler.

## Dependencies

GrSim depends on:

- [CMake](https://cmake.org/) version 3.5+
- [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
- [OpenGL](https://www.opengl.org)
- [Qt5 Development Libraries](https://www.qt.io)
- [Open Dynamics Engine (ODE)](http://www.ode.org)
- [VarTypes Library](https://github.com/jpfeltracco/vartypes) forked from [Szi's Vartypes](https://github.com/szi/vartypes)
- [Google Protobuf](https://github.com/google/protobuf)
- [Boost development libraries](http://www.boost.org/) (needed by VarTypes)

**Note:** It's necessary to compile ODE in double precision. This is default when installing the ODE binaries in Ubuntu. However, if you are compiling ODE from source (e.g on Mac OS), please make sure to enable the double precision during the configuration step: `./configure --enable-double-precision`.



## Run from pre-build packages
### Installing from Arch Linux package manager

A package of grSim is avaliable on the [Arch User Repository](https://aur.archlinux.org/packages/grsim-git/), you can install it with your preferred AUR manager. Using `yay` it can be done with:
```bash
yay -S grsim-git
```

### Using docker image
You can get latest grSim from [Docker Hub](https://hub.docker.com/r/robocupssl/grsim) with:
```shell
docker pull robocupssl/grsim:latest
```

The container can be run in two flavors:
1. Headless: `docker run robocupssl/grsim`
1. With VNC: `docker run --net=host -eVNC_PASSWORD=vnc -eVNC_GEOMETRY=1920x1080 robocupssl/grsim vnc`
    1. Then launch your VNC client app (e.g. [Remmina](https://remmina.org/)).
    1. Connect to `localhot:5900`.
    1. Enter a password (default:`vnc`) to login.

## Building and installing from the source code

### Installing Dependencies

#### Arch Linux

If you are running Arch Linux or an Arch Linux based distribution, install the dependencies with:
```
$ sudo pacman -S base-devel boost hicolor-icon-theme \
                 mesa ode protobuf qt5-base cmake git
```

#### Ubuntu / Debian

For Debian, or derivative
```
sudo apt install git build-essential cmake pkg-config qtbase5-dev \
                   libqt5opengl5-dev libgl1-mesa-dev libglu1-mesa-dev \
                   libprotobuf-dev protobuf-compiler libode-dev libboost-dev
```

#### Mac OS X

For Mac OS X, you will need to have installed:

- [Xcode](https://developer.apple.com/xcode/) or Xcode Command Line Tools 8.0 or newer;
- [Homebrew](http://brew.sh/) package manager.

Than install the dependencies needed:

```bash
brew install cmake
brew install pkg-config
brew tap robotology/formulae         
brew install robotology/formulae/ode
brew install qt@5
brew install protobuf@21
```

If you run into build issues, you may need to run this first:

```bash
brew update
brew doctor
```

#### Windows (64 bits)

For Windows, you will need to have installed:

- [CMake](https://cmake.org/) (tested with version 3.17.2 ). Download and install cmake for windows.
- [Visual Studio](https://visualstudio.microsoft.com/) (tested with version 16.7.0). During installation make sure to include workload `Desktop development with C++` and `C++ MFC for latest v142 build tools (x86 x64)`
- [vcpkg](https://github.com/microsoft/vcpkg) package manager. Follow installation instructions on their github website.

To install the dependencies, open a terminal in vcpkg installation folder and run the following command (it will take very long to run):

```bash
$ ./vcpkg install qt5:x64-windows ode:x64-windows protobuf:x64-windows
```

### Building

First clone grSim into your preferred location.

```bash
$ cd /path/to/grsim_ws
$ git clone https://github.com/RoboCup-SSL/grSim.git
$ cd grSim
```

Create a build directory within the project (this is ignored by .gitignore):

```bash
$ mkdir build
$ cd build
```

#### Linux and Mac OS X

Run CMake to generate the makefile (note: if you proceed with the installation, grSim will be installed into directory chosen, by default `/usr/local`):

```bash
$ cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
```

Then compile the program:

```bash
$ make
```

The executable will be located on the `bin` directory.

#### Windows

Run CMake to generate a solution in visual studio and the build the solution (note: modify the command below to reflect your vcpkg installation folder).

```bash
$ cmake -DCMAKE_TOOLCHAIN_FILE=${PATH_TO_VCPKG}\\scripts\\buildsystems\\vcpkg.cmake ..
$ cmake --build . --config Release
```

The executable will be located on the `bin` directory.

### Installing (Linux and Mac OS X)

At least, if you want to install grSim on your system, run the follow:

```bash
$ sudo make install
```

grSim will be — by default — installed on the `/usr/local` directory.


## RL / Python Environment (grsim-rl)

grsim-rl adds a headless RL training platform on top of grSim. You can use it **without building the full grSim GUI**.

### Prerequisites

- Python 3.8+
- [uv](https://docs.astral.sh/uv/) (recommended package manager)

### Install Python dependencies with uv

```bash
# Install uv (if not already installed)
# See: https://docs.astral.sh/uv/getting-started/installation/

# Core dependencies
uv pip install --system gymnasium numpy pytest

# Install pygrsim package
uv pip install --system --no-deps -e python/

# Optional: multi-agent, rendering, training
uv pip install --system pettingzoo matplotlib stable-baselines3
```

### Verify installation

```bash
python -c "import pygrsim; print(pygrsim.__version__); print(pygrsim.list_scenarios())"
```

### Build native C++ physics engine (optional but recommended)

Without the native module, environments run in mock mode (zero observations). With it, you get real ODE physics at ~700-3700 steps/sec.

#### Windows

```bash
# 1. Install build tools
#    - CMake: winget install Kitware.CMake
#    - Visual Studio Build Tools 2022 (with C++ workload)
#    - vcpkg: git clone https://github.com/microsoft/vcpkg && ./vcpkg/bootstrap-vcpkg.bat

# 2. Install ODE and pybind11
./vcpkg/vcpkg install ode:x64-windows
uv pip install --system pybind11

# 3. Build
cd core_standalone
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake ^
      -DVCPKG_MANIFEST_MODE=OFF -G "Visual Studio 17 2022" -A x64 ^
      -Dpybind11_DIR=%PYTHON_DIR%/Lib/site-packages/pybind11/share/cmake/pybind11 ..
cmake --build . --config Release

# 4. Run smoke test
Release\test_core.exe

# 5. Copy native module into Python package
copy Release\pygrsim_native.*.pyd ..\python\pygrsim\
copy Release\ode_double.dll ..\python\pygrsim\
```

#### Linux

```bash
# 1. Install dependencies
sudo apt install cmake build-essential libode-dev python3-dev
uv pip install --system pybind11

# 2. Build
cd core_standalone && mkdir build && cd build
cmake -DVCPKG_MANIFEST_MODE=OFF ..
cmake --build . --config Release

# 3. Copy native module
cp pygrsim_native*.so ../../python/pygrsim/
```

### Run tests

```bash
python -m pytest tests/ -v
```

### Run benchmarks

```bash
python benchmarks/bench_env_overhead.py
python benchmarks/bench_reproducibility.py
```

### Run training examples

```bash
python examples/basic_training.py
python examples/multi_agent.py
```

## Troubleshooting

If you face any problem regarding of updating the grsim version, you can try removing the `grsim.xml`.
If grSim crashes almost instantly with some ODE error the issue might by your ODE version.
Try adding -DBUILD_ODE=TRUE to build ODE from source instead of using the system dependency.

## Notes on the performance

When running grSim, check the FPS in the status bar. If it is running at **60 FPS** or higher, everything is ok. Otherwise check the graphics card's driver installation and OpenGL settings.
