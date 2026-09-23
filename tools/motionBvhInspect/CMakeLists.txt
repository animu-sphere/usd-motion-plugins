# SPDX-License-Identifier: Apache-2.0
#
# motion_bvh_inspect: what a BVH file holds, and which profiles fit it
# (WORKSPACE.md §1.2). It arrived from usd-vrm-plugins' tools/motionBvh, where
# it shared a directory with the converter. Here each CLI has its own directory.
#
# This tool names no OpenUSD. It prints what a file says, and the values it
# prints have no basis, no unit and no axis for a Gf type to imply. OpenUSD is
# still on its configure and, on macOS, on its artifact, because
# `find_package(motionBvh)` resolves motionSource and, through it, pxr. That is
# the cost of the declared motionBvh -> motionSource edge.

cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _motion_bvh_inspect_version LIMIT_COUNT 1)
    string(STRIP "${_motion_bvh_inspect_version}" _motion_bvh_inspect_version)
else()
    set(_motion_bvh_inspect_version "0.5.1")
endif()

project(motionBvhInspect
    VERSION ${_motion_bvh_inspect_version}
    DESCRIPTION "What a BVH file holds, and which profiles fit it"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED MOTIONBVHINSPECT_BUILD_TESTS)
    if(DEFINED USDMOTION_BUILD_TESTS)
        set(_motion_bvh_inspect_tests_default "${USDMOTION_BUILD_TESTS}")
    else()
        set(_motion_bvh_inspect_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MOTIONBVHINSPECT_BUILD_TESTS "Build motion_bvh_inspect tests"
           ${_motion_bvh_inspect_tests_default})
endif()

# A standalone configure of this directory resolves the library as an installed
# package; the composed build already has the in-tree target.
if(NOT TARGET motionBvh::motionBvh)
    find_package(motionBvh REQUIRED CONFIG)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionUtf8CodePage.cmake")

add_executable(motion_bvh_inspect
    src/Options.cpp
    src/Report.cpp
    src/main.cpp)
usdmotion_use_utf8_code_page(motion_bvh_inspect)

target_compile_features(motion_bvh_inspect PRIVATE cxx_std_17)
target_link_libraries(motion_bvh_inspect PRIVATE motionBvh::motionBvh)

if(MSVC)
    # The headers cite contract sections by their `§` sign, and MSVC reads
    # source in the *system* code page unless told otherwise.
    target_compile_options(motion_bvh_inspect PRIVATE /utf-8)
endif()

# Staged into the member root's bin/: `ost plugin package` reads the
# directories openstrata.tool.yaml declares rather than a build tree.
set_target_properties(motion_bvh_inspect PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(motion_bvh_inspect PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endforeach()

include(GNUInstallDirs)
install(TARGETS motion_bvh_inspect RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(MOTIONBVHINSPECT_BUILD_TESTS)
    enable_testing()
    if(NOT USDMOTION_TEST_PYTHON)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            set(USDMOTION_TEST_PYTHON "${Python3_EXECUTABLE}")
        endif()
    endif()
    add_subdirectory(tests)
endif()
