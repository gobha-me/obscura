#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <pty.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <expected>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <obscura/core/app.hpp>

#include <termforge/core/requirements.hpp>
#include <termforge/core/screen.hpp>
#include <termforge/core/terminal.hpp>
#include <termforge/core/types.hpp>

namespace {

constexpr std::string_view kAltScreenEnter{"\033[?1049h"};

class PtyPair {
 public:
  PtyPair(unsigned short cols, unsigned short rows, unsigned short px_width,
          unsigned short px_height) {
    winsize size{};
    size.ws_col = cols;
    size.ws_row = rows;
    size.ws_xpixel = px_width;
    size.ws_ypixel = px_height;
    if (::openpty(&m_master, &m_slave, nullptr, nullptr, &size) != 0) {
      return;
    }
    const int flags = ::fcntl(m_master, F_GETFL);
    if (flags < 0 || ::fcntl(m_master, F_SETFL, flags | O_NONBLOCK) != 0) {
      return;
    }
    m_ok = true;
  }

  ~PtyPair() {
    if (m_master >= 0) ::close(m_master);
    if (m_slave >= 0) ::close(m_slave);
  }

  PtyPair(const PtyPair&) = delete;
  auto operator=(const PtyPair&) -> PtyPair& = delete;

  [[nodiscard]] auto ok() const noexcept -> bool { return m_ok; }
  [[nodiscard]] auto slave() const noexcept -> int { return m_slave; }

  auto drain() -> std::string {
    std::string bytes;
    std::array<char, 512> buffer{};
    for (;;) {
      const auto count = ::read(m_master, buffer.data(), buffer.size());
      if (count <= 0) break;
      bytes.append(buffer.data(), static_cast<std::size_t>(count));
    }
    return bytes;
  }

 private:
  int m_master{-1};
  int m_slave{-1};
  bool m_ok{false};
};

class StderrCapture {
 public:
  StderrCapture() {
    if (::pipe(m_pipe) != 0) return;
    m_saved = ::dup(STDERR_FILENO);
    if (m_saved < 0) return;
    std::fflush(stderr);
    if (::dup2(m_pipe[1], STDERR_FILENO) < 0) return;
    ::close(m_pipe[1]);
    m_pipe[1] = -1;
    m_active = true;
  }

  ~StderrCapture() { restore(); }

  StderrCapture(const StderrCapture&) = delete;
  auto operator=(const StderrCapture&) -> StderrCapture& = delete;

  [[nodiscard]] auto ok() const noexcept -> bool { return m_active; }

  auto finish() -> std::string {
    restore();
    std::string bytes;
    std::array<char, 512> buffer{};
    for (;;) {
      const auto count = ::read(m_pipe[0], buffer.data(), buffer.size());
      if (count <= 0) break;
      bytes.append(buffer.data(), static_cast<std::size_t>(count));
    }
    ::close(m_pipe[0]);
    m_pipe[0] = -1;
    return bytes;
  }

 private:
  auto restore() -> void {
    if (!m_active) return;
    std::fflush(stderr);
    (void)::dup2(m_saved, STDERR_FILENO);
    ::close(m_saved);
    m_saved = -1;
    m_active = false;
  }

  int m_pipe[2]{-1, -1};
  int m_saved{-1};
  bool m_active{false};
};

[[nodiscard]] auto same_termios(const termios& lhs, const termios& rhs)
    -> bool {
  return lhs.c_iflag == rhs.c_iflag && lhs.c_oflag == rhs.c_oflag &&
         lhs.c_cflag == rhs.c_cflag && lhs.c_lflag == rhs.c_lflag &&
         std::equal(std::begin(lhs.c_cc), std::end(lhs.c_cc),
                    std::begin(rhs.c_cc)) &&
         ::cfgetispeed(&lhs) == ::cfgetispeed(&rhs) &&
         ::cfgetospeed(&lhs) == ::cfgetospeed(&rhs);
}

[[nodiscard]] auto full_capabilities() -> termforge::Capabilities {
  termforge::Capabilities caps;
  caps.kitty_graphics = true;
  caps.kitty_keyboard = true;
  return caps;
}

class StartupProbe final : public obscura::core::App {
 public:
  auto inject(int fd, termforge::Capabilities caps) -> bool {
    return terminal().set_io(termforge::TerminalIo{fd, fd}).has_value() &&
           terminal().set_capabilities(std::move(caps)).has_value();
  }

  auto setup() -> std::expected<void, termforge::ErrorEvent> {
    return test_setup();
  }
  auto teardown() -> void { test_teardown(); }
};

class EventProbe final : public obscura::core::App {
 public:
  auto start_offline() -> void {
    std::string sink;
    test_run_frames(0, 120, 40, &sink);
  }
  auto deliver(termforge::Event event) -> void { on_event(event); }
};

class ResizeProbe final : public obscura::core::App {
 public:
  ResizeProbe() {
    // This test isolates OBSCURA's runtime policy from the independently tested
    // startup floor. TermForge's production resize evaluator is still what
    // creates the Warning and Info events consumed by App.
    require(termforge::AppRequirements{.min_cols = 120, .min_rows = 40});
  }

  auto run_resize_sequence() -> void {
    REQUIRE(set_size({120, 40}).has_value());
    test_run_frames(4, 120, 40, &m_sink);
  }

  [[nodiscard]] auto overlay_samples() const
      -> const std::vector<std::size_t>& {
    return m_overlay_samples;
  }
  [[nodiscard]] auto resize_count() const noexcept -> int {
    return m_resize_count;
  }
  [[nodiscard]] auto pushes_succeeded() const noexcept -> bool {
    return m_pushes_succeeded;
  }

 protected:
  auto on_event(const termforge::Event& event) -> void override {
    if (std::holds_alternative<termforge::ResizeEvent>(event)) {
      ++m_resize_count;
      if (m_resize_count == 1 || m_resize_count == 2) {
        m_pushes_succeeded &= set_size({119, 40}).has_value();
      } else if (m_resize_count == 3) {
        m_pushes_succeeded &= set_size({120, 40}).has_value();
      }
    }
    obscura::core::App::on_event(event);
  }

  [[nodiscard]] auto now_steady() const
      -> std::chrono::steady_clock::time_point override {
    return m_now;
  }

  auto wait_readable(int timeout_ms) -> bool override {
    m_overlay_samples.push_back(overlay_count());
    m_now += std::chrono::milliseconds(timeout_ms);
    return false;
  }

  auto read_available(char*, int) -> int override { return 0; }

 private:
  std::string m_sink;
  std::vector<std::size_t> m_overlay_samples;
  std::chrono::steady_clock::time_point m_now{};
  int m_resize_count{0};
  bool m_pushes_succeeded{true};
};

} // namespace

// Failure matrix first: each fact is independently insufficient.
TEST_CASE("App startup rejects every missing terminal-floor fact",
          "[app][requirements][failure]") {
  struct FailureCase {
    std::string_view label;
    unsigned short cols;
    unsigned short rows;
    unsigned short px_width;
    unsigned short px_height;
    termforge::Capabilities caps;
    std::string_view diagnostic;
  };

  auto no_graphics = full_capabilities();
  no_graphics.kitty_graphics = false;
  auto no_event_types = full_capabilities();
  no_event_types.kitty_keyboard = false;

  const std::array cases{
      FailureCase{"graphics", 120, 40, 720, 480, no_graphics, "graphics"},
      FailureCase{"event types", 120, 40, 720, 480, no_event_types,
                  "effective input route"},
      FailureCase{"columns", 119, 40, 714, 480, full_capabilities(), "grid"},
      FailureCase{"rows", 120, 39, 720, 468, full_capabilities(), "grid"},
      FailureCase{"unknown pixels", 120, 40, 0, 0, full_capabilities(),
                  "cell-pixel"},
      FailureCase{"narrow cells", 120, 40, 600, 480, full_capabilities(),
                  "cell width"},
      FailureCase{"short cells", 120, 40, 720, 440, full_capabilities(),
                  "cell height"},
  };

  for (const auto& failure : cases) {
    CAPTURE(failure.label);
    PtyPair pty{failure.cols, failure.rows, failure.px_width,
                failure.px_height};
    REQUIRE(pty.ok());

    StartupProbe app;
    REQUIRE(app.inject(pty.slave(), failure.caps));
    const auto result = app.setup();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().source == "requirements");
    CHECK(result.error().message.find(failure.diagnostic) != std::string::npos);
    CHECK_FALSE(app.requirements_met());
    app.teardown();
    CHECK(pty.drain().find(kAltScreenEnter) == std::string::npos);
  }
}

TEST_CASE("App startup accepts the exact terminal-floor boundary",
          "[app][requirements]") {
  PtyPair pty{120, 40, 720, 480};
  REQUIRE(pty.ok());

  StartupProbe app;
  REQUIRE(app.inject(pty.slave(), full_capabilities()));
  REQUIRE(app.setup().has_value());
  CHECK(app.requirements_met());
  CHECK(pty.drain().find(kAltScreenEnter) != std::string::npos);
  app.teardown();
}

TEST_CASE("below-floor run exits 78 in cooked mode before alt-screen",
          "[app][requirements][failure]") {
  PtyPair pty{120, 40, 720, 480};
  REQUIRE(pty.ok());

  termios before{};
  REQUIRE(::tcgetattr(pty.slave(), &before) == 0);

  StartupProbe app;
  auto caps = full_capabilities();
  caps.kitty_graphics = false;
  REQUIRE(app.inject(pty.slave(), caps));

  StderrCapture diagnostics;
  REQUIRE(diagnostics.ok());
  const int result = app.run();
  const std::string stderr_text = diagnostics.finish();

  termios after{};
  REQUIRE(::tcgetattr(pty.slave(), &after) == 0);
  CHECK(result == 78);
  CHECK(same_termios(before, after));
  CHECK(stderr_text.find("termforge: setup failed:") != std::string::npos);
  CHECK(stderr_text.find("graphics") != std::string::npos);
  CHECK(pty.drain().find(kAltScreenEnter) == std::string::npos);
}

TEST_CASE("requirement-loss modal cannot be dismissed and restores once",
          "[app][requirements][runtime][failure]") {
  EventProbe app;
  app.deliver(
      termforge::ErrorEvent{termforge::Severity::Warning, "requirements",
                            "declared minimum grid 120x40, found 119x40"});
  REQUIRE(app.overlay_count() == 1);

  app.deliver(
      termforge::ErrorEvent{termforge::Severity::Warning, "requirements",
                            "declared minimum grid 120x40, found 118x40"});
  CHECK(app.overlay_count() == 1);

  app.test_pump({"\033[13u", "\033[27u"});
  CHECK(app.overlay_count() == 1);

  app.deliver(
      termforge::ErrorEvent{termforge::Severity::Info, "requirements",
                            "the declared AppRequirements floor is met again"});
  CHECK(app.overlay_count() == 0);
}

TEST_CASE("production resize cadence pauses once and resumes on restoration",
          "[app][requirements][runtime][failure]") {
  ResizeProbe app;
  app.run_resize_sequence();

  CHECK(app.pushes_succeeded());
  CHECK(app.resize_count() == 4);
  CHECK(app.overlay_samples() == std::vector<std::size_t>{0, 1, 1, 0});
  CHECK(app.requirements_met());
}

TEST_CASE("protocol loss remains terminal above the requirement modal",
          "[app][requirements][runtime][failure]") {
  EventProbe app;
  app.start_offline();
  REQUIRE(app.running());

  app.deliver(
      termforge::ErrorEvent{termforge::Severity::Warning, "requirements",
                            "declared minimum grid 120x40, found 119x40"});
  REQUIRE(app.overlay_count() == 1);

  app.deliver(termforge::ErrorEvent{
      termforge::Severity::Warning, "keyboard",
      "keyboard release reporting is no longer available"});
  REQUIRE(app.overlay_count() == 2);

  app.deliver(
      termforge::ErrorEvent{termforge::Severity::Info, "requirements",
                            "the declared AppRequirements floor is met again"});
  CHECK(app.overlay_count() == 2);

  app.test_pump({"\033[13u"});
  CHECK_FALSE(app.running());
  CHECK(app.overlay_count() == 0);
  CHECK(app.session().phase() == obscura::core::Phase::Closed);
}
