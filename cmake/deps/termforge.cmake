# TermForge — the terminal UI toolkit OBSCURA renders through.
#
# Follows the shape documented in cmake/deps/catch2.cmake (the annotated recipe):
#   1. find_package(termforge <version> QUIET) — prefer a compatible installed copy.
#   2. On miss, FetchContent fallback with overridable, pinned coordinates.
#   3. Decide the dependency's install rules by where it is linked.
#
# The consumed target is termforge::lib. TermForge sets EXPORT_NAME lib on
# termforge_lib, so that one spelling is correct for all three acquisition
# modes (add_subdirectory, FetchContent, installed find_package) — nothing here
# has to branch on how it was obtained.
#
# ── Step 3: this one is linked into OUR library ─────────────────────────────
# src/lib/CMakeLists.txt links termforge::lib into ${PROJECT_NAME}_lib, which is
# exported by cmake/install.cmake. That is the case that needs the *tracking*
# form below rather than a fixed OFF:
#
#   * A fixed OFF would put termforge's target in no install export set, so our
#     own install(EXPORT) could not resolve the reference to it and generation
#     would stop.
#   * Leaving it at TermForge's own default (PROJECT_IS_TOP_LEVEL, i.e. OFF for
#     a fetched subproject) is the same failure by a different route.
#   * Forcing it ON unconditionally would make a project that consumes OBSCURA
#     deposit termforge's headers, archive and package config into *its* prefix.
#
# Tracking ${PROJECT_NAME}_INSTALL is the only value that is right in both
# directions: ON while we are top level and generating our own export set, OFF
# while we are somebody else's subproject and installing nothing.
#
# The other half of this lives in cmake/project-config.cmake.in, which carries
# the matching find_dependency(termforge). This half makes the package build;
# that half makes it consumable.
#
# CMP0077 is not a concern here: TermForge declares cmake_minimum_required
# 3.28, well above the 3.13 floor where `set()` before `option()` starts being
# honoured, so the value below actually reaches its option().

# This is both the oldest package we may consume and the version our fallback
# fetches.  Keep it available to cmake/project-config.cmake.in: our public App
# header derives from termforge::App, so an installed obscura archive and the
# TermForge headers its consumer sees must agree on this pre-1.0 ABI line.
if (NOT TERMFORGE_REQUIRED_VERSION)
    set(TERMFORGE_REQUIRED_VERSION 0.57.20)
endif()

find_package(termforge ${TERMFORGE_REQUIRED_VERSION} QUIET)

if (termforge_FOUND)
else ()
    if (NOT TERMFORGE_URI)
        set(TERMFORGE_URI https://github.com/gobha-me/termforge.git)
    endif()

    if (NOT TERMFORGE_TAG)
        set(TERMFORGE_TAG v${TERMFORGE_REQUIRED_VERSION})
    endif()

    # Match our own install option — see the long note above. Not a fixed OFF.
    set(termforge_INSTALL ${${PROJECT_NAME}_INSTALL})

    # We want the library only. TermForge defaults these to PROJECT_IS_TOP_LEVEL,
    # which is already OFF for a fetched subproject; stating them keeps a stray
    # -D on the command line from pulling TermForge's demo binary, examples and
    # Catch2-backed suite into our build.
    set(termforge_TESTS    OFF)
    set(termforge_EXAMPLES OFF)
    set(termforge_BIN      OFF)

    include(FetchContent)
    FetchContent_Declare(
        termforge
        GIT_REPOSITORY ${TERMFORGE_URI}
        GIT_TAG ${TERMFORGE_TAG}
        # SOURCE_DIR is pinned because TermForge hardcodes its project name and
        # would otherwise be fine — but our own project name comes from the
        # directory, so keeping the fetch layout explicit here documents the
        # convention for the next dependency, which may not hardcode it.
        SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/termforge
    )

    FetchContent_MakeAvailable(termforge)
endif()
