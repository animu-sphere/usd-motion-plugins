# SPDX-License-Identifier: Apache-2.0

if(NOT USDMOTION_TEST_PYTHON)
    message(STATUS "motion_record: Python3 interpreter not found; skipping tests")
    return()
endif()

# The recording library's own corpus, driven through the CLI rather than through
# a second set of fixtures: the tool's claim is that it replays what the library
# records, and a trace only this tool ever saw could not check that.
get_filename_component(_motion_recording_corpus
                       "${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/motionRecording/tests/corpus"
                       ABSOLUTE)

if(EXISTS "${_motion_recording_corpus}")
    # This test needs the USD runtime twice over: the executable it launches
    # links the stage libraries, and the script opens the result with pxr.
    usdmotion_openusd_root(_usd_root)

    add_test(NAME motion_record_replay
        COMMAND "${USDMOTION_TEST_PYTHON}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_motion_record.py"
                --tool "$<TARGET_FILE:motion_record>"
                --corpus "${_motion_recording_corpus}")
    set_tests_properties(motion_record_replay PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_usd_root}/bin;PATH=path_list_prepend:${_usd_root}/lib;PATH=path_list_prepend:$<TARGET_FILE_DIR:motion_record>;PYTHONPATH=path_list_prepend:${_usd_root}/lib/python;PYTHONPATH=path_list_prepend:${_usd_root}/lib/site-packages")
endif()

get_property(_motion_record_labelled DIRECTORY PROPERTY TESTS)
set_property(TEST ${_motion_record_labelled}
    APPEND PROPERTY LABELS "motion.cli")
