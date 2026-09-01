// The TermForge App subclass. See include/obscura/core/app.hpp for what belongs
// in this file.

#include <obscura/core/app.hpp>

#include <termforge/core/screen.hpp>

namespace obscura::core {

// Defined out of line rather than defaulted in the header: the header would
// otherwise need the complete definition of everything the base class holds by
// pointer, and the compiler would emit the vtable into every translation unit
// that includes it.
App::App() = default;
App::~App() = default;

auto App::on_render(termforge::Screen& screen) -> void {
  // TermForge's loop does not clear the Screen between frames; each widget
  // repaints its own rect. Until there are widgets, owning the whole background
  // is the honest thing to do — a stale frame underneath would read as state
  // that is no longer true, which in this game is a lie rather than a smudge.
  screen.clear();
}

} // namespace obscura::core
