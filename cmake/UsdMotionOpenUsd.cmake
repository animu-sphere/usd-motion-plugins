# SPDX-License-Identifier: Apache-2.0
#
# UsdMotionOpenUsd.cmake -- the workspace's OpenUSD pin, enforced in one place.
#
# OpenUSD is 26.08 and nothing else (docs/architecture/DEPENDENCIES.md §1). A
# plugin built against one OpenUSD release is not loadable in another, and
# usd-avatar-runtime composes every animu-sphere plugin into one OpenUSD
# process, so this is the release the rest of the ecosystem pins
# (usd-vrm-plugins' cmake/UsdVrmOpenUsd.cmake).
#
# Every entry point that resolves OpenUSD includes this immediately after its
# `find_package(pxr ...)`: the root project, each library built standalone,
# and the optional plugins/execMotion bundle when it arrives. A component built
# standalone by `ost` never composes the root project, so the pin travels with
# the find_package call, not with the root.
#
# WHY NOT `find_package(pxr 26.08 EXACT ...)`: OpenUSD installs no
# pxrConfigVersion.cmake, so any version argument makes find_package fail with
# "no config version file" whichever OpenUSD is present. pxrConfig.cmake does
# set the version variables directly, and those are what this tests.
#
# The OpenExec probe is a function rather than part of the pin, because
# `plugins/execMotion` is the only member that evaluates anything
# (docs/architecture/DEPENDENCIES.md §1): every library and tool here builds
# against a runtime with no OpenExec at all, and only the bundle calls
# `usdmotion_require_openexec()`. In usd-vrm-plugins, where the whole workspace
# is committed to OpenExec, the same probe runs unconditionally.
#
# Sets, for callers that report build metadata:
#   USDMOTION_OPENUSD_RELEASE   - "26.08"
#
# `usdmotion_require_openexec()` sets, in its caller's scope:
#   USDMOTION_OPENEXEC_COMPONENTS - the exec libraries found, as a list
include_guard(GLOBAL)

# The single supported point. `PXR_VERSION` is OpenUSD's own packed form: 2608
# is 26.08. The display form is built from MINOR/PATCH because OpenUSD's
# PXR_MAJOR_VERSION is 0 -- "0.26.8", the value pxrConfig.cmake publishes, is
# not what anyone calls this release.
set(USDMOTION_OPENUSD_REQUIRED_PXR_VERSION 2608)
set(USDMOTION_OPENUSD_REQUIRED_RELEASE "26.08")

if(NOT pxr_FOUND)
    message(FATAL_ERROR
        "UsdMotionOpenUsd.cmake was included before OpenUSD was resolved. "
        "Include it after find_package(pxr REQUIRED CONFIG).")
endif()

if(NOT DEFINED PXR_VERSION)
    message(FATAL_ERROR
        "This OpenUSD install publishes no PXR_VERSION, so its version cannot "
        "be verified. usd-motion-plugins requires OpenUSD "
        "${USDMOTION_OPENUSD_REQUIRED_RELEASE} exactly.\n"
        "  pxrConfig.cmake: ${pxr_DIR}")
endif()

if(DEFINED PXR_MINOR_VERSION AND DEFINED PXR_PATCH_VERSION)
    # 26 + 8 -> "26.08"; OpenUSD zero-pads the month in every name it uses.
    string(REGEX REPLACE "^([0-9])$" "0\\1" _usdmotion_patch "${PXR_PATCH_VERSION}")
    set(USDMOTION_OPENUSD_RELEASE "${PXR_MINOR_VERSION}.${_usdmotion_patch}")
    unset(_usdmotion_patch)
else()
    set(USDMOTION_OPENUSD_RELEASE "${PXR_VERSION}")
endif()

if(NOT PXR_VERSION EQUAL USDMOTION_OPENUSD_REQUIRED_PXR_VERSION)
    message(FATAL_ERROR
        "Unsupported OpenUSD: found ${USDMOTION_OPENUSD_RELEASE} "
        "(PXR_VERSION ${PXR_VERSION}), require "
        "${USDMOTION_OPENUSD_REQUIRED_RELEASE} "
        "(PXR_VERSION ${USDMOTION_OPENUSD_REQUIRED_PXR_VERSION}) exactly.\n"
        "  pxrConfig.cmake: ${pxr_DIR}\n"
        "OpenUSD guarantees no ABI stability across releases, so a plugin "
        "built against another release could not be loaded beside the rest "
        "of the ecosystem. See docs/architecture/DEPENDENCIES.md.")
endif()

# include_guard(GLOBAL) makes this the only time the line is printed per
# configure, however many members include the module.
message(STATUS
    "OpenUSD ${USDMOTION_OPENUSD_RELEASE} (PXR_VERSION ${PXR_VERSION})")

# ---------------------------------------------------------------------------
# The OpenExec capability probe, for the one member that needs it
# ---------------------------------------------------------------------------
# OpenUSD 26.08 does have a build toggle, `PXR_BUILD_EXEC`, but `build_usd.py`
# exposes no flag for it -- so a runtime built the normal way carries OpenExec
# and this is a detection check rather than a build-option check. A runtime can
# still lack it: `PXR_BUILD_EXEC=OFF`, a slimmed export, a hand-built install
# with components stripped, or any OpenUSD that predates OpenExec. Each
# component is probed by both its imported target (does it link?) and one
# header (is the development half installed?), because `ost` stages those two
# halves separately and a runtime missing one of them fails much later and far
# less clearly.
#
# The list is the one usd-vrm-plugins' 26.08 migration audit measured, not the
# one the exec libraries' names suggest:
#
#   * `ef`, `esf` and `esfUsd` are probed because the *public* exec headers
#     include them -- `exec/system.h` includes `esf/stage.h`,
#     `exec/valueKey.h` includes `esf/object.h`, `exec/requestImpl.h` includes
#     `ef/timeInterval.h`, and `EfTime` is the result type of the builtin
#     `computeTime` computation. Every execMotion translation unit includes
#     `execUsd/system.h`, so a runtime carrying `exec` without these fails at
#     *compile* time inside the bundle: the exact failure this probe exists to
#     move to configure time.
#   * `usdIrImaging` is the imaging-side sentinel rather than `usdExecImaging`.
#     `usdExecImaging` is built whenever `PXR_BUILD_USD_IMAGING=ON` -- with
#     exec off it compiles a stub whose factory returns null, and its target
#     and all its headers still exist -- so its presence carries no information
#     about OpenExec. `usdIrImaging`'s CMakeLists returns early when
#     `PXR_BUILD_EXEC` is off, so its presence does.
#
# Requiring an imaging-side component at all is deliberate: it is what refuses
# `ost`'s `core` runtime leaves, which are built `--no-imaging`.
function(usdmotion_require_openexec)
    set(_probe
        "vdf"             "pxr/exec/vdf/api.h"
        "ef"              "pxr/exec/ef/timeInterval.h"
        "esf"             "pxr/exec/esf/stage.h"
        "esfUsd"          "pxr/exec/esfUsd/sceneAdapter.h"
        "exec"            "pxr/exec/exec/system.h"
        "execGeom"        "pxr/exec/execGeom/tokens.h"
        "execIr"          "pxr/exec/execIr/controller.h"
        "execUsd"         "pxr/exec/execUsd/system.h"
        "usdIrImaging"    "pxr/usdImaging/usdIrImaging/api.h")

    set(_found)
    set(_missing)
    list(LENGTH _probe _length)
    math(EXPR _last "${_length} - 1")
    foreach(_i RANGE 0 ${_last} 2)
        list(GET _probe ${_i} _component)
        math(EXPR _j "${_i} + 1")
        list(GET _probe ${_j} _header)

        # PXR_INCLUDE_DIRS is one directory in every install we ship against,
        # but it is documented as a list, so treat it as one.
        set(_header_found FALSE)
        foreach(_dir IN LISTS PXR_INCLUDE_DIRS)
            if(EXISTS "${_dir}/${_header}")
                set(_header_found TRUE)
                break()
            endif()
        endforeach()

        if(NOT TARGET ${_component})
            list(APPEND _missing "${_component} (no imported CMake target)")
        elseif(NOT _header_found)
            list(APPEND _missing "${_component} (headers absent: ${_header})")
        else()
            list(APPEND _found "${_component}")
        endif()
    endforeach()

    if(_missing)
        string(REPLACE ";" "\n  - " _missing_text "${_missing}")
        message(FATAL_ERROR
            "This OpenUSD ${USDMOTION_OPENUSD_RELEASE} install has no usable "
            "OpenExec. Missing:\n  - ${_missing_text}\n"
            "  OpenUSD prefix: ${PXR_INCLUDE_DIRS}\n"
            "plugins/execMotion is the one member that needs it. Build or "
            "pull an OpenUSD 26.08 that installs the exec libraries and their "
            "headers, or configure with -DUSDMOTION_BUILD_EXEC_MOTION=OFF to "
            "build the rest of the workspace without the bundle.")
    endif()

    string(REPLACE ";" " " _found_text "${_found}")
    message(STATUS "OpenExec: ${_found_text}")
    set(USDMOTION_OPENEXEC_COMPONENTS "${_found}" PARENT_SCOPE)
endfunction()
