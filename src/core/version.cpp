// The one definition behind include/obscura/obscura.hpp's declaration.
//
// It exists to make linking the library observable. Everything else in the
// public API could in principle drift into headers; a test that only used
// constexpr data would then compile and pass with the archive missing, and the
// build's own wiring would go unproved. This function cannot: the declaration
// is in the header, the definition is here, and a test that calls it links or
// does not.

#include <obscura/obscura.hpp>

#include <version.hpp>

namespace obscura {

auto version_string() -> const char* {
  // PROGRAM_NAME is a constexpr std::string_view over a string literal from the
  // generated header, so .data() is NUL-terminated and outlives every caller.
  return PROGRAM_NAME.data();
}

} // namespace obscura
