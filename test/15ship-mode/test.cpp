#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <obscura/cases/case_data.hpp>
#include <obscura/render/glyph_substrate.hpp>
#include <obscura/render/ship.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>

#include <termforge/core/screen.hpp>
#include <termforge/core/types.hpp>

namespace {

using obscura::render::ShipRenderStatus;
using obscura::world::CellRect;
using obscura::world::Compartment;
using obscura::world::Hull;
using obscura::world::Resolution;
using obscura::world::RoomId;
using termforge::Screen;

constexpr termforge::Rgb kForeground{.r = 0xE0, .g = 0xE0, .b = 0xF0};
constexpr termforge::Rgb kBackground{.r = 0x0A, .g = 0x0A, .b = 0x14};

struct CellSnapshot {
  termforge::Cell cell{};
  std::string text{};

  auto operator==(const CellSnapshot&) const -> bool = default;
};

auto snapshot(const Screen& screen) -> std::vector<CellSnapshot> {
  std::vector<CellSnapshot> result{};
  result.reserve(static_cast<std::size_t>(screen.cols() * screen.rows()));
  for (int row = 0; row < screen.rows(); ++row) {
    for (int column = 0; column < screen.cols(); ++column) {
      result.push_back(
          {.cell = screen.at(column, row),
           .text = std::string{screen.text_at(column, row)}});
    }
  }
  return result;
}

auto marked_screen(int columns = 120, int rows = 40) -> Screen {
  Screen screen{columns, rows};
  screen.write_text(0, 0, "sentinel", kForeground, kBackground,
                    termforge::Attr::Reverse);
  return screen;
}

auto add_room(Hull& hull, std::uint16_t column, std::uint16_t row,
              Resolution state = Resolution::unknown) -> RoomId {
  const RoomId id = static_cast<RoomId>(hull.room_count());
  return hull.add_room({
      .id = id,
      .archetype = obscura::world::Archetype::hold,
      .state = state,
      .bounds = {.col = column, .row = row, .width = 22, .height = 9},
  });
}

auto cold_lantern() -> Hull {
  const auto cases = obscura::cases::all();
  REQUIRE(cases.size() > 1);
  return obscura::cases::build(cases[1]).hull;
}

auto logical_hash(const Screen& screen, const termforge::Rect& viewport)
    -> std::uint64_t {
  constexpr std::uint64_t kOffset = 14'695'981'039'346'656'037ULL;
  constexpr std::uint64_t kPrime = 1'099'511'628'211ULL;
  std::uint64_t hash = kOffset;
  for (int row = 0; row < viewport.h; ++row) {
    for (int column = 0; column < viewport.w; ++column) {
      for (const unsigned char byte : screen.text_at(viewport.x + column,
                                                     viewport.y + row)) {
        hash ^= byte;
        hash *= kPrime;
      }
      hash ^= 0xFFU;
      hash *= kPrime;
    }
  }
  return hash;
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

TEST_CASE("SHIP refuses invalid inputs without changing the screen",
          "[render][ship][failure]") {
  const Hull valid = cold_lantern();

  for (const auto& [columns, rows] :
       std::array<std::pair<int, int>, 2>{{{119, 40}, {120, 39}}}) {
    Screen screen = marked_screen(columns, rows);
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(screen, valid, 1, 0).status ==
          ShipRenderStatus::terminal_too_small);
    CHECK(snapshot(screen) == before);
  }

  SECTION("too many compartments wins before their layout is inspected") {
    Hull hull{};
    for (std::uint16_t room = 0; room < 16; ++room) {
      REQUIRE(add_room(hull, 0, 0) == room);
    }
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::too_many_compartments);
    CHECK(snapshot(screen) == before);
  }

  SECTION("invalid cursor") {
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(
              screen, valid, 1, obscura::world::ROOM_ANY)
              .status == ShipRenderStatus::invalid_cursor);
    CHECK(snapshot(screen) == before);
  }

  SECTION("duplicate slots") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0) == 0);
    REQUIRE(add_room(hull, 0, 0) == 1);
    hull.connect(0, 1);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("off-grid bounds") {
    Hull hull{};
    REQUIRE(add_room(hull, 1, 0) == 0);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("wrong room extent") {
    Hull hull{};
    Compartment room{
        .id = 0,
        .archetype = obscura::world::Archetype::hold,
        .state = Resolution::unknown,
        .bounds = {.col = 0, .row = 0, .width = 21, .height = 9},
    };
    REQUIRE(hull.add_room(room) == 0);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("diagonal edges") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0) == 0);
    REQUIRE(add_room(hull, 24, 11) == 1);
    hull.connect(0, 1);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("nonlocal orthogonal edges") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0) == 0);
    REQUIRE(add_room(hull, 48, 0) == 1);
    hull.connect(0, 1);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("disconnected rooms") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0) == 0);
    REQUIRE(add_room(hull, 24, 0) == 1);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("surveyed rooms remain owned by the next renderer ticket") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0, Resolution::surveyed) == 0);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(obscura::render::draw_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::unsupported_resolution);
    CHECK(snapshot(screen) == before);
  }
}

TEST_CASE("SHIP accepts all fifteen canonical slots", "[render][ship][cap]") {
  Hull hull{};
  RoomId previous = obscura::world::ROOM_ANY;
  for (const std::uint16_t row : {std::uint16_t{0}, std::uint16_t{11},
                                  std::uint16_t{22}}) {
    for (const std::uint16_t column : {std::uint16_t{0}, std::uint16_t{24},
                                       std::uint16_t{48}, std::uint16_t{72},
                                       std::uint16_t{96}}) {
      const RoomId room = add_room(hull, column, row);
      REQUIRE(room != obscura::world::ROOM_ANY);
      if (previous != obscura::world::ROOM_ANY) {
        const auto& lhs = hull.all()[previous].bounds;
        const auto& rhs = hull.all()[room].bounds;
        if ((lhs.row == rhs.row && lhs.col + 24 == rhs.col) ||
            (lhs.col == rhs.col && lhs.row + 11 == rhs.row)) {
          hull.connect(previous, room);
        }
      }
      previous = room;
    }
  }
  // Join the end of each band to the next through canonical vertical edges.
  hull.connect(0, 5);
  hull.connect(5, 10);

  Screen screen{120, 40};
  CHECK(obscura::render::draw_ship(screen, hull, 5, 0).status ==
        ShipRenderStatus::drawn);
}

TEST_CASE("Cold Lantern matches the normative 120 by 40 grid",
          "[render][ship][golden]") {
  const Hull hull = cold_lantern();
  Screen screen{120, 40};
  const auto result = obscura::render::draw_ship(screen, hull,
                                                 0xC01D'1A47ULL, 7);
  REQUIRE(result.status == ShipRenderStatus::drawn);
  CHECK(result.viewport == termforge::Rect{.x = 0, .y = 0, .w = 120, .h = 40});

  for (const Compartment& room : hull.all()) {
    const auto distance = hull.distance(7, room.id);
    REQUIRE(distance.has_value());
    const auto expected = obscura::render::compose_glyph_substrate(
        0xC01D'1A47ULL, room, *distance);
    REQUIRE(expected.has_value());
    for (std::size_t row = 0; row < expected->rows.size(); ++row) {
      for (std::size_t column = 0; column < expected->rows[row].size();
           ++column) {
        CHECK(screen.text_at(room.bounds.col + static_cast<int>(column),
                             room.bounds.row + static_cast<int>(row)) ==
              utf8(expected->rows[row][column].glyph));
      }
    }
  }

  for (const Compartment& room : hull.all()) {
    for (const RoomId neighbor : hull.neighbors(room.id)) {
      if (room.id >= neighbor) {
        continue;
      }
      const CellRect& other = hull.all()[neighbor].bounds;
      if (room.bounds.row == other.row) {
        const int left = std::min(room.bounds.col, other.col);
        CHECK(screen.text_at(left + 22, room.bounds.row + 4) == "─");
        CHECK(screen.text_at(left + 23, room.bounds.row + 4) == "─");
      } else {
        const int upper = std::min(room.bounds.row, other.row);
        CHECK(screen.text_at(room.bounds.col + 10, upper + 9) == "│");
        CHECK(screen.text_at(room.bounds.col + 10, upper + 10) == "│");
      }
    }
  }

  constexpr std::array<int, 4> kUpperTrunks{10, 34, 58, 106};
  constexpr std::array<int, 2> kLowerTrunks{58, 82};
  for (const int row : {9, 10}) {
    for (int column = 0; column < 120; ++column) {
      const bool connected =
          std::ranges::find(kUpperTrunks, column) != kUpperTrunks.end();
      CHECK(screen.text_at(column, row) == (connected ? "│" : ""));
    }
  }
  for (const int row : {20, 21}) {
    for (int column = 0; column < 120; ++column) {
      const bool connected =
          std::ranges::find(kLowerTrunks, column) != kLowerTrunks.end();
      CHECK(screen.text_at(column, row) == (connected ? "│" : ""));
    }
  }

  for (int column = 0; column < 120; ++column) {
    CHECK(screen.text_at(column, 33) == "─");
    CHECK(screen.text_at(column, 36) == "─");
  }
  for (const int row : {34, 35}) {
    for (int column = 0; column < 120; ++column) {
      CHECK(screen.text_at(column, row) ==
            (column == 39 || column == 79 ? "│" : ""));
    }
  }
  for (int row = 0; row < 40; ++row) {
    for (int column = 0; column < 120; ++column) {
      CHECK(screen.text_at(column, row) != "/");
      CHECK(screen.text_at(column, row) != "\\");
    }
  }

  CHECK(logical_hash(screen, result.viewport) == 0xE390'D002'15A7'C604ULL);
}

TEST_CASE("SHIP letterboxes without reflow", "[render][ship][letterbox]") {
  const Hull hull = cold_lantern();
  Screen reference{120, 40};
  Screen larger = marked_screen(200, 60);
  REQUIRE(obscura::render::draw_ship(reference, hull, 91, 3).status ==
          ShipRenderStatus::drawn);
  const auto result = obscura::render::draw_ship(larger, hull, 91, 3);
  REQUIRE(result.status == ShipRenderStatus::drawn);
  CHECK(result.viewport ==
        termforge::Rect{.x = 40, .y = 10, .w = 120, .h = 40});

  for (int row = 0; row < larger.rows(); ++row) {
    for (int column = 0; column < larger.cols(); ++column) {
      if (result.viewport.contains(column, row)) {
        const int source_column = column - result.viewport.x;
        const int source_row = row - result.viewport.y;
        CHECK(larger.text_at(column, row) ==
              reference.text_at(source_column, source_row));
      } else {
        CHECK(larger.at(column, row).blank());
      }
    }
  }
}

TEST_CASE("seed and cursor affect substrates but never fixed chrome",
          "[render][ship][determinism]") {
  const Hull hull = cold_lantern();
  Screen first{120, 40};
  Screen second{120, 40};
  REQUIRE(obscura::render::draw_ship(first, hull, 7, 0).status ==
          ShipRenderStatus::drawn);
  REQUIRE(obscura::render::draw_ship(second, hull, 8, 8).status ==
          ShipRenderStatus::drawn);

  std::size_t different_room_cells = 0;
  for (const Compartment& room : hull.all()) {
    for (int row = 0; row < room.bounds.height; ++row) {
      for (int column = 0; column < room.bounds.width; ++column) {
        different_room_cells +=
            first.text_at(room.bounds.col + column, room.bounds.row + row) !=
            second.text_at(room.bounds.col + column, room.bounds.row + row);
      }
    }
  }
  CHECK(different_room_cells > 0);
  for (int column = 0; column < 120; ++column) {
    CHECK(first.text_at(column, 33) == second.text_at(column, 33));
    CHECK(first.text_at(column, 36) == second.text_at(column, 36));
  }
}

} // namespace
