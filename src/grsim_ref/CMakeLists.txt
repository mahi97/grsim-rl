cmake_minimum_required(VERSION 3.14)

# grsim_ref — event detection and reward compilation library
# Dependencies: grsim_core only

add_library(grsim_ref STATIC
    detectors.cpp
    reward_compiler.cpp
)

target_include_directories(grsim_ref PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(grsim_ref PUBLIC grsim_core)
target_compile_features(grsim_ref PUBLIC cxx_std_17)
set_target_properties(grsim_ref PROPERTIES POSITION_INDEPENDENT_CODE ON)
