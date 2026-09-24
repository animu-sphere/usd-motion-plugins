# SPDX-License-Identifier: Apache-2.0
#
# motion_convert: a recorded source and a named profile become a motion stage
# (WORKSPACE.md §1.2). It arrived from usd-vrm-plugins as motion_bvh_convert and
# takes this repository's command name. BVH is the one source format it reads
# today.
#
# It is the first program that holds a reader and a profile at once
# (WORKSPACE.md §2). It needs motionSource by name rather than transitively: it
# matches a profile, converts, and reads a `CanonicalRestPose` back, none of
# which motionBvh's headers expose. It links OpenUSD's stage libraries, because
# authoring a clip is what it is for.

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionConvert
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "A recorded source and a named profile become a motion stage"
    LANGUAGES CXX)

usdmotion_component_project(TESTS_OPTION MOTIONCONVERT_BUILD_TESTS)

usdmotion_require_openusd()
usdmotion_require_dependency(motionBvh)
usdmotion_require_dependency(motionSource)
usdmotion_require_dependency(motionUsd)

add_executable(motion_convert
    src/ClipWriter.cpp
    src/ConvertOptions.cpp
    src/ProfileLocator.cpp
    src/convert_main.cpp)
usdmotion_use_utf8_code_page(motion_convert)

target_compile_features(motion_convert PRIVATE cxx_std_17)
target_link_libraries(motion_convert
    PRIVATE
        motionBvh::motionBvh
        motionSource::motionSource
        motionUsd::motionUsd
        usdmotion::pxr::usd
        usdmotion::pxr::usdGeom
        usdmotion::pxr::usdSkel
        usdmotion::pxr::sdf
        usdmotion::pxr::tf
        usdmotion::pxr::gf
        usdmotion::pxr::vt
        usdmotion::pxr::arch)

usdmotion_target_utf8(motion_convert PRIVATE)
if(MSVC)
    # This tool includes the canonical half, so windows.h's min/max macros meet
    # OpenUSD's own numeric_limits<>::max() uses in tf's robin map.
    target_compile_definitions(motion_convert PRIVATE NOMINMAX _USE_MATH_DEFINES)
endif()

# Staged into the member root's bin/: `ost plugin package` reads the
# directories openstrata.tool.yaml declares rather than a build tree.
set_target_properties(motion_convert PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(motion_convert PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endforeach()

usdmotion_install_tool(motion_convert)

# The profiles ship beside the tool, at the location ProfileLocator.cpp derives
# from the executable's own path: a converter with no profile available refuses
# every file it is given.
#
# `share/` is spelled out rather than taken from `CMAKE_INSTALL_DATADIR`,
# because the destination is a contract: the root CMakeLists installs there and
# ProfileLocator.cpp looks there, and a rule that followed GNUInstallDirs would
# move the files while the converter kept looking under `share`.
#
# It runs only in a standalone configure of this directory. The root
# CMakeLists installs the same directory to the same place.
if(PROJECT_IS_TOP_LEVEL
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/motion")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/motion/"
            DESTINATION "share/usd-motion-plugins/profiles/motion"
            FILES_MATCHING PATTERN "*.yaml")
endif()

if(MOTIONCONVERT_BUILD_TESTS)
    enable_testing()
    usdmotion_find_test_python()
    add_subdirectory(tests)
endif()
