# SPDX-License-Identifier: Apache-2.0

add_executable(motionCore_tests test_motion_pose.cpp)
add_executable(motionCore_compare_tests test_compare.cpp)
# These suites check with `assert()`, which NDEBUG compiles away -- and Release
# is the default build type, so NDEBUG is undefined for them explicitly. A suite
# that asserts nothing in the configuration CI runs passes by construction.
foreach(_motion_core_test motionCore_tests motionCore_compare_tests)
    target_link_libraries(${_motion_core_test} PRIVATE motionCore::motionCore)
    target_compile_options(${_motion_core_test}
        PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
endforeach()
add_test(NAME motionCore_unit COMMAND motionCore_tests)
add_test(NAME motionCore_compare COMMAND motionCore_compare_tests)
if(WIN32)
    set_tests_properties(motionCore_unit motionCore_compare PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:motionCore>")
endif()

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
