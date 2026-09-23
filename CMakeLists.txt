# SPDX-License-Identifier: Apache-2.0
#
# Top-level project for usd-motion-plugins.
#
# The workspace builds two ways, and both are kept working
# (docs/architecture/WORKSPACE.md §4):
#   * OpenStrata: `ost build` and `ost test` configure this file against the
#     activated runtime's toolchain.
#   * Plain CMake: configure this file with CMAKE_PREFIX_PATH pointing at an
#     OpenUSD 26.08 install, or use CMakePresets.json:
#       cmake -S . -B build -DCMAKE_PREFIX_PATH=<usd-install>
#       cmake --build build --config Release
#       ctest --test-dir build -C Release
#
# Each identity WORKSPACE.md §1 reserves
# arrives as a self-contained CMake project (so `ost` can build it standalone)
# that is also add_subdirectory()-able from here, and is named below in full,
# in the dependency order of WORKSPACE.md §2.1.
cmake_minimum_required(VERSION 3.22)

# The shared build infrastructure is cmake/ (UsdMotionProject.cmake names the
# rest); this file is the workspace's orchestration only.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(UsdMotionPlugins
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "Vendor-neutral, avatar-format-neutral motion for OpenUSD"
    LANGUAGES CXX)

# C++20, and Release for a single-config generator with no build type.
usdmotion_project_policy()

option(USDMOTION_BUILD_TESTS "Build usd-motion-plugins tests" ${PROJECT_IS_TOP_LEVEL})

# The one optional member (WORKSPACE.md §1.2). Every library and tool here
# builds against an OpenUSD with no OpenExec at all; plugins/execMotion is the
# only one that cannot, and it is the only one a consumer may not want. Turning
# it off is how a runtime without the exec libraries builds the rest.
option(USDMOTION_BUILD_EXEC_MOTION "Build the optional plugins/execMotion bundle" ON)

if(USDMOTION_BUILD_TESTS)
    # At the top level, so tests registered by components are discoverable by
    # `ctest` from this build tree.
    enable_testing()
endif()

# Resolve OpenUSD once for the whole project, and refuse any release but the
# one the ecosystem pins. Every library links OpenUSD's foundation types
# (WORKSPACE.md §2.1), so nothing here is configured before this.
usdmotion_require_openusd()

# The interpreter the workspace tests run under. It must be the Python OpenUSD
# was built against, and pxrConfig.cmake names that one -- it sets
# Python3_EXECUTABLE unless it is already defined -- so the interpreter is found
# here, after it. It is kept under a name of our own because pxrConfig.cmake
# re-finds Python3 for its Development components in *this* scope, which leaves
# Python3_Interpreter_FOUND false for anything guarded on it later
# (usd-vrm-plugins lost a test that way without a red lane).
if(USDMOTION_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMOTION_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()

# Components, in dependency order (WORKSPACE.md §2.1), as each import lands.
add_subdirectory("libs/motionCore")
add_subdirectory("libs/motionSampling")
add_subdirectory("libs/motionRecording")
add_subdirectory("libs/motionRetarget")
add_subdirectory("libs/motionUsd")
add_subdirectory("libs/motionSource")
add_subdirectory("libs/motionBvh")
add_subdirectory("tools/motionBvhInspect")
add_subdirectory("tools/motionConvert")
add_subdirectory("tools/motionRecord")

if(USDMOTION_BUILD_EXEC_MOTION)
    add_subdirectory("plugins/execMotion")
endif()

# Workspace-level tests: the documentation, and the installed-consumer lane.
if(USDMOTION_BUILD_TESTS)
    add_subdirectory("tests")
endif()

install(FILES
    LICENSE
    README.md
    DESTINATION "share/doc/usd-motion-plugins")

# The producer profiles are package data, installed where motion_convert's
# ProfileLocator.cpp looks for them beside its own executable
# (WORKSPACE.md §1.2). `share/` is spelled out for the reason
# tools/motionConvert/CMakeLists.txt gives: the path is a contract with the
# locator, not a packaging preference.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/profiles/motion/"
        DESTINATION "share/usd-motion-plugins/profiles/motion"
        FILES_MATCHING PATTERN "*.yaml")
