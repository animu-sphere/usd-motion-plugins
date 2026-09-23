# SPDX-License-Identifier: Apache-2.0

add_executable(motionSampling_tests test_motion_sampling.cpp)
target_link_libraries(motionSampling_tests PRIVATE motionSampling::motionSampling)
usdmotion_keep_assertions(motionSampling_tests)
add_test(NAME motionSampling_unit COMMAND motionSampling_tests)

usdmotion_test_windows_path(TESTS motionSampling_unit
    TARGETS motionSampling OPENUSD gf)

if(USDMOTION_TEST_PYTHON)
    add_test(NAME motionSampling_boundaries
        COMMAND "${USDMOTION_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/check_boundaries.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/.."
            "$<TARGET_FILE:motionSampling>")
endif()

# The layer's label, on every test this directory registers, so sampling can be
# run and reported on its own (`ctest -L motion.sampling`).
get_property(_motion_sampling_labelled DIRECTORY PROPERTY TESTS)
set_property(TEST ${_motion_sampling_labelled}
    APPEND PROPERTY LABELS "motion.sampling")
