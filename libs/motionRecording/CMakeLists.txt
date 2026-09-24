# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionRecording
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "Motion stream intake, recording, the capture trace format and replay"
    LANGUAGES CXX)

usdmotion_component_project(TESTS_OPTION MOTIONRECORDING_BUILD_TESTS)

# Like motionCore, motionRecording uses only OpenUSD's Gf value types. It has no
# USD stage, Sdf, plug, or file-format dependency, and no transport or clock:
# a stream receives poses already decoded (WORKSPACE.md §2, MOTION_CONTRACT.md
# §9).
usdmotion_require_openusd()

usdmotion_require_dependency(motionCore)
usdmotion_require_dependency(motionSampling)

add_library(motionRecording STATIC
    src/CaptureTrace.cpp
    src/LiveCaptureSource.cpp
    src/MotionRecorder.cpp
    src/ReplaySender.cpp)
add_library(motionRecording::motionRecording ALIAS motionRecording)
set_target_properties(motionRecording PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionRecording PUBLIC cxx_std_17)
target_compile_definitions(motionRecording PUBLIC MOTIONRECORDING_STATIC)
target_include_directories(motionRecording
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_link_libraries(motionRecording
    PUBLIC motionCore::motionCore motionSampling::motionSampling)

# PUBLIC: the installed headers cite the contracts by their `§` sign.
usdmotion_target_utf8(motionRecording PUBLIC)

usdmotion_install_library(TARGET motionRecording PACKAGE motionRecording)

if(MOTIONRECORDING_BUILD_TESTS)
    enable_testing()
    usdmotion_find_test_python()
    add_subdirectory(tests)
endif()
