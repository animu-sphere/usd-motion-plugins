# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _motion_core_version LIMIT_COUNT 1)
    string(STRIP "${_motion_core_version}" _motion_core_version)
else()
    set(_motion_core_version "0.5.0")
endif()

project(motionCore
    VERSION ${_motion_core_version}
    DESCRIPTION "Vendor-neutral motion value types"
    LANGUAGES CXX)

# C++20 as the workspace builds (docs/architecture/DEPENDENCIES.md §2); the
# public interface asks consumers for no more than C++17.
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED MOTIONCORE_BUILD_TESTS)
    if(DEFINED USDMOTION_BUILD_TESTS)
        set(_motion_core_tests_default "${USDMOTION_BUILD_TESTS}")
    else()
        set(_motion_core_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MOTIONCORE_BUILD_TESTS "Build motionCore tests"
           ${_motion_core_tests_default})
endif()

# motionCore deliberately uses OpenUSD's small Gf value types in its public
# contract, but has no USD stage, Sdf, plug, or file-format dependency.
if(NOT pxr_FOUND)
    find_package(pxr REQUIRED CONFIG)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionOpenUsd.cmake")

add_library(motionCore STATIC
    src/Compare.cpp
    src/MotionPose.cpp)
add_library(motionCore::motionCore ALIAS motionCore)
set_target_properties(motionCore PROPERTIES
    EXPORT_NAME motionCore
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionCore PUBLIC cxx_std_17)
target_compile_definitions(motionCore PUBLIC MOTIONCORE_STATIC)
target_include_directories(motionCore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if(TARGET gf)
    target_link_libraries(motionCore PUBLIC gf)
elseif(TARGET pxr::gf)
    target_link_libraries(motionCore PUBLIC pxr::gf)
else()
    message(FATAL_ERROR "motionCore requires the OpenUSD gf CMake target.")
endif()

if(MSVC)
    target_compile_options(motionCore PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS motionCore
    EXPORT motionCoreTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/motionCoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/motionCoreConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionCore")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/motionCoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    # While the API is 0.x a minor release may break it, so a consumer asking
    # for 0.1 must not be handed 0.2 (the installed-consumer lane asks this way).
    COMPATIBILITY SameMinorVersion)
install(EXPORT motionCoreTargets
    FILE motionCoreTargets.cmake
    NAMESPACE motionCore::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionCore")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/motionCoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/motionCoreConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionCore")

if(MOTIONCORE_BUILD_TESTS)
    enable_testing()
    # In the workspace the root has already found the interpreter, under a name
    # pxrConfig.cmake cannot clobber (the root CMakeLists.txt says why).
    # Standalone, find it here, after find_package(pxr).
    if(NOT USDMOTION_TEST_PYTHON)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            set(USDMOTION_TEST_PYTHON "${Python3_EXECUTABLE}")
        endif()
    endif()
    add_subdirectory(tests)
endif()
