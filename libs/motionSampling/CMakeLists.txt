# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionSampling
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "Motion sampling with status, interpolation, resampling, filtering and blending"
    LANGUAGES CXX)

# C++20 as the workspace builds (docs/architecture/DEPENDENCIES.md §2); the
# public interface asks consumers for no more than C++17.
usdmotion_component_project(TESTS_OPTION MOTIONSAMPLING_BUILD_TESTS)

# Like motionCore, motionSampling uses only OpenUSD's Gf value types. It has no
# USD stage, Sdf, plug, or file-format dependency (WORKSPACE.md §2).
usdmotion_require_openusd()

# In the workspace build motionCore is already an in-tree target; a standalone
# configure resolves its installed package.
if(NOT TARGET motionCore::motionCore)
    find_package(motionCore REQUIRED CONFIG)
endif()

add_library(motionSampling STATIC
    src/Blend.cpp
    src/Filter.cpp
    src/Interpolation.cpp
    src/MotionSource.cpp
    src/PoseBuffer.cpp
    src/Resample.cpp)
add_library(motionSampling::motionSampling ALIAS motionSampling)
set_target_properties(motionSampling PROPERTIES
    EXPORT_NAME motionSampling
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionSampling PUBLIC cxx_std_17)
target_compile_definitions(motionSampling PUBLIC MOTIONSAMPLING_STATIC)
target_include_directories(motionSampling
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_link_libraries(motionSampling PUBLIC motionCore::motionCore)

if(MSVC)
    # PUBLIC, not PRIVATE: the installed headers carry UTF-8 (the §-references
    # into the contracts), so every consumer that includes them needs the flag
    # too or MSVC warns C4819 on a non-UTF-8 code page.
    target_compile_options(motionSampling PUBLIC /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS motionSampling
    EXPORT motionSamplingTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/motionSamplingConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/motionSamplingConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionSampling")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/motionSamplingConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    # While the API is 0.x a minor release may break it, as motionCore's does.
    COMPATIBILITY SameMinorVersion)
install(EXPORT motionSamplingTargets
    FILE motionSamplingTargets.cmake
    NAMESPACE motionSampling::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionSampling")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/motionSamplingConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/motionSamplingConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionSampling")

if(MOTIONSAMPLING_BUILD_TESTS)
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
