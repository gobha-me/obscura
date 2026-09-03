// OBSCURA — launcher.
//
// Deliberately thin. Everything the game does lives in obscura::lib, which is
// what the test suite links; a main() that grew logic of its own would be the
// one part of the project nothing could test. Its whole job is to construct the
// App and hand control to TermForge's loop.
//
// No gameplay argument parser, on purpose. A seed and a case index are the
// only two inputs a run has, and until they can actually be handed to
// something, accepting them would create a contract serving nothing. The
// maintenance-only --version switch remains available to release automation.

#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>

#include <version.hpp>

#if defined(OBSCURA_WITH_APP)
#include <obscura/core/app.hpp>
#endif

namespace {

auto print_version() -> void {
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
  std::cout << PROGRAM_NAME << ' ' << VERSION_MAJOR << '.' << VERSION_MINOR
            << '.' << VERSION_PATCH << '\n';
}

} // namespace

auto main(int argc, char* argv[]) -> int {
  const std::span arguments{argv, static_cast<std::size_t>(argc)};
  if (arguments.size() == 2 &&
      std::string_view{arguments.back()} == "--version") {
    print_version();
    return EXIT_SUCCESS;
  }

  if (argc != 1) {
    std::cerr << "usage: " << PROGRAM_NAME << " [--version]\n";
    return EXIT_FAILURE;
  }

#if defined(OBSCURA_WITH_APP)
  obscura::core::App app;
  return app.run();
#else
  // A library-disabled build has no game App to launch, but remains a runnable
  // and installable artifact as promised by the build option.
  print_version();
  return EXIT_SUCCESS;
#endif
}
