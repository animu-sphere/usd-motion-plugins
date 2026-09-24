# SPDX-License-Identifier: Apache-2.0
#
# UsdMotionTesting.cmake -- test plumbing, and only plumbing. What a suite
# checks stays in the component's tests/ directory; this is how a suite finds
# its interpreter, its OpenUSD and its assertions.
include_guard(GLOBAL)

# usdmotion_find_test_python()
#
# Sets USDMOTION_TEST_PYTHON, the interpreter every Python-driven test runs
# under, unless it is set already -- by the root, for the whole workspace, or
# on the command line.
#
# It must be the Python OpenUSD was built against, and pxrConfig.cmake names
# that one -- it sets Python3_EXECUTABLE unless it is already defined -- so this
# runs after usdmotion_require_openusd(). The result is kept under a name of our
# own because pxrConfig.cmake re-finds Python3 for its Development components in
# the caller's scope, which leaves Python3_Interpreter_FOUND false for anything
# guarded on it later (usd-vrm-plugins lost a test that way without a red lane).
# A macro, so the find's variables stay where they were before this was shared:
# the workspace tests forward Python3_EXECUTABLE to the installed consumer.
macro(usdmotion_find_test_python)
    if(NOT USDMOTION_TEST_PYTHON)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            set(USDMOTION_TEST_PYTHON "${Python3_EXECUTABLE}")
        endif()
    endif()
endmacro()

# usdmotion_openusd_root(<out-var>)
#
# The OpenUSD install root, from where pxrConfig.cmake was found: the runtime
# root itself when it has a bin/ beside the config, else three levels up from
# <root>/lib/cmake/pxr. Empty when OpenUSD was not found by config.
function(usdmotion_openusd_root out_var)
    set(_root "")
    if(pxr_DIR AND EXISTS "${pxr_DIR}/bin")
        set(_root "${pxr_DIR}")
    elseif(pxr_DIR)
        get_filename_component(_root "${pxr_DIR}/../../.." ABSOLUTE)
    endif()
    set(${out_var} "${_root}" PARENT_SCOPE)
endfunction()

# usdmotion_test_windows_path(TESTS <test>...
#                             [TARGETS <target>...] [OPENUSD <library>])
#
# On Windows, prepends to each test's PATH what its executable loads at run
# time: the directory of each TARGETS entry and, with OPENUSD, the directory of
# that OpenUSD library, the runtime's bin/ beside it and the test Python's
# directory. A static library that calls exported Gf entry points still makes
# its test load usd_gf.dll, which pulls in usd_tf.dll -> tbb.dll from bin/ and
# python3xx.dll from the Python install. An activated `ost` session already has
# all three on PATH; a plain-CMake `ctest` run does not, and the test exits
# 0xC0000135 (DLL not found) without them. Elsewhere this does nothing.
function(usdmotion_test_windows_path)
    cmake_parse_arguments(ARG "" "OPENUSD" "TESTS;TARGETS" ${ARGN})
    if(NOT WIN32)
        return()
    endif()
    set(_path)
    foreach(_target IN LISTS ARG_TARGETS)
        list(APPEND _path "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_target}>")
    endforeach()
    if(ARG_OPENUSD)
        set(_library usdmotion::pxr::${ARG_OPENUSD})
        list(APPEND _path
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_library}>"
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_library}>/../bin")
        if(USDMOTION_TEST_PYTHON)
            get_filename_component(_python_dir "${USDMOTION_TEST_PYTHON}" DIRECTORY)
            list(APPEND _path "PATH=path_list_prepend:${_python_dir}")
        endif()
    endif()
    set_tests_properties(${ARG_TESTS} PROPERTIES ENVIRONMENT_MODIFICATION "${_path}")
endfunction()

# usdmotion_keep_assertions(<target>...)
#
# Every suite here checks with `assert()`, and NDEBUG compiles `assert()` away
# -- while Release is the default build type -- so a suite built the normal way
# would print "passed" and return 0 without having checked anything. NDEBUG is
# undefined for the test targets alone: the code under test stays Release, the
# checks stay live.
function(usdmotion_keep_assertions)
    foreach(_target IN LISTS ARGN)
        target_compile_options(${_target}
            PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
    endforeach()
endfunction()
