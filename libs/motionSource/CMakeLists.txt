# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionSource
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "Format-neutral recorded-motion source model"
    LANGUAGES CXX)

usdmotion_component_project(TESTS_OPTION MOTIONSOURCE_BUILD_TESTS)

# The declared motionSource -> motionCore edge (WORKSPACE.md §2), and the only
# OpenUSD this library sees. It arrives through motionCore rather than directly.
# Four files cross into canonical motion -- CanonicalMetadata, SourceProfile for
# the right-hand side of a joint map, SourceProfileFile, which reads that side
# out of a file, and CanonicalConversion, which is the crossing rather than a
# corner of one -- and motionSource_boundaries carries the list, so a fifth joins
# it in review rather than quietly. Everywhere else a value in a basis this layer
# does not know is still not a geometric vector (SourceSkeleton.h), and the check
# is what keeps that true now that one file here does name a Gf type.
usdmotion_require_openusd()

usdmotion_require_dependency(motionCore)

add_library(motionSource STATIC
    src/CanonicalConversion.cpp
    src/CanonicalMetadata.cpp
    src/SourceAnimation.cpp
    src/SourceProfile.cpp
    src/SourceProfileFile.cpp
    src/SourceProvenance.cpp
    src/SourceSkeleton.cpp)
add_library(motionSource::motionSource ALIAS motionSource)
set_target_properties(motionSource PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionSource PUBLIC cxx_std_17)
target_compile_definitions(motionSource PUBLIC MOTIONSOURCE_STATIC)
target_include_directories(motionSource
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_link_libraries(motionSource PUBLIC motionCore::motionCore)

# PUBLIC: the installed headers cite the contracts by their `§` sign.
usdmotion_target_utf8(motionSource PUBLIC)

usdmotion_install_library(TARGET motionSource PACKAGE motionSource)

if(MOTIONSOURCE_BUILD_TESTS)
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
