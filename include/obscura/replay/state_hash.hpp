#pragma once

// State hashing — one number that says whether two runs are the same run.
//
// What belongs here: a stable digest over the parts of a run that must
// reproduce. It is the assertion a replay makes: play the recorded intents back
// against a fresh world from the same seed, hash both, compare. A mismatch
// names the frame where determinism broke, which is far more useful than a test
// that says the ending differed.
//
// Stable means stable across compilers and standard libraries, not just across
// two runs of the same binary. That rules out hashing anything whose iteration
// order is an implementation detail, and it rules out std::hash, whose values
// are explicitly allowed to differ between implementations and between runs.

#include <cstdint>

#include <obscura/core/ledger.hpp>
#include <obscura/world/evidence.hpp>
#include <obscura/world/incident.hpp>
#include <obscura/world/redaction.hpp>

namespace obscura::replay {

using Digest = std::uint64_t;

// FNV-1a's offset basis. Named rather than spelled at each call site so that a
// caller can seed a chain of updates explicitly.
inline constexpr Digest kInitial = 0xCBF29CE484222325ULL;

// One byte in, one digest out. Everything below is built from this, so a new
// field is folded in by calling it — never by inventing a second scheme.
[[nodiscard]] auto update(Digest digest, std::uint8_t byte) -> Digest;

[[nodiscard]] auto update_u64(Digest digest, std::uint64_t value) -> Digest;

[[nodiscard]] auto hash(const world::Incident& incident) -> Digest;

// Order-sensitive by design: world::derive promises a stable sequence, so two
// sets holding the same items in a different order are NOT the same state —
// they came from different code paths, and hiding that would hide the bug.
[[nodiscard]] auto hash(const world::EvidenceSet& evidence) -> Digest;

[[nodiscard]] auto hash(const world::RedactionMask& mask) -> Digest;

[[nodiscard]] auto hash(const core::Ledger& ledger) -> Digest;

} // namespace obscura::replay
