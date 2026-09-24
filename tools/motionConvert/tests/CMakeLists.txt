# SPDX-License-Identifier: Apache-2.0

if(NOT USDMOTION_TEST_PYTHON)
    message(STATUS "motion_convert: Python3 interpreter not found; skipping tests")
    return()
endif()

get_filename_component(_motion_bvh_lib "${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/motionBvh"
                       ABSOLUTE)

# The converter's boundary is not the library's, and `--crossing` is where that
# is said out loud. It may author a stage and speak the humanoid vocabulary --
# that is what a converter is -- and it may not name a retargeter or a target
# avatar's schema, because the target avatar is the one thing this layer never
# binds to.
if(EXISTS "${_motion_bvh_lib}/tests/check_boundaries.py")
    add_test(NAME motion_convert_boundaries
        COMMAND "${USDMOTION_TEST_PYTHON}"
                "${_motion_bvh_lib}/tests/check_boundaries.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/.."
                --cmake-target motion_convert --crossing)
endif()

# The converter over the recorded export the library's own conversion test
# drives, checking the clip on disk rather than the values in memory, which is
# the half nothing else covers.
get_filename_component(_motion_profiles
                       "${CMAKE_CURRENT_SOURCE_DIR}/../../../profiles/motion"
                       ABSOLUTE)
if(EXISTS "${_motion_bvh_lib}/tests/corpus/recorded/redistributable"
   AND EXISTS "${_motion_profiles}")
    # This test needs the USD runtime twice over: the executable it launches
    # links the stage libraries, and the script opens the result with pxr.
    usdmotion_openusd_root(_usd_root)

    add_test(NAME motion_convert_clip
        COMMAND "${USDMOTION_TEST_PYTHON}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_motion_convert.py"
                --tool "$<TARGET_FILE:motion_convert>"
                --corpus "${_motion_bvh_lib}/tests/corpus"
                --profiles "${_motion_profiles}")
    set_tests_properties(motion_convert_clip PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_usd_root}/bin;PATH=path_list_prepend:${_usd_root}/lib;PATH=path_list_prepend:$<TARGET_FILE_DIR:motion_convert>;PYTHONPATH=path_list_prepend:${_usd_root}/lib/python;PYTHONPATH=path_list_prepend:${_usd_root}/lib/site-packages"
        # A real producer's export.
        LABELS "motion.real-corpus")
endif()

get_property(_motion_convert_labelled DIRECTORY PROPERTY TESTS)
set_property(TEST ${_motion_convert_labelled}
    APPEND PROPERTY LABELS "motion.cli")
