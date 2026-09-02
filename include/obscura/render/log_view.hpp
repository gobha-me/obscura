#pragma once

// LOG mode: a fixed-grid projection of examined evidence plus the terminal-
// resident images embedded in that document. Cell composition and image
// emission are separate because TermForge requires direct image work in
// App::on_pixels(), after the Screen diff.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <obscura/core/ledger.hpp>
#include <obscura/world/model.hpp>

#include <termforge/core/screen.hpp>
#include <termforge/core/types.hpp>
#include <termforge/drivers/terminal_driver.hpp>

namespace obscura::render {

inline constexpr int kLogReferenceColumns = 120;
inline constexpr int kLogReferenceRows = 40;
inline constexpr int kLogScrollRows = 36;
inline constexpr int kLogBodyColumns = 96;
inline constexpr int kLogImageColumns = 16;
inline constexpr int kLogImageRows = 3;

struct EvidenceLogProjection {
  world::EvidenceId id{world::EVIDENCE_NONE};
  world::EvidenceKind kind{world::EvidenceKind::physical_trace};
  std::string_view room_label{};
  std::string_view body{};
  std::string_view instrument{};
  termforge::EncodedImage image{};
};

struct EvidenceLogInput {
  std::reference_wrapper<const core::Ledger> ledger;
  std::string_view case_name{};
  std::span<const EvidenceLogProjection> evidence{};
  std::size_t total_evidence{0};
  std::size_t top_row{0};
};

enum class EvidenceLogStatus : std::uint8_t {
  drawn,
  terminal_too_small,
  invalid_projection,
  invalid_scroll,
};

struct EvidenceImagePlacement {
  world::EvidenceId id{world::EVIDENCE_NONE};
  termforge::EncodedImage image{};
  termforge::Rect cells{};
};

struct EvidenceLogRenderResult {
  EvidenceLogStatus status{EvidenceLogStatus::invalid_projection};
  termforge::Rect viewport{};
  std::size_t document_rows{0};
  std::size_t maximum_top_row{0};
  std::vector<EvidenceImagePlacement> images{};
};

// Draws only after validating the complete projection and scroll boundary. The
// returned image placements are emitted later through EvidenceImageCache.
[[nodiscard]] auto draw_evidence_log(termforge::Screen& screen,
                                     const EvidenceLogInput& input)
    -> EvidenceLogRenderResult;

// Moves over valid document boundaries. Text advances one row at a time;
// image blocks are atomic, so a step that would bisect one skips to its far
// edge. Positive moves toward newer content.
[[nodiscard]] auto scroll_evidence_log(const EvidenceLogInput& input,
                                       std::size_t current, int delta)
    -> std::optional<std::size_t>;

class EvidenceImageCache {
 public:
  explicit EvidenceImageCache(
      std::size_t resident_limit = std::numeric_limits<std::size_t>::max())
      : m_resident_limit(resident_limit) {}

  EvidenceImageCache(const EvidenceImageCache&) = delete;
  auto operator=(const EvidenceImageCache&) -> EvidenceImageCache& = delete;
  EvidenceImageCache(EvidenceImageCache&&) = delete;
  auto operator=(EvidenceImageCache&&) -> EvidenceImageCache& = delete;
  ~EvidenceImageCache() = default;

  // Places every visible image. Payloads remain pinned when they scroll out;
  // quota pressure evicts the least-recently-visible non-visible image.
  [[nodiscard]] auto draw_visible(
      termforge::TerminalDriver& driver,
      std::span<const EvidenceImagePlacement> visible)
      -> std::vector<termforge::ErrorEvent>;

  // Use after ImageInvalidatedEvent: the terminal has already discarded the
  // payloads, so forgetting handles must not queue protocol deletes.
  auto invalidate() noexcept -> void;

  // Explicit view-close lifecycle. Errors are surfaced; local handles are
  // forgotten either way so a later open always re-pins from compiled bytes.
  [[nodiscard]] auto release(termforge::TerminalDriver& driver)
      -> std::vector<termforge::ErrorEvent>;

  [[nodiscard]] auto resident_count() const noexcept -> std::size_t {
    return m_entries.size();
  }

 private:
  struct Resident {
    world::EvidenceId id{world::EVIDENCE_NONE};
    termforge::PinnedImage handle{};
    std::uint64_t last_visible{0};
  };

  using ResidentIterator = std::vector<Resident>::iterator;

  [[nodiscard]] auto evict_one(termforge::TerminalDriver& driver,
                               std::span<const world::EvidenceId> visible_ids,
                               std::vector<termforge::ErrorEvent>& events)
      -> bool;
  [[nodiscard]] auto ensure_resident(
      termforge::TerminalDriver& driver,
      const EvidenceImagePlacement& placement,
      std::span<const world::EvidenceId> visible_ids, std::size_t capacity,
      std::vector<termforge::ErrorEvent>& events) -> ResidentIterator;

  std::size_t m_resident_limit;
  std::uint64_t m_sequence{0};
  std::vector<Resident> m_entries{};
};

// Compatibility helpers for the compact ledger strip and existing callers.
[[nodiscard]] auto tail(const core::Ledger& ledger, std::size_t rows)
    -> std::vector<std::string>;
[[nodiscard]] auto format_entry(const core::Entry& entry) -> std::string;

} // namespace obscura::render
