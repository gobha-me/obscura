// The audio sink interface's out-of-line pieces. See
// include/obscura/audio/sink.hpp for what belongs in this file.

#include <obscura/audio/sink.hpp>

namespace obscura::audio {

// Out of line rather than inline in the header so the vtable for Sink and
// NullSink is emitted here, in one translation unit, instead of in every unit
// that includes the header. It also gives the linker a reason to pull this
// object in, which is what makes "the interface exists" a link-time fact.
auto NullSink::play(Cue cue) -> void {
  m_last = cue;
  ++m_count;
}

} // namespace obscura::audio
