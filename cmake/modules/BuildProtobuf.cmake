# ***************************************************************************
# *   Copyright 2017 Michael Eischer                                        *
# *   Robotics Erlangen e.V.                                                *
# *   http://www.robotics-erlangen.de/                                      *
# *   info@robotics-erlangen.de                                             *
# *                                                                         *
# *   This program is free software: you can redistribute it and/or modify  *
# *   it under the terms of the GNU General Public License as published by  *
# *   the Free Software Foundation, either version 3 of the License, or     *
# *   any later version.                                                    *
# *                                                                         *
# *   This program is distributed in the hope that it will be useful,       *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU General Public License for more details.                          *
# *                                                                         *
# *   You should have received a copy of the GNU General Public License     *
# *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
# ***************************************************************************

include(ExternalProject)

set(PROTOBUF_CMAKE_ARGS )

ExternalProject_Add(protobuf_external
                    # Use protobuf 3.19.6 — last version without abseil dependency,
                    # compatible with MSVC 2022 C++17
  URL               https://github.com/protocolbuffers/protobuf/releases/download/v3.19.6/protobuf-cpp-3.19.6.tar.gz
  SOURCE_SUBDIR     cmake
  CMAKE_ARGS
                    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
                    -DCMAKE_C_COMPILER:PATH=${CMAKE_C_COMPILER}
                    -DCMAKE_CXX_COMPILER:PATH=${CMAKE_CXX_COMPILER}
                    -DCMAKE_MAKE_PROGRAM:PATH=${CMAKE_MAKE_PROGRAM}
                    -DCMAKE_CXX_STANDARD=17
                    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
                    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
                    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
                    # the tests fail to build :-(
                    -Dprotobuf_BUILD_TESTS:BOOL=OFF
                    -Dprotobuf_MSVC_STATIC_RUNTIME:BOOL=OFF
  STEP_TARGETS install
)

set(PROTOBUF_SUBPATH "${CMAKE_INSTALL_LIBDIR}/libprotobuf${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(LIBPROTOC_SUBPATH "${CMAKE_INSTALL_LIBDIR}/libprotoc${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(PROTOC_SUBPATH "bin/protoc${CMAKE_EXECUTABLE_SUFFIX}")

# the byproducts are available after the install step
ExternalProject_Add_Step(protobuf_external out
    DEPENDEES install
    BYPRODUCTS
        "<INSTALL_DIR>/${PROTOBUF_SUBPATH}"
        "<INSTALL_DIR>/${LIBPROTOC_SUBPATH}"
        "<INSTALL_DIR>/${PROTOC_SUBPATH}"
)

ExternalProject_Get_Property(protobuf_external install_dir)
set_target_properties(protobuf_external PROPERTIES EXCLUDE_FROM_ALL true)

# override all necessary variables originally set by find_package
# if FORCE is not set cmake does not allow us to override the variables, for some unknown reason
set(Protobuf_FOUND true CACHE BOOL "" FORCE)
set(Protobuf_VERSION "3.19.6" CACHE STRING "" FORCE)
set(Protobuf_INCLUDE_DIR "${install_dir}/include" CACHE PATH "" FORCE)
set(Protobuf_INCLUDE_DIRS "${Protobuf_INCLUDE_DIR}" CACHE PATH "" FORCE)
set(Protobuf_LIBRARY "${install_dir}/${PROTOBUF_SUBPATH}" CACHE PATH "" FORCE)
set(Protobuf_LIBRARIES "${Protobuf_LIBRARY}" CACHE PATH "" FORCE)
set(Protobuf_LIBRARY_DEBUG "${install_dir}/${PROTOBUF_SUBPATH}" CACHE PATH "" FORCE)
set(Protobuf_LIBRARY_RELEASE "${install_dir}/${PROTOBUF_SUBPATH}" CACHE PATH "" FORCE)
set(Protobuf_LITE_LIBRARY_DEBUG "${install_dir}/${PROTOBUF_SUBPATH}" CACHE PATH "" FORCE)
set(Protobuf_LITE_LIBRARY_RELEASE "${install_dir}/${PROTOBUF_SUBPATH}" CACHE PATH "" FORCE)
set(Protobuf_PROTOC_EXECUTABLE "${install_dir}/${PROTOC_SUBPATH}" CACHE PATH "" FORCE)
set(Protobuf_PROTOC_LIBRARY_DEBUG "${install_dir}/${LIBPROTOC_SUBPATH}" CACHE PATH "" FORCE)
set(Protobuf_PROTOC_LIBRARY_RELEASE "${install_dir}/${LIBPROTOC_SUBPATH}" CACHE PATH "" FORCE)
# this is a dependency for the protobuf_generate_cpp custom command
# if this is not set the generate command sometimes get executed before protoc is compiled
set(protobuf_generate_DEPENDENCIES protobuf_external CACHE STRING "" FORCE)

# compatibility with cmake 3.10
if(NOT TARGET protobuf::protoc)
    # avoid error if target was already created for an older version
    add_executable(protobuf::protoc IMPORTED)
endif()
# override the protobuf::protoc path used by the protobuf_generate_cpp macro
set_target_properties(protobuf::protoc PROPERTIES
    IMPORTED_LOCATION "${Protobuf_PROTOC_EXECUTABLE}"
)

# Define protobuf_generate_cpp() ourselves since cmake 4.3's FindProtobuf
# tries to read version from a header that doesn't exist yet.
if(NOT COMMAND protobuf_generate_cpp)
  function(PROTOBUF_GENERATE_CPP SRCS HDRS)
    set(${SRCS})
    set(${HDRS})
    foreach(FIL ${ARGN})
      get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
      get_filename_component(FIL_WE ${FIL} NAME_WE)
      get_filename_component(FIL_DIR ${ABS_FIL} DIRECTORY)
      list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc")
      list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h")
      add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.cc"
               "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.pb.h"
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
          --cpp_out ${CMAKE_CURRENT_BINARY_DIR}
          -I${FIL_DIR} -I${Protobuf_INCLUDE_DIR}
          ${ABS_FIL}
        DEPENDS ${ABS_FIL} ${protobuf_generate_DEPENDENCIES}
        COMMENT "Running protoc on ${FIL}"
      )
    endforeach()
    set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
  endfunction()
endif()

message(STATUS "Building protobuf ${Protobuf_VERSION}")
