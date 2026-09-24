# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionUsd
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "MotionClip to and from OpenUSD UsdSkel"
    LANGUAGES CXX)

usdmotion_component_project(TESTS_OPTION MOTIONUSD_BUILD_TESTS)

# motionUsd is the one library here that authors a stage: it links OpenUSD's
# usd, sdf, usdGeom and usdSkel, and still no plug registration, file format,
# imaging or OpenExec (WORKSPACE.md §2, DEPENDENCIES.md).
usdmotion_require_openusd()

usdmotion_require_dependency(motionCore)

add_library(motionUsd STATIC
    src/ClipReader.cpp
    src/ClipWriter.cpp)
add_library(motionUsd::motionUsd ALIAS motionUsd)
set_target_properties(motionUsd PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionUsd PUBLIC cxx_std_17)
target_compile_definitions(motionUsd PUBLIC MOTIONUSD_STATIC)
target_include_directories(motionUsd
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
# The stage types are in the public header, so the OpenUSD edge is PUBLIC.
target_link_libraries(motionUsd
    PUBLIC
        motionCore::motionCore
        usdmotion::pxr::usd
        usdmotion::pxr::sdf
        usdmotion::pxr::usdGeom
        usdmotion::pxr::usdSkel)

# PUBLIC: the installed headers cite the contracts by their `§` sign.
usdmotion_target_utf8(motionUsd PUBLIC)

usdmotion_install_library(TARGET motionUsd PACKAGE motionUsd)

if(MOTIONUSD_BUILD_TESTS)
    enable_testing()
    usdmotion_find_test_python()
    add_subdirectory(tests)
endif()
