// OBSCURA's first test over its own code, and the seam it proves is the one
// every M0 bandwidth criterion is written against: obscura::core::App drives
// TermForge's real frame body offline, into a std::string, and the driver's
// byte meter reports what that frame cost.
//
// The meter is T-H4 (termforge#139), which landed upstream in v0.6.8 — the
// ticket the M0 plan flags "do this one first, everything else is measured
// against it". This file is its first consumer here. The v0.57.24 pin keeps the
// meter and activates the rest of the M0 TermForge primitives.
//
// No terminal, per CLAUDE.md: "Driver-facing tests are offline against an
// in-memory sink." test_run_frames() wires a Screen, a Renderer and a
// FallbackDriver redirected into a std::string, then calls the same
// frame_step() that run() calls — the shipped code path, not a reimplementation
// of it.
//
// Failure matrix first, per the testing philosophy in AGENTS.md. The smoke
// check is last.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include <obscura/core/app.hpp>
#include <obscura/core/session.hpp>

#include <termforge/core/screen.hpp>
#include <termforge/drivers/terminal_driver.hpp>

namespace {

using termforge::FrameBytes;

constexpr int kCols = 80;
constexpr int kRows = 24;

// What a full repaint of a cleared grid MUST cost, derived from the protocol
// rather than read back off the meter.
//
// This is the independent oracle the rest of the file needs. Checking
// `total_bytes()` against the sink's size proves very little: `emit_frame()`
// hands the same `bytes.size()` to the sink and to `tally_frame()` two lines
// apart, so that equality is arithmetic, not behaviour. This function knows
// nothing about either.
//
// TermForge's Renderer still calls draw_text once per changed cell, but
// FallbackDriver now remembers where the previous call left the cursor.
// Therefore only the first cell in a row emits "\033[{row};1H"; the remaining
// cells are adjacent and cost one space each. A row costs `cols` spaces plus 5
// fixed cursor bytes (ESC, '[', ';', '1', 'H') and the decimal digits of its
// 1-based row.
//
// It is deliberately coupled to that wire format. A change to it is not a
// cosmetic upstream detail: it moves the cost of every frame OBSCURA will ever
// draw, which is the quantity M0 gates on. If this stops matching, read the new
// escape in FallbackDriver::draw_text and update the arithmetic here — do not
// relax it into the band below.
constexpr auto digits(int n) -> std::uint64_t {
  return n < 10 ? 1 : (n < 100 ? 2 : 3);
}

constexpr auto full_repaint_bytes(int cols, int rows) -> std::uint64_t {
  std::uint64_t total = 0;
  for (int y = 1; y <= rows; ++y)
    total += static_cast<std::uint64_t>(cols) + 5 + digits(y);
  return total;
}

constexpr std::uint64_t kFullRepaint =
    full_repaint_bytes(kCols, kRows); // 2,079
constexpr std::uint64_t kIdleCeiling =
    2048; // docs/08-determinism.md, section 7.7
static_assert(kFullRepaint == 2079, "the 80x24 wire-format oracle moved");

// The meter lives on TerminalDriver, and App::driver() is protected — an
// application reads its own frame cost from inside its own App subclass, which
// is the access path this probe reproduces rather than reaching around. Note
// that this is App's own path and NOT one src/render/ can use: render/ is not
// an App subclass, and per include/obscura/core/app.hpp it is not App's
// business to tell it what a frame cost.
//
// Deriving from obscura::core::App rather than termforge::App is what makes
// this a test of THIS repo — it runs our on_render, our screen.clear().
//
// The three loop-seam overrides are load-bearing, not tidiness.
// test_run_frames() deliberately skips setup(), so the terminal's
// VMIN=0/VTIME=0 was never applied — yet the frame body still calls
// pump_input(), which issues a blocking ::read on whatever fd Terminal picked.
// Under ctest stdin is /dev/null and it returns immediately, so the omission
// would be invisible exactly where it matters: run this binary straight from a
// shell and frame 1 waits for a keypress.
//
// now_steady() is the one that matters most, and it is the one a copy of this
// file would most likely drop. Left alone it feeds the REAL elapsed delta to
// on_tick, so two runs of the same binary see different numbers. Nothing reads
// it yet — obscura::core::App does not override on_tick — but wiring TermForge
// into the simulation is precisely this class's job, and CLAUDE.md's rule is
// "no wall-clock in simulation". Pinning the clock at construction keeps every
// dt exactly zero, so a later on_tick cannot import wall time through the back
// door without someone deleting this line on purpose.
//
// These are private virtuals on termforge::App; access control does not affect
// overriding, and TermForge's own test/23pacing does the same thing.
class OfflineProbe : public obscura::core::App {
 protected:
  auto now_steady() const -> std::chrono::steady_clock::time_point override {
    return m_now;
  }
  auto wait_readable(int /*timeout_ms*/) -> bool override { return false; }
  auto read_available(char* /*out*/, int /*max*/) -> int override { return 0; }

 private:
  std::chrono::steady_clock::time_point m_now{};
};

class FrameProbe : public OfflineProbe {
 public:
  // ONE call, N frames — never N calls of one frame. test_run_frames() builds a
  // FRESH driver on every call, so a second call restarts the meter at zero
  // while the sink keeps accumulating, and a cumulative assertion across two
  // calls then fails against a meter that is working perfectly.
  auto run_offline(int frames, int cols, int rows) -> void {
    test_run_frames(frames, cols, rows, &m_sink);
    // Detach before returning. m_sink is a member of THIS class, but the driver
    // holding a pointer to it lives in the termforge::App base subobject —
    // and derived members are destroyed before base subobjects, so without
    // this the driver spends both destructors pointing at a dead string.
    // Harmless with today's FallbackDriver, whose destructor writes nothing;
    // a use-after-free under ASan the day this harness defaults to a tier that
    // flushes on the way out. byte_sink.hpp states the contract: the sink must
    // outlive the driver, or be detached before it dies.
    driver().clear_output();
  }

  [[nodiscard]] auto last_frame() -> FrameBytes {
    return driver().last_frame_bytes();
  }
  [[nodiscard]] auto cumulative() -> FrameBytes {
    return driver().total_bytes();
  }
  [[nodiscard]] auto emitted() const -> const std::string& { return m_sink; }

 private:
  std::string m_sink;
};

struct RenderFailure {};

// on_render is where an exhausted image quota or a bad projection will
// eventually surface. This probe makes it throw, to pin the half of the
// teardown guarantee that is observable without a terminal.
class ThrowingProbe : public OfflineProbe {
 public:
  // nullptr, not a member string: test_run_guarded documents it as "pass
  // nullptr to discard", and the throw skips present() anyway, so a sink here
  // would only ever be written to and never read.
  auto guarded() -> int { return test_run_guarded(kCols, kRows, nullptr); }
  [[nodiscard]] auto renders() const -> int { return m_renders; }

 protected:
  auto on_render(termforge::Screen& screen) -> void override {
    obscura::core::App::on_render(screen); // the real one first, then fail
    // The cap comes BEFORE the throw and is the reason this test cannot hang.
    // test_run_guarded drives the real loop, which exits only on quit() or an
    // exception; if a future on_render change meant the throw below were never
    // reached, the loop would spin and ctest would sit on its 1500 s default in
    // all eight CI matrix jobs. A regressed guard must fail the suite, not hang
    // it — so the cap turns that into a red assertion instead.
    if (++m_renders > 4) {
      quit();
      return;
    }
    throw RenderFailure{};
  }

 private:
  int m_renders{0};
};

} // namespace

// ── Failure modes first ─────────────────────────────────────────────────────

TEST_CASE("app: a degenerate grid is distinguishable from a grid never drawn",
          "[app][bytes][failure]") {
  // An application whose terminal reported a nonsense size has to be able to
  // tell "this frame cost me nothing" from "I never ran a frame". Asserting
  // zero on the degenerate cases alone cannot show that — a meter stubbed to
  // return zero would pass every one of them. So the contrast is the test:
  // the same probe, the same assertions, against a grid that DOES draw.

  SECTION("a real grid is the control, and it is loud") {
    FrameProbe app;
    app.run_offline(1, kCols, kRows);
    CHECK(app.cumulative().total() == kFullRepaint);
    CHECK_FALSE(app.emitted().empty());
  }

  SECTION("zero by zero draws nothing") {
    FrameProbe app;
    app.run_offline(1, 0, 0);
    CHECK(app.cumulative().total() == 0);
    CHECK(app.emitted().empty());
  }

  SECTION("negative geometry clamps rather than wrapping") {
    // Screen clamps a negative extent to zero. Worth its own section because
    // the failure being excluded is not "draws nothing" but "wraps to a huge
    // unsigned extent and indexes backwards" — which would not be quiet.
    FrameProbe app;
    app.run_offline(1, -kCols, -kRows);
    CHECK(app.cumulative().total() == 0);
    CHECK(app.emitted().empty());
  }

  SECTION("one degenerate axis") {
    FrameProbe app;
    app.run_offline(1, kCols, 0);
    CHECK(app.cumulative().total() == 0);
    CHECK(app.emitted().empty());
  }

  SECTION("zero frames wires the driver but never runs the body") {
    FrameProbe app;
    app.run_offline(0, kCols, kRows);
    CHECK(app.cumulative().total() == 0);
    CHECK(app.emitted().empty());
  }
}

TEST_CASE("app: a throwing frame runs teardown before it propagates",
          "[app][failure]") {
  // CLAUDE.md: "Never leave the terminal raw, including on the exception path."
  // run_loop()'s guard calls teardown() and rethrows, and BOTH halves are the
  // contract — swallowing the exception would hide a bug, skipping the teardown
  // would leave the player's shell in raw mode.
  //
  // Be precise about what is witnessed here, because the honest scope is
  // narrower than "the terminal is restored". test_run_guarded never entered
  // raw mode or the alt-screen (setup() did not run), so neither is observable.
  // What it does do is set the SIGWINCH hook explicitly, so that teardown has
  // one piece of real production state to undo — and that undo writes nothing
  // to a terminal, which is what makes it assertable headless. The hook is the
  // proxy; it is not the whole guarantee.
  ThrowingProbe app;
  REQUIRE_THROWS_AS(app.guarded(), RenderFailure);
  CHECK_FALSE(app.test_winch_hooked());
  CHECK(app.renders() == 1); // it failed on frame 1; it did not spin
}

TEST_CASE("app: rendering decides nothing", "[app][failure]") {
  // AGENTS.md: core::App routes, it does not decide. And app.hpp: "on_render
  // must not mutate the world — it is the one place a frame drop would
  // otherwise turn into a change in simulation state." The tick is cosmetic, so
  // N frames must move the session FSM exactly nowhere.
  //
  // Half a test today, and worth knowing which half: Boot is the initial phase
  // and only CaseLoaded and Abort move it, so an on_render that dispatched one
  // of the other five signals would leave this green. It catches the transition
  // that matters now; widen it when on_render is given anything to do.
  FrameProbe app;
  REQUIRE(app.session().phase() == obscura::core::Phase::Boot);
  app.run_offline(4, kCols, kRows);
  CHECK(app.session().phase() == obscura::core::Phase::Boot);
}

// ── The meter ───────────────────────────────────────────────────────────────

TEST_CASE("app: the meter reports the cost the wire format predicts",
          "[app][bytes]") {
  // The one assertion in this file with a genuinely independent oracle:
  // kFullRepaint is computed from the escape sequence, not read back off the
  // instrument being tested.
  FrameProbe app;
  app.run_offline(1, kCols, kRows);

  CHECK(app.last_frame().total() == kFullRepaint);
  CHECK(app.emitted().size() == kFullRepaint);

  // Every byte is cell traffic. On this tier that is the documented contract
  // rather than a measurement — FallbackDriver has no out-of-band image
  // channel and never calls the image tallies at all, so these two cannot
  // currently fail. They are here to be the line that moves when a plate path
  // arrives, and they will need a kitty-backed harness to mean anything then:
  // test_run_frames hardcodes the fallback tier.
  CHECK(app.cumulative().cells == kFullRepaint);
  CHECK(app.cumulative().image_transmit == 0);
  CHECK(app.cumulative().image_edit == 0);
}

TEST_CASE("app: a second identical frame costs nothing at all",
          "[app][bytes]") {
  // T-H4's own acceptance criterion, at OBSCURA's call site — and here it is
  // exact rather than approximate. Frame 1 is a full repaint, because the
  // Renderer has no previous frame to diff against. Frame 2 draws the same
  // cleared grid, the diff finds no changed cell, and the driver flushes an
  // empty buffer.
  //
  // `== 0` and not `< first` on purpose: `<` would also pass if a regression
  // made the idle frame cost one byte less than a full repaint. Verified to
  // fail — drawing a single changed cell in on_render turns this red at 7.
  //
  // This is the whole basis of the idle-frame budget: it holds only while an
  // idle frame changes nothing. When src/render/ starts drawing, this case is
  // where the new idle number gets written down.
  FrameProbe app;
  app.run_offline(2, kCols, kRows);

  CHECK(app.last_frame().total() == 0);
  CHECK(app.last_frame().total() <= kIdleCeiling);
  CHECK(app.cumulative().total() == kFullRepaint); // frame 1 only
}

TEST_CASE("app: the full-repaint cost is now just above the idle budget",
          "[app][bytes]") {
  // THE number, and this case owns it: 2,079 bytes for one 80x24 repaint.
  // The docs point here rather than restating it, so when it moves there is
  // one place to change and it goes red on its own.
  //
  // A first paint is not an idle frame, so it does not have to fit the 2 KiB
  // idle ceiling. The comparison remains useful: v0.7.1 cost 16,344 bytes and
  // sat eight ceilings away; cursor-aware sequential writes have reduced the
  // gap to 31 bytes. The preceding case is the actual idle assertion and is
  // deliberately stronger than the budget: an unchanged frame costs zero.
  FrameProbe app;
  app.run_offline(1, kCols, kRows);

  CHECK(app.last_frame().total() == kFullRepaint);
  CHECK(app.last_frame().total() > kIdleCeiling);
  CHECK(app.last_frame().total() - kIdleCeiling == 31);
}

// ── The smoke check, last ───────────────────────────────────────────────────

TEST_CASE("app: it runs offline with no terminal at all", "[app][bytes]") {
  FrameProbe app;
  app.run_offline(3, kCols, kRows);
  CHECK_FALSE(app.emitted().empty());
}
