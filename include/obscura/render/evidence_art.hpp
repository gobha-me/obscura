#pragma once

// Baked evidence images used inside the LOG document. Like compartment art,
// these are compiled into the library: opening the log never touches the
// filesystem and cannot turn a missing asset into missing evidence.

#include <termforge/core/types.hpp>

namespace obscura::render {

// Case 001, E02: the cargo-authority chop on the recovered manifest.
[[nodiscard]] auto e02_manifest_seal() -> termforge::EncodedImage;

} // namespace obscura::render
