cmake_minimum_required(VERSION 3.14)

# grsim_scenarios — scenario system and hierarchical action skills
# Dependencies: grsim_core, grsim_ref

add_library(grsim_scenarios STATIC
    scenarios.cpp
    skills.cpp
    coach.cpp
)

target_include_directories(grsim_scenarios PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(grsim_scenarios PUBLIC grsim_core grsim_ref)
target_compile_features(grsim_scenarios PUBLIC cxx_std_17)
set_target_properties(grsim_scenarios PROPERTIES POSITION_INDEPENDENT_CODE ON)
