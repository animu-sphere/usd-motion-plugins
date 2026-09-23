# SPDX-License-Identifier: Apache-2.0

add_executable(motionUsd_tests test_motion_usd.cpp)
target_link_libraries(motionUsd_tests PRIVATE motionUsd::motionUsd)
usdmotion_keep_assertions(motionUsd_tests)
add_test(NAME motionUsd_unit COMMAND motionUsd_tests)

usdmotion_test_windows_path(TESTS motionUsd_unit
    TARGETS motionUsd OPENUSD usdSkel)

if(USDMOTION_TEST_PYTHON)
    add_test(NAME motionUsd_boundaries
        COMMAND "${USDMOTION_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/check_boundaries.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/.."
            "$<TARGET_FILE:motionUsd>")
endif()

# The layer's label, on every test this directory registers, so the stage writer can be
# run and reported on its own (`ctest -L motion.usd`).
get_property(_motion_usd_labelled DIRECTORY PROPERTY TESTS)
set_property(TEST ${_motion_usd_labelled}
    APPEND PROPERTY LABELS "motion.usd")
