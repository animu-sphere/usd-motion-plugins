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
# There is no OpenExec probe yet: nothing here evaluates anything until
# execMotion arrives (docs/architecture/DEPENDENCIES.md §1). It will need the
# probe usd-vrm-plugins' cmake/UsdVrmOpenUsd.cmake carries.
#
# Sets, for callers that report build metadata:
#   USDMOTION_OPENUSD_RELEASE   - "26.08"
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
