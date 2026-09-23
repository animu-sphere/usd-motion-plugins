# SPDX-License-Identifier: Apache-2.0
#
# motion_record: a recorded trace becomes a motion stage (WORKSPACE.md §1.2).
# It arrived from usd-vrm-plugins as motion_capture and takes this repository's
# command name.
#
# It replays a session through motionRecording, on a tick sized by
# motionSampling's tolerance, and authors the result through motionUsd, so it
# names all three. It links OpenUSD's stage libraries, because authoring a clip
# is what it is for.

cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(motionRecord
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "A recorded trace becomes a motion stage"
    LANGUAGES CXX)

usdmotion_component_project(TESTS_OPTION MOTIONRECORD_BUILD_TESTS)

usdmotion_require_openusd()
usdmotion_require_dependency(motionRecording)
usdmotion_require_dependency(motionSampling)
usdmotion_require_dependency(motionUsd)

add_executable(motion_record
    src/Options.cpp
    src/main.cpp)
usdmotion_use_utf8_code_page(motion_record)

target_compile_features(motion_record PRIVATE cxx_std_17)
target_link_libraries(motion_record
    PRIVATE
        motionRecording::motionRecording
        motionSampling::motionSampling
        motionUsd::motionUsd)

target_link_libraries(motion_record
    PRIVATE
        usdmotion::pxr::usd
        usdmotion::pxr::usdGeom
        usdmotion::pxr::usdSkel
        usdmotion::pxr::sdf
        usdmotion::pxr::tf
        usdmotion::pxr::gf
        usdmotion::pxr::vt
        usdmotion::pxr::arch)

usdmotion_target_utf8(motion_record PRIVATE)
if(MSVC)
    # windows.h's min/max macros meet OpenUSD's own numeric_limits<>::max()
    # uses in tf's robin map.
    target_compile_definitions(motion_record PRIVATE NOMINMAX _USE_MATH_DEFINES)
endif()

# Staged into the member root's bin/: `ost plugin package` reads the
# directories openstrata.tool.yaml declares rather than a build tree.
set_target_properties(motion_record PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(motion_record PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endforeach()

usdmotion_install_tool(motion_record)

if(MOTIONRECORD_BUILD_TESTS)
    enable_testing()
    usdmotion_find_test_python()
    add_subdirectory(tests)
endif()
