// Plate dissolve. See include/obscura/render/dissolve.hpp for what belongs in
// this file.

#include <obscura/render/dissolve.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <obscura/render/plates.hpp>

namespace obscura::render {

auto blend_line(std::string_view from, std::string_view to, std::uint8_t step)
    -> std::string {
  if (step == 0) {
    return std::string{from};
  }
  if (step >= kDissolveSteps) {
    return std::string{to};
  }

  const std::size_t width = std::max(from.size(), to.size());

  // A deterministic diagonal rather than a per-character coin flip: character i
  // switches over once step passes its share of the width. Same picture every
  // time, no generator to thread through the renderer, and the seam sweeps left
  // to right, which reads as revealing rather than as noise.
  std::string out{};
  out.reserve(width);

  for (std::size_t index = 0; index < width; ++index) {
    const bool switched = (index * kDissolveSteps) < (step * width);
    const std::string_view source = switched ? to : from;
    out.push_back(index < source.size() ? source[index] : ' ');
  }

  return out;
}

auto blend(const Plate& from, const Plate& to, std::uint8_t step) -> Plate {
  Plate out{};

  const std::size_t height = std::max(from.lines.size(), to.lines.size());
  out.lines.reserve(height);

  static const std::string kEmpty{};

  for (std::size_t index = 0; index < height; ++index) {
    const std::string& lhs =
        index < from.lines.size() ? from.lines[index] : kEmpty;
    const std::string& rhs = index < to.lines.size() ? to.lines[index] : kEmpty;
    out.lines.push_back(blend_line(lhs, rhs, step));
  }

  return out;
}

} // namespace obscura::render
