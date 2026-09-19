# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _motion_usd_version LIMIT_COUNT 1)
    string(STRIP "${_motion_usd_version}" _motion_usd_version)
else()
    set(_motion_usd_version "0.1.0")
endif()

project(motionUsd
    VERSION ${_motion_usd_version}
    DESCRIPTION "MotionClip to and from OpenUSD UsdSkel"
    LANGUAGES CXX)

# C++20 as the workspace builds (docs/architecture/DEPENDENCIES.md §2); the
# public interface asks consumers for no more than C++17.
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED MOTIONUSD_BUILD_TESTS)
    if(DEFINED USDMOTION_BUILD_TESTS)
        set(_motion_usd_tests_default "${USDMOTION_BUILD_TESTS}")
    else()
        set(_motion_usd_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MOTIONUSD_BUILD_TESTS "Build motionUsd tests"
           ${_motion_usd_tests_default})
endif()

# motionUsd is the one library here that authors a stage: it links OpenUSD's
# usd, sdf, usdGeom and usdSkel, and still no plug registration, file format,
# imaging or OpenExec (WORKSPACE.md §2, DEPENDENCIES.md).
if(NOT pxr_FOUND)
    find_package(pxr REQUIRED CONFIG)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionOpenUsd.cmake")

# In the workspace build motionCore is already an in-tree target; a standalone
# configure resolves its installed package.
if(NOT TARGET motionCore::motionCore)
    find_package(motionCore REQUIRED CONFIG)
endif()

add_library(motionUsd STATIC
    src/ClipWriter.cpp)
add_library(motionUsd::motionUsd ALIAS motionUsd)
set_target_properties(motionUsd PROPERTIES
    EXPORT_NAME motionUsd
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionUsd PUBLIC cxx_std_17)
target_compile_definitions(motionUsd PUBLIC MOTIONUSD_STATIC)
target_include_directories(motionUsd
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_link_libraries(motionUsd PUBLIC motionCore::motionCore)
# The stage types are in the public header, so the edge is PUBLIC. The runtime
# names its targets bare or under pxr::, as motionCore's gf lookup says.
foreach(_motion_usd_pxr IN ITEMS usd sdf usdGeom usdSkel)
    if(TARGET ${_motion_usd_pxr})
        target_link_libraries(motionUsd PUBLIC ${_motion_usd_pxr})
    elseif(TARGET pxr::${_motion_usd_pxr})
        target_link_libraries(motionUsd PUBLIC pxr::${_motion_usd_pxr})
    else()
        message(FATAL_ERROR "motionUsd requires the OpenUSD ${_motion_usd_pxr} CMake target.")
    endif()
endforeach()

if(MSVC)
    # PUBLIC, not PRIVATE: the installed headers carry UTF-8 (the §-references
    # into the contracts), so every consumer that includes them needs the flag
    # too or MSVC warns C4819 on a non-UTF-8 code page.
    target_compile_options(motionUsd PUBLIC /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS motionUsd
    EXPORT motionUsdTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/motionUsdConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/motionUsdConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionUsd")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/motionUsdConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    # While the API is 0.x a minor release may break it, as motionCore's does.
    COMPATIBILITY SameMinorVersion)
install(EXPORT motionUsdTargets
    FILE motionUsdTargets.cmake
    NAMESPACE motionUsd::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionUsd")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/motionUsdConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/motionUsdConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionUsd")

if(MOTIONUSD_BUILD_TESTS)
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
