# SPDX-License-Identifier: Apache-2.0

add_executable(motionUsd_tests test_motion_usd.cpp)
target_link_libraries(motionUsd_tests PRIVATE motionUsd::motionUsd)
# This suite checks with `assert()`, which NDEBUG compiles away -- and Release
# is the default build type, so NDEBUG is undefined for it explicitly. A suite
# that asserts nothing in the configuration CI runs passes by construction.
target_compile_options(motionUsd_tests
    PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME motionUsd_unit COMMAND motionUsd_tests)

if(WIN32)
    set(_motion_usd_skel usdmotion::pxr::usdSkel)
    # The library is static, but it links OpenUSD's stage libraries, so the
    # test executable loads usd_usdSkel.dll and its dependencies at run time.
    # That DLL is in the runtime's lib/ and pulls in usd_tf.dll -> tbb.dll from
    # bin/ plus python3xx.dll from the Python install. An activated `ost`
    # session already has all three on PATH; a plain-CMake `ctest` run does not,
    # and the test exits 0xC0000135 (DLL not found) without them.
    set(_motion_usd_test_path
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:motionUsd>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_motion_usd_skel}>"
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_motion_usd_skel}>/../bin")
    if(USDMOTION_TEST_PYTHON)
        get_filename_component(_motion_usd_python_dir
                               "${USDMOTION_TEST_PYTHON}" DIRECTORY)
        list(APPEND _motion_usd_test_path
             "PATH=path_list_prepend:${_motion_usd_python_dir}")
    endif()
    set_tests_properties(motionUsd_unit PROPERTIES
        ENVIRONMENT_MODIFICATION "${_motion_usd_test_path}")
endif()

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
