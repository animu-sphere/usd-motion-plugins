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

# The single product version: the repository-root VERSION file. CHANGELOG.md,
# the git tag and every manifest mirror it; nothing restates the number here.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _motion_project_version LIMIT_COUNT 1)
string(STRIP "${_motion_project_version}" _motion_project_version)

project(UsdMotionPlugins
    VERSION ${_motion_project_version}
    DESCRIPTION "Vendor-neutral, avatar-format-neutral motion for OpenUSD"
    LANGUAGES CXX)

# C++20, as the siblings build (docs/architecture/DEPENDENCIES.md §2). `ost`'s
# toolchain sets it too; a plain configure must not quietly get C++17.
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-config generators (Ninja) default to an empty build type. The OpenUSD
# installs this builds against are Release-only, so default to Release rather
# than pull debug-only imported dependencies.
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _isMultiConfig AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(USDMOTION_BUILD_TESTS "Build usd-motion-plugins tests" ${PROJECT_IS_TOP_LEVEL})

if(USDMOTION_BUILD_TESTS)
    # At the top level, so tests registered by components are discoverable by
    # `ctest` from this build tree.
    enable_testing()
endif()

# Resolve OpenUSD once for the whole project, and refuse any release but the
# one the ecosystem pins. Every library links OpenUSD's foundation types
# (WORKSPACE.md §2.1), so nothing here is configured before this.
find_package(pxr REQUIRED CONFIG)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/UsdMotionOpenUsd.cmake")

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

# Workspace-level tests: the documentation, and the installed-consumer lane.
if(USDMOTION_BUILD_TESTS)
    add_subdirectory("tests")
endif()

install(FILES
    LICENSE
    README.md
    DESTINATION "share/doc/usd-motion-plugins")
