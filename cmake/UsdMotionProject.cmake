# SPDX-License-Identifier: Apache-2.0
#
# UsdMotionProject.cmake -- the project policy every entry point shares.
#
# Every CMakeLists.txt that calls project() includes this first: the root, and
# each library, tool and bundle, which `ost` configures standalone with no root
# project in scope (docs/architecture/WORKSPACE.md §4). It is reached by a
# relative path from the component, so a standalone configure reads the same
# file, and the same VERSION, as the workspace does.
#
#   include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMotionProject.cmake")
#   usdmotion_read_version(USDMOTION_VERSION)
#   project(motionCore VERSION ${USDMOTION_VERSION} ... LANGUAGES CXX)
#   usdmotion_component_project(TESTS_OPTION MOTIONCORE_BUILD_TESTS)
#
# project() itself stays in each CMakeLists.txt: CMake requires a literal call
# in the top-level file, and a component is the top-level file whenever it is
# built standalone. So this module reads the version before project() and
# applies the policy after it, and wraps neither.
include_guard(GLOBAL)

# usdmotion_read_version(<out-var>)
#
# The single product version: the repository-root VERSION file. CHANGELOG.md,
# the git tag and every manifest mirror it; no CMake file restates the number,
# so there is no fallback to drift (scripts/check_docs.py holds the mirrors).
function(usdmotion_read_version out_var)
    set(_file "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../VERSION")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR
            "usd-motion-plugins: no VERSION at ${_file}. Components are "
            "configured from inside a checkout, never from a copy of their "
            "directory alone.")
    endif()
    file(STRINGS "${_file}" _version LIMIT_COUNT 1)
    string(STRIP "${_version}" _version)
    if(NOT _version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "usd-motion-plugins: VERSION holds '${_version}', "
                            "not a MAJOR.MINOR.PATCH version")
    endif()
    # A VERSION bump reconfigures, rather than leaving the old number in every
    # generated package version file until something else does.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_file}")
    set(${out_var} "${_version}" PARENT_SCOPE)
endfunction()

# usdmotion_project_policy()
#
# The build policy of every project here, root or component. A macro, because
# it sets the caller's variables.
#
#   * C++20, as the siblings build (docs/architecture/DEPENDENCIES.md §2).
#     `ost`'s toolchain sets it too; a plain configure must not quietly get
#     C++17. A library's *public* requirement is separate and stays on its
#     target -- `target_compile_features(<lib> PUBLIC cxx_std_17)` -- so a
#     consumer is asked for no more than its headers need.
#   * Release for a single-config generator with no build type. The OpenUSD
#     installs this builds against are Release-only, and an empty build type
#     pulls their debug-only imported dependencies.
macro(usdmotion_project_policy)
    if(NOT CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 20)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    get_property(_usdmotion_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(NOT _usdmotion_multi_config AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
    unset(_usdmotion_multi_config)
endmacro()

# usdmotion_component_project(TESTS_OPTION <option>)
#
# Called immediately after a component's project(): the project policy, and the
# component's tests option. The option defaults to the workspace's
# USDMOTION_BUILD_TESTS when the root defines it, and otherwise to whether the
# component is the top-level project -- so a standalone `ost` build tests the
# component, and a consumer that add_subdirectory()s it does not.
macro(usdmotion_component_project)
    cmake_parse_arguments(_usdmotion_component "" "TESTS_OPTION" "" ${ARGN})
    if(NOT _usdmotion_component_TESTS_OPTION)
        message(FATAL_ERROR "usdmotion_component_project: TESTS_OPTION is required")
    endif()

    usdmotion_project_policy()

    if(NOT DEFINED ${_usdmotion_component_TESTS_OPTION})
        if(DEFINED USDMOTION_BUILD_TESTS)
            set(_usdmotion_component_tests_default "${USDMOTION_BUILD_TESTS}")
        else()
            set(_usdmotion_component_tests_default "${PROJECT_IS_TOP_LEVEL}")
        endif()
        option(${_usdmotion_component_TESTS_OPTION} "Build ${PROJECT_NAME} tests"
               ${_usdmotion_component_tests_default})
        unset(_usdmotion_component_tests_default)
    endif()
    unset(_usdmotion_component_TESTS_OPTION)
    unset(_usdmotion_component_UNPARSED_ARGUMENTS)
    unset(_usdmotion_component_KEYWORDS_MISSING_VALUES)
endmacro()

# The rest of the shared infrastructure, so a component includes one file.
include("${CMAKE_CURRENT_LIST_DIR}/UsdMotionOpenUsd.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/UsdMotionTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/UsdMotionUtf8CodePage.cmake")
