// The ledger log view. See include/obscura/render/log_view.hpp for what belongs
// in this file.

#include <obscura/render/log_view.hpp>

namespace obscura::render {

auto format_entry(const core::Entry& entry) -> std::string {
  switch (entry.kind) {
    case core::EntryKind::Spend:
      return "spend  #" + std::to_string(entry.subject);
    case core::EntryKind::Resolve:
      return "resolve #" + std::to_string(entry.subject);
    case core::EntryKind::Note:
      return "note   " + entry.text;
    case core::EntryKind::Accuse:
      return "accuse actor " + std::to_string(entry.subject);
  }
  return {};
}

auto tail(const core::Ledger& ledger, std::size_t rows) -> std::vector<std::string> {
  const std::vector<core::Entry>& entries = ledger.entries();

  std::vector<std::string> out{};
  if (rows == 0 || entries.empty()) {
    return out;
  }

  // Computed as a subtraction on sizes rather than by walking backwards with an
  // iterator: entries.size() is unsigned, so `size - rows` with rows the larger
  // of the two wraps, and the window would silently become the whole log.
  const std::size_t take  = rows < entries.size() ? rows : entries.size();
  const std::size_t first = entries.size() - take;

  out.reserve(take);
  for (std::size_t index = first; index < entries.size(); ++index) {
    out.push_back(format_entry(entries[index]));
  }

  return out;
}

}  // namespace obscura::render
