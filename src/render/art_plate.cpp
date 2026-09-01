#include <obscura/render/art_plate.hpp>

#include <span>

#include <termforge/core/types.hpp>

// GENERATED, and reachable only from here: cmake/embed_asset.cmake writes it
// into the build tree and src/lib/CMakeLists.txt puts that tree on this
// target's PRIVATE include path. A public header that named it would not
// compile for a consumer, which is the point.
#include <obscura/render/hold_d0.hpp>

namespace obscura::render {

auto hold_d0() -> termforge::EncodedImage {
  return termforge::EncodedImage{
      .format = termforge::ImageFormat::Png,
      .bytes = std::as_bytes(std::span{detail::kHoldD0Png}),
      // The plate's own IHDR says 240x160. This says so independently, and
      // test/11art-plate parses the payload to prove the two still agree —
      // the only check there is, since the library never parses a PNG.
      .pixels = termforge::Extent{.w = 240, .h = 160},
  };
}

} // namespace obscura::render
