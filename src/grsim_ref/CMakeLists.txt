cmake_minimum_required(VERSION 3.14)

# grsim_ref — event detection and reward compilation library
# Dependencies: grsim_core only

add_library(grsim_ref STATIC
    detectors.cpp
    reward_compiler.cpp
    gc_adapter.cpp
)

target_include_directories(grsim_ref PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# gc_adapter.cpp uses QTcpSocket for game-controller CI communication
find_package(Qt5 COMPONENTS Network QUIET)
if(Qt5Network_FOUND)
    target_link_libraries(grsim_ref PUBLIC grsim_core Qt5::Network)
    target_compile_definitions(grsim_ref PUBLIC GRSIM_HAS_QT_NETWORK)
else()
    message(STATUS "Qt5::Network not found — gc_adapter will not be built")
    # Remove gc_adapter.cpp from the source list if Qt Network is unavailable
    get_target_property(_ref_sources grsim_ref SOURCES)
    list(REMOVE_ITEM _ref_sources gc_adapter.cpp)
    set_target_properties(grsim_ref PROPERTIES SOURCES "${_ref_sources}")
    target_link_libraries(grsim_ref PUBLIC grsim_core)
endif()

target_compile_features(grsim_ref PUBLIC cxx_std_17)
set_target_properties(grsim_ref PROPERTIES POSITION_INDEPENDENT_CODE ON)
