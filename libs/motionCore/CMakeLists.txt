# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionCore
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "Vendor-neutral motion value types"
    LANGUAGES CXX)

usdmotion_component_project(TESTS_OPTION MOTIONCORE_BUILD_TESTS)

# motionCore deliberately uses OpenUSD's small Gf value types in its public
# contract, but has no USD stage, Sdf, plug, or file-format dependency.
usdmotion_require_openusd()

add_library(motionCore STATIC
    src/Compare.cpp
    src/MotionPose.cpp)
add_library(motionCore::motionCore ALIAS motionCore)
set_target_properties(motionCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionCore PUBLIC cxx_std_17)
target_compile_definitions(motionCore PUBLIC MOTIONCORE_STATIC)
target_include_directories(motionCore
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

target_link_libraries(motionCore PUBLIC usdmotion::pxr::gf)

usdmotion_target_utf8(motionCore PRIVATE)

usdmotion_install_library(TARGET motionCore PACKAGE motionCore)

if(MOTIONCORE_BUILD_TESTS)
    enable_testing()
    usdmotion_find_test_python()
    add_subdirectory(tests)
endif()
