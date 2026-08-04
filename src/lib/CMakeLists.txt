# ── Library target ──────────────────────────────────────────────────────────
# obscura_lib is the whole game; src/bin/ is a launcher. Everything worth
# testing lives here, which is also why the test suite links it automatically
# (see test/CMakeLists.txt).
#
# Disable with:
#   cmake -B build -Dobscura_BUILD_LIB=OFF
# OFF removes the target outright, so nothing can link it — including the
# auto-discovered tests. Pair it with -Dobscura_TESTS=OFF.
#
# ${PROJECT_NAME}_BUILD_LIB is declared in the ROOT CMakeLists, not here, and
# that move is load-bearing: the dependency list is gated on it, and this
# directory is added after the dependencies have already been resolved.
# Declaring it here left that gate reading an undefined variable.

if (${PROJECT_NAME}_BUILD_LIB)
  # ── Sources ───────────────────────────────────────────────────────────────
  # Globbed by area rather than listed, because the areas ARE the architecture:
  # a new file in src/world/ is a new [SIM] file and is subject to the purity
  # lint wherever it sits in that directory. A hand-maintained list would make
  # "did you remember to add it here" a second, invisible condition for that.
  #
  # CONFIGURE_DEPENDS re-globs when a directory changes, which is what keeps
  # `cmake --build` alone from missing a new file. It is not free — the check
  # runs on every build — and not every generator honours it, so `cmake -B
  # build` after adding a file stays the reliable move.
  #
  # cases/ is in the list and is deliberately NOT under src/: it is authored
  # content that happens to be written in C++, and the top-level directory says
  # so.
  file(GLOB_RECURSE OBSCURA_LIB_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/core/*.cpp
    ${PROJECT_SOURCE_DIR}/src/world/*.cpp
    ${PROJECT_SOURCE_DIR}/src/render/*.cpp
    ${PROJECT_SOURCE_DIR}/src/input/*.cpp
    ${PROJECT_SOURCE_DIR}/src/audio/*.cpp
    ${PROJECT_SOURCE_DIR}/src/replay/*.cpp
    ${PROJECT_SOURCE_DIR}/cases/*.cpp
  )

  if (NOT OBSCURA_LIB_SOURCES)
    message(FATAL_ERROR
      "No library sources found under src/ or cases/. A STATIC library with no "
      "translation units is ill-formed on some generators, and an empty archive "
      "would let every test link against nothing and pass.")
  endif ()

  add_library(${PROJECT_NAME}_lib STATIC
    ${OBSCURA_LIB_SOURCES}
  )

  # Compiled libs get the project's include dir PUBLIC so consumers (bin, tests,
  # downstream) inherit it transitively.
  #
  # The two generator expressions are not decoration. PROJECT_SOURCE_DIR is this
  # project's root; CMAKE_SOURCE_DIR would be the *consumer's* root the moment
  # someone add_subdirectory()s us, silently pointing the include path at the
  # wrong tree. And a bare absolute source path in a PUBLIC include directory
  # makes install(TARGETS ... EXPORT ...) fail outright — an installed package
  # must describe where its headers landed, not where they were built from.
  target_include_directories(${PROJECT_NAME}_lib
    PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  # ── Baked art plates ──────────────────────────────────────────────────────
  # assets/plates/*.png are committed, authored by tools/venice-bake/, and
  # compiled in as constexpr byte arrays. Inside the BUILD_LIB guard on purpose:
  # with the library off there is no translation unit to consume the header.
  #
  # The supported spelling of that configuration is BUILD_LIB=OFF paired with
  # TESTS=OFF, which is what CI exercises. Unpaired, the root CMakeLists drops
  # termforge from the dependency list while test/ still auto-discovers every
  # test directory, so the build dies reaching <termforge/core/types.hpp> — now
  # from include/obscura/render/art_plate.hpp, which reads like the library is
  # broken rather than like two options were mixed. test/CMakeLists.txt says to
  # pair them and says why.
  include(${PROJECT_SOURCE_DIR}/cmake/embed_asset.cmake)

  # The 6081-byte budget is derived, not picked, and it is a property of how a
  # plate is TRANSMITTED rather than of embedding bytes — which is why it is set
  # here and not inside obscura_embed_asset(). kitty base64s the payload (4/3)
  # and adds a first-chunk header plus a few bytes per 4096-byte continuation:
  #
  #     image_transmit = ceil(P/3)*4 + 43 + 9*(ceil(b64/4096) - 1)
  #
  # The cap is 8192 bytes ON THE WIRE, and the wire carries the placement too:
  # a plate at the origin costs another 30 bytes of cursor address and a=p. So
  # the payload has to fit in 8162, not 8192, and the largest P that does is
  # 6081 — NOT the 6105 that transmit alone allows. Getting that wrong makes
  # this gate looser than the assertion it exists to front-run, which is the one
  # way a gate like this can be actively harmful: it waves through the only
  # sizes where it was ever going to be the thing that caught you.
  #
  # 30 is the minimum placement. A plate drawn away from the origin, or the
  # sixteenth one with a two-digit image id, costs a few bytes more — so this is
  # a floor and test/11art-plate, which measures the real total against the
  # meter, remains the authority. It owns that figure; do not restate it here.
  obscura_embed_asset(
    ${PROJECT_SOURCE_DIR}/assets/plates/hold-d0.png
    kHoldD0Png
    ${PROJECT_BINARY_DIR}/generated/obscura/render/hold_d0.hpp
    6081
  )

  # PRIVATE, and that is load-bearing twice over.
  #
  # A build-tree absolute path in a PUBLIC include directory makes
  # install(TARGETS ... EXPORT ...) fail outright — the same trap the
  # $<BUILD_INTERFACE:> above exists to avoid for the source tree. PRIVATE never
  # reaches INTERFACE_INCLUDE_DIRECTORIES, so the export is untouched and the
  # generated header needs no install rule.
  #
  # And it mechanically enforces "no public header names the generated one":
  # include <obscura/render/hold_d0.hpp> from anything under include/obscura/
  # and the first consumer to include that header fails to compile. Only
  # src/render/art_plate.cpp may name it.
  #
  # PROJECT_BINARY_DIR, not CMAKE_BINARY_DIR: the latter is the top-level
  # build's root, which belongs to someone else the moment we are consumed via
  # add_subdirectory().
  target_include_directories(${PROJECT_NAME}_lib
    PRIVATE ${PROJECT_BINARY_DIR}/generated
  )

  # The C++ standard is a property of this library's public headers, not of the
  # build that happens to be compiling it. cmake/toolchain/default.cmake sets
  # CMAKE_CXX_STANDARD for *our* builds, but a consumer uses their own toolchain
  # — without this they would compile include/obscura/*.hpp at whatever their
  # default is.
  target_compile_features(${PROJECT_NAME}_lib PUBLIC cxx_std_23)

  # TermForge, PUBLIC because include/obscura/core/app.hpp includes
  # <termforge/core/app.hpp> and derives from termforge::App. Anything that
  # includes our public API therefore needs TermForge's headers, so the usage
  # requirement has to travel with the target; PRIVATE would compile here and
  # fail in the first test that includes obscura.hpp.
  #
  # This is the link that makes termforge part of what this project exports, and
  # it is why cmake/deps/termforge.cmake sets termforge_INSTALL to match ours and
  # cmake/project-config.cmake.in carries find_dependency(termforge). Both halves
  # are required: one makes the package build, the other makes it consumable.
  target_link_libraries(${PROJECT_NAME}_lib
    PUBLIC termforge::lib
  )

  add_library(${PROJECT_NAME}::lib ALIAS ${PROJECT_NAME}_lib)
endif ()
