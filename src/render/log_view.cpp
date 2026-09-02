#include <obscura/render/log_view.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
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

namespace {

constexpr int kScrollOriginY = 2;
constexpr int kConsoleOriginY = 38;
constexpr int kBodyIndent = 6;
constexpr termforge::Rgb kForeground{.r = 0xE0, .g = 0xE0, .b = 0xF0};
constexpr termforge::Rgb kDim{.r = 0x78, .g = 0x82, .b = 0x8E};
constexpr termforge::Rgb kBackground{.r = 0x0A, .g = 0x0A, .b = 0x14};

struct ImageBlock {
  std::size_t row{0};
  world::EvidenceId id{world::EVIDENCE_NONE};
  termforge::EncodedImage image{};
};

struct Document {
  std::vector<std::string> rows{};
  std::vector<ImageBlock> images{};
};

auto printable_ascii(std::string_view text, bool allow_empty = false) -> bool {
  if (text.empty()) {
    return allow_empty;
  }
  return std::ranges::all_of(
      text, [](unsigned char byte) { return byte >= 0x20U && byte <= 0x7EU; });
}

auto kind_label(world::EvidenceKind kind) -> std::optional<std::string_view> {
  switch (kind) {
    case world::EvidenceKind::log_fragment: return "LOG FRAGMENT";
    case world::EvidenceKind::physical_trace: return "PHYSICAL TRACE";
    case world::EvidenceKind::corpse: return "CORPSE";
    case world::EvidenceKind::cargo_seal: return "CARGO SEAL";
    case world::EvidenceKind::manifest: return "MANIFEST";
    case world::EvidenceKind::damage_pattern: return "DAMAGE PATTERN";
    case world::EvidenceKind::personal_effect: return "PERSONAL EFFECT";
  }
  return std::nullopt;
}

auto evidence_label(world::EvidenceId id) -> std::string {
  const unsigned shown = static_cast<unsigned>(id) + 1U;
  return shown < 10U ? "E0" + std::to_string(shown)
                     : "E" + std::to_string(shown);
}

auto append_body_row(std::vector<std::string>& rows, std::string& current)
    -> void {
  if (!current.empty()) {
    rows.push_back(std::string(static_cast<std::size_t>(kBodyIndent), ' ') +
                   current);
    current.clear();
  }
}

auto append_body_word(std::vector<std::string>& rows, std::string& current,
                      std::string_view word) -> void {
  if (word.size() > static_cast<std::size_t>(kLogBodyColumns)) {
    append_body_row(rows, current);
    while (word.size() > static_cast<std::size_t>(kLogBodyColumns)) {
      rows.push_back(std::string(static_cast<std::size_t>(kBodyIndent), ' ') +
                     std::string{word.substr(0, kLogBodyColumns)});
      word.remove_prefix(kLogBodyColumns);
    }
  }
  if (!current.empty() && current.size() + 1U + word.size() >
                              static_cast<std::size_t>(kLogBodyColumns)) {
    append_body_row(rows, current);
  }
  if (!word.empty()) {
    if (!current.empty()) {
      current.push_back(' ');
    }
    current.append(word);
  }
}

auto wrap_body(std::string_view body) -> std::vector<std::string> {
  std::vector<std::string> rows{};
  std::string current{};
  std::size_t at = 0;
  while (at < body.size()) {
    while (at < body.size() && body[at] == ' ') {
      ++at;
    }
    const std::size_t start = at;
    while (at < body.size() && body[at] != ' ') {
      ++at;
    }
    append_body_word(rows, current, body.substr(start, at - start));
  }
  append_body_row(rows, current);
  return rows;
}

auto find_projection(std::span<const EvidenceLogProjection> evidence,
                     world::EvidenceId id) -> const EvidenceLogProjection* {
  const auto found =
      std::ranges::find(evidence, id, &EvidenceLogProjection::id);
  return found == evidence.end() ? nullptr : &*found;
}

auto projections_valid(const EvidenceLogInput& input) -> bool {
  if (!printable_ascii(input.case_name) || input.case_name.size() > 48U ||
      input.total_evidence == 0 ||
      input.evidence.size() > input.total_evidence) {
    return false;
  }

  std::vector<world::EvidenceId> projection_ids{};
  projection_ids.reserve(input.evidence.size());
  for (const EvidenceLogProjection& item : input.evidence) {
    if (item.id == world::EVIDENCE_NONE ||
        std::ranges::find(projection_ids, item.id) != projection_ids.end() ||
        !kind_label(item.kind).has_value() ||
        !printable_ascii(item.room_label) || !printable_ascii(item.body) ||
        !printable_ascii(item.instrument, true)) {
      return false;
    }
    projection_ids.push_back(item.id);
  }
  return true;
}

auto append_examined(Document& document, const EvidenceLogInput& input,
                     const core::Entry& entry,
                     std::vector<world::EvidenceId>& examined) -> bool {
  if (entry.subject > std::numeric_limits<world::EvidenceId>::max()) {
    return false;
  }
  const auto id = static_cast<world::EvidenceId>(entry.subject);
  if (std::ranges::find(examined, id) != examined.end()) {
    return false;
  }
  const EvidenceLogProjection* item = find_projection(input.evidence, id);
  if (item == nullptr) {
    return false;
  }
  const std::optional<std::string_view> kind = kind_label(item->kind);
  if (!kind.has_value()) {
    return false;
  }
  examined.push_back(id);

  std::string heading = evidence_label(id) + " | " + std::string{*kind} +
                        " | " + std::string{item->room_label};
  if (!item->instrument.empty()) {
    heading += " | " + std::string{item->instrument};
  }
  heading += " | read at " + std::to_string(entry.remaining) + " SC remaining";
  if (heading.size() > static_cast<std::size_t>(kLogReferenceColumns - 2)) {
    return false;
  }
  document.rows.push_back("  " + heading);
  std::vector<std::string> body = wrap_body(item->body);
  document.rows.insert(document.rows.end(),
                       std::make_move_iterator(body.begin()),
                       std::make_move_iterator(body.end()));
  if (!item->image.empty()) {
    document.images.push_back(
        {.row = document.rows.size(), .id = id, .image = item->image});
    for (int row = 0; row < kLogImageRows; ++row) {
      document.rows.emplace_back();
    }
  }
  document.rows.emplace_back();
  return true;
}

auto build_document(const EvidenceLogInput& input) -> std::optional<Document> {
  if (!projections_valid(input)) {
    return std::nullopt;
  }

  Document document{};
  std::vector<world::EvidenceId> examined{};
  for (const core::Entry& entry : input.ledger.get().entries()) {
    if (entry.kind == core::EntryKind::Examine &&
        !append_examined(document, input, entry, examined)) {
      return std::nullopt;
    }
  }

  if (examined.size() != input.evidence.size() ||
      examined.size() > input.total_evidence) {
    return std::nullopt;
  }
  return document;
}

auto maximum_top(const Document& document) -> std::size_t {
  return document.rows.size() > static_cast<std::size_t>(kLogScrollRows)
             ? document.rows.size() - static_cast<std::size_t>(kLogScrollRows)
             : 0U;
}

auto valid_top(const Document& document, std::size_t top) -> bool {
  if (top > maximum_top(document)) {
    return false;
  }
  const std::size_t bottom = std::min(
      document.rows.size(), top + static_cast<std::size_t>(kLogScrollRows));
  return std::ranges::all_of(document.images, [top, bottom](const auto& image) {
    const std::size_t image_bottom =
        image.row + static_cast<std::size_t>(kLogImageRows);
    const bool overlaps = image.row < bottom && image_bottom > top;
    return !overlaps || (image.row >= top && image_bottom <= bottom);
  });
}

auto valid_tops(const Document& document) -> std::vector<std::size_t> {
  std::vector<std::size_t> result{};
  for (std::size_t top = 0; top <= maximum_top(document); ++top) {
    if (valid_top(document, top)) {
      result.push_back(top);
    }
  }
  return result;
}

auto write_rule(termforge::Screen& screen, int x, int y) -> void {
  std::string rule{};
  rule.reserve(static_cast<std::size_t>(kLogReferenceColumns) * 3U);
  for (int column = 0; column < kLogReferenceColumns; ++column) {
    rule += "\xE2\x94\x80";
  }
  screen.write_text(x, y, rule, kDim, kBackground);
}

} // namespace

auto draw_evidence_log(termforge::Screen& screen, const EvidenceLogInput& input)
    -> EvidenceLogRenderResult {
  if (screen.cols() < kLogReferenceColumns ||
      screen.rows() < kLogReferenceRows) {
    return {.status = EvidenceLogStatus::terminal_too_small};
  }
  const std::optional<Document> document = build_document(input);
  if (!document.has_value()) {
    return {.status = EvidenceLogStatus::invalid_projection};
  }
  if (!valid_top(*document, input.top_row)) {
    return {.status = EvidenceLogStatus::invalid_scroll,
            .document_rows = document->rows.size(),
            .maximum_top_row = maximum_top(*document)};
  }

  const int origin_x = (screen.cols() - kLogReferenceColumns) / 2;
  const int origin_y = (screen.rows() - kLogReferenceRows) / 2;
  const termforge::Rect viewport{.x = origin_x,
                                 .y = origin_y,
                                 .w = kLogReferenceColumns,
                                 .h = kLogReferenceRows};
  screen.fill_rect(viewport.x, viewport.y, viewport.w, viewport.h, kForeground,
                   kBackground);

  const std::size_t examined = input.evidence.size();
  const std::string header =
      "  EVIDENCE LOG | " + std::string{input.case_name} + " | " +
      std::to_string(examined) + " of " + std::to_string(input.total_evidence) +
      " items examined | filter: ALL";
  screen.write_text(origin_x, origin_y, header, kForeground, kBackground);
  write_rule(screen, origin_x, origin_y + 1);

  const std::size_t bottom =
      std::min(document->rows.size(),
               input.top_row + static_cast<std::size_t>(kLogScrollRows));
  for (std::size_t row = input.top_row; row < bottom; ++row) {
    screen.write_text(origin_x,
                      origin_y + kScrollOriginY +
                          static_cast<int>(row - input.top_row),
                      document->rows[row], kForeground, kBackground);
  }
  write_rule(screen, origin_x, origin_y + kConsoleOriginY);
  const std::string console =
      "  [j/k] scroll  [a] actor  [r] room  [t] kind  [/] search  [Esc] back"
      "    rows " +
      std::to_string(document->rows.empty() ? 0U : input.top_row + 1U) + "-" +
      std::to_string(bottom) + "/" + std::to_string(document->rows.size());
  screen.write_text(origin_x, origin_y + kConsoleOriginY + 1, console,
                    kForeground, kBackground);

  EvidenceLogRenderResult result{.status = EvidenceLogStatus::drawn,
                                 .viewport = viewport,
                                 .document_rows = document->rows.size(),
                                 .maximum_top_row = maximum_top(*document),
                                 .images = {}};
  for (const ImageBlock& image : document->images) {
    if (image.row >= input.top_row &&
        image.row + static_cast<std::size_t>(kLogImageRows) <= bottom) {
      result.images.push_back({
          .id = image.id,
          .image = image.image,
          .cells = {.x = origin_x + kBodyIndent,
                    .y = origin_y + kScrollOriginY +
                         static_cast<int>(image.row - input.top_row),
                    .w = kLogImageColumns,
                    .h = kLogImageRows},
      });
    }
  }
  return result;
}

auto scroll_evidence_log(const EvidenceLogInput& input, std::size_t current,
                         int delta) -> std::optional<std::size_t> {
  const std::optional<Document> document = build_document(input);
  if (!document.has_value()) {
    return std::nullopt;
  }
  const std::vector<std::size_t> tops = valid_tops(*document);
  const auto found = std::ranges::find(tops, current);
  if (found == tops.end()) {
    return std::nullopt;
  }
  const std::ptrdiff_t index = found - tops.begin();
  const auto last = static_cast<std::ptrdiff_t>(tops.size() - 1U);
  const std::ptrdiff_t target = std::clamp(
      index + static_cast<std::ptrdiff_t>(delta), std::ptrdiff_t{0}, last);
  return tops[static_cast<std::size_t>(target)];
}

namespace {

auto validate_visible(std::span<const EvidenceImagePlacement> visible,
                      std::vector<termforge::ErrorEvent>& events)
    -> std::optional<std::vector<world::EvidenceId>> {
  std::vector<world::EvidenceId> visible_ids{};
  visible_ids.reserve(visible.size());
  for (const EvidenceImagePlacement& placement : visible) {
    if (placement.id == world::EVIDENCE_NONE || placement.image.empty() ||
        placement.cells.empty() ||
        std::ranges::find(visible_ids, placement.id) != visible_ids.end()) {
      events.push_back({termforge::Severity::Warning, "obscura.log",
                        "invalid or duplicate visible evidence image"});
      return std::nullopt;
    }
    visible_ids.push_back(placement.id);
  }
  return visible_ids;
}

} // namespace

auto EvidenceImageCache::evict_one(
    termforge::TerminalDriver& driver,
    std::span<const world::EvidenceId> visible_ids,
    std::vector<termforge::ErrorEvent>& events) -> bool {
  auto victim = m_entries.end();
  for (auto candidate = m_entries.begin(); candidate != m_entries.end();
       ++candidate) {
    if (std::ranges::find(visible_ids, candidate->id) != visible_ids.end()) {
      continue;
    }
    if (victim == m_entries.end() ||
        candidate->last_visible < victim->last_visible) {
      victim = candidate;
    }
  }
  if (victim == m_entries.end()) {
    events.push_back({termforge::Severity::Warning, "obscura.log",
                      "evidence image quota cannot fit the visible log"});
    return false;
  }
  const world::EvidenceId victim_id = victim->id;
  if (auto released = driver.unpin_image(victim->handle); !released) {
    events.push_back(released.error());
    return false;
  }
  m_entries.erase(victim);
  events.push_back({termforge::Severity::Info, "obscura.log",
                    "evicted " + evidence_label(victim_id) +
                        " from the resident evidence-image quota"});
  return true;
}

auto EvidenceImageCache::ensure_resident(
    termforge::TerminalDriver& driver, const EvidenceImagePlacement& placement,
    std::span<const world::EvidenceId> visible_ids, std::size_t capacity,
    std::vector<termforge::ErrorEvent>& events) -> ResidentIterator {
  auto resident = std::ranges::find(m_entries, placement.id, &Resident::id);
  if (resident != m_entries.end() &&
      !driver.pinned_image_status(resident->handle).valid) {
    resident = m_entries.erase(resident);
  }
  if (resident != m_entries.end()) {
    return resident;
  }
  if (capacity == 0) {
    events.push_back({termforge::Severity::Warning, "obscura.log",
                      "evidence image residency is unavailable"});
    return m_entries.end();
  }
  if (m_entries.size() >= capacity && !evict_one(driver, visible_ids, events)) {
    return m_entries.end();
  }
  auto pinned = driver.pin_image(placement.image);
  if (!pinned) {
    events.push_back(pinned.error());
    return m_entries.end();
  }
  m_entries.push_back(
      {.id = placement.id, .handle = *pinned, .last_visible = 0});
  return std::prev(m_entries.end());
}

auto EvidenceImageCache::draw_visible(
    termforge::TerminalDriver& driver,
    std::span<const EvidenceImagePlacement> visible)
    -> std::vector<termforge::ErrorEvent> {
  std::vector<termforge::ErrorEvent> events{};
  const auto visible_ids = validate_visible(visible, events);
  if (!visible_ids.has_value()) {
    return events;
  }

  const std::size_t capacity =
      std::min(m_resident_limit, driver.max_pinned_images());
  for (const EvidenceImagePlacement& placement : visible) {
    auto resident =
        ensure_resident(driver, placement, *visible_ids, capacity, events);
    if (resident == m_entries.end()) {
      continue;
    }

    resident->last_visible = ++m_sequence;
    if (auto drawn = driver.draw_pinned(placement.cells, resident->handle,
                                        termforge::PlacementFit::Stretch);
        !drawn) {
      events.push_back(drawn.error());
    }
  }
  return events;
}

auto EvidenceImageCache::invalidate() noexcept -> void {
  m_entries.clear();
}

auto EvidenceImageCache::release(termforge::TerminalDriver& driver)
    -> std::vector<termforge::ErrorEvent> {
  std::vector<termforge::ErrorEvent> events{};
  for (const Resident& resident : m_entries) {
    if (auto released = driver.unpin_image(resident.handle); !released) {
      events.push_back(released.error());
    }
  }
  m_entries.clear();
  return events;
}

auto format_entry(const core::Entry& entry) -> std::string {
  switch (entry.kind) {
    case core::EntryKind::Spend:
      return "spend  #" + std::to_string(entry.subject);
    case core::EntryKind::Examine:
      return "examine #" + std::to_string(entry.subject);
    case core::EntryKind::Reread:
      return "reread #" + std::to_string(entry.subject);
    case core::EntryKind::Resolve:
      return "resolve #" + std::to_string(entry.subject);
    case core::EntryKind::Note: return "note   " + entry.text;
    case core::EntryKind::Accuse:
      return "accuse actor " + std::to_string(entry.subject);
  }
  return {};
}

auto tail(const core::Ledger& ledger, std::size_t rows)
    -> std::vector<std::string> {
  const std::vector<core::Entry>& entries = ledger.entries();
  std::vector<std::string> out{};
  if (rows == 0 || entries.empty()) {
    return out;
  }
  const std::size_t take = rows < entries.size() ? rows : entries.size();
  const std::size_t first = entries.size() - take;
  out.reserve(take);
  for (std::size_t index = first; index < entries.size(); ++index) {
    out.push_back(format_entry(entries[index]));
  }
  return out;
}

} // namespace obscura::render
