# SPDX-License-Identifier: Apache-2.0
#
# UsdMotionInstall.cmake -- what a library or tool installs, written once.
#
# The package model is fixed (docs/architecture/WORKSPACE.md §1): library <P>
# installs as CMake package <P>, exporting the one target <P>::<P>, with its
# config under <libdir>/cmake/<P>. These helpers install exactly that and
# nothing else, so what a consumer finds is predictable from the name alone.
# The installed-consumer lane (tests/installed_consumer) holds every package
# to it from a clean prefix.
include_guard(GLOBAL)

# usdmotion_install_library(TARGET <target> PACKAGE <package>
#                           [COMPATIBILITY <SameMinorVersion|SameMajorVersion>])
#
# Installs, for one library:
#   <bindir>|<libdir>          the library itself
#   <includedir>               the component's include/ tree, as is
#   <libdir>/cmake/<package>/  <package>Config.cmake, from the component's
#                              cmake/<package>Config.cmake.in, which resolves
#                              the package's own dependencies;
#                              <package>ConfigVersion.cmake; and the export,
#                              <package>Targets.cmake, in namespace <package>::
#
# COMPATIBILITY defaults to SameMinorVersion: while a package is 0.x a minor
# release may break its API, so a consumer asking for 0.5 must not be handed
# 0.6 (docs/architecture/DEPENDENCIES.md §2). A package declared stable passes
# SameMajorVersion.
function(usdmotion_install_library)
    cmake_parse_arguments(ARG "" "TARGET;PACKAGE;COMPATIBILITY" "" ${ARGN})
    if(NOT ARG_TARGET OR NOT ARG_PACKAGE)
        message(FATAL_ERROR "usdmotion_install_library: TARGET and PACKAGE are required")
    endif()
    if(NOT ARG_COMPATIBILITY)
        set(ARG_COMPATIBILITY SameMinorVersion)
    endif()
    # Installed after project(), so GNUInstallDirs knows the target architecture.
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(_config_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${ARG_PACKAGE}")

    # The exported name is the package name, so the installed target is
    # <package>::<package>, the same name as the in-tree ALIAS.
    set_target_properties(${ARG_TARGET} PROPERTIES EXPORT_NAME ${ARG_PACKAGE})

    install(TARGETS ${ARG_TARGET}
        EXPORT ${ARG_PACKAGE}Targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${ARG_PACKAGE}Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${ARG_PACKAGE}Config.cmake"
        INSTALL_DESTINATION "${_config_dir}")
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${ARG_PACKAGE}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ${ARG_COMPATIBILITY})
    install(EXPORT ${ARG_PACKAGE}Targets
        FILE ${ARG_PACKAGE}Targets.cmake
        NAMESPACE ${ARG_PACKAGE}::
        DESTINATION "${_config_dir}")
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${ARG_PACKAGE}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${ARG_PACKAGE}ConfigVersion.cmake"
        DESTINATION "${_config_dir}")
endfunction()

# usdmotion_install_tool(<target>)
#
# A command-line tool installs its executable to <bindir> and nothing else. A
# tool's package data (motion_convert's profiles) is installed by whoever owns
# it, beside this call, because its destination is a contract with the tool.
function(usdmotion_install_tool target)
    include(GNUInstallDirs)
    install(TARGETS ${target} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endfunction()
