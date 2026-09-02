#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <obscura/cases/case_data.hpp>
#include <obscura/render/art_plate.hpp>
#include <obscura/render/glyph_substrate.hpp>
#include <obscura/render/ship.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>
#include <obscura/world/projection.hpp>

#include <termforge/core/renderer.hpp>
#include <termforge/core/screen.hpp>
#include <termforge/core/types.hpp>
#include <termforge/drivers/kitty_driver.hpp>

namespace {

using obscura::render::ShipRenderStatus;
using obscura::render::ShipRoomLabel;
using obscura::world::CellRect;
using obscura::world::Compartment;
using obscura::world::Damage;
using obscura::world::EvidenceProjection;
using obscura::world::Hull;
using obscura::world::Instrument;
using obscura::world::InstrumentMask;
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
      result.push_back({.cell = screen.at(column, row),
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
              Resolution state = Resolution::unknown,
              Damage damage = Damage::intact) -> RoomId {
  const RoomId id = static_cast<RoomId>(hull.room_count());
  return hull.add_room({
      .id = id,
      .archetype = obscura::world::Archetype::hold,
      .damage = damage,
      .state = state,
      .bounds = {.col = column, .row = row, .width = 22, .height = 9},
  });
}

auto row_text(const Screen& screen, int x, int y, int width) -> std::string {
  std::string result{};
  for (int column = 0; column < width; ++column) {
    const std::string_view text = screen.text_at(x + column, y);
    result += text.empty() ? " " : std::string{text};
  }
  return result;
}

auto cold_lantern() -> Hull {
  const auto cases = obscura::cases::all();
  REQUIRE(cases.size() > 1);
  return obscura::cases::build(cases[1]).hull;
}

auto render_ship(Screen& screen, const Hull& hull, std::uint64_t seed,
                 RoomId cursor) -> obscura::render::ShipRenderResult {
  std::vector<std::string> storage{};
  std::vector<ShipRoomLabel> labels{};
  storage.reserve(hull.room_count());
  labels.reserve(hull.room_count());
  for (const Compartment& room : hull.all()) {
    storage.push_back("R" + std::to_string(room.id));
    labels.push_back({.id = room.id, .text = storage.back()});
  }
  return obscura::render::draw_ship(
      screen,
      {.hull = hull, .room_labels = labels, .seed = seed, .cursor = cursor});
}

auto render_ship(Screen& screen, const Hull& hull,
                 std::span<const ShipRoomLabel> labels,
                 std::span<const EvidenceProjection> evidence,
                 InstrumentMask instruments, std::uint64_t seed, RoomId cursor)
    -> obscura::render::ShipRenderResult {
  return obscura::render::draw_ship(screen, {.hull = hull,
                                             .room_labels = labels,
                                             .evidence = evidence,
                                             .instruments = instruments,
                                             .seed = seed,
                                             .cursor = cursor});
}

auto logical_hash(const Screen& screen, const termforge::Rect& viewport)
    -> std::uint64_t {
  constexpr std::uint64_t kOffset = 14'695'981'039'346'656'037ULL;
  constexpr std::uint64_t kPrime = 1'099'511'628'211ULL;
  std::uint64_t hash = kOffset;
  for (int row = 0; row < viewport.h; ++row) {
    for (int column = 0; column < viewport.w; ++column) {
      for (const unsigned char byte :
           screen.text_at(viewport.x + column, viewport.y + row)) {
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
    CHECK(render_ship(screen, valid, 1, 0).status ==
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
    CHECK(render_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::too_many_compartments);
    CHECK(snapshot(screen) == before);
  }

  SECTION("invalid cursor") {
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(render_ship(screen, valid, 1, obscura::world::ROOM_ANY).status ==
          ShipRenderStatus::invalid_cursor);
    CHECK(snapshot(screen) == before);
  }

  SECTION("duplicate slots") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0) == 0);
    REQUIRE(add_room(hull, 0, 0) == 1);
    hull.connect(0, 1);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(render_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("off-grid bounds") {
    Hull hull{};
    REQUIRE(add_room(hull, 1, 0) == 0);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(render_ship(screen, hull, 1, 0).status ==
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
    CHECK(render_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("invalid damage state") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0, Resolution::surveyed,
                     static_cast<Damage>(0xFFU)) == 0);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(render_ship(screen, hull, 1, 0).status ==
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
    CHECK(render_ship(screen, hull, 1, 0).status ==
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
    CHECK(render_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("disconnected rooms") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0) == 0);
    REQUIRE(add_room(hull, 24, 0) == 1);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(render_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::invalid_layout);
    CHECK(snapshot(screen) == before);
  }

  SECTION("resolved rooms remain owned by the plate renderer") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0, Resolution::resolved) == 0);
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(render_ship(screen, hull, 1, 0).status ==
          ShipRenderStatus::unsupported_resolution);
    CHECK(snapshot(screen) == before);
  }

  SECTION("every room needs one dense safe display label") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0, Resolution::surveyed) == 0);
    const std::array<ShipRoomLabel, 1> wrong_id{{{.id = 1, .text = "hold"}}};
    const std::array<ShipRoomLabel, 1> too_long{{
        {.id = 0, .text = "twelve chars!"},
    }};
    const std::array<ShipRoomLabel, 1> control{{
        {.id = 0, .text = "hold\n1"},
    }};
    const std::array<std::span<const ShipRoomLabel>, 4> invalid_labels{{
        {},
        wrong_id,
        too_long,
        control,
    }};
    for (const auto labels : invalid_labels) {
      Screen screen = marked_screen();
      const auto before = snapshot(screen);
      CHECK(render_ship(screen, hull, labels, {}, 0, 1, 0).status ==
            ShipRenderStatus::invalid_projection);
      CHECK(snapshot(screen) == before);
    }
  }

  SECTION("evidence identities and concrete locations must be coherent") {
    Hull surveyed{};
    REQUIRE(add_room(surveyed, 0, 0, Resolution::surveyed) == 0);
    const std::array<ShipRoomLabel, 1> labels{{{.id = 0, .text = "hold"}}};
    const std::array<EvidenceProjection, 2> duplicate{{
        {.id = 1, .location = 0},
        {.id = 1, .location = 0},
    }};
    const std::array<EvidenceProjection, 1> sentinel{{
        {.id = obscura::world::EVIDENCE_NONE, .location = 0},
    }};
    const std::array<EvidenceProjection, 1> invalid_room{{
        {.id = 1, .location = 1},
    }};
    const std::array<std::span<const EvidenceProjection>, 3> invalid_evidence{
        {duplicate, sentinel, invalid_room}};
    for (const auto evidence : invalid_evidence) {
      Screen screen = marked_screen();
      const auto before = snapshot(screen);
      CHECK(render_ship(screen, surveyed, labels, evidence, 0, 1, 0).status ==
            ShipRenderStatus::invalid_projection);
      CHECK(snapshot(screen) == before);
    }

    Hull unknown{};
    REQUIRE(add_room(unknown, 0, 0) == 0);
    const std::array<EvidenceProjection, 1> premature{{
        {.id = 1, .location = 0},
    }};
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(render_ship(screen, unknown, labels, premature, 0, 1, 0).status ==
          ShipRenderStatus::invalid_projection);
    CHECK(snapshot(screen) == before);
  }

  SECTION("a room cannot promise more markers than its firm-up can place") {
    Hull hull{};
    REQUIRE(add_room(hull, 0, 0, Resolution::surveyed) == 0);
    const std::array<ShipRoomLabel, 1> labels{{{.id = 0, .text = "hold"}}};
    const std::array<EvidenceProjection, 5> evidence{{
        {.id = 0, .location = 0},
        {.id = 1, .location = 0},
        {.id = 2, .location = 0},
        {.id = 3, .location = 0},
        {.id = 4, .location = 0},
    }};
    Screen screen = marked_screen();
    const auto before = snapshot(screen);
    CHECK(render_ship(screen, hull, labels, evidence, 0, 1, 0).status ==
          ShipRenderStatus::too_many_evidence_markers);
    CHECK(snapshot(screen) == before);
  }
}

TEST_CASE("SHIP accepts all fifteen canonical slots", "[render][ship][cap]") {
  Hull hull{};
  RoomId previous = obscura::world::ROOM_ANY;
  for (const std::uint16_t row :
       {std::uint16_t{0}, std::uint16_t{11}, std::uint16_t{22}}) {
    for (const std::uint16_t column :
         {std::uint16_t{0}, std::uint16_t{24}, std::uint16_t{48},
          std::uint16_t{72}, std::uint16_t{96}}) {
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
  CHECK(render_ship(screen, hull, 5, 0).status == ShipRenderStatus::drawn);
}

TEST_CASE("surveyed rooms firm up with tint and instrument-aware evidence",
          "[render][ship][surveyed][golden]") {
  constexpr termforge::Rgb kIntactTint{.r = 0x0E, .g = 0x18, .b = 0x1D};
  constexpr termforge::Rgb kDamagedTint{.r = 0x2A, .g = 0x1B, .b = 0x08};
  constexpr termforge::Rgb kBreachedTint{.r = 0x2A, .g = 0x0B, .b = 0x10};
  constexpr termforge::Rgb kLockedForeground{.r = 0xD8, .g = 0xA8, .b = 0x58};

  Hull hull{};
  REQUIRE(add_room(hull, 0, 0, Resolution::surveyed, Damage::intact) == 0);
  REQUIRE(add_room(hull, 24, 0, Resolution::surveyed, Damage::damaged) == 1);
  REQUIRE(add_room(hull, 48, 0, Resolution::surveyed, Damage::breached) == 2);
  hull.connect(0, 1);
  hull.connect(1, 2);

  const std::array<ShipRoomLabel, 3> labels{{
      {.id = 0, .text = "Fwd airlock"},
      {.id = 1, .text = "Hold 1"},
      {.id = 2, .text = "Hold 2"},
  }};
  const InstrumentMask lamp =
      obscura::world::instrument_mask(Instrument::spectral_lamp);
  const InstrumentMask thermal =
      obscura::world::instrument_mask(Instrument::thermal_tap);
  const std::array<EvidenceProjection, 9> evidence{{
      {.id = 10, .location = 0},
      {.id = 9, .location = 1},
      {.id = 5, .location = 1, .requires_ = thermal},
      {.id = 7, .location = 1, .requires_ = lamp},
      {.id = 20, .location = 2},
      {.id = 17, .location = 2},
      {.id = 19, .location = 2},
      {.id = 18, .location = 2},
      {.id =
           30}, // Sensed: visible, but not located and therefore not a marker.
  }};

  Screen screen{120, 40};
  const auto result =
      render_ship(screen, hull, labels, evidence, lamp, 0xBAD5EED, 1);
  REQUIRE(result.status == ShipRenderStatus::drawn);

  CHECK(row_text(screen, 0, 0, 22) == "┌─[ FWD AIRLOCK ]────┐");
  CHECK(row_text(screen, 24, 0, 22) == "┌─[ HOLD 1 ]─────DMG─┐");
  CHECK(row_text(screen, 48, 0, 22) == "┌─[ HOLD 2 ]──BREACH─┐");
  CHECK(row_text(screen, 0, 8, 22) == "└────────────────────┘");

  CHECK(screen.at(1, 1).blank());
  CHECK(screen.at(1, 1).bg == kIntactTint);
  CHECK(screen.at(25, 1).blank());
  CHECK(screen.at(25, 1).bg == kDamagedTint);
  CHECK(screen.at(49, 1).blank());
  CHECK(screen.at(49, 1).bg == kBreachedTint);
  CHECK(screen.at(24, 0).bg == kBackground);

  CHECK(screen.text_at(10, 2) == "◆");
  CHECK(screen.text_at(28, 2) == "◇"); // E06, sorted first and thermal-locked.
  CHECK(screen.at(28, 2).fg == kLockedForeground);
  CHECK(screen.text_at(41, 2) == "◆");
  CHECK(screen.text_at(31, 6) == "◆");
  CHECK(row_text(screen, 27, 4, 15) == "3 EVID · 1 LOCK");

  for (const auto& [x, y] : std::array<std::pair<int, int>, 4>{
           {{52, 2}, {65, 2}, {55, 6}, {62, 6}}}) {
    CHECK(screen.text_at(x, y) == "◆");
    CHECK(screen.at(x, y).bg == kBreachedTint);
  }
  CHECK(row_text(screen, 56, 4, 6) == "4 EVID");
}

TEST_CASE("surveyed firm-up is independent of seed and cursor",
          "[render][ship][surveyed][determinism]") {
  Hull hull{};
  REQUIRE(add_room(hull, 0, 0, Resolution::surveyed, Damage::damaged) == 0);
  REQUIRE(add_room(hull, 24, 0) == 1);
  hull.connect(0, 1);
  const std::array<ShipRoomLabel, 2> labels{{
      {.id = 0, .text = "Hold 1"},
      {.id = 1, .text = "Galley"},
  }};
  const std::array<EvidenceProjection, 1> evidence{{
      {.id = 3, .location = 0},
  }};

  Screen first{120, 40};
  Screen second{120, 40};
  REQUIRE(render_ship(first, hull, labels, evidence, 0, 7, 0).status ==
          ShipRenderStatus::drawn);
  REQUIRE(render_ship(second, hull, labels, evidence, 0, 91, 1).status ==
          ShipRenderStatus::drawn);

  for (int row = 0; row < 9; ++row) {
    for (int column = 0; column < 22; ++column) {
      CHECK(first.text_at(column, row) == second.text_at(column, row));
      CHECK(first.at(column, row) == second.at(column, row));
    }
  }
  std::size_t different_unknown_cells = 0;
  for (int row = 0; row < 9; ++row) {
    for (int column = 24; column < 46; ++column) {
      different_unknown_cells +=
          first.text_at(column, row) != second.text_at(column, row);
    }
  }
  CHECK(different_unknown_cells > 0);
}

TEST_CASE("a tint-only firm-up frame emits no image bytes",
          "[render][ship][surveyed][bytes]") {
  std::string sink{};
  termforge::KittyDriver driver{};
  driver.set_output(&sink);
  termforge::Renderer renderer{driver};
  const auto plate = obscura::render::hold_d0();
  constexpr termforge::Rect placement{1, 1, 20, 7};
  constexpr termforge::ImagePlacementOptions options{
      .fit = termforge::PlacementFit::Stretch,
      .layer = termforge::ImageLayer::below_text(),
  };
  const std::array<ShipRoomLabel, 1> labels{{{.id = 0, .text = "Hold 1"}}};

  Hull intact{};
  REQUIRE(add_room(intact, 0, 0, Resolution::surveyed, Damage::intact) == 0);
  Screen first{120, 40};
  REQUIRE(render_ship(first, intact, labels, {}, 0, 1, 0).status ==
          ShipRenderStatus::drawn);
  renderer.present(first);
  REQUIRE(driver.draw_image(placement, plate, options));
  renderer.flush();
  REQUIRE(driver.last_frame_bytes().image_transmit > 0);

  Hull damaged{};
  REQUIRE(add_room(damaged, 0, 0, Resolution::surveyed, Damage::damaged) == 0);
  Screen second{120, 40};
  REQUIRE(render_ship(second, damaged, labels, {}, 0, 1, 0).status ==
          ShipRenderStatus::drawn);
  renderer.present(second);
  REQUIRE(driver.draw_image(placement, plate, options));
  renderer.flush();

  const termforge::FrameBytes bytes = driver.last_frame_bytes();
  CHECK(bytes.cells > 0);
  CHECK(bytes.image_transmit == 0);
  CHECK(bytes.image_edit == 0);
}

TEST_CASE("Cold Lantern matches the normative 120 by 40 grid",
          "[render][ship][golden]") {
  const Hull hull = cold_lantern();
  Screen screen{120, 40};
  const auto result = render_ship(screen, hull, 0xC01D'1A47ULL, 7);
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
  REQUIRE(render_ship(reference, hull, 91, 3).status ==
          ShipRenderStatus::drawn);
  const auto result = render_ship(larger, hull, 91, 3);
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
  REQUIRE(render_ship(first, hull, 7, 0).status == ShipRenderStatus::drawn);
  REQUIRE(render_ship(second, hull, 8, 8).status == ShipRenderStatus::drawn);

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
