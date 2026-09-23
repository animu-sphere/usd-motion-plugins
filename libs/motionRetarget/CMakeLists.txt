# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionRetarget
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "Humanoid retargeting onto a target skeleton described as plain values"
    LANGUAGES CXX)

# C++20 as the workspace builds (docs/architecture/DEPENDENCIES.md §2); the
# public interface asks consumers for no more than C++17.
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
set_target_properties(motionRetarget PROPERTIES
    EXPORT_NAME motionRetarget
    POSITION_INDEPENDENT_CODE ON)

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

include(GNUInstallDirs)
install(TARGETS motionRetarget
    EXPORT motionRetargetTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/motionRetargetConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/motionRetargetConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionRetarget")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/motionRetargetConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    # While the API is 0.x a minor release may break it, as motionCore's does.
    COMPATIBILITY SameMinorVersion)
install(EXPORT motionRetargetTargets
    FILE motionRetargetTargets.cmake
    NAMESPACE motionRetarget::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionRetarget")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/motionRetargetConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/motionRetargetConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionRetarget")

if(MOTIONRETARGET_BUILD_TESTS)
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
