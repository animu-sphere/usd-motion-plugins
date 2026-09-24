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

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionBvhInspect
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "What a BVH file holds, and which profiles fit it"
    LANGUAGES CXX)

usdmotion_component_project(TESTS_OPTION MOTIONBVHINSPECT_BUILD_TESTS)

usdmotion_require_dependency(motionBvh)

add_executable(motion_bvh_inspect
    src/Options.cpp
    src/Report.cpp
    src/main.cpp)
usdmotion_use_utf8_code_page(motion_bvh_inspect)

target_compile_features(motion_bvh_inspect PRIVATE cxx_std_17)
target_link_libraries(motion_bvh_inspect PRIVATE motionBvh::motionBvh)

usdmotion_target_utf8(motion_bvh_inspect PRIVATE)

# Staged into the member's binary-tree bin/, the directory openstrata.tool.yaml
# declares, where `ost build` finds it (cmake/UsdMotionStage.cmake).
usdmotion_stage_tool(motion_bvh_inspect)

usdmotion_install_tool(motion_bvh_inspect)

if(MOTIONBVHINSPECT_BUILD_TESTS)
    enable_testing()
    usdmotion_find_test_python()
    add_subdirectory(tests)
endif()
