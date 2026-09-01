// Band layout arithmetic. See include/obscura/render/bands.hpp for what belongs
// in this file.

#include <obscura/render/bands.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace obscura::render {

auto lay_out(const std::vector<Band>& bands, std::uint16_t total_rows) -> std::vector<BandRect> {
  std::vector<BandRect> out{};
  out.reserve(bands.size());

  // Pass one: hand every band its minimum, in declaration order, until the
  // terminal runs out. A band that gets nothing still appears in the result with
  // rows == 0 so that callers index the two lists in parallel.
  std::uint32_t used         = 0;
  std::uint32_t total_weight = 0;

  for (const Band& band : bands) {
    const std::uint32_t want  = band.min_rows;
    const std::uint32_t grant = (used + want <= total_rows) ? want : 0;

    out.push_back(BandRect{
        .name = band.name,
        .top  = static_cast<std::uint16_t>(used),
        .rows = static_cast<std::uint16_t>(grant),
    });
    used += grant;

    if (grant > 0) {
      total_weight += band.weight;
    }
  }

  if (total_weight == 0 || used >= total_rows) {
    return out;
  }

  // Pass two: distribute the leftover by weight. Integer division plus an
  // explicit remainder loop, so the assigned rows sum to exactly total_rows —
  // rounding each share independently loses or invents a row depending on the
  // terminal height, and the seam it leaves moves as the window is resized.
  std::uint32_t spare = total_rows - used;
  const std::uint32_t base_pool = spare;

  for (std::size_t index = 0; index < bands.size() && spare > 0; ++index) {
    if (out[index].rows == 0) {
      continue;
    }
    const std::uint32_t share = (base_pool * bands[index].weight) / total_weight;
    const std::uint32_t give  = share < spare ? share : spare;
    out[index].rows = static_cast<std::uint16_t>(out[index].rows + give);
    spare -= give;
  }

  // Whatever integer division left over goes to the first band that took part,
  // one row at a time.
  for (std::size_t index = 0; index < out.size() && spare > 0; ++index) {
    if (out[index].rows == 0 || bands[index].weight == 0) {
      continue;
    }
    out[index].rows = static_cast<std::uint16_t>(out[index].rows + 1);
    --spare;
  }

  // Re-run the tops now that the heights are final. Cheaper and far harder to
  // get wrong than adjusting them incrementally above.
  std::uint32_t top = 0;
  for (BandRect& rect : out) {
    rect.top = static_cast<std::uint16_t>(top);
    top += rect.rows;
  }

  return out;
}

}  // namespace obscura::render
