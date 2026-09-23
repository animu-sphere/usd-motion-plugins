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

if(NOT pxr_FOUND)
    find_package(pxr REQUIRED CONFIG)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionOpenUsd.cmake")
if(NOT TARGET motionRecording::motionRecording)
    find_package(motionRecording REQUIRED CONFIG)
endif()
if(NOT TARGET motionSampling::motionSampling)
    find_package(motionSampling REQUIRED CONFIG)
endif()
if(NOT TARGET motionUsd::motionUsd)
    find_package(motionUsd REQUIRED CONFIG)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionUtf8CodePage.cmake")

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

foreach(_usd_lib usd usdGeom usdSkel sdf tf gf vt arch)
    if(TARGET ${_usd_lib})
        target_link_libraries(motion_record PRIVATE ${_usd_lib})
    elseif(TARGET pxr::${_usd_lib})
        target_link_libraries(motion_record PRIVATE pxr::${_usd_lib})
    else()
        message(FATAL_ERROR
                "motion_record requires the OpenUSD '${_usd_lib}' target.")
    endif()
endforeach()

if(MSVC)
    target_compile_options(motion_record PRIVATE /utf-8)
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

include(GNUInstallDirs)
install(TARGETS motion_record RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(MOTIONRECORD_BUILD_TESTS)
    enable_testing()
    if(NOT USDMOTION_TEST_PYTHON)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            set(USDMOTION_TEST_PYTHON "${Python3_EXECUTABLE}")
        endif()
    endif()
    add_subdirectory(tests)
endif()
