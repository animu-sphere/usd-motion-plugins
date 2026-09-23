# SPDX-License-Identifier: Apache-2.0
#
# execMotion — the vendor-neutral OpenExec motion bundle, and this
# workspace's one optional plugin (WORKSPACE.md §1.2). The boundary, four
# registered value types and eight computations: `motion.identityPose`, which
# is the mechanism held at its weakest possible value,
# `motion.sampleAnimation`, which reads a clip at the evaluated frame,
# `motion.filterPose` with the `motion.priorPose` seam an override enters
# through, `motion.extractRootMotion`, which is where the body is under the
# clip's stated intake policy, `motion.interpolatePose`, which samples the
# history a driver hands in through `motion.poseHistory`, and
# `motion.blendPoses`, which weights the clips a relationship targets.
#
# Self-contained so `ost plugin build plugins/execMotion` can configure it
# directly, and also add_subdirectory()-able from the repo-root CMakeLists.txt
# for the plain-CMake workflow — the same shape as every sibling member.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
usdmotion_read_version(USDMOTION_VERSION)

project(ExecMotionPlugin
    VERSION ${USDMOTION_VERSION}
    DESCRIPTION "OpenExec computations over canonical motion"
    LANGUAGES CXX)

# The tests option is USDMOTION_BUILD_TESTS itself rather than a member-private
# spelling: the root defines it for every member, and a private spelling is how
# usd-vrm-plugins once shipped a workspace lane that built a bundle on three
# platforms and tested it on none.
usdmotion_component_project(TESTS_OPTION USDMOTION_BUILD_TESTS)

if(NOT pxr_FOUND)
    find_package(pxr REQUIRED CONFIG)
endif()
# The OpenUSD pin, and then the OpenExec probe -- which this member alone
# calls, because it is the only one that cannot link without OpenExec, and
# every library and tool here builds against a runtime that has none
# (docs/architecture/DEPENDENCIES.md §1). A runtime the probe refuses would
# otherwise fail here as an unresolved symbol.
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionOpenUsd.cmake")
usdmotion_require_openexec()

if(NOT TARGET motionCore::motionCore)
    find_package(motionCore CONFIG REQUIRED)
endif()

# The sampling half, from `motion.filterPose` onward: the node is one step of
# `PoseFilter` and the algorithm stays in the library (WORKSPACE.md §2, design
# policy §21) -- as it does for `motion.interpolatePose`, which is `SampleClip`
# and whose result type, `PoseSampleResult`, is this library's, and for
# `motion.blendPoses`, which is the N-way `BlendPoses`.
# `motion.identityPose` and `motion.sampleAnimation` need none of it -- an exec
# input arrives already resolved at the evaluated frame, so the sampler had
# nothing to wrap.
if(NOT TARGET motionSampling::motionSampling)
    find_package(motionSampling CONFIG REQUIRED)
endif()

# The recording half, for one node and one rule: `motion.extractRootMotion` is
# `ConditionRootMotion`, the intake rule a capture session applies to every
# frame it accepts. Nothing else here reaches into a session.
if(NOT TARGET motionRecording::motionRecording)
    find_package(motionRecording CONFIG REQUIRED)
endif()

# The stage bridge, for one call: `PoseFromStageSample`, the rule that turns a
# UsdSkelAnimation's already-resolved arrays into a pose. It lived in
# usd-vrm-plugins' retarget CLI, which a computation cannot call, so this
# bundle carried a copy until the reading half arrived here (USD_MAPPING.md
# §7). The call takes values, so this is not a stage edge.
if(NOT TARGET motionUsd::motionUsd)
    find_package(motionUsd CONFIG REQUIRED)
endif()

set(PLUGIN_NAME ExecMotion)
add_library(${PLUGIN_NAME} SHARED
    src/ExecMotionPose.cpp
    src/ExecMotionRegistration.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${PXR_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_NAME} PRIVATE
    motionCore::motionCore
    motionSampling::motionSampling
    motionRecording::motionRecording
    motionUsd::motionUsd)

if(MSVC)
    target_compile_definitions(${PLUGIN_NAME} PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(${PLUGIN_NAME} PRIVATE /bigobj /utf-8)
endif()

# The exec half is named explicitly rather than taken from usd_ms: the
# monolithic target does not carry the exec libraries, and `esf`/`esfUsd`/`ef`
# are here because the public exec headers include them transitively — the same
# list `usdmotion_require_openexec()` probes for.
set(_exec_motion_targets)
foreach(_t IN ITEMS arch tf gf vt plug sdf usd usdSkel
                    vdf ef esf esfUsd exec execUsd)
    if(TARGET ${_t})
        list(APPEND _exec_motion_targets ${_t})
    elseif(TARGET pxr::${_t})
        list(APPEND _exec_motion_targets pxr::${_t})
    else()
        message(FATAL_ERROR
            "execMotion needs the OpenUSD target '${_t}', which this install "
            "does not define. usdmotion_require_openexec() should have "
            "refused this runtime already; if it did not, its probe list "
            "and this one have drifted apart.")
    endif()
endforeach()
target_link_libraries(${PLUGIN_NAME} PRIVATE ${_exec_motion_targets})

set(OPENSTRATA_PLUGIN_LIBRARY_PREFIX "lib")
set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX "${OPENSTRATA_PLUGIN_LIBRARY_PREFIX}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/lib")
endforeach()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/execMotion/plugInfo.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/execMotion/plugInfo.json"
    @ONLY)

if(USDMOTION_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()
