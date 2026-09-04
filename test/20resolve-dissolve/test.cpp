#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <obscura/core/ledger.hpp>
#include <obscura/render/dissolve.hpp>
#include <obscura/render/ship.hpp>
#include <obscura/replay/recorder.hpp>
#include <obscura/world/hull.hpp>
#include <obscura/world/model.hpp>

#include <termforge/core/app.hpp>
#include <termforge/core/byte_sink.hpp>
#include <termforge/core/renderer.hpp>
#include <termforge/core/screen.hpp>
#include <termforge/core/types.hpp>
#include <termforge/drivers/fallback_driver.hpp>
#include <termforge/drivers/kitty_driver.hpp>

namespace {

using namespace std::chrono_literals;
using obscura::render::DissolveTimeline;
using obscura::render::DissolveVisual;
using obscura::render::ResolveDissolve;
using obscura::render::ShipDissolveInput;
using obscura::render::ShipRenderInput;
using obscura::render::ShipRenderStatus;
using obscura::render::ShipRoomLabel;
using obscura::world::Archetype;
using obscura::world::Compartment;
using obscura::world::Damage;
using obscura::world::Hull;
using obscura::world::Resolution;
using termforge::KittyDriver;
using termforge::Screen;

struct CellSnapshot {
  termforge::Cell cell{};
  std::string text{};

  auto operator==(const CellSnapshot&) const -> bool = default;
};

class RefusingSink final : public termforge::ByteSink {
 public:
  auto write(std::span<const char>)
      -> std::expected<void, termforge::ErrorEvent> override {
    return std::unexpected{termforge::ErrorEvent{
        termforge::Severity::Error, "sink", "dissolve output refused"}};
  }
};

auto snapshot(const Screen& screen) -> std::vector<CellSnapshot> {
  std::vector<CellSnapshot> result{};
  result.reserve(static_cast<std::size_t>(screen.cols() * screen.rows()));
  for (int y = 0; y < screen.rows(); ++y) {
    for (int x = 0; x < screen.cols(); ++x) {
      result.push_back(
          {.cell = screen.at(x, y), .text = std::string{screen.text_at(x, y)}});
    }
  }
  return result;
}

auto one_room(Resolution state, Damage damage = Damage::intact,
              Archetype archetype = Archetype::hold) -> Hull {
  Hull hull{};
  const auto id = hull.add_room(Compartment{
      .id = 0,
      .archetype = archetype,
      .damage = damage,
      .state = state,
      .bounds = {.col = 0, .row = 0, .width = 22, .height = 9},
  });
  REQUIRE(id == 0);
  return hull;
}

auto render_room(Screen& screen, const Hull& hull,
                 std::optional<DissolveVisual> visual = std::nullopt)
    -> obscura::render::ShipRenderResult {
  const std::array<ShipRoomLabel, 1> labels{{{.id = 0, .text = "Hold 1"}}};
  return obscura::render::draw_ship(
      screen,
      ShipRenderInput{
          .hull = std::cref(hull),
          .room_labels = labels,
          .cursor = 0,
          .dissolve = visual.has_value()
                          ? std::optional<ShipDissolveInput>{ShipDissolveInput{
                                .room = 0, .visual = *visual}}
                          : std::nullopt,
      });
}

auto acknowledge_registration(KittyDriver& driver,
                              const ResolveDissolve& dissolve) -> void {
  REQUIRE(dissolve.animation());
  for (std::size_t reply = 0; reply < obscura::render::kDissolveRevealSteps;
       ++reply) {
    driver.consume_reply(
        termforge::TerminalReply{dissolve.animation().id, std::nullopt, "OK"});
  }
  REQUIRE(driver.take_driver_events().empty());
}

auto count_command(std::string_view wire, std::string_view field) -> int {
  int count = 0;
  std::size_t offset = 0;
  while ((offset = wire.find("\033_G", offset)) != std::string_view::npos) {
    const std::size_t end = wire.find("\033\\", offset + 3);
    if (end == std::string_view::npos) {
      break;
    }
    const std::size_t separator = wire.find(';', offset + 3);
    const std::size_t header_end =
        separator == std::string_view::npos || separator > end ? end
                                                               : separator;
    if (wire.substr(offset + 3, header_end - offset - 3).find(field) !=
        std::string_view::npos) {
      ++count;
    }
    offset = end + 2;
  }
  return count;
}

// This is the downstream seam #40 will use: simulation starts the presenter,
// then App's ordinary callbacks route events, ticks, cells and pixels without
// allowing the presentation object to mutate the run.
class ResolvePresentationProbe final : public termforge::App {
 public:
  ResolvePresentationProbe() : m_hull(one_room(Resolution::resolved)) {
    m_driver.set_image_animation_support(true);
    m_driver.set_output(&m_wire);
  }

  auto start() -> void { m_ok = m_dissolve.begin(m_driver).has_value(); }

  auto acknowledge() -> void {
    m_driver.flush();
    acknowledge_registration(m_driver, m_dissolve);
  }

  auto deliver(termforge::Event event) -> void { on_event(event); }

  auto frame(Screen& screen, std::chrono::nanoseconds elapsed) -> void {
    m_now += elapsed;
    on_tick(std::chrono::duration<double>{elapsed});
    on_render(screen);
    on_pixels(m_driver);
    m_driver.flush();
    m_dissolve.observe_output(true);
  }

  [[nodiscard]] auto ok() const noexcept -> bool { return m_ok; }
  [[nodiscard]] auto dissolve() const noexcept -> const ResolveDissolve& {
    return m_dissolve;
  }

 protected:
  auto on_event(const termforge::Event& event) -> void override {
    if (std::holds_alternative<termforge::ImageInvalidatedEvent>(event)) {
      m_ok = m_dissolve.recover(m_driver).has_value();
      return;
    }
    const auto* key = std::get_if<termforge::KeyEvent>(&event);
    if (key != nullptr && key->action == termforge::KeyAction::Press &&
        key->key != termforge::Key::Escape && !key->ctrl) {
      m_ok = m_dissolve.skip(m_driver).has_value();
      return;
    }
    termforge::App::on_event(event);
  }

  auto on_tick(std::chrono::duration<double> elapsed) -> void override {
    const auto advanced = m_dissolve.advance(
        m_driver, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed),
        m_now);
    m_ok = advanced.has_value();
  }

  auto on_render(Screen& screen) -> void override {
    m_render = render_room(screen, m_hull, m_dissolve.visual());
    m_ok = m_ok && m_render.status == ShipRenderStatus::drawn &&
           m_render.plate_count == 1;
  }

  auto on_pixels(termforge::TerminalDriver& selected) -> void override {
    if (!m_ok) {
      return;
    }
    m_ok =
        m_dissolve.place(selected, m_render.plates[0].cells, m_now).has_value();
  }

 private:
  Hull m_hull;
  KittyDriver m_driver{};
  ResolveDissolve m_dissolve{};
  obscura::render::ShipRenderResult m_render{};
  std::chrono::steady_clock::time_point m_now{};
  std::string m_wire{};
  bool m_ok{true};
};

} // namespace

// Failure matrix first: unsupported output and artwork are refused without
// leaving terminal or logical-screen residue.
TEST_CASE("resolve dissolve refuses unsupported output and plate mappings",
          "[render][dissolve][failure]") {
  termforge::FallbackDriver fallback{};
  std::string wire{};
  fallback.set_output(&wire);
  ResolveDissolve dissolve{};
  const auto begun = dissolve.begin(fallback);
  REQUIRE_FALSE(begun.has_value());
  CHECK(begun.error().severity == termforge::Severity::Warning);
  fallback.flush();
  CHECK(wire.empty());
  CHECK_FALSE(dissolve.active());

  KittyDriver refused{};
  refused.set_image_animation_support(true);
  RefusingSink sink{};
  refused.set_output(&sink);
  ResolveDissolve rejected{};
  REQUIRE(rejected.begin(refused));
  refused.flush();
  REQUIRE(refused.take_output_error().has_value());
  CHECK(refused.residency() == termforge::ImageResidency{});
  const auto missing = rejected.advance(refused, 0ns, {});
  REQUIRE_FALSE(missing.has_value());

  for (const auto& [archetype, damage] :
       std::array{std::pair{Archetype::hold, Damage::damaged},
                  std::pair{Archetype::bridge, Damage::intact}}) {
    CAPTURE(archetype, damage);
    const Hull hull = one_room(Resolution::resolved, damage, archetype);
    Screen screen{120, 40};
    screen.write_text(0, 0, "sentinel", {}, {});
    const auto before = snapshot(screen);
    const auto result = render_room(screen, hull);
    CHECK(result.status == ShipRenderStatus::unsupported_resolution);
    CHECK(result.plate_count == 0);
    CHECK(snapshot(screen) == before);
  }
}

TEST_CASE("resolve dissolve schedule owns exactly thirteen indexed steps",
          "[render][dissolve][schedule]") {
  CHECK(obscura::render::kDissolveSteps == 13);
  CHECK(obscura::render::kDissolveRevealSteps == 8);
  CHECK(obscura::render::kDissolveGlyphSteps == 4);
  CHECK(obscura::render::kDissolveTintSteps == 1);
  CHECK(std::accumulate(obscura::render::kDissolveStepGaps.begin(),
                        obscura::render::kDissolveStepGaps.end(),
                        0ms) == obscura::render::kDissolveDuration);

  DissolveTimeline timeline{};
  REQUIRE(timeline.advance(159ms));
  CHECK(timeline.step() == 7);
  REQUIRE(timeline.advance(1ms));
  CHECK(timeline.step() == 8);
  CHECK(timeline.visual().glyph_strata == 3);
  REQUIRE(timeline.advance(140ms));
  CHECK(timeline.step() == 12);
  CHECK_FALSE(timeline.visual().damage_tint);
  CHECK_FALSE(timeline.finished());
  REQUIRE(timeline.advance(100ms));
  CHECK(timeline.finished());
}

TEST_CASE("every skip boundary converges on identical final logical pixels",
          "[render][dissolve][skip][failure]") {
  const Hull hull = one_room(Resolution::resolved);
  DissolveTimeline natural{};
  REQUIRE(natural.advance(obscura::render::kDissolveDuration));
  REQUIRE(natural.finished());
  const DissolveVisual final = natural.visual();
  REQUIRE(final == DissolveVisual{.reveal_frame = 7,
                                  .glyph_strata = 0,
                                  .damage_tint = false});
  Screen expected{120, 40};
  const auto expected_result = render_room(expected, hull, final);
  REQUIRE(expected_result.status == ShipRenderStatus::drawn);
  REQUIRE(expected_result.plate_count == 1);

  for (std::size_t boundary = 0; boundary < obscura::render::kDissolveSteps;
       ++boundary) {
    CAPTURE(boundary);
    DissolveTimeline skipped{};
    for (std::size_t step = 0; step < boundary; ++step) {
      REQUIRE(skipped.advance(obscura::render::kDissolveStepGaps[step]));
    }
    skipped.skip();
    REQUIRE(skipped.finished());
    CHECK(skipped.visual() == final);
    CHECK(obscura::render::dissolve_reveal_frame_for_resume(boundary, true) ==
          obscura::render::kDissolveRevealSteps - 1);

    Screen actual{120, 40};
    const auto result = render_room(actual, hull, skipped.visual());
    REQUIRE(result.status == ShipRenderStatus::drawn);
    CHECK(snapshot(actual) == snapshot(expected));
    CHECK(result.plates[0] == expected_result.plates[0]);
  }
}

TEST_CASE("skip while upload is pending selects the final resident frame",
          "[render][dissolve][skip][wire][failure]") {
  KittyDriver driver{};
  driver.set_image_animation_support(true);
  std::string wire{};
  driver.set_output(&wire);
  ResolveDissolve dissolve{};
  REQUIRE(dissolve.begin(driver));
  REQUIRE(dissolve.skip(driver));
  CHECK_FALSE(dissolve.finished());
  driver.flush();
  acknowledge_registration(driver, dissolve);
  const auto advanced = dissolve.advance(driver, 0ns, {});
  REQUIRE(advanced.has_value());
  CHECK(*advanced);
  CHECK(dissolve.finished());
  driver.flush();
  CHECK(count_command(wire, "a=t") == 1);
  CHECK(count_command(wire, "a=f") == 7);
  CHECK(count_command(wire, "c=8") >= 1);
}

TEST_CASE("image invalidation resumes the current deterministic reveal",
          "[render][dissolve][lifecycle][wire][failure]") {
  for (const std::size_t wanted_step : std::array<std::size_t, 3>{3, 8, 12}) {
    CAPTURE(wanted_step);
    KittyDriver driver{};
    driver.set_image_animation_support(true);
    std::string wire{};
    driver.set_output(&wire);
    ResolveDissolve dissolve{};
    REQUIRE(dissolve.begin(driver));
    driver.flush();
    acknowledge_registration(driver, dissolve);
    REQUIRE(dissolve.advance(driver, 0ns, {}));
    for (std::size_t step = 0; step < wanted_step; ++step) {
      REQUIRE(dissolve.advance(driver, obscura::render::kDissolveStepGaps[step],
                               {}));
    }
    REQUIRE(dissolve.step() == wanted_step);

    driver.invalidate_images();
    REQUIRE(dissolve.recover(driver));
    driver.flush();
    acknowledge_registration(driver, dissolve);
    wire.clear();
    REQUIRE(dissolve.advance(driver, 0ns, {}));
    driver.flush();
    CHECK(count_command(wire,
                        "c=" + std::to_string(
                                   obscura::render::dissolve_visual(wanted_step)
                                       .reveal_frame +
                                   1)) >= 1);
  }
}

TEST_CASE("complete resolve uses one upload and no playback retransmission",
          "[render][dissolve][bytes][kitty]") {
  KittyDriver driver{};
  driver.set_image_animation_support(true);
  std::string wire{};
  driver.set_output(&wire);
  ResolveDissolve dissolve{};
  REQUIRE(dissolve.begin(driver));
  driver.flush();
  CHECK(count_command(wire, "a=t") == 1);
  CHECK(count_command(wire, "a=f") == 7);
  CHECK(driver.last_frame_bytes().image_transmit > 0);
  CHECK(driver.last_frame_bytes().image_edit > 0);
  acknowledge_registration(driver, dissolve);
  const std::size_t registration_end = wire.size();

  std::chrono::steady_clock::time_point now{};
  REQUIRE(dissolve.advance(driver, 0ns, now));
  const Hull hull = one_room(Resolution::resolved);
  termforge::Renderer renderer{driver};
  Screen screen{120, 40};
  for (std::size_t step = 0; step < obscura::render::kDissolveSteps; ++step) {
    const auto result = render_room(screen, hull, dissolve.visual());
    REQUIRE(result.status == ShipRenderStatus::drawn);
    REQUIRE(result.plate_count == 1);
    renderer.present(screen);
    REQUIRE(dissolve.place(driver, result.plates[0].cells, now));
    renderer.flush();
    dissolve.observe_output(true);
    if (step + 1 < obscura::render::kDissolveSteps) {
      now += obscura::render::kDissolveStepGaps[step];
      REQUIRE(dissolve.advance(driver, obscura::render::kDissolveStepGaps[step],
                               now));
    }
  }
  now += obscura::render::kDissolveStepGaps.back();
  REQUIRE(
      dissolve.advance(driver, obscura::render::kDissolveStepGaps.back(), now));
  CHECK(dissolve.finished());

  const std::string_view playback{wire.data() + registration_end,
                                  wire.size() - registration_end};
  CHECK(count_command(playback, "a=t") == 0);
  CHECK(count_command(playback, "a=f") == 0);
  CHECK(driver.total_bytes().total() == wire.size());
  CHECK(driver.total_bytes().total() <= 40U * 1024U);
}

TEST_CASE("stopping a resolve releases terminal residency and permits reuse",
          "[render][dissolve][lifecycle]") {
  KittyDriver driver{};
  driver.set_image_animation_support(true);
  std::string wire{};
  driver.set_output(&wire);
  ResolveDissolve dissolve{};

  REQUIRE(dissolve.begin(driver));
  driver.flush();
  acknowledge_registration(driver, dissolve);
  REQUIRE(driver.residency().pinned_images == 1);
  REQUIRE(dissolve.stop(driver));
  CHECK_FALSE(dissolve.active());
  driver.flush();
  CHECK(driver.residency() == termforge::ImageResidency{});

  wire.clear();
  REQUIRE(dissolve.begin(driver));
  CHECK(dissolve.active());
  CHECK(dissolve.step() == 0);
  driver.flush();
  CHECK(count_command(wire, "a=t") == 1);
  CHECK(count_command(wire, "a=f") == 7);
  REQUIRE(dissolve.stop(driver));
  driver.flush();
  CHECK(driver.residency() == termforge::ImageResidency{});
}

TEST_CASE("watched and skipped presentation add no replay or ledger state",
          "[render][dissolve][replay]") {
  obscura::replay::Recorder watched{0, 0xC01D'1A47ULL};
  obscura::replay::Recorder skipped{0, 0xC01D'1A47ULL};
  watched.record(obscura::input::Intent::ResolveBatch, 0);
  skipped.record(obscura::input::Intent::ResolveBatch, 0);
  obscura::core::Ledger watched_ledger{120};
  obscura::core::Ledger skipped_ledger{120};

  DissolveTimeline natural{};
  REQUIRE(natural.advance(obscura::render::kDissolveDuration));
  DissolveTimeline immediate{};
  immediate.skip();

  REQUIRE(watched.recording().steps.size() == 1);
  REQUIRE(skipped.recording().steps.size() == 1);
  CHECK(watched.recording().steps[0].intent ==
        skipped.recording().steps[0].intent);
  CHECK(watched.recording().steps[0].subject ==
        skipped.recording().steps[0].subject);
  CHECK(watched_ledger.entries().size() == skipped_ledger.entries().size());
  CHECK(watched_ledger.remaining() == skipped_ledger.remaining());
  CHECK(natural.visual() == immediate.visual());
}

TEST_CASE("App callbacks route a skip to the final resolved composition",
          "[render][dissolve][app][skip]") {
  ResolvePresentationProbe app{};
  app.start();
  REQUIRE(app.ok());
  app.acknowledge();
  Screen screen{120, 40};
  app.frame(screen, 0ns);
  REQUIRE(app.ok());
  app.frame(screen, 20ms);
  REQUIRE(app.dissolve().step() == 1);

  app.deliver(termforge::KeyEvent{.key = termforge::Key::Char,
                                  .ch = U'x',
                                  .action = termforge::KeyAction::Press});
  app.frame(screen, 0ns);
  REQUIRE(app.ok());
  CHECK(app.dissolve().finished());

  const Hull hull = one_room(Resolution::resolved);
  Screen expected{120, 40};
  REQUIRE(render_room(expected, hull,
                      obscura::render::dissolve_visual(
                          obscura::render::kDissolveSteps - 1))
              .status == ShipRenderStatus::drawn);
  CHECK(snapshot(screen) == snapshot(expected));
}
