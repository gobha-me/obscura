#include <obscura/render/dissolve.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <span>

#include <obscura/render/art_plate.hpp>
#include <obscura/render/hold_d0_reveal_01.hpp>
#include <obscura/render/hold_d0_reveal_02.hpp>
#include <obscura/render/hold_d0_reveal_03.hpp>
#include <obscura/render/hold_d0_reveal_04.hpp>
#include <obscura/render/hold_d0_reveal_05.hpp>
#include <obscura/render/hold_d0_reveal_06.hpp>
#include <obscura/render/hold_d0_reveal_07.hpp>

#include <termforge/core/types.hpp>
#include <termforge/drivers/terminal_driver.hpp>

namespace obscura::render {

namespace {

constexpr termforge::Extent kPlatePixels{.w = 240, .h = 160};
constexpr termforge::ImagePlacementOptions kPlatePlacement{
    .fit = termforge::PlacementFit::Stretch,
    .layer = termforge::ImageLayer::below_text(),
};

template <std::size_t Size>
[[nodiscard]] auto png_bytes(const std::array<unsigned char, Size>& bytes)
    -> std::span<const std::byte> {
  return std::as_bytes(std::span{bytes});
}

[[nodiscard]] auto reveal_images()
    -> std::array<termforge::EncodedImage, kDissolveRevealSteps> {
  return {
      termforge::EncodedImage{.format = termforge::ImageFormat::Png,
                              .bytes = png_bytes(detail::kHoldD0Reveal01),
                              .pixels = kPlatePixels},
      termforge::EncodedImage{.format = termforge::ImageFormat::Png,
                              .bytes = png_bytes(detail::kHoldD0Reveal02),
                              .pixels = kPlatePixels},
      termforge::EncodedImage{.format = termforge::ImageFormat::Png,
                              .bytes = png_bytes(detail::kHoldD0Reveal03),
                              .pixels = kPlatePixels},
      termforge::EncodedImage{.format = termforge::ImageFormat::Png,
                              .bytes = png_bytes(detail::kHoldD0Reveal04),
                              .pixels = kPlatePixels},
      termforge::EncodedImage{.format = termforge::ImageFormat::Png,
                              .bytes = png_bytes(detail::kHoldD0Reveal05),
                              .pixels = kPlatePixels},
      termforge::EncodedImage{.format = termforge::ImageFormat::Png,
                              .bytes = png_bytes(detail::kHoldD0Reveal06),
                              .pixels = kPlatePixels},
      termforge::EncodedImage{.format = termforge::ImageFormat::Png,
                              .bytes = png_bytes(detail::kHoldD0Reveal07),
                              .pixels = kPlatePixels},
      hold_d0(),
  };
}

[[nodiscard]] auto reveal_frames(
    const std::array<termforge::EncodedImage, kDissolveRevealSteps>& images)
    -> std::array<termforge::AnimationFrame, kDissolveRevealSteps> {
  return {
      termforge::AnimationFrame{images[0], std::chrono::milliseconds{20}},
      termforge::AnimationFrame{images[1], std::chrono::milliseconds{20}},
      termforge::AnimationFrame{images[2], std::chrono::milliseconds{20}},
      termforge::AnimationFrame{images[3], std::chrono::milliseconds{20}},
      termforge::AnimationFrame{images[4], std::chrono::milliseconds{20}},
      termforge::AnimationFrame{images[5], std::chrono::milliseconds{20}},
      termforge::AnimationFrame{images[6], std::chrono::milliseconds{20}},
      termforge::AnimationFrame{images[7], std::chrono::milliseconds{20}},
  };
}

} // namespace

auto DissolveTimeline::advance(std::chrono::nanoseconds elapsed) noexcept
    -> bool {
  if (m_finished || elapsed <= std::chrono::nanoseconds{0}) {
    return false;
  }
  bool changed = false;
  m_carry += elapsed;
  while (!m_finished && m_carry >= kDissolveStepGaps.at(m_step)) {
    m_carry -= kDissolveStepGaps.at(m_step);
    if (m_step + 1 == kDissolveSteps) {
      m_finished = true;
    } else {
      ++m_step;
    }
    changed = true;
  }
  return changed;
}

auto DissolveTimeline::skip() noexcept -> void {
  m_step = kDissolveSteps - 1;
  m_carry = std::chrono::nanoseconds{0};
  m_finished = true;
}

ResolveDissolve::ResolveDissolve()
    : m_images(reveal_images()), m_frames(reveal_frames(m_images)) {
}

auto ResolveDissolve::register_sequence(termforge::TerminalDriver& driver)
    -> std::expected<void, termforge::ErrorEvent> {
  const auto registered = driver.register_animation(m_frames);
  if (!registered) {
    return std::unexpected{registered.error()};
  }
  m_animation = *registered;
  return {};
}

auto ResolveDissolve::begin(termforge::TerminalDriver& driver)
    -> std::expected<void, termforge::ErrorEvent> {
  if (m_active) {
    return std::unexpected{termforge::ErrorEvent{
        .severity = termforge::Severity::Warning,
        .source = "dissolve",
        .message = "a resolve dissolve is already active"}};
  }
  if (!driver.supports_image_animation()) {
    return std::unexpected{termforge::ErrorEvent{
        .severity = termforge::Severity::Warning,
        .source = "dissolve",
        .message = "selected graphics route cannot register image animations"}};
  }
  m_timeline = {};
  m_started = false;
  m_placed = false;
  m_skip_requested = false;
  m_recovering = false;
  m_manual_reveal = false;
  m_retry_frame = false;
  m_finish_control_queued = false;
  const auto registered = register_sequence(driver);
  if (!registered) {
    return registered;
  }
  m_active = true;
  return {};
}

auto ResolveDissolve::stop(termforge::TerminalDriver& driver)
    -> std::expected<void, termforge::ErrorEvent> {
  if (m_animation) {
    const auto unregistered = driver.unregister_animation(m_animation);
    if (!unregistered) {
      return std::unexpected{unregistered.error()};
    }
  }
  m_animation = {};
  m_active = false;
  return {};
}

auto ResolveDissolve::skip(termforge::TerminalDriver& driver)
    -> std::expected<void, termforge::ErrorEvent> {
  if (!m_active || m_timeline.finished()) {
    return {};
  }
  m_skip_requested = true;
  if (!m_started) {
    return {};
  }
  const auto stopped =
      driver.stop_animation(m_animation, termforge::AnimationStopMode::Finish);
  if (!stopped) {
    return std::unexpected{stopped.error()};
  }
  m_timeline.skip();
  m_finish_control_queued = true;
  return {};
}

auto ResolveDissolve::advance(termforge::TerminalDriver& driver,
                              std::chrono::nanoseconds elapsed,
                              std::chrono::steady_clock::time_point now)
    -> std::expected<bool, termforge::ErrorEvent> {
  if (!m_active) {
    return false;
  }
  if (m_retry_frame) {
    return retry_rejected_output(driver);
  }
  if (m_timeline.finished() && !m_recovering) {
    return false;
  }

  const auto status = driver.animation_status(m_animation, now);
  if (!status) {
    return std::unexpected{status.error()};
  }
  if (status->state == termforge::AnimationRunState::Pending) {
    return false;
  }

  if (!m_started) {
    return start_or_resume(driver, now);
  }

  return advance_started(driver, elapsed, now);
}

auto ResolveDissolve::retry_rejected_output(termforge::TerminalDriver& driver)
    -> std::expected<bool, termforge::ErrorEvent> {
  m_retry_frame = false;
  if (!m_finish_control_queued) {
    return true;
  }
  const auto stopped =
      driver.stop_animation(m_animation, termforge::AnimationStopMode::Finish);
  if (!stopped) {
    return std::unexpected{stopped.error()};
  }
  return true;
}

auto ResolveDissolve::start_or_resume(termforge::TerminalDriver& driver,
                                      std::chrono::steady_clock::time_point now)
    -> std::expected<bool, termforge::ErrorEvent> {
  const bool resume = m_skip_requested || m_timeline.finished() || m_recovering;
  if (resume) {
    const std::size_t target =
        dissolve_reveal_frame_for_resume(m_timeline.step(), m_skip_requested);
    const auto sought = driver.seek_animation(m_animation, target, now);
    if (!sought) {
      return std::unexpected{sought.error()};
    }
    if (m_skip_requested) {
      m_timeline.skip();
    }
    m_manual_reveal = m_recovering && m_timeline.step() < kDissolveRevealSteps;
  } else {
    const auto played =
        driver.play_animation(m_animation, termforge::AnimationPlayMode::Once,
                              termforge::AnimationReplay::Restart, now);
    if (!played) {
      return std::unexpected{played.error()};
    }
  }
  m_started = true;
  m_recovering = false;
  return true;
}

auto ResolveDissolve::advance_started(termforge::TerminalDriver& driver,
                                      std::chrono::nanoseconds elapsed,
                                      std::chrono::steady_clock::time_point now)
    -> std::expected<bool, termforge::ErrorEvent> {
  const std::size_t previous = m_timeline.step();
  if (!m_timeline.advance(elapsed)) {
    return false;
  }
  if (m_manual_reveal && previous < kDissolveRevealSteps &&
      m_timeline.step() < kDissolveRevealSteps) {
    const auto sought =
        driver.seek_animation(m_animation, m_timeline.step(), now);
    if (!sought) {
      return std::unexpected{sought.error()};
    }
  }
  return true;
}

auto ResolveDissolve::place(termforge::TerminalDriver& driver,
                            const termforge::Rect& cells,
                            std::chrono::steady_clock::time_point now)
    -> std::expected<void, termforge::ErrorEvent> {
  if (!m_active) {
    return {};
  }
  const auto status = driver.animation_status(m_animation, now);
  if (!status || status->state == termforge::AnimationRunState::Pending) {
    return status ? std::expected<void, termforge::ErrorEvent>{}
                  : std::unexpected{status.error()};
  }
  auto placed =
      m_placed ? driver.retain_animation(cells, m_animation, kPlatePlacement)
               : driver.draw_animation(cells, m_animation, kPlatePlacement);
  if (!placed) {
    return std::unexpected{placed.error()};
  }
  m_placed = true;
  return {};
}

auto ResolveDissolve::recover(termforge::TerminalDriver& driver)
    -> std::expected<void, termforge::ErrorEvent> {
  if (!m_active) {
    return {};
  }
  m_animation = {};
  m_placed = false;
  m_started = false;
  m_recovering = true;
  return register_sequence(driver);
}

auto ResolveDissolve::observe_output(bool accepted) noexcept -> void {
  if (accepted) {
    m_finish_control_queued = false;
  } else {
    m_retry_frame = true;
  }
}

} // namespace obscura::render
