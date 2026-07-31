// OBSCURA — launcher.
//
// Deliberately thin. Everything the game does lives in obscura::lib, which is
// what the test suite links; a main() that grew logic of its own would be the
// one part of the project nothing could test. Its whole job is to construct the
// App and hand control to TermForge's loop.
//
// No argument parser, on purpose. A seed and a case index are the only two
// inputs a run has, and until they can actually be handed to something, a CLI
// would be a dependency serving nothing.

#include <cstdlib>
#include <print>

#include <version.hpp>

auto main() -> int {
  // <print> rather than iostream: it is the C++23 spelling, the project already
  // requires C++23 for the library's public headers, and it keeps the iostream
  // static initialisers out of a binary that prints one line.
  std::println("{} {}.{}.{}", PROGRAM_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

  return EXIT_SUCCESS;
}
