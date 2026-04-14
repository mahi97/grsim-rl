# Standalone build for grsim_core only (no Qt, no Protobuf, no VarTypes)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg> -P CMakeLists_core_only.cmake
# Or:    cmake -S . -B build_core -DCMAKE_TOOLCHAIN_FILE=<vcpkg> -C CMakeLists_core_only.cmake

cmake_minimum_required(VERSION 3.14)
project(grsim_core_standalone CXX)

set(CMAKE_CXX_STANDARD 17)

# Find ODE
find_package(ODE CONFIG REQUIRED)

# --- grsim_core ---
add_library(grsim_core STATIC
    src/grsim_core/config.cpp
    src/grsim_core/world_state.cpp
    src/grsim_core/engine.cpp
    src/grsim_core/noise.cpp
)
target_include_directories(grsim_core PUBLIC include)
target_link_libraries(grsim_core PUBLIC ODE::ODE)
set_target_properties(grsim_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- grsim_ref ---
add_library(grsim_ref STATIC
    src/grsim_ref/detectors.cpp
    src/grsim_ref/reward_compiler.cpp
)
target_include_directories(grsim_ref PUBLIC include)
target_link_libraries(grsim_ref PUBLIC grsim_core)

# --- grsim_scenarios ---
add_library(grsim_scenarios STATIC
    src/grsim_scenarios/scenarios.cpp
    src/grsim_scenarios/skills.cpp
    src/grsim_scenarios/coach.cpp
)
target_include_directories(grsim_scenarios PUBLIC include)
target_link_libraries(grsim_scenarios PUBLIC grsim_core grsim_ref)

# --- Simple test executable ---
add_executable(test_core tests/test_core_smoke.cpp)
target_link_libraries(test_core grsim_core grsim_ref grsim_scenarios)

message(STATUS "grsim_core standalone build configured successfully")
