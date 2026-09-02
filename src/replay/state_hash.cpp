// State hashing. See include/obscura/replay/state_hash.hpp for what belongs in
// this file.

#include <obscura/replay/state_hash.hpp>

#include <cstddef>
#include <cstdint>

#include <obscura/core/ledger.hpp>
#include <obscura/world/incident.hpp>
#include <obscura/world/model.hpp>
#include <obscura/world/redaction.hpp>
#include <obscura/world/truth.hpp>

namespace obscura::replay {

namespace {

constexpr Digest kPrime = 0x0000'0100'0000'01B3ULL; // FNV-1a 64-bit prime

} // namespace

auto update(Digest digest, std::uint8_t byte) -> Digest {
  return (digest ^ static_cast<Digest>(byte)) * kPrime;
}

auto update_u64(Digest digest, std::uint64_t value) -> Digest {
  // Little-endian byte order, spelled out rather than memcpy'd from the object
  // representation: the digest has to match on a big-endian host too, and a
  // reinterpret would quietly not.
  for (unsigned shift = 0; shift < 64U; shift += 8U) {
    digest =
        update(digest, static_cast<std::uint8_t>((value >> shift) & 0xFFU));
  }
  return digest;
}

auto hash(const world::Incident& incident) -> Digest {
  Digest digest = kInitial;
  digest = update_u64(digest, incident.culprit);
  digest = update_u64(digest, incident.scene);
  digest = update_u64(digest, incident.when);
  return digest;
}

auto hash(const world::EvidenceSet& evidence) -> Digest {
  Digest digest = kInitial;

  // The count goes in first. Without it, a set of N items and a set of N+1
  // where the extra one hashes to the identity would collide — and "the run
  // produced one more piece of evidence" is exactly the difference this is here
  // to catch.
  digest = update_u64(digest, evidence.size());

  for (const world::Evidence& item : evidence) {
    digest = update_u64(digest, item.id);
    digest = update_u64(digest, item.location);
    digest = update(digest, static_cast<std::uint8_t>(item.kind));
    digest = update_u64(digest, item.asserts.size());
    for (const world::Fact& fact : item.asserts) {
      digest = update_u64(digest, fact.actor);
      digest = update_u64(digest, fact.when);
      digest = update_u64(digest, fact.where);
      digest = update(digest, static_cast<std::uint8_t>(fact.what));
    }
    digest = update(digest, static_cast<std::uint8_t>(item.veracity));
    digest = update(digest, item.requires_);
    digest = update_u64(digest, item.body);
  }

  return digest;
}

auto hash(const world::RedactionMask& mask) -> Digest {
  Digest digest = kInitial;
  digest = update_u64(digest, mask.size());
  for (std::size_t index = 0; index < mask.size(); ++index) {
    digest = update(digest, static_cast<std::uint8_t>(mask.level_of(index)));
  }
  return digest;
}

auto hash(const core::Ledger& ledger) -> Digest {
  Digest digest = kInitial;
  digest = update_u64(digest, ledger.remaining());
  digest = update_u64(digest, ledger.size());

  for (const core::Entry& entry : ledger.entries()) {
    digest = update(digest, static_cast<std::uint8_t>(entry.kind));
    digest = update_u64(digest, entry.subject);
    digest = update_u64(digest, entry.text.size());
    for (const char character : entry.text) {
      digest = update(digest, static_cast<std::uint8_t>(character));
    }
  }

  return digest;
}

} // namespace obscura::replay
