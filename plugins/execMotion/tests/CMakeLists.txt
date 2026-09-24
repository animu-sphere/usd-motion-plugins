# SPDX-License-Identifier: Apache-2.0

# ---------------------------------------------------------------------------
# The seam, with no stage and no exec
# ---------------------------------------------------------------------------
# ExecMotionPose.cpp is compiled into the test rather than linked out of the
# plugin: it is deliberately not exported, because nothing outside the bundle
# calls it and a bundle that exports its internals is a bundle other things
# start depending on.
add_executable(execMotion_pose_tests
    test_exec_motion_pose.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ExecMotionPose.cpp")
target_include_directories(execMotion_pose_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    ${PXR_INCLUDE_DIRS})
target_link_libraries(execMotion_pose_tests PRIVATE
    motionCore::motionCore
    motionSampling::motionSampling
    motionRecording::motionRecording
    # The seam's sampling rule is motionUsd's since 2026-09-20, so the suite
    # that exercises the seam with no stage links it too.
    motionUsd::motionUsd)
usdmotion_keep_assertions(execMotion_pose_tests)
usdmotion_target_utf8(execMotion_pose_tests PRIVATE)
if(MSVC)
    target_compile_definitions(execMotion_pose_tests
        PRIVATE NOMINMAX _USE_MATH_DEFINES)
endif()
add_test(NAME execMotion_pose COMMAND execMotion_pose_tests)

# ---------------------------------------------------------------------------
# The mechanism, through the built bundle
# ---------------------------------------------------------------------------
# This one links OpenUSD and motionCore but NOT the plugin: discovery has to go
# through PXR_PLUGINPATH_NAME and the staged plugInfo.json, which is the path a
# packaged bundle takes.
add_executable(execMotion_mechanism_tests test_exec_motion_mechanism.cpp)
target_include_directories(execMotion_mechanism_tests PRIVATE
    ${PXR_INCLUDE_DIRS})
target_link_libraries(execMotion_mechanism_tests PRIVATE motionCore::motionCore)
usdmotion_keep_assertions(execMotion_mechanism_tests)
usdmotion_target_utf8(execMotion_mechanism_tests PRIVATE)
if(MSVC)
    target_compile_definitions(execMotion_mechanism_tests
        PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(execMotion_mechanism_tests PRIVATE /bigobj)
endif()

# The bundle's own OpenUSD half: the suites below drive exec directly.
set(_exec_motion_test_targets
    usdmotion::pxr::arch usdmotion::pxr::tf usdmotion::pxr::gf
    usdmotion::pxr::vt usdmotion::pxr::plug usdmotion::pxr::sdf
    usdmotion::pxr::usd usdmotion::pxr::usdSkel usdmotion::pxr::vdf
    usdmotion::pxr::ef usdmotion::pxr::esf usdmotion::pxr::esfUsd
    usdmotion::pxr::exec usdmotion::pxr::execUsd)
target_link_libraries(execMotion_mechanism_tests
    PRIVATE ${_exec_motion_test_targets})

add_test(NAME execMotion_mechanism
    COMMAND execMotion_mechanism_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/identity_clip.usda")

# ---------------------------------------------------------------------------
# The sampled clip, through the built bundle
# ---------------------------------------------------------------------------
# Separate from the mechanism suite on purpose. That one holds the mechanism at
# its weakest possible value -- the identity -- so a red result there is a
# discovery, compile, cache or invalidation failure and nothing else. This one
# holds the first computation that has a value to get wrong, and it drives time:
# the same request at three frames, and a clip that cannot be stamped.
add_executable(execMotion_sample_tests test_exec_motion_sample.cpp)
target_include_directories(execMotion_sample_tests PRIVATE
    ${PXR_INCLUDE_DIRS})
target_link_libraries(execMotion_sample_tests PRIVATE motionCore::motionCore)
usdmotion_keep_assertions(execMotion_sample_tests)
usdmotion_target_utf8(execMotion_sample_tests PRIVATE)
if(MSVC)
    target_compile_definitions(execMotion_sample_tests
        PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(execMotion_sample_tests PRIVATE /bigobj)
endif()
target_link_libraries(execMotion_sample_tests
    PRIVATE ${_exec_motion_test_targets})

# Three fixtures, each differing from the first by one thing, because two of
# what this suite measures are attributions rather than values: the second holds
# still, so `computeTime` is the only input left that moves with the frame, and
# the third states no rate, so what exec does with a `.Required()` input the
# stage does not carry is measured rather than assumed from the documentation.
add_test(NAME execMotion_sample
    COMMAND execMotion_sample_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sampled_clip.usda"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/static_clip.usda"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/unrated_clip.usda")

# ---------------------------------------------------------------------------
# The filter, through the built bundle
# ---------------------------------------------------------------------------
# Separate again, and for the same reason as the sample suite: this one holds
# the first node that consumes another computation and the first whose missing
# half a caller supplies, so a red result here is about chaining, overrides or
# the filter's policy -- never about reading a clip.
#
# It does NOT link motionSampling, although the computation under test wraps
# it.
# An expected value produced by the same function would assert that the library
# equals itself; the suite writes out `PoseFilter`'s documented step weight
# instead, so a clip's stated cutoff failing to reach the library is a red test
# rather than two agreeing wrong answers.
add_executable(execMotion_filter_tests test_exec_motion_filter.cpp)
target_include_directories(execMotion_filter_tests PRIVATE
    ${PXR_INCLUDE_DIRS})
target_link_libraries(execMotion_filter_tests PRIVATE motionCore::motionCore)
usdmotion_keep_assertions(execMotion_filter_tests)
usdmotion_target_utf8(execMotion_filter_tests PRIVATE)
if(MSVC)
    target_compile_definitions(execMotion_filter_tests
        PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(execMotion_filter_tests PRIVATE /bigobj)
endif()
target_link_libraries(execMotion_filter_tests
    PRIVATE ${_exec_motion_test_targets})

# Two fixtures differing by one thing: filtered_clip.usda is sampled_clip.usda
# plus the three `motion:filter:*` attributes. So the pair separates the policy
# a clip states from the default `PoseFilter` applies when it states
# none -- which is the only way to tell the library's default from a default
# this bundle might have invented.
add_test(NAME execMotion_filter
    COMMAND execMotion_filter_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/filtered_clip.usda"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sampled_clip.usda")

# ---------------------------------------------------------------------------
# The root intake, through the built bundle
# ---------------------------------------------------------------------------
# The first computation that returns something other than a pose, so this is
# where the bundle's SECOND registered value type is exercised: one request
# hands back a `MotionPose` and a `RootMotion` side by side,
# and the dependent value key is a different type from the one it reads.
#
# Like the filter suite it does not link motionRecording, although the rule
# under test is `ConditionRootMotion`. The expected values are written out from
# the rule's definition, which is the only way this suite can say anything the
# seam did not already say to itself.
add_executable(execMotion_root_tests test_exec_motion_root.cpp)
target_include_directories(execMotion_root_tests PRIVATE
    ${PXR_INCLUDE_DIRS})
target_link_libraries(execMotion_root_tests PRIVATE motionCore::motionCore)
usdmotion_keep_assertions(execMotion_root_tests)
usdmotion_target_utf8(execMotion_root_tests PRIVATE)
if(MSVC)
    target_compile_definitions(execMotion_root_tests
        PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(execMotion_root_tests PRIVATE /bigobj)
endif()
target_link_libraries(execMotion_root_tests
    PRIVATE ${_exec_motion_test_targets})

# Two fixtures differing by one thing again: rooted_clip.usda is
# sampled_clip.usda plus `motion:root:intake`. The pair is what separates the
# policy a clip states from `LiveCaptureConfig`'s own default -- and the
# fixture states `passthrough` rather than the default precisely so the two
# cannot agree by accident.
add_test(NAME execMotion_root
    COMMAND execMotion_root_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/rooted_clip.usda"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sampled_clip.usda")

# ---------------------------------------------------------------------------
# The history, through the built bundle
# ---------------------------------------------------------------------------
# The first node whose input a driver hands in rather than feeds back: a
# timestamped history, supplied as an override on `motion.poseHistory` and
# sampled at the evaluated instant. So this is where the bundle's third and
# fourth registered value types are exercised -- `MotionClip` in,
# `PoseSampleResult` out -- and where an override of a key that is not a
# pose, two overrides in one call, and an override of the wrong type are
# measured.
#
# Unlike the filter and root suites it links motionSampling, because the result
# type is declared there -- and it calls none of the library's functions. The
# suite reads a `PoseSampleResult`'s plain fields, so every expected value is
# still written from a definition rather than produced by the code under test.
add_executable(execMotion_interpolate_tests test_exec_motion_interpolate.cpp)
target_include_directories(execMotion_interpolate_tests PRIVATE
    ${PXR_INCLUDE_DIRS})
target_link_libraries(execMotion_interpolate_tests PRIVATE
    motionCore::motionCore
    motionSampling::motionSampling)
usdmotion_keep_assertions(execMotion_interpolate_tests)
usdmotion_target_utf8(execMotion_interpolate_tests PRIVATE)
if(MSVC)
    target_compile_definitions(execMotion_interpolate_tests
        PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(execMotion_interpolate_tests PRIVATE /bigobj)
endif()
target_link_libraries(execMotion_interpolate_tests
    PRIVATE ${_exec_motion_test_targets})

# No fixture of its own. The node reads nothing off the clip that the sampler
# does not already read, so the sampled clip and the unrated one are the pair:
# the first has an instant to sample a history at, the second has none.
add_test(NAME execMotion_interpolate
    COMMAND execMotion_interpolate_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sampled_clip.usda"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/unrated_clip.usda")

# ---------------------------------------------------------------------------
# The blend, through the built bundle
# ---------------------------------------------------------------------------
# The one node that reads poses from several prims, so this is where 26.08's
# fan-in is measured: the order a relationship's targets arrive in, what happens
# to a target that is not a clip and to a source that refused, an edit of the
# relationship itself, and an override of one prim's key reaching a node on
# another.
#
# Like the filter and root suites it does not link motionSampling, although the
# computation under test is `BlendPoses`. The fixture turns one head about
# one axis, so every expected blend is an angle interpolated along it, written
# out from the library's documented fold rather than produced by the library.
add_executable(execMotion_blend_tests test_exec_motion_blend.cpp)
target_include_directories(execMotion_blend_tests PRIVATE
    ${PXR_INCLUDE_DIRS})
target_link_libraries(execMotion_blend_tests PRIVATE motionCore::motionCore)
usdmotion_keep_assertions(execMotion_blend_tests)
usdmotion_target_utf8(execMotion_blend_tests PRIVATE)
if(MSVC)
    target_compile_definitions(execMotion_blend_tests
        PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(execMotion_blend_tests PRIVATE /bigobj)
endif()
target_link_libraries(execMotion_blend_tests
    PRIVATE ${_exec_motion_test_targets})

# One fixture, three prims: two clips that disagree in every field the suite
# reads, and a blend over them. Every other case -- a target that is not a clip,
# a source that refuses, weights that do not pair, two rates -- is one statement
# the suite authors onto that stage.
add_test(NAME execMotion_blend
    COMMAND execMotion_blend_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/blended_clips.usda")

# ---------------------------------------------------------------------------
# The display slice, through the built bundle and usdExecImaging
# ---------------------------------------------------------------------------
# The display case: a clip's root motion placing an Xformable, through
# `execGeom`'s own transform computation and `usdExecImaging`'s stage scene
# index -- the object UsdImagingGLEngine merges over the USD one when
# USDIMAGINGGL_ENGINE_ENABLE_EXEC_SCENE_INDEX is set. Driven directly, with an
# observer where Hydra would be, so it needs no GL context and runs on every
# lane.
#
# The only suite here that links imaging, and it links `execGeom` for nothing
# but the computation's token: the computation itself is found the way this
# bundle's are, through plugInfo.json. Neither is linked by the bundle, which
# registers for no schema of theirs. motionCore is for the one pose a driver
# hands in -- a root orientation, which no clip states.
add_executable(execMotion_display_tests test_exec_motion_display.cpp)
target_include_directories(execMotion_display_tests PRIVATE
    ${PXR_INCLUDE_DIRS})
usdmotion_keep_assertions(execMotion_display_tests)
usdmotion_target_utf8(execMotion_display_tests PRIVATE)
if(MSVC)
    target_compile_definitions(execMotion_display_tests
        PRIVATE NOMINMAX _USE_MATH_DEFINES)
    target_compile_options(execMotion_display_tests PRIVATE /bigobj)
endif()
set(_exec_motion_display_targets ${_exec_motion_test_targets}
    usdmotion::pxr::usdGeom usdmotion::pxr::execGeom usdmotion::pxr::hd
    usdmotion::pxr::usdImaging usdmotion::pxr::usdExecImaging)
target_link_libraries(execMotion_display_tests
    PRIVATE motionCore::motionCore ${_exec_motion_display_targets})

# One fixture: the sampled clip plus the attribute a display connects to, a
# prop that connects to it under a translated parent, and a material nothing
# the placement reads. Every broken shape -- a route that fails, a policy that
# refuses, an op exec does not read -- is authored by the suite.
add_test(NAME execMotion_display
    COMMAND execMotion_display_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/displayed_clip.usda")

# ---------------------------------------------------------------------------
# The boundary, and the snapshot rule
# ---------------------------------------------------------------------------
# Motion policy §11.4: no socket, file, clock, thread or mutable global state in
# a computation. check_boundaries.py reads the rule off the source, the built
# library's imports, the target's link libraries and the plugInfo.json schema
# declarations. A consumer's exec bundle keeps its own copy of the rule's
# tables; this one is the vendor-neutral half.
#
# The link libraries are handed in from the target itself, because the
# workspace libraries a bundle could reach a socket through are static: no
# dependency list of the built binary would ever name one.
if(USDMOTION_TEST_PYTHON)
    add_test(NAME execMotion_boundaries
        COMMAND "${USDMOTION_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/check_boundaries.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/.."
            "$<TARGET_FILE:${PLUGIN_NAME}>"
            "$<JOIN:$<TARGET_PROPERTY:${PLUGIN_NAME},LINK_LIBRARIES>,|>")
    # The check's own tables, against text and symbol lists of both platforms.
    # A boundary check that passes proves nothing about whether it can fail,
    # and its first review found two tables that could not.
    add_test(NAME execMotion_boundaries_selftest
        COMMAND "${USDMOTION_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_check_boundaries.py")
else()
    message(WARNING
        "execMotion: no Python interpreter, so execMotion_boundaries is not "
        "registered and the snapshot rule is unchecked in this build")
endif()

# ---------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------
# The bundle's own resource directory is what PXR_PLUGINPATH_NAME points at, and
# its plugInfo.json resolves LibraryPath relative to itself -- so the staged
# tree the bundle already produces is what the test loads, not a copy.
usdmotion_openusd_root(_usd_root)

set(_bundle_dir "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(_plug_resources "${_bundle_dir}/plugin/resources/execMotion")

# motionCore is static, so nothing of it is loaded at run time; OpenUSD is not,
# and on Windows a plain-CMake ctest run finds none of it without this. Same
# reason as libs/motionSampling's block: an activated `ost` session already has
# these on PATH and a bare ctest exits 0xC0000135 without them.
set(_exec_motion_env
    "PATH=path_list_prepend:${_usd_root}/bin"
    "PATH=path_list_prepend:${_usd_root}/lib"
    "PATH=path_list_prepend:${_bundle_dir}/lib"
    "PXR_PLUGINPATH_NAME=path_list_prepend:${_plug_resources}")

set_tests_properties(execMotion_mechanism execMotion_sample execMotion_filter
                     execMotion_root execMotion_interpolate execMotion_blend
                     execMotion_display
    PROPERTIES ENVIRONMENT_MODIFICATION "${_exec_motion_env}")

# The seam suite needs no plugin and no plugin path, but it does link
# motionCore, motionSampling and motionRecording and -- through Gf -- loads
# usd_gf.dll on Windows all the same.
set_tests_properties(execMotion_pose PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_usd_root}/bin;PATH=path_list_prepend:${_usd_root}/lib")

# One label, so the OpenExec half of the suite can be run and reported on its
# own.
set_tests_properties(execMotion_pose execMotion_mechanism execMotion_sample
                     execMotion_filter execMotion_root execMotion_interpolate
                     execMotion_blend execMotion_display
    PROPERTIES LABELS "motion.openexec")
if(TEST execMotion_boundaries)
    set_tests_properties(execMotion_boundaries execMotion_boundaries_selftest
        PROPERTIES LABELS "motion.openexec")
endif()
