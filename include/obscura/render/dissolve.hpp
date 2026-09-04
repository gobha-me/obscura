#pragma once

// The presentation-only transition from a surveyed compartment to a resolved
// one. Simulation commits Resolution::resolved before this begins; this object
// owns only which deterministic visual step is currently being presented.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>

#include <termforge/core/types.hpp>

namespace termforge {
class TerminalDriver;
}

namespace obscura::render {

inline constexpr std::size_t kDissolveRevealSteps = 8;
inline constexpr std::size_t kDissolveGlyphSteps = 4;
inline constexpr std::size_t kDissolveTintSteps = 1;
inline constexpr std::size_t kDissolveSteps =
    kDissolveRevealSteps + kDissolveGlyphSteps + kDissolveTintSteps;
inline constexpr std::array<std::chrono::milliseconds, kDissolveSteps>
    kDissolveStepGaps{
        std::chrono::milliseconds{20},  std::chrono::milliseconds{20},
        std::chrono::milliseconds{20},  std::chrono::milliseconds{20},
        std::chrono::milliseconds{20},  std::chrono::milliseconds{20},
        std::chrono::milliseconds{20},  std::chrono::milliseconds{20},
        std::chrono::milliseconds{35},  std::chrono::milliseconds{35},
        std::chrono::milliseconds{35},  std::chrono::milliseconds{35},
        std::chrono::milliseconds{100},
    };
inline constexpr auto kDissolveDuration = std::chrono::milliseconds{400};

struct DissolveVisual {
  std::size_t reveal_frame{0};
  std::uint8_t glyph_strata{kDissolveGlyphSteps};
  bool damage_tint{true};

  constexpr auto operator==(const DissolveVisual&) const noexcept
      -> bool = default;
};

[[nodiscard]] constexpr auto dissolve_visual(std::size_t step) noexcept
    -> DissolveVisual {
  step = step < kDissolveSteps ? step : kDissolveSteps - 1;
  const std::size_t reveal =
      step < kDissolveRevealSteps ? step : kDissolveRevealSteps - 1;
  const std::size_t removed =
      step < kDissolveRevealSteps
          ? 0
          : std::min(step - kDissolveRevealSteps + 1, kDissolveGlyphSteps);
  return {
      .reveal_frame = reveal,
      .glyph_strata = static_cast<std::uint8_t>(kDissolveGlyphSteps - removed),
      .damage_tint = step < kDissolveRevealSteps + kDissolveGlyphSteps,
  };
}

[[nodiscard]] constexpr auto dissolve_reveal_frame_for_resume(
    std::size_t step, bool finish_requested) noexcept -> std::size_t {
  return finish_requested ? kDissolveRevealSteps - 1
                          : dissolve_visual(step).reveal_frame;
}

class DissolveTimeline {
 public:
  [[nodiscard]] auto step() const noexcept -> std::size_t { return m_step; }
  [[nodiscard]] auto finished() const noexcept -> bool { return m_finished; }
  [[nodiscard]] auto visual() const noexcept -> DissolveVisual {
    return dissolve_visual(m_step);
  }

  auto advance(std::chrono::nanoseconds elapsed) noexcept -> bool;
  auto skip() noexcept -> void;

 private:
  std::size_t m_step{0};
  std::chrono::nanoseconds m_carry{0};
  bool m_finished{false};
};

// Owns the terminal image-animation lifecycle for one resolve transition. It
// deliberately owns no room or simulation state: the caller commits the room
// resolution, then begins this disposable presentation.
class ResolveDissolve {
 public:
  ResolveDissolve();
  ~ResolveDissolve() = default;

  ResolveDissolve(const ResolveDissolve&) = delete;
  auto operator=(const ResolveDissolve&) -> ResolveDissolve& = delete;
  ResolveDissolve(ResolveDissolve&&) = delete;
  auto operator=(ResolveDissolve&&) -> ResolveDissolve& = delete;

  [[nodiscard]] auto begin(termforge::TerminalDriver& driver)
      -> std::expected<void, termforge::ErrorEvent>;
  [[nodiscard]] auto stop(termforge::TerminalDriver& driver)
      -> std::expected<void, termforge::ErrorEvent>;

  [[nodiscard]] auto skip(termforge::TerminalDriver& driver)
      -> std::expected<void, termforge::ErrorEvent>;
  [[nodiscard]] auto advance(termforge::TerminalDriver& driver,
                             std::chrono::nanoseconds elapsed,
                             std::chrono::steady_clock::time_point now)
      -> std::expected<bool, termforge::ErrorEvent>;
  [[nodiscard]] auto place(termforge::TerminalDriver& driver,
                           const termforge::Rect& cells,
                           std::chrono::steady_clock::time_point now)
      -> std::expected<void, termforge::ErrorEvent>;
  [[nodiscard]] auto recover(termforge::TerminalDriver& driver)
      -> std::expected<void, termforge::ErrorEvent>;

  // A rejected output frame has to be replayed. In particular, a skipped
  // animation must re-issue its finish command before the final composition is
  // allowed to count as presented.
  auto observe_output(bool accepted) noexcept -> void;

  [[nodiscard]] auto active() const noexcept -> bool { return m_active; }
  [[nodiscard]] auto finished() const noexcept -> bool {
    return m_timeline.finished();
  }
  [[nodiscard]] auto step() const noexcept -> std::size_t {
    return m_timeline.step();
  }
  [[nodiscard]] auto visual() const noexcept -> DissolveVisual {
    return m_timeline.visual();
  }
  [[nodiscard]] auto animation() const noexcept -> termforge::AnimationHandle {
    return m_animation;
  }

 private:
  [[nodiscard]] auto register_sequence(termforge::TerminalDriver& driver)
      -> std::expected<void, termforge::ErrorEvent>;
  [[nodiscard]] auto retry_rejected_output(termforge::TerminalDriver& driver)
      -> std::expected<bool, termforge::ErrorEvent>;
  [[nodiscard]] auto start_or_resume(termforge::TerminalDriver& driver,
                                     std::chrono::steady_clock::time_point now)
      -> std::expected<bool, termforge::ErrorEvent>;
  [[nodiscard]] auto advance_started(termforge::TerminalDriver& driver,
                                     std::chrono::nanoseconds elapsed,
                                     std::chrono::steady_clock::time_point now)
      -> std::expected<bool, termforge::ErrorEvent>;

  std::array<termforge::EncodedImage, kDissolveRevealSteps> m_images;
  std::array<termforge::AnimationFrame, kDissolveRevealSteps> m_frames;
  termforge::AnimationHandle m_animation{};
  DissolveTimeline m_timeline{};
  bool m_active{false};
  bool m_started{false};
  bool m_placed{false};
  bool m_skip_requested{false};
  bool m_recovering{false};
  bool m_manual_reveal{false};
  bool m_retry_frame{false};
  bool m_finish_control_queued{false};
};

} // namespace obscura::render
