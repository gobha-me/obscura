// SHIP mode's fixed-grid renderer. See include/obscura/render/ship.hpp.

#include <obscura/render/ship.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <obscura/render/glyph_substrate.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>

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
constexpr termforge::Rgb kBackground{.r = 0x0A, .g = 0x0A, .b = 0x14};

using ShipRow = std::array<char32_t, kShipReferenceColumns>;
using ShipFrame = std::array<ShipRow, kShipReferenceRows>;

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

auto edge_is_drawable(const world::CellRect& lhs,
                      const world::CellRect& rhs) -> bool {
  const auto column_difference = lhs.col > rhs.col ? lhs.col - rhs.col
                                                    : rhs.col - lhs.col;
  const auto row_difference =
      lhs.row > rhs.row ? lhs.row - rhs.row : rhs.row - lhs.row;
  return (lhs.row == rhs.row && column_difference == 24) ||
         (lhs.col == rhs.col && row_difference == 11);
}

auto validate_rooms(const world::Hull& hull) -> ShipRenderStatus {
  std::array<bool, kShipMaximumCompartments> occupied{};
  const auto& rooms = hull.all();
  for (std::size_t index = 0; index < rooms.size(); ++index) {
    const world::Compartment& room = rooms[index];
    if (room.id != index) {
      return ShipRenderStatus::invalid_layout;
    }
    if (room.state != world::Resolution::unknown) {
      return ShipRenderStatus::unsupported_resolution;
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

auto validate_ship(const world::Hull& hull, world::RoomId cursor)
    -> ShipRenderStatus {
  if (hull.room_count() > kShipMaximumCompartments) {
    return ShipRenderStatus::too_many_compartments;
  }
  if (cursor >= hull.room_count()) {
    return ShipRenderStatus::invalid_cursor;
  }
  const ShipRenderStatus rooms = validate_rooms(hull);
  if (rooms != ShipRenderStatus::drawn) {
    return rooms;
  }
  if (!validate_edges(hull)) {
    return ShipRenderStatus::invalid_layout;
  }
  for (const world::Compartment& room : hull.all()) {
    if (!hull.distance(cursor, room.id).has_value()) {
      return ShipRenderStatus::invalid_layout;
    }
  }
  return ShipRenderStatus::drawn;
}

auto draw_edge(ShipFrame& frame, const world::CellRect& lhs,
               const world::CellRect& rhs) -> void {
  if (lhs.row == rhs.row) {
    const std::uint16_t left = lhs.col < rhs.col ? lhs.col : rhs.col;
    frame.at(lhs.row + kRoomMidRow).at(left + kRoomWidth) = U'─';
    frame.at(lhs.row + kRoomMidRow).at(left + kRoomWidth + 1U) = U'─';
    return;
  }

  const std::uint16_t upper = lhs.row < rhs.row ? lhs.row : rhs.row;
  frame.at(upper + kRoomHeight).at(lhs.col + kRoomMidColumn) = U'│';
  frame.at(upper + kRoomHeight + 1U).at(lhs.col + kRoomMidColumn) = U'│';
}

auto compose_frame(const world::Hull& hull, std::uint64_t seed,
                   world::RoomId cursor) -> std::optional<ShipFrame> {
  ShipFrame frame{};
  for (ShipRow& row : frame) {
    row.fill(U' ');
  }

  for (const world::Compartment& room : hull.all()) {
    const auto distance = hull.distance(cursor, room.id);
    if (!distance.has_value()) {
      return std::nullopt;
    }
    const auto substrate = compose_glyph_substrate(seed, room, *distance);
    if (!substrate.has_value()) {
      return std::nullopt;
    }
    for (std::size_t row = 0; row < kGlyphSubstrateHeight; ++row) {
      for (std::size_t column = 0; column < kGlyphSubstrateWidth; ++column) {
        frame.at(room.bounds.row + row).at(room.bounds.col + column) =
            substrate->rows.at(row).at(column).glyph;
      }
    }
  }

  for (const world::Compartment& room : hull.all()) {
    for (const world::RoomId neighbor : hull.neighbors(room.id)) {
      if (room.id < neighbor) {
        draw_edge(frame, room.bounds, hull.all()[neighbor].bounds);
      }
    }
  }

  frame.at(kSootRow).fill(U'─');
  frame.at(kLedgerRuleRow).fill(U'─');
  for (const int column : kLedgerSeparators) {
    frame.at(34).at(column) = U'│';
    frame.at(35).at(column) = U'│';
  }
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
  for (std::size_t row = 0; row < frame.size(); ++row) {
    for (std::size_t column = 0; column < frame.at(row).size(); ++column) {
      if (frame.at(row).at(column) == U' ') {
        continue;
      }
      const std::string glyph = utf8(frame.at(row).at(column));
      screen.write_text(viewport.x + static_cast<int>(column),
                        viewport.y + static_cast<int>(row), glyph, kForeground,
                        kBackground);
    }
  }
}

} // namespace

auto draw_ship(termforge::Screen& screen, const world::Hull& hull,
               std::uint64_t seed, world::RoomId cursor) -> ShipRenderResult {
  if (screen.cols() < kShipReferenceColumns ||
      screen.rows() < kShipReferenceRows) {
    return {.status = ShipRenderStatus::terminal_too_small};
  }
  const ShipRenderStatus status = validate_ship(hull, cursor);
  if (status != ShipRenderStatus::drawn) {
    return {.status = status};
  }
  const auto frame = compose_frame(hull, seed, cursor);
  if (!frame.has_value()) {
    return {.status = ShipRenderStatus::invalid_layout};
  }

  const termforge::Rect viewport{
      .x = (screen.cols() - kShipReferenceColumns) / 2,
      .y = (screen.rows() - kShipReferenceRows) / 2,
      .w = kShipReferenceColumns,
      .h = kShipReferenceRows,
  };
  paint(screen, *frame, viewport);
  return {.status = ShipRenderStatus::drawn, .viewport = viewport};
}

} // namespace obscura::render
