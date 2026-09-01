#pragma once

// The audio sink interface, and the implementation that does nothing.
//
// What belongs here: the cue vocabulary and the one virtual call that plays it.
// A sink is deliberately fire-and-forget — the game says "a plate resolved",
// not "play resolve.wav at 40% for 300ms". Anything that would make audio
// depend on a mixer's state belongs behind a concrete sink, not in this header.
//
// NullSink is not a placeholder to be replaced later. It is the default, and it
// is what every test runs against: a headless CI box has no audio device, and a
// suite that needed one would be a suite nobody runs. A real sink is an
// injected alternative, never the assumption.

#include <cstdint>

namespace obscura::audio {

// A closed set of cues, not a filename. The game names the *event*; a sink
// decides what that sounds like, so swapping a sound pack touches one
// implementation rather than every call site.
enum class Cue : std::uint8_t {
  Resolve, // a plate gained fidelity
  Deny,    // an action was refused (no attention left, illegal move)
  Arm,     // the commit gesture armed
  Commit,  // an accusation fired
  Verdict, // the run resolved
};

class Sink {
 public:
  Sink() = default;
  virtual ~Sink() = default;

  Sink(const Sink&) = delete;
  auto operator=(const Sink&) -> Sink& = delete;
  Sink(Sink&&) = delete;
  auto operator=(Sink&&) -> Sink& = delete;

  // Non-blocking by contract. A sink that needs to do work owns its own thread;
  // this is called from the render loop, where a stall is a dropped frame.
  virtual auto play(Cue cue) -> void = 0;
};

class NullSink final : public Sink {
 public:
  auto play(Cue cue) -> void override;

  // Counting rather than truly doing nothing: a test can assert that the game
  // asked for the cue it should have, without a device, a file or a mixer.
  [[nodiscard]] auto count() const -> std::uint32_t { return m_count; }
  [[nodiscard]] auto last() const -> Cue { return m_last; }

 private:
  std::uint32_t m_count{0};
  Cue m_last{Cue::Resolve};
};

} // namespace obscura::audio
