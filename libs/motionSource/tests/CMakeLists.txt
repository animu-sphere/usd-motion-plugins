# SPDX-License-Identifier: Apache-2.0

# The source rig: its queries, its equality, and the invariants a hand-built
# skeleton can break.
add_executable(motionSource_skeleton_tests test_source_skeleton.cpp)
# The source animation: the angle vocabulary, timing, and the checks that keep a
# track index meaning a joint index.
add_executable(motionSource_animation_tests test_source_animation.cpp)
# Provenance, and the one narrowing crossing into canonical motion.
add_executable(motionSource_provenance_tests test_source_provenance.cpp)
# The profile contract: the vocabulary a profile states, its invariants, and what
# matching one against a rig concludes.
add_executable(motionSource_profile_tests test_source_profile.cpp)
# The profile as a file: the keys, and the line a file that gets one wrong is
# told about.
add_executable(motionSource_profileFile_tests test_source_profile_file.cpp)
# The converter: the change of basis, the angle composition, the path rule, and
# the four ways a conversion refuses.
add_executable(motionSource_conversion_tests test_canonical_conversion.cpp)

set(_motion_source_tests
    motionSource_skeleton_tests
    motionSource_animation_tests
    motionSource_provenance_tests
    motionSource_profile_tests
    motionSource_profileFile_tests
    motionSource_conversion_tests)

foreach(_motion_source_test ${_motion_source_tests})
    target_link_libraries(${_motion_source_test} PRIVATE
                          motionSource::motionSource)
    usdmotion_target_utf8(${_motion_source_test} PRIVATE)
endforeach()
usdmotion_keep_assertions(${_motion_source_tests})

add_test(NAME motionSource_skeleton COMMAND motionSource_skeleton_tests)
add_test(NAME motionSource_animation COMMAND motionSource_animation_tests)
add_test(NAME motionSource_provenance COMMAND motionSource_provenance_tests)
add_test(NAME motionSource_profile COMMAND motionSource_profile_tests)
add_test(NAME motionSource_profileFile COMMAND motionSource_profileFile_tests)
add_test(NAME motionSource_conversion COMMAND motionSource_conversion_tests)

# Every profile this repository ships, read by the library that defines what one
# is. It takes the directory rather than referencing a target from another
# directory, so a standalone configure of this library still generates -- and it
# is skipped rather than failed where the directory is absent, because those
# files are package data and this is a library test.
set(_motion_source_profiles "${CMAKE_CURRENT_SOURCE_DIR}/../../../profiles/motion")
if(EXISTS "${_motion_source_profiles}")
    add_test(NAME motionSource_shippedProfiles
        COMMAND motionSource_profileFile_tests "${_motion_source_profiles}")
endif()

# The converter names Gf types and the rest of this library links motionCore,
# which does -- so every executable here imports usd_gf.dll.
get_property(_motion_source_test_names DIRECTORY PROPERTY TESTS)
usdmotion_test_windows_path(TESTS ${_motion_source_test_names}
    TARGETS motionSource OPENUSD gf)

if(USDMOTION_TEST_PYTHON)
    # Source and CMake only, and no binary argument, unlike every other boundary
    # check in this workspace: this library's one permitted dependency is
    # motionCore, whose Gf value types make an "imports no OpenUSD" claim false
    # by design. check_boundaries.py says so in place of pretending otherwise.
    add_test(NAME motionSource_boundaries
        COMMAND "${USDMOTION_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/check_boundaries.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()
