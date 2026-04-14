cmake_minimum_required(VERSION 3.14)

# pygrsim_native — pybind11 Python module
# Dependencies: grsim_core, grsim_ref, grsim_scenarios, pybind11

find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG)

if(NOT pybind11_FOUND)
    # Try fetching pybind11 if not found
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)
endif()

pybind11_add_module(pygrsim_native bindings.cpp)

target_link_libraries(pygrsim_native PRIVATE
    grsim_core
    grsim_ref
    grsim_scenarios
)

target_include_directories(pygrsim_native PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Install into the Python package directory
install(TARGETS pygrsim_native
    LIBRARY DESTINATION ${Python_SITEARCH}/pygrsim
)
