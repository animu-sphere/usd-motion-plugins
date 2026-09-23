# SPDX-License-Identifier: Apache-2.0
#
# UsdMotionTargets.cmake -- how a member reaches another member, and the one
# compiler flag every member states with a scope.
include_guard(GLOBAL)

# usdmotion_require_dependency(<package>)
#
# One dependency on another package of this repository, reached the same way
# in all three places a member is built:
#
#   workspace build      the root has already add_subdirectory()'d it, so
#                        `<package>::<package>` is the in-tree ALIAS;
#   standalone member    `ost` (or a plain configure) builds the member alone,
#                        and the package comes from an installed prefix;
#   another repository   the same find_package(), against an installed prefix.
#
# That the in-tree ALIAS and the installed target share one name is the package
# contract (docs/architecture/WORKSPACE.md §1), so the name is not a parameter.
# A macro, because a package's config resolves its own dependencies -- OpenUSD
# among them -- and their variables belong to the caller.
macro(usdmotion_require_dependency package)
    if(NOT TARGET ${package}::${package})
        find_package(${package} CONFIG REQUIRED)
    endif()
endmacro()

# usdmotion_target_utf8(<target> <PUBLIC|PRIVATE>)
#
# MSVC reads source in the *system* code page unless told otherwise, and the
# headers here cite the contracts by their `§` sign -- so a Japanese-Windows
# build warns C4819 on every include without /utf-8. The scope is the caller's
# to state, because it means something: PUBLIC when an installed header carries
# UTF-8, so every consumer that includes it needs the flag too; PRIVATE when
# only this target's own sources do. It is not a genex, so the flag lands in an
# exported package exactly as a plain `if(MSVC)` block wrote it.
function(usdmotion_target_utf8 target scope)
    if(MSVC)
        target_compile_options(${target} ${scope} /utf-8)
    endif()
endfunction()
