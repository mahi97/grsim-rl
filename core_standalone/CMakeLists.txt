cmake_minimum_required(VERSION 3.14)
project(grsim_core_standalone CXX)

set(CMAKE_CXX_STANDARD 17)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Find ODE
find_package(ODE CONFIG REQUIRED)

# --- grsim_core ---
add_library(grsim_core STATIC
    ${ROOT}/src/grsim_core/config.cpp
    ${ROOT}/src/grsim_core/world_state.cpp
    ${ROOT}/src/grsim_core/engine.cpp
    ${ROOT}/src/grsim_core/noise.cpp
)
target_include_directories(grsim_core PUBLIC ${ROOT}/include)
target_link_libraries(grsim_core PUBLIC ODE::ODE)

# --- grsim_ref ---
add_library(grsim_ref STATIC
    ${ROOT}/src/grsim_ref/detectors.cpp
    ${ROOT}/src/grsim_ref/reward_compiler.cpp
)
target_include_directories(grsim_ref PUBLIC ${ROOT}/include)
target_link_libraries(grsim_ref PUBLIC grsim_core)

# --- grsim_scenarios ---
add_library(grsim_scenarios STATIC
    ${ROOT}/src/grsim_scenarios/scenarios.cpp
    ${ROOT}/src/grsim_scenarios/skills.cpp
    ${ROOT}/src/grsim_scenarios/coach.cpp
)
target_include_directories(grsim_scenarios PUBLIC ${ROOT}/include)
target_link_libraries(grsim_scenarios PUBLIC grsim_core grsim_ref)

# --- Smoke test ---
add_executable(test_core ${ROOT}/tests/test_core_smoke.cpp)
target_link_libraries(test_core grsim_core grsim_ref grsim_scenarios)
