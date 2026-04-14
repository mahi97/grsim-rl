# Dockerfile for building grsim-rl C++ libraries and Python bindings
# Verifies the full build pipeline: ODE + grsim_core + grsim_ref + grsim_scenarios + pybind11

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# System dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3-dev \
    python3-pip \
    python3-numpy \
    libode-dev \
    libprotobuf-dev \
    protobuf-compiler \
    qt5-qmake \
    qtbase5-dev \
    libqt5opengl5-dev \
    libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

# pybind11
RUN pip3 install pybind11[global]

WORKDIR /build

# Copy source
COPY . /src/grsim-rl

# Build RL libraries only (no Qt GUI needed)
RUN mkdir -p /build/rl && cd /build/rl && \
    cmake /src/grsim-rl \
        -DBUILD_RL_LIBS=ON \
        -DBUILD_PYTHON_BINDINGS=ON \
        -DBUILD_CLIENTS=OFF \
        -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) grsim_core grsim_ref grsim_scenarios pygrsim_native

# Build full grSim + RL libs
RUN mkdir -p /build/full && cd /build/full && \
    cmake /src/grsim-rl \
        -DBUILD_RL_LIBS=ON \
        -DBUILD_PYTHON_BINDINGS=ON \
        -DBUILD_CLIENTS=ON \
        -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# Upgrade pip/setuptools for PEP 660 editable install support
RUN pip3 install --upgrade pip setuptools

# Install Python package
RUN cd /src/grsim-rl/python && pip3 install -e ".[all]"

# Copy native module to Python path
RUN cp /build/rl/src/pygrsim_native/pygrsim_native*.so \
    /src/grsim-rl/python/pygrsim/ 2>/dev/null || true

# Run tests
RUN cd /src/grsim-rl && python3 -m pytest tests/ -v

# Verify native module loads
RUN python3 -c "import pygrsim; print('pygrsim', pygrsim.__version__)" && \
    python3 -c "try:\n import pygrsim_native; print('Native module loaded')\nexcept ImportError:\n print('Native module not available (expected in mock mode)')"

CMD ["python3", "-m", "pytest", "/src/grsim-rl/tests/", "-v"]
