# SPDX-License-Identifier: Apache-2.0

if(NOT USDMOTION_TEST_PYTHON)
    message(STATUS "motion_bvh_inspect: Python3 interpreter not found; skipping tests")
    return()
endif()

# The library's own corpus, driven through the CLI rather than through a second
# set of fixtures: the tool's claim is that it reports what the parser read, and
# a fixture only this tool ever sees could not check that.
get_filename_component(_motion_bvh_lib "${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/motionBvh"
                       ABSOLUTE)

if(EXISTS "${_motion_bvh_lib}/tests/corpus/generated")
    add_test(NAME motion_bvh_inspect_report
        COMMAND "${USDMOTION_TEST_PYTHON}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_motion_bvh_inspect.py"
                --tool "$<TARGET_FILE:motion_bvh_inspect>"
                --corpus "${_motion_bvh_lib}/tests/corpus")
endif()

# No ENVIRONMENT_MODIFICATION: this executable references nothing outside
# motionBvh's syntax half, so on Windows and Linux it records no OpenUSD import
# and a plain `ctest` finds everything it needs.
if(EXISTS "${_motion_bvh_lib}/tests/check_boundaries.py")
    add_test(NAME motion_bvh_inspect_boundaries
        COMMAND "${USDMOTION_TEST_PYTHON}"
                "${_motion_bvh_lib}/tests/check_boundaries.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/.."
                --cmake-target motion_bvh_inspect)

    # The same check pointed at a target that does not exist, and it must FAIL.
    # `--cmake-target` filters which link lines are examined, so a misspelled
    # one examines none and the link check quietly stops existing while still
    # printing "boundary check passed".
    add_test(NAME motion_bvh_inspect_boundariesGuard
        COMMAND "${USDMOTION_TEST_PYTHON}"
                "${_motion_bvh_lib}/tests/check_boundaries.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/.."
                --cmake-target no_such_target)
    set_tests_properties(motion_bvh_inspect_boundariesGuard PROPERTIES
        WILL_FAIL TRUE)
endif()

get_property(_motion_bvh_inspect_labelled DIRECTORY PROPERTY TESTS)
set_property(TEST ${_motion_bvh_inspect_labelled}
    APPEND PROPERTY LABELS "motion.cli")
