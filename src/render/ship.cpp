// SHIP mode's fixed-grid renderer. See include/obscura/render/ship.hpp.

#include <obscura/render/ship.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <obscura/render/art_plate.hpp>
#include <obscura/render/dissolve.hpp>
#include <obscura/render/glyph_substrate.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>
#include <obscura/world/projection.hpp>

#include <termforge/core/screen.hpp>
#include <termforge/core/types.hpp>

namespace obscura::render {

namespace {

constexpr std::array<std::uint16_t, 5> kRoomColumns{0, 24, 48, 72, 96};
constexpr std::array<std::uint16_t, 3> kRoomRows{0, 11, 22};
constexpr std::uint16_t kRoomWidth = 22;
constexpr std::uint16_t kRoomHeight = 9;
constexpr int kRoomMidRow = 4;
constexpr int kRoomMidColumn = 10;
constexpr int kSootRow = 33;
constexpr int kLedgerRuleRow = 36;
constexpr std::array<int, 2> kLedgerSeparators{39, 79};
constexpr termforge::Rgb kForeground{.r = 0xE0, .g = 0xE0, .b = 0xF0};
constexpr termforge::Rgb kLockedForeground{.r = 0xD8, .g = 0xA8, .b = 0x58};
constexpr termforge::Rgb kBackground{.r = 0x0A, .g = 0x0A, .b = 0x14};
constexpr termforge::Rgb kIntactTint{.r = 0x0E, .g = 0x18, .b = 0x1D};
constexpr termforge::Rgb kDamagedTint{.r = 0x2A, .g = 0x1B, .b = 0x08};
constexpr termforge::Rgb kBreachedTint{.r = 0x2A, .g = 0x0B, .b = 0x10};
constexpr termforge::Rgb kSettledTint{.r = 0x12, .g = 0x16, .b = 0x1A};
constexpr std::array<char32_t, kDissolveGlyphSteps> kDissolveNoise{
    U'+',
    U'#',
    U'%',
    U'?',
};

struct ShipCell {
  char32_t glyph{U' '};
  termforge::Rgb foreground{kForeground};
  termforge::Rgb background{kBackground};
  bool styled{false};
};

using ShipRow = std::array<ShipCell, kShipReferenceColumns>;
using ShipFrame = std::array<ShipRow, kShipReferenceRows>;

struct Marker {
  world::EvidenceId id{world::EVIDENCE_NONE};
  bool locked{false};
};

struct PreparedProjection {
  std::array<std::string, kShipMaximumCompartments> labels{};
  std::array<std::vector<Marker>, kShipMaximumCompartments> markers{};
};

struct PreparedFrame {
  ShipFrame frame{};
  std::array<ShipPlatePlacement, kShipMaximumCompartments> plates{};
  std::size_t plate_count{0};
};

struct Point {
  constexpr Point(int x_value, int y_value) : x{x_value}, y{y_value} {}

  int x{};
  int y{};
};

constexpr std::array<std::array<Point, kShipMaximumEvidenceMarkers>,
                     kShipMaximumEvidenceMarkers>
    kMarkerLayouts{{
        {{{10, 2}, {0, 0}, {0, 0}, {0, 0}}},
        {{{7, 2}, {14, 6}, {0, 0}, {0, 0}}},
        {{{4, 2}, {17, 2}, {7, 6}, {0, 0}}},
        {{{4, 2}, {17, 2}, {7, 6}, {14, 6}}},
    }};

auto index_of(std::span<const std::uint16_t> values, std::uint16_t value)
    -> std::optional<std::size_t> {
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (values[index] == value) {
      return index;
    }
  }
  return std::nullopt;
}

auto slot_of(const world::CellRect& bounds) -> std::optional<std::size_t> {
  if (bounds.width != kRoomWidth || bounds.height != kRoomHeight) {
    return std::nullopt;
  }
  const auto column = index_of(kRoomColumns, bounds.col);
  const auto row = index_of(kRoomRows, bounds.row);
  if (!column.has_value() || !row.has_value()) {
    return std::nullopt;
  }
  return (*row * kRoomColumns.size()) + *column;
}

auto edge_is_drawable(const world::CellRect& lhs, const world::CellRect& rhs)
    -> bool {
  const auto column_difference =
      lhs.col > rhs.col ? lhs.col - rhs.col : rhs.col - lhs.col;
  const auto row_difference =
      lhs.row > rhs.row ? lhs.row - rhs.row : rhs.row - lhs.row;
  return (lhs.row == rhs.row && column_difference == 24) ||
         (lhs.col == rhs.col && row_difference == 11);
}

auto damage_suffix(world::Damage damage) -> std::optional<std::string_view> {
  switch (damage) {
    case world::Damage::intact: return std::string_view{};
    case world::Damage::damaged: return "DMG";
    case world::Damage::breached: return "BREACH";
  }
  return std::nullopt;
}

auto damage_tint(world::Damage damage) -> std::optional<termforge::Rgb> {
  switch (damage) {
    case world::Damage::intact: return kIntactTint;
    case world::Damage::damaged: return kDamagedTint;
    case world::Damage::breached: return kBreachedTint;
  }
  return std::nullopt;
}

auto normalized_label(std::string_view label) -> std::optional<std::string> {
  if (label.empty() || label.size() > 11) {
    return std::nullopt;
  }
  std::string result{};
  result.reserve(label.size());
  for (const unsigned char byte : label) {
    if (byte < 0x20U || byte > 0x7EU) {
      return std::nullopt;
    }
    if (byte >= static_cast<unsigned char>('a') &&
        byte <= static_cast<unsigned char>('z')) {
      result.push_back(static_cast<char>(byte - 'a' + 'A'));
    } else {
      result.push_back(static_cast<char>(byte));
    }
  }
  return result;
}

auto room_header_fits(std::string_view label, world::Damage damage) -> bool {
  const auto suffix = damage_suffix(damage);
  if (!suffix.has_value()) {
    return false;
  }
  const std::size_t label_end = 2U + label.size() + 4U;
  const std::size_t suffix_start = 20U - suffix->size();
  return suffix->empty() || label_end < suffix_start;
}

auto validate_rooms(const world::Hull& hull) -> ShipRenderStatus {
  std::array<bool, kShipMaximumCompartments> occupied{};
  const auto& rooms = hull.all();
  for (std::size_t index = 0; index < rooms.size(); ++index) {
    const world::Compartment& room = rooms[index];
    if (room.id != index) {
      return ShipRenderStatus::invalid_layout;
    }
    if (room.state == world::Resolution::resolved &&
        !art_plate_for(room.archetype, room.damage).has_value()) {
      return ShipRenderStatus::unsupported_resolution;
    }
    if (room.state != world::Resolution::unknown &&
        room.state != world::Resolution::surveyed &&
        room.state != world::Resolution::resolved) {
      return ShipRenderStatus::unsupported_resolution;
    }
    if (!damage_tint(room.damage).has_value()) {
      return ShipRenderStatus::invalid_layout;
    }
    const auto slot = slot_of(room.bounds);
    if (!slot.has_value() || occupied.at(*slot)) {
      return ShipRenderStatus::invalid_layout;
    }
    occupied.at(*slot) = true;
  }
  return ShipRenderStatus::drawn;
}

auto validate_edges(const world::Hull& hull) -> bool {
  const auto& rooms = hull.all();
  for (const world::Compartment& room : rooms) {
    for (const world::RoomId neighbor : hull.neighbors(room.id)) {
      if (neighbor >= rooms.size() ||
          !edge_is_drawable(room.bounds, rooms[neighbor].bounds)) {
        return false;
      }
    }
  }
  return true;
}

auto prepare_projection(const ShipRenderInput& input,
                        PreparedProjection& prepared) -> ShipRenderStatus {
  const auto& rooms = input.hull.get().all();
  if (input.room_labels.size() != rooms.size()) {
    return ShipRenderStatus::invalid_projection;
  }
  for (std::size_t index = 0; index < input.room_labels.size(); ++index) {
    const ShipRoomLabel& source = input.room_labels[index];
    if (source.id != index) {
      return ShipRenderStatus::invalid_projection;
    }
    const auto label = normalized_label(source.text);
    if (!label.has_value() || !room_header_fits(*label, rooms[index].damage)) {
      return ShipRenderStatus::invalid_projection;
    }
    prepared.labels.at(index) = *label;
  }

  std::vector<world::EvidenceId> seen{};
  seen.reserve(input.evidence.size());
  for (const world::EvidenceProjection& item : input.evidence) {
    if (item.id == world::EVIDENCE_NONE ||
        std::ranges::find(seen, item.id) != seen.end()) {
      return ShipRenderStatus::invalid_projection;
    }
    seen.push_back(item.id);
    if (item.location == world::ROOM_ANY) {
      continue;
    }
    if (item.location >= rooms.size() ||
        rooms[item.location].state == world::Resolution::unknown) {
      return ShipRenderStatus::invalid_projection;
    }
    auto& markers = prepared.markers.at(item.location);
    markers.push_back({
        .id = item.id,
        .locked = (item.requires_ & input.instruments) != item.requires_,
    });
    if (markers.size() > kShipMaximumEvidenceMarkers) {
      return ShipRenderStatus::too_many_evidence_markers;
    }
  }

  for (auto& markers : prepared.markers) {
    std::ranges::sort(markers, {}, &Marker::id);
  }
  return ShipRenderStatus::drawn;
}

auto validate_ship(const ShipRenderInput& input, PreparedProjection& prepared)
    -> ShipRenderStatus {
  const world::Hull& hull = input.hull.get();
  if (hull.room_count() > kShipMaximumCompartments) {
    return ShipRenderStatus::too_many_compartments;
  }
  if (input.cursor >= hull.room_count()) {
    return ShipRenderStatus::invalid_cursor;
  }
  const ShipRenderStatus rooms = validate_rooms(hull);
  if (rooms != ShipRenderStatus::drawn) {
    return rooms;
  }
  if (!validate_edges(hull)) {
    return ShipRenderStatus::invalid_layout;
  }
  if (input.dissolve.has_value()) {
    const ShipDissolveInput& dissolve = *input.dissolve;
    if (dissolve.room >= hull.room_count() ||
        hull.all()[dissolve.room].state != world::Resolution::resolved ||
        dissolve.visual.reveal_frame >= kDissolveRevealSteps ||
        dissolve.visual.glyph_strata > kDissolveGlyphSteps) {
      return ShipRenderStatus::invalid_projection;
    }
  }
  for (const world::Compartment& room : hull.all()) {
    if (!hull.distance(input.cursor, room.id).has_value()) {
      return ShipRenderStatus::invalid_layout;
    }
  }
  return prepare_projection(input, prepared);
}

auto set_cell(ShipFrame& frame, int x, int y, char32_t glyph,
              termforge::Rgb foreground = kForeground,
              termforge::Rgb background = kBackground) -> void {
  frame.at(static_cast<std::size_t>(y)).at(static_cast<std::size_t>(x)) = {
      .glyph = glyph,
      .foreground = foreground,
      .background = background,
      .styled = true,
  };
}

auto draw_ascii(ShipFrame& frame, int x, int y, std::string_view text,
                termforge::Rgb foreground = kForeground,
                termforge::Rgb background = kBackground) -> void {
  for (const unsigned char byte : text) {
    set_cell(frame, x, y, static_cast<char32_t>(byte), foreground, background);
    ++x;
  }
}

auto draw_u32(ShipFrame& frame, int x, int y, std::u32string_view text,
              termforge::Rgb foreground = kForeground,
              termforge::Rgb background = kBackground) -> void {
  for (const char32_t glyph : text) {
    set_cell(frame, x, y, glyph, foreground, background);
    ++x;
  }
}

auto draw_edge(ShipFrame& frame, const world::CellRect& lhs,
               const world::CellRect& rhs) -> void {
  if (lhs.row == rhs.row) {
    const std::uint16_t left = lhs.col < rhs.col ? lhs.col : rhs.col;
    set_cell(frame, static_cast<int>(left) + kRoomWidth, lhs.row + kRoomMidRow,
             U'─');
    set_cell(frame, static_cast<int>(left) + kRoomWidth + 1,
             lhs.row + kRoomMidRow, U'─');
    return;
  }

  const std::uint16_t upper = lhs.row < rhs.row ? lhs.row : rhs.row;
  set_cell(frame, lhs.col + kRoomMidColumn,
           static_cast<int>(upper) + kRoomHeight, U'│');
  set_cell(frame, lhs.col + kRoomMidColumn,
           static_cast<int>(upper) + kRoomHeight + 1, U'│');
}

auto draw_surveyed(ShipFrame& frame, const world::Compartment& room,
                   std::string_view label, std::span<const Marker> markers)
    -> bool {
  const auto tint = damage_tint(room.damage);
  const auto suffix = damage_suffix(room.damage);
  if (!tint.has_value() || !suffix.has_value()) {
    return false;
  }
  const int origin_x = room.bounds.col;
  const int origin_y = room.bounds.row;

  for (int y = 1; y < kRoomHeight - 1; ++y) {
    for (int x = 1; x < kRoomWidth - 1; ++x) {
      set_cell(frame, origin_x + x, origin_y + y, U' ', kForeground, *tint);
    }
  }
  for (int x = 1; x < kRoomWidth - 1; ++x) {
    set_cell(frame, origin_x + x, origin_y, U'─');
    set_cell(frame, origin_x + x, origin_y + kRoomHeight - 1, U'─');
  }
  for (int y = 1; y < kRoomHeight - 1; ++y) {
    set_cell(frame, origin_x, origin_y + y, U'│');
    set_cell(frame, origin_x + kRoomWidth - 1, origin_y + y, U'│');
  }
  set_cell(frame, origin_x, origin_y, U'┌');
  set_cell(frame, origin_x + kRoomWidth - 1, origin_y, U'┐');
  set_cell(frame, origin_x, origin_y + kRoomHeight - 1, U'└');
  set_cell(frame, origin_x + kRoomWidth - 1, origin_y + kRoomHeight - 1, U'┘');

  draw_ascii(frame, origin_x + 2, origin_y, "[ " + std::string{label} + " ]");
  if (!suffix->empty()) {
    draw_ascii(frame, origin_x + 20 - static_cast<int>(suffix->size()),
               origin_y, *suffix);
  }

  for (int x = 3; x <= 18; ++x) {
    set_cell(frame, origin_x + x, origin_y + 3, U'─', kForeground, *tint);
    set_cell(frame, origin_x + x, origin_y + 5, U'─', kForeground, *tint);
  }
  set_cell(frame, origin_x + 2, origin_y + 3, U'┌', kForeground, *tint);
  set_cell(frame, origin_x + 19, origin_y + 3, U'┐', kForeground, *tint);
  set_cell(frame, origin_x + 2, origin_y + 4, U'│', kForeground, *tint);
  set_cell(frame, origin_x + 19, origin_y + 4, U'│', kForeground, *tint);
  set_cell(frame, origin_x + 2, origin_y + 5, U'└', kForeground, *tint);
  set_cell(frame, origin_x + 19, origin_y + 5, U'┘', kForeground, *tint);

  const std::size_t locked = std::ranges::count(markers, true, &Marker::locked);
  std::u32string summary{};
  const std::string total_text = std::to_string(markers.size()) + " EVID";
  for (const unsigned char byte : total_text) {
    summary.push_back(static_cast<char32_t>(byte));
  }
  if (locked != 0) {
    summary += U" · ";
    const std::string locked_text = std::to_string(locked) + " LOCK";
    for (const unsigned char byte : locked_text) {
      summary.push_back(static_cast<char32_t>(byte));
    }
  }
  const int summary_x =
      origin_x + 3 + static_cast<int>((16U - summary.size()) / 2U);
  draw_u32(frame, summary_x, origin_y + 4, summary, kForeground, *tint);

  if (!markers.empty()) {
    const auto& layout = kMarkerLayouts.at(markers.size() - 1U);
    for (std::size_t index = 0; index < markers.size(); ++index) {
      const Marker& marker = markers[index];
      set_cell(frame, origin_x + layout.at(index).x,
               origin_y + layout.at(index).y, marker.locked ? U'◇' : U'◆',
               marker.locked ? kLockedForeground : kForeground, *tint);
    }
  }
  return true;
}

auto draw_resolved(PreparedFrame& prepared, const world::Compartment& room,
                   std::string_view label, DissolveVisual visual) -> bool {
  const auto damage = damage_tint(room.damage);
  if (!damage.has_value()) {
    return false;
  }
  const termforge::Rgb tint = visual.damage_tint ? *damage : kSettledTint;
  const int origin_x = room.bounds.col;
  const int origin_y = room.bounds.row;

  for (int y = 1; y < kRoomHeight - 1; ++y) {
    for (int x = 1; x < kRoomWidth - 1; ++x) {
      set_cell(prepared.frame, origin_x + x, origin_y + y, U' ', kForeground,
               tint);
    }
  }
  for (int x = 1; x < kRoomWidth - 1; ++x) {
    set_cell(prepared.frame, origin_x + x, origin_y, U'═');
    set_cell(prepared.frame, origin_x + x, origin_y + kRoomHeight - 1, U'═');
  }
  for (int y = 1; y < kRoomHeight - 1; ++y) {
    set_cell(prepared.frame, origin_x, origin_y + y, U'║');
    set_cell(prepared.frame, origin_x + kRoomWidth - 1, origin_y + y, U'║');
  }
  set_cell(prepared.frame, origin_x, origin_y, U'╔');
  set_cell(prepared.frame, origin_x + kRoomWidth - 1, origin_y, U'╗');
  set_cell(prepared.frame, origin_x, origin_y + kRoomHeight - 1, U'╚');
  set_cell(prepared.frame, origin_x + kRoomWidth - 1,
           origin_y + kRoomHeight - 1, U'╝');
  draw_ascii(prepared.frame, origin_x + 2, origin_y,
             "[ " + std::string{label} + " ]");

  for (int y = 0; y < kRoomHeight - 2; ++y) {
    for (int x = 0; x < kRoomWidth - 2; ++x) {
      const auto stratum = static_cast<std::size_t>((x * 7 + y * 11) % 4);
      if (stratum >= visual.glyph_strata) {
        continue;
      }
      set_cell(prepared.frame, origin_x + x + 1, origin_y + y + 1,
               kDissolveNoise.at(stratum), kForeground, tint);
    }
  }

  prepared.plates.at(prepared.plate_count) = {
      .room = room.id,
      .cells = {.x = origin_x + 1,
                .y = origin_y + 1,
                .w = kRoomWidth - 2,
                .h = kRoomHeight - 2},
      .reveal_frame = visual.reveal_frame,
  };
  ++prepared.plate_count;
  return true;
}

auto draw_unknown(ShipFrame& frame, const world::Hull& hull,
                  const world::Compartment& room, std::uint64_t seed,
                  world::RoomId cursor) -> bool {
  const auto distance = hull.distance(cursor, room.id);
  if (!distance.has_value()) {
    return false;
  }
  const auto substrate = compose_glyph_substrate(seed, room, *distance);
  if (!substrate.has_value()) {
    return false;
  }
  for (std::size_t row = 0; row < kGlyphSubstrateHeight; ++row) {
    for (std::size_t column = 0; column < kGlyphSubstrateWidth; ++column) {
      set_cell(frame, room.bounds.col + static_cast<int>(column),
               room.bounds.row + static_cast<int>(row),
               substrate->rows.at(row).at(column).glyph);
    }
  }
  return true;
}

auto draw_rooms(PreparedFrame& frame, const ShipRenderInput& input,
                const PreparedProjection& projection) -> bool {
  const world::Hull& hull = input.hull.get();
  for (const world::Compartment& room : hull.all()) {
    if (room.state == world::Resolution::unknown) {
      if (!draw_unknown(frame.frame, hull, room, input.seed, input.cursor)) {
        return false;
      }
    } else if (room.state == world::Resolution::surveyed) {
      if (!draw_surveyed(frame.frame, room, projection.labels.at(room.id),
                         projection.markers.at(room.id))) {
        return false;
      }
    } else {
      DissolveVisual visual = dissolve_visual(kDissolveSteps - 1);
      if (input.dissolve.has_value() && input.dissolve->room == room.id) {
        visual = input.dissolve->visual;
      }
      if (!draw_resolved(frame, room, projection.labels.at(room.id), visual)) {
        return false;
      }
    }
  }
  return true;
}

auto draw_edges(ShipFrame& frame, const world::Hull& hull) -> void {
  for (const world::Compartment& room : hull.all()) {
    for (const world::RoomId neighbor : hull.neighbors(room.id)) {
      if (room.id < neighbor) {
        draw_edge(frame, room.bounds, hull.all().at(neighbor).bounds);
      }
    }
  }
}

auto draw_chrome(ShipFrame& frame) -> void {
  for (int column = 0; column < kShipReferenceColumns; ++column) {
    set_cell(frame, column, kSootRow, U'─');
    set_cell(frame, column, kLedgerRuleRow, U'─');
  }
  for (const int column : kLedgerSeparators) {
    set_cell(frame, column, 34, U'│');
    set_cell(frame, column, 35, U'│');
  }
}

auto compose_frame(const ShipRenderInput& input,
                   const PreparedProjection& prepared)
    -> std::optional<PreparedFrame> {
  PreparedFrame frame{};
  if (!draw_rooms(frame, input, prepared)) {
    return std::nullopt;
  }
  draw_edges(frame.frame, input.hull.get());
  draw_chrome(frame.frame);
  return frame;
}

auto utf8(char32_t codepoint) -> std::string {
  std::string result{};
  if (codepoint <= 0x7FU) {
    result.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FFU) {
    result.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
    result.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
  } else {
    result.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
    result.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
    result.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
  }
  return result;
}

auto paint(termforge::Screen& screen, const ShipFrame& frame,
           const termforge::Rect& viewport) -> void {
  screen.clear();
  int row_offset = 0;
  for (const ShipRow& row : frame) {
    std::size_t column = 0;
    while (column < row.size()) {
      const ShipCell& cell = row.at(column);
      if (!cell.styled || cell.background == kBackground) {
        ++column;
        continue;
      }
      const std::size_t first = column;
      while (column < row.size() && row.at(column).styled &&
             row.at(column).background == cell.background) {
        ++column;
      }
      screen.fill_rect(
          viewport.x + static_cast<int>(first), viewport.y + row_offset,
          static_cast<int>(column - first), 1, kForeground, cell.background);
    }
    ++row_offset;
  }

  row_offset = 0;
  for (const ShipRow& row : frame) {
    for (std::size_t column = 0; column < row.size(); ++column) {
      const ShipCell& cell = row.at(column);
      if (!cell.styled || cell.glyph == U' ') {
        continue;
      }
      const std::string glyph = utf8(cell.glyph);
      screen.write_text(viewport.x + static_cast<int>(column),
                        viewport.y + row_offset, glyph, cell.foreground,
                        cell.background);
    }
    ++row_offset;
  }
}

} // namespace

auto draw_ship(termforge::Screen& screen, const ShipRenderInput& input)
    -> ShipRenderResult {
  if (screen.cols() < kShipReferenceColumns ||
      screen.rows() < kShipReferenceRows) {
    return {.status = ShipRenderStatus::terminal_too_small};
  }
  PreparedProjection prepared{};
  const ShipRenderStatus status = validate_ship(input, prepared);
  if (status != ShipRenderStatus::drawn) {
    return {.status = status};
  }
  const auto frame = compose_frame(input, prepared);
  if (!frame.has_value()) {
    return {.status = ShipRenderStatus::invalid_layout};
  }

  const termforge::Rect viewport{
      .x = (screen.cols() - kShipReferenceColumns) / 2,
      .y = (screen.rows() - kShipReferenceRows) / 2,
      .w = kShipReferenceColumns,
      .h = kShipReferenceRows,
  };
  paint(screen, frame->frame, viewport);
  ShipRenderResult result{.status = ShipRenderStatus::drawn,
                          .viewport = viewport,
                          .plates = frame->plates,
                          .plate_count = frame->plate_count};
  for (std::size_t index = 0; index < result.plate_count; ++index) {
    result.plates.at(index).cells.x += viewport.x;
    result.plates.at(index).cells.y += viewport.y;
  }
  return result;
}

} // namespace obscura::render
