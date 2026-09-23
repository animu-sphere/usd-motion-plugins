# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionRecording
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "Motion stream intake, recording, the capture trace format and replay"
    LANGUAGES CXX)

# C++20 as the workspace builds (docs/architecture/DEPENDENCIES.md §2); the
# public interface asks consumers for no more than C++17.
usdmotion_component_project(TESTS_OPTION MOTIONRECORDING_BUILD_TESTS)

# Like motionCore, motionRecording uses only OpenUSD's Gf value types. It has no
# USD stage, Sdf, plug, or file-format dependency, and no transport or clock:
# a stream receives poses already decoded (WORKSPACE.md §2, MOTION_CONTRACT.md
# §9).
usdmotion_require_openusd()

# In the workspace build motionCore and motionSampling are already in-tree
# targets; a standalone configure resolves their installed packages.
if(NOT TARGET motionCore::motionCore)
    find_package(motionCore REQUIRED CONFIG)
endif()
if(NOT TARGET motionSampling::motionSampling)
    find_package(motionSampling REQUIRED CONFIG)
endif()

add_library(motionRecording STATIC
    src/CaptureTrace.cpp
    src/LiveCaptureSource.cpp
    src/MotionRecorder.cpp
    src/ReplaySender.cpp)
add_library(motionRecording::motionRecording ALIAS motionRecording)
set_target_properties(motionRecording PROPERTIES
    EXPORT_NAME motionRecording
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionRecording PUBLIC cxx_std_17)
target_compile_definitions(motionRecording PUBLIC MOTIONRECORDING_STATIC)
target_include_directories(motionRecording
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_link_libraries(motionRecording
    PUBLIC motionCore::motionCore motionSampling::motionSampling)

if(MSVC)
    # PUBLIC, not PRIVATE: the installed headers carry UTF-8 (the §-references
    # into the contracts), so every consumer that includes them needs the flag
    # too or MSVC warns C4819 on a non-UTF-8 code page.
    target_compile_options(motionRecording PUBLIC /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS motionRecording
    EXPORT motionRecordingTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/motionRecordingConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/motionRecordingConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionRecording")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/motionRecordingConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    # While the API is 0.x a minor release may break it, as motionCore's does.
    COMPATIBILITY SameMinorVersion)
install(EXPORT motionRecordingTargets
    FILE motionRecordingTargets.cmake
    NAMESPACE motionRecording::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionRecording")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/motionRecordingConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/motionRecordingConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionRecording")

if(MOTIONRECORDING_BUILD_TESTS)
    enable_testing()
    # In the workspace the root has already found the interpreter (the root
    # CMakeLists.txt says why under its own name); standalone, find it here.
    if(NOT USDMOTION_TEST_PYTHON)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            set(USDMOTION_TEST_PYTHON "${Python3_EXECUTABLE}")
        endif()
    endif()
    add_subdirectory(tests)
endif()
