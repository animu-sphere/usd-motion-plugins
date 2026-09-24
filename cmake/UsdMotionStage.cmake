# SPDX-License-Identifier: Apache-2.0
#
# UsdMotionStage.cmake -- where a build puts what it produces: the binary tree,
# never the source tree.
#
# A member's *stage* is its CMake binary directory, laid out the way the member
# is laid out: a tool's executable in `bin/`, a bundle's library in `lib/` and
# its registration under the `plugin/resources/...` path its manifest names. A
# multi-config generator gets one stage per configuration, `<binary>/<config>/`,
# so a Debug build and a Release build never overwrite each other's files.
#
# The stage is what the build's own tests run against. It is not what ships:
#
#   * a bundle ships what its install rules install. `ost plugin build` (0.23.5
#     and later) installs the bundle into its target-local bundle stage, and
#     `ost plugin test` and `ost plugin package` read that;
#   * a tool ships the executable `ost build` finds in the root build tree under
#     the member's `bin/`, which is this stage under a single-config generator,
#     the only kind `ost` configures.
#
# So the source tree stays an input only: a build writes nothing into it, and a
# Debug/Release pair, two targets or a parallel build cannot collide there.
include_guard(GLOBAL)

# usdmotion_member_stage(<out-var>)
#
# The current member's stage directory, as a path that may hold `$<CONFIG>`:
# usable in target properties, file(GENERATE), custom commands and test
# properties, which all evaluate generator expressions.
function(usdmotion_member_stage out_var)
    get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(_multi_config)
        set(${out_var} "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>" PARENT_SCOPE)
    else()
        set(${out_var} "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
    endif()
endfunction()

# usdmotion_stage_tool(<target>)
#
# A tool's executable goes to `<stage>/bin/`, the directory its
# openstrata.tool.yaml declares. The path carries a generator expression under
# a multi-config generator, which also stops CMake appending a configuration
# directory of its own.
function(usdmotion_stage_tool target)
    usdmotion_member_stage(_stage)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_stage}/bin")
endfunction()

# usdmotion_stage_bundle(<target> PLUG_INFO <plugInfo.json> RESOURCES <dir>)
#
# A bundle's library goes to `<stage>/lib/`, and the registration PLUG_INFO
# names -- already configured -- to `<stage>/<RESOURCES>/plugInfo.json`, so the
# stage is a loadable bundle: its plugInfo.json finds the library by the same
# relative LibraryPath the installed bundle uses. Point PXR_PLUGINPATH_NAME at
# `$<TARGET_FILE_DIR:<target>>/../<RESOURCES>` to load it.
function(usdmotion_stage_bundle target)
    cmake_parse_arguments(ARG "" "PLUG_INFO;RESOURCES" "" ${ARGN})
    if(NOT ARG_PLUG_INFO OR NOT ARG_RESOURCES)
        message(FATAL_ERROR "usdmotion_stage_bundle: PLUG_INFO and RESOURCES are required")
    endif()
    usdmotion_member_stage(_stage)
    # RUNTIME is the DLL on Windows, LIBRARY the shared object elsewhere. A
    # plugin is loaded rather than linked, so its import library, if any, stays
    # out of the stage.
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_stage}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${_stage}/lib")
    file(GENERATE
        OUTPUT "${_stage}/${ARG_RESOURCES}/plugInfo.json"
        INPUT "${ARG_PLUG_INFO}")
endfunction()
