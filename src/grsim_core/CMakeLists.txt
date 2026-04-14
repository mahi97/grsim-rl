cmake_minimum_required(VERSION 3.14)

# grsim_core — headless simulation library
# Dependencies: ODE only (no Qt, no OpenGL, no Protobuf)

add_library(grsim_core STATIC
    config.cpp
    world_state.cpp
    engine.cpp
)

target_include_directories(grsim_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# ODE dependency
if(WIN32)
    find_package(ODE CONFIG)
    if(ODE_FOUND)
        target_link_libraries(grsim_core PUBLIC ODE::ODE)
    endif()
else()
    find_package(ODE)
    if(ODE_FOUND)
        target_link_libraries(grsim_core PUBLIC ode::ode)
    endif()
endif()

# C++17 for structured bindings, optional, etc.
target_compile_features(grsim_core PUBLIC cxx_std_17)

# Position-independent code for shared lib / Python bindings
set_target_properties(grsim_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
