# SPDX-License-Identifier: Apache-2.0

add_executable(motionRetarget_tests test_motion_retarget.cpp)
target_link_libraries(motionRetarget_tests PRIVATE motionRetarget::motionRetarget)
usdmotion_keep_assertions(motionRetarget_tests)
add_test(NAME motionRetarget_unit COMMAND motionRetarget_tests)

usdmotion_test_windows_path(TESTS motionRetarget_unit
    TARGETS motionRetarget OPENUSD gf)

if(USDMOTION_TEST_PYTHON)
    add_test(NAME motionRetarget_boundaries
        COMMAND "${USDMOTION_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/check_boundaries.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/.."
            "$<TARGET_FILE:motionRetarget>")
endif()

# The layer's label, on every test this directory registers, so retargeting can be
# run and reported on its own (`ctest -L motion.retarget`).
get_property(_motion_retarget_labelled DIRECTORY PROPERTY TESTS)
set_property(TEST ${_motion_retarget_labelled}
    APPEND PROPERTY LABELS "motion.retarget")
