# SPDX-License-Identifier: Apache-2.0

add_executable(motionRetarget_tests test_motion_retarget.cpp)
target_link_libraries(motionRetarget_tests PRIVATE motionRetarget::motionRetarget)
# This suite checks with `assert()`, which NDEBUG compiles away -- and Release
# is the default build type, so NDEBUG is undefined for it explicitly. A suite
# that asserts nothing in the configuration CI runs passes by construction.
target_compile_options(motionRetarget_tests
    PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME motionRetarget_unit COMMAND motionRetarget_tests)

if(WIN32)
    if(TARGET gf)
        set(_motion_retarget_gf gf)
    else()
        set(_motion_retarget_gf pxr::gf)
    endif()
    # The library is static, but it calls exported Gf entry points (quaternion
    # multiply, matrix decomposition), so the test executable loads usd_gf.dll
    # at run time.
    # That DLL is in the runtime's lib/ and pulls in usd_tf.dll -> tbb.dll from
    # bin/ plus python3xx.dll from the Python install. An activated `ost`
    # session already has all three on PATH; a plain-CMake `ctest` run does not,
    # and the test exits 0xC0000135 (DLL not found) without them.
    set(_motion_retarget_test_path
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:motionRetarget>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_motion_retarget_gf}>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_motion_retarget_gf}>/../bin")
    if(USDMOTION_TEST_PYTHON)
        get_filename_component(_motion_retarget_python_dir
                               "${USDMOTION_TEST_PYTHON}" DIRECTORY)
        list(APPEND _motion_retarget_test_path
             "PATH=path_list_prepend:${_motion_retarget_python_dir}")
    endif()
    set_tests_properties(motionRetarget_unit PROPERTIES
        ENVIRONMENT_MODIFICATION "${_motion_retarget_test_path}")
endif()

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
