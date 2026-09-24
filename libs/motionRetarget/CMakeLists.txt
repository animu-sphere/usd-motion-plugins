# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionRetarget
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "Humanoid retargeting onto a target skeleton described as plain values"
    LANGUAGES CXX)

usdmotion_component_project(TESTS_OPTION MOTIONRETARGET_BUILD_TESTS)

# motionRetarget takes a rig as plain values and returns plain values: OpenUSD
# Gf value types only, no stage, no plugin registration and no OpenExec
# (WORKSPACE.md §2; RETARGETING_POLICY.md §1). Reading a rig off a stage is
# motionUsd's or a format repository's job.
usdmotion_require_openusd()

usdmotion_require_dependency(motionCore)

add_library(motionRetarget STATIC
    src/Diagnostics.cpp
    src/PoseRetargeter.cpp
    src/RestPose.cpp
    src/RetargetMap.cpp
    src/RootMotionPolicy.cpp
    src/SkeletonDescriptor.cpp)
add_library(motionRetarget::motionRetarget ALIAS motionRetarget)
set_target_properties(motionRetarget PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionRetarget PUBLIC cxx_std_17)
target_compile_definitions(motionRetarget PUBLIC MOTIONRETARGET_STATIC)
target_include_directories(motionRetarget
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
# motionCore alone (WS-O2): the retarget answers at a clip's own times, and a
# caller that wants another timeline resamples before it retargets.
target_link_libraries(motionRetarget PUBLIC motionCore::motionCore)

# PUBLIC: the installed headers carry UTF-8.
usdmotion_target_utf8(motionRetarget PUBLIC)

usdmotion_install_library(TARGET motionRetarget PACKAGE motionRetarget)

if(MOTIONRETARGET_BUILD_TESTS)
    enable_testing()
    usdmotion_find_test_python()
    add_subdirectory(tests)
endif()
