// Evidence log failures first: charge mutations are atomic, projections cannot
// smuggle unseen evidence into LOG, image blocks are never clipped, and quota
// pressure is observable without becoming simulation state.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <obscura/core/charge.hpp>
#include <obscura/core/ledger.hpp>
#include <obscura/render/evidence_art.hpp>
#include <obscura/render/log_view.hpp>
#include <obscura/world/model.hpp>

#include <termforge/core/screen.hpp>
#include <termforge/core/types.hpp>
#include <termforge/drivers/fallback_driver.hpp>
#include <termforge/drivers/kitty_driver.hpp>

namespace {

using obscura::core::ChargeAction;
using obscura::core::EvidenceReadStatus;
using obscura::core::Ledger;
using obscura::render::EvidenceImageCache;
using obscura::render::EvidenceImagePlacement;
using obscura::render::EvidenceLogInput;
using obscura::render::EvidenceLogProjection;
using obscura::render::EvidenceLogStatus;
using obscura::world::EvidenceId;
using obscura::world::EvidenceKind;
using termforge::KittyDriver;
using termforge::Screen;

constexpr termforge::Rgb kForeground{.r = 0xE0, .g = 0xE0, .b = 0xF0};
constexpr termforge::Rgb kBackground{.r = 0x0A, .g = 0x0A, .b = 0x14};

auto row_text(const Screen& screen, int y) -> std::string {
  std::string result{};
  for (int x = 0; x < screen.cols(); ++x) {
    const std::string_view cell = screen.text_at(x, y);
    result += cell.empty() ? " " : std::string{cell};
  }
  return result;
}

auto marked_screen(int columns = 120, int rows = 40) -> Screen {
  Screen screen{columns, rows};
  screen.write_text(0, 0, "sentinel", kForeground, kBackground,
                    termforge::Attr::Reverse);
  return screen;
}

struct LogFixture {
  Ledger ledger{120};
  std::vector<std::string> rooms{};
  std::vector<std::string> bodies{};
  std::vector<EvidenceLogProjection> projections{};

  explicit LogFixture(std::size_t count) {
    rooms.reserve(count);
    bodies.reserve(count);
    projections.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      rooms.push_back("R" + std::to_string(index) + " HOLD");
      bodies.push_back("Recovered evidence body number " +
                       std::to_string(index) +
                       " with enough stable words for the document.");
      const auto id = static_cast<EvidenceId>(index);
      REQUIRE(ledger.read_evidence(id).status == EvidenceReadStatus::examined);
      projections.push_back({
          .id = id,
          .kind = index == 1 ? EvidenceKind::manifest
                             : EvidenceKind::physical_trace,
          .room_label = rooms.back(),
          .body = bodies.back(),
          .instrument = {},
          .image = index == 1 ? obscura::render::e02_manifest_seal()
                              : termforge::EncodedImage{},
      });
    }
  }

  auto input(std::size_t top = 0) const -> EvidenceLogInput {
    return {.ledger = ledger,
            .case_name = "COLD LANTERN",
            .evidence = projections,
            .total_evidence = 16,
            .top_row = top};
  }
};

auto read_u32(std::span<const std::byte> bytes, std::size_t offset)
    -> std::uint32_t {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
         (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
         (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
         static_cast<std::uint32_t>(bytes[offset + 3U]);
}

} // namespace

TEST_CASE("evidence read failures never spend or append partial state",
          "[log][charge][failure]") {
  SECTION("a first read one charge short is refused") {
    Ledger ledger{2};
    const auto result = ledger.read_evidence(1);
    CHECK(result.status == EvidenceReadStatus::insufficient_charge);
    CHECK(result.cost == 3);
    CHECK(result.remaining == 2);
    CHECK(ledger.remaining() == 2);
    CHECK(ledger.entries().empty());
  }

  SECTION("a re-read one charge short preserves the first read") {
    Ledger ledger{4};
    REQUIRE(ledger.read_evidence(1).status == EvidenceReadStatus::examined);
    const auto result = ledger.read_evidence(1);
    CHECK(result.status == EvidenceReadStatus::insufficient_charge);
    CHECK(result.cost == 2);
    CHECK(result.remaining == 1);
    REQUIRE(ledger.entries().size() == 1);
    CHECK(ledger.entries().front().kind == obscura::core::EntryKind::Examine);
  }

  SECTION("the exact boundary distinguishes examine from re-read") {
    Ledger ledger{5};
    const auto first = ledger.read_evidence(1);
    const auto second = ledger.read_evidence(1);
    CHECK(first.status == EvidenceReadStatus::examined);
    CHECK(first.cost == 3);
    CHECK(first.remaining == 2);
    CHECK(second.status == EvidenceReadStatus::reread);
    CHECK(second.cost == 2);
    CHECK(second.remaining == 0);
    REQUIRE(ledger.entries().size() == 2);
    CHECK(ledger.entries()[0].charge_delta == -3);
    CHECK(ledger.entries()[1].kind == obscura::core::EntryKind::Reread);
    CHECK(ledger.entries()[1].charge_delta == -2);
  }
}

TEST_CASE("the complete charge schedule has one authoritative table",
          "[log][charge]") {
  using obscura::core::charge_delta;
  CHECK(charge_delta(ChargeAction::Start) == 120);
  CHECK(charge_delta(ChargeAction::Move) == -1);
  CHECK(charge_delta(ChargeAction::MoveBreached) == -3);
  CHECK(charge_delta(ChargeAction::Survey) == -8);
  CHECK(charge_delta(ChargeAction::Examine) == -3);
  CHECK(charge_delta(ChargeAction::Reread) == -2);
  CHECK(charge_delta(ChargeAction::Abort) == 0);
  CHECK(charge_delta(ChargeAction::BatchCorrect) == 15);
  CHECK(charge_delta(ChargeAction::BatchWrong) == -20);
}

TEST_CASE("LOG refuses malformed projections without touching the screen",
          "[log][render][failure]") {
  LogFixture fixture{2};

  SECTION("the fixed reference grid is mandatory") {
    for (const auto& [columns, rows] :
         std::array<std::pair<int, int>, 2>{{{119, 40}, {120, 39}}}) {
      Screen screen = marked_screen(columns, rows);
      const std::string before = row_text(screen, 0);
      CHECK(
          obscura::render::draw_evidence_log(screen, fixture.input()).status ==
          EvidenceLogStatus::terminal_too_small);
      CHECK(row_text(screen, 0) == before);
    }
  }

  SECTION("an unexamined projection is rejected rather than leaked") {
    fixture.projections.push_back({.id = 7,
                                   .kind = EvidenceKind::manifest,
                                   .room_label = "R07 HOLD 1",
                                   .body = "not examined"});
    Screen screen = marked_screen();
    const std::string before = row_text(screen, 0);
    CHECK(obscura::render::draw_evidence_log(screen, fixture.input()).status ==
          EvidenceLogStatus::invalid_projection);
    CHECK(row_text(screen, 0) == before);
  }

  SECTION("duplicate identities are rejected") {
    fixture.projections[1].id = fixture.projections[0].id;
    Screen screen = marked_screen();
    CHECK(obscura::render::draw_evidence_log(screen, fixture.input()).status ==
          EvidenceLogStatus::invalid_projection);
    CHECK(row_text(screen, 0).starts_with("sentinel"));
  }

  SECTION("control bytes are rejected before Screen sanitization") {
    fixture.bodies[0] = "visible\x1b[2Jnot evidence";
    fixture.projections[0].body = fixture.bodies[0];
    Screen screen = marked_screen();
    CHECK(obscura::render::draw_evidence_log(screen, fixture.input()).status ==
          EvidenceLogStatus::invalid_projection);
    CHECK(row_text(screen, 0).starts_with("sentinel"));
  }

  SECTION("an unbroken body word hard-wraps at the body width") {
    LogFixture one{1};
    one.bodies[0] = std::string(200, 'x');
    one.projections[0].body = one.bodies[0];
    Screen screen{120, 40};
    const auto result = obscura::render::draw_evidence_log(screen, one.input());
    REQUIRE(result.status == EvidenceLogStatus::drawn);
    CHECK(result.document_rows == 5);
  }
}

TEST_CASE("LOG paints its normative regions and keeps image blocks atomic",
          "[log][render]") {
  LogFixture fixture{14};
  Screen screen{124, 44};
  const auto first =
      obscura::render::draw_evidence_log(screen, fixture.input());
  REQUIRE(first.status == EvidenceLogStatus::drawn);
  CHECK(first.viewport == termforge::Rect{2, 2, 120, 40});
  CHECK(row_text(screen, 2).find("EVIDENCE LOG | COLD LANTERN") !=
        std::string::npos);
  CHECK(row_text(screen, 3).find("─") != std::string::npos);
  REQUIRE(first.images.size() == 1);
  CHECK(first.images.front().id == 1);
  CHECK(first.images.front().cells.w == obscura::render::kLogImageColumns);
  CHECK(first.images.front().cells.h == obscura::render::kLogImageRows);
  CHECK(first.maximum_top_row > 5);

  // The E02 image begins at document row 5 in this fixture. Top rows 6 and 7
  // would cut it, so row-wise movement jumps across the whole block.
  REQUIRE(obscura::render::scroll_evidence_log(fixture.input(), 5, 1));
  CHECK(*obscura::render::scroll_evidence_log(fixture.input(), 5, 1) == 8);

  Screen marked = marked_screen();
  const std::string before = row_text(marked, 0);
  CHECK(obscura::render::draw_evidence_log(marked, fixture.input(6)).status ==
        EvidenceLogStatus::invalid_scroll);
  CHECK(row_text(marked, 0) == before);
}

TEST_CASE("the E02 asset envelope is the committed terminal image",
          "[log][asset]") {
  const auto image = obscura::render::e02_manifest_seal();
  REQUIRE(image.format == termforge::ImageFormat::Png);
  REQUIRE(image.bytes.size() == 17'353);
  REQUIRE(image.bytes.size() >= 24);
  CHECK(image.pixels == termforge::Extent{320, 120});
  CHECK(read_u32(image.bytes, 16) == 320);
  CHECK(read_u32(image.bytes, 20) == 120);
}

TEST_CASE("resident LOG images transmit once across scrolling",
          "[log][image][bytes]") {
  std::string sink{};
  KittyDriver driver{};
  driver.set_output(&sink);
  driver.set_placement_mode(KittyDriver::PlacementMode::UnicodePlaceholders);
  EvidenceImageCache cache{};
  const EvidenceImagePlacement placement{
      .id = 1,
      .image = obscura::render::e02_manifest_seal(),
      .cells = {6, 8, obscura::render::kLogImageColumns,
                obscura::render::kLogImageRows},
  };

  const auto pending = cache.draw_visible(driver, std::span{&placement, 1});
  REQUIRE(pending.size() == 1);
  CHECK(pending.front().message.find("awaiting a terminal acknowledgement") !=
        std::string::npos);
  driver.flush();
  CHECK(driver.last_frame_bytes().image_transmit > 0);
  CHECK(driver.last_frame_bytes().image_edit == 0);
  CHECK(cache.resident_count() == 1);
  driver.consume_reply(termforge::TerminalReply{
      KittyDriver::kFirstPinnedImageId + KittyDriver::kMaxPinnedImages - 1,
      std::nullopt, "OK"});

  CHECK(cache.draw_visible(driver, {}).empty());
  driver.flush();
  CHECK(cache.resident_count() == 1);

  const auto redrawn = cache.draw_visible(driver, std::span{&placement, 1});
  INFO((redrawn.empty() ? std::string{} : redrawn.front().message));
  CHECK(redrawn.empty());
  driver.flush();
  CHECK(driver.last_frame_bytes().image_transmit == 0);
  CHECK(driver.last_frame_bytes().image_edit > 0);

  CHECK(cache.release(driver).empty());
  CHECK(cache.resident_count() == 0);
}

TEST_CASE("quota eviction is an event and never changes the log document",
          "[log][image][failure]") {
  std::string sink{};
  KittyDriver driver{};
  driver.set_output(&sink);
  driver.set_placement_mode(KittyDriver::PlacementMode::UnicodePlaceholders);
  EvidenceImageCache cache{1};
  const auto image = obscura::render::e02_manifest_seal();
  const EvidenceImagePlacement first{
      .id = 1, .image = image, .cells = {6, 8, 16, 3}};
  const EvidenceImagePlacement second{
      .id = 2, .image = image, .cells = {6, 12, 16, 3}};

  REQUIRE(cache.draw_visible(driver, std::span{&first, 1}).size() == 1);
  driver.flush();
  driver.consume_reply(termforge::TerminalReply{
      KittyDriver::kFirstPinnedImageId + KittyDriver::kMaxPinnedImages - 1,
      std::nullopt, "OK"});
  const auto accepted = cache.draw_visible(driver, std::span{&first, 1});
  INFO((accepted.empty() ? std::string{} : accepted.front().message));
  REQUIRE(accepted.empty());
  driver.flush();
  REQUIRE(cache.draw_visible(driver, {}).empty());
  driver.flush();
  const auto events = cache.draw_visible(driver, std::span{&second, 1});
  REQUIRE(events.size() == 2);
  CHECK(events[0].severity == termforge::Severity::Info);
  CHECK(events[0].source == "obscura.log");
  CHECK(events[0].message.find("evicted E02") != std::string::npos);
  CHECK(events[1].message.find("awaiting a terminal acknowledgement") !=
        std::string::npos);
  CHECK(cache.resident_count() == 1);

  LogFixture fixture{2};
  Screen screen{120, 40};
  const auto rendered =
      obscura::render::draw_evidence_log(screen, fixture.input());
  CHECK(rendered.status == EvidenceLogStatus::drawn);
  CHECK(row_text(screen, 3).find("Recovered evidence body") !=
        std::string::npos);

  cache.invalidate();
  CHECK(cache.resident_count() == 0);
}

TEST_CASE("invalid or unsupported image work fails before partial placement",
          "[log][image][failure]") {
  const auto image = obscura::render::e02_manifest_seal();
  const EvidenceImagePlacement placement{
      .id = 1, .image = image, .cells = {6, 8, 16, 3}};

  SECTION("a duplicate visible identity is a total refusal") {
    std::string sink{};
    KittyDriver driver{};
    driver.set_output(&sink);
    EvidenceImageCache cache{};
    const std::array duplicate{placement, placement};
    const auto events = cache.draw_visible(driver, duplicate);
    REQUIRE(events.size() == 1);
    CHECK(events.front().source == "obscura.log");
    CHECK(cache.resident_count() == 0);
    driver.flush();
    CHECK(driver.last_frame_bytes().total() == 0);
  }

  SECTION("a tier without residency reports the missing capability") {
    std::string sink{};
    termforge::FallbackDriver driver{};
    driver.set_output(&sink);
    EvidenceImageCache cache{};
    const auto events = cache.draw_visible(driver, std::span{&placement, 1});
    REQUIRE(events.size() == 1);
    CHECK(events.front().severity == termforge::Severity::Warning);
    CHECK(events.front().message.find("residency is unavailable") !=
          std::string::npos);
    CHECK(cache.resident_count() == 0);
  }
}
