# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionUsd
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "MotionClip to and from OpenUSD UsdSkel"
    LANGUAGES CXX)

# C++20 as the workspace builds (docs/architecture/DEPENDENCIES.md §2); the
# public interface asks consumers for no more than C++17.
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
set_target_properties(motionUsd PROPERTIES
    EXPORT_NAME motionUsd
    POSITION_INDEPENDENT_CODE ON)

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
