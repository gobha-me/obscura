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
#include <iostream>

#include <version.hpp>

auto main() -> int {
  // Not <print>, despite it being the C++23 spelling: libstdc++ only ships it
  // from GCC 14, and the declared floor is GCC 13. CI runs ubuntu-24.04 (GCC
  // 13), so <print> builds fine on a developer box with a newer toolchain and
  // fails in CI — the worst shape of portability bug, because local green tells
  // you nothing. Raising the floor to buy one line of syntax is the wrong
  // trade; revisit if GCC 14 becomes the baseline for other reasons.
  //
  // Not std::printf either. PROGRAM_NAME is a std::string_view, and handing one
  // to %s is undefined behaviour — it is neither a char* nor guaranteed
  // null-terminated. %.*s with an explicit length would be correct today and a
  // trap the next time someone adds a field. An ostream just takes the
  // string_view.
  std::cout << PROGRAM_NAME << ' ' << VERSION_MAJOR << '.' << VERSION_MINOR << '.'
            << VERSION_PATCH << '\n';

  return EXIT_SUCCESS;
}
