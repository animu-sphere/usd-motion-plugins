# SPDX-License-Identifier: Apache-2.0

add_executable(motionCore_tests test_motion_pose.cpp)
add_executable(motionCore_compare_tests test_compare.cpp)
add_executable(motionCore_basis_tests test_basis_conversion.cpp)
target_link_libraries(motionCore_tests PRIVATE motionCore::motionCore)
target_link_libraries(motionCore_compare_tests PRIVATE motionCore::motionCore)
target_link_libraries(motionCore_basis_tests PRIVATE motionCore::motionCore)
usdmotion_keep_assertions(motionCore_tests motionCore_compare_tests motionCore_basis_tests)
add_test(NAME motionCore_unit COMMAND motionCore_tests)
add_test(NAME motionCore_compare COMMAND motionCore_compare_tests)
add_test(NAME motionCore_basis COMMAND motionCore_basis_tests)
usdmotion_test_windows_path(TESTS motionCore_unit motionCore_compare motionCore_basis
    TARGETS motionCore OPENUSD gf)

if(USDMOTION_TEST_PYTHON)
    add_test(NAME motionCore_boundaries
        COMMAND "${USDMOTION_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/check_boundaries.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/.."
            "$<TARGET_FILE:motionCore>")
endif()

# The layer's label, on every test this directory registers, so the core can be
# run and reported on its own (`ctest -L motion.core`); it is the label the
# suites carried in usd-vrm-plugins.
get_property(_motion_core_labelled DIRECTORY PROPERTY TESTS)
set_property(TEST ${_motion_core_labelled} APPEND PROPERTY LABELS "motion.core")
