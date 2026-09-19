# SPDX-License-Identifier: Apache-2.0
#
# Workspace-level tests: the documentation, and what spans components. A
# component's own tests live in that component.

if(NOT USDMOTION_TEST_PYTHON)
    message(WARNING
        "usd-motion-plugins: no Python 3 interpreter; the workspace tests are "
        "not registered")
    return()
endif()

# The docs check the hand-written docs-check workflow runs, so a local ctest
# catches a broken link or a half-bumped version before a push does.
add_test(NAME workspace_docs
    COMMAND "${USDMOTION_TEST_PYTHON}" "${PROJECT_SOURCE_DIR}/scripts/check_docs.py")
add_test(NAME workspace_docs_selftest
    COMMAND "${USDMOTION_TEST_PYTHON}" "${PROJECT_SOURCE_DIR}/scripts/check_docs.py"
        --selftest)

# The OpenUSD install root, from where pxrConfig.cmake was found.
set(_usd_root "")
if(pxr_DIR AND EXISTS "${pxr_DIR}/bin")
    set(_usd_root "${pxr_DIR}")
elseif(pxr_DIR)
    get_filename_component(_usd_root "${pxr_DIR}/../../.." ABSOLUTE)
endif()

# The installed-consumer lane: install this build tree into a clean prefix and
# consume every package installed_consumer/packages.json lists from a project
# copied outside the repository (scripts/check_installed_consumer.py). It
# exists before the first package does, so the first import lands in a lane
# that already runs rather than in one written beside it. It configures and
# builds a second CMake project, so it is labelled and can be left out with
# `ctest -LE installed-consumer`.
set(_consumer_toolchain --generator "${CMAKE_GENERATOR}")
if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
    list(APPEND _consumer_toolchain
        --make-program "${CMAKE_MAKE_PROGRAM}"
        --cxx-compiler "${CMAKE_CXX_COMPILER}")
endif()
# The Python this configure handed OpenUSD, forwarded so the consumer's
# find_package(pxr) resolves the same one (scripts/check_installed_consumer.py
# says why it cannot find it alone). Only what is set is forwarded: under
# `ost build` the toolchain sets all three, and a plain configure that found
# Python by itself needs none.
foreach(_python_var Python3_EXECUTABLE Python3_LIBRARY Python3_INCLUDE_DIR)
    if(${_python_var})
        string(TOLOWER "${_python_var}" _python_arg)
        string(REPLACE "python3_" "--python-" _python_arg "${_python_arg}")
        string(REPLACE "_" "-" _python_arg "${_python_arg}")
        list(APPEND _consumer_toolchain "${_python_arg}" "${${_python_var}}")
    endif()
endforeach()

add_test(NAME workspace_installed_consumer
    COMMAND "${USDMOTION_TEST_PYTHON}"
        "${PROJECT_SOURCE_DIR}/scripts/check_installed_consumer.py"
        --build-dir "${PROJECT_BINARY_DIR}"
        --config "$<CONFIG>"
        --usd-root "${_usd_root}"
        ${_consumer_toolchain})
set_tests_properties(workspace_installed_consumer PROPERTIES
    LABELS "installed-consumer")
