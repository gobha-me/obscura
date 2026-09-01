#pragma once

// The log view — the scrollback band showing what the run has done.
//
// What belongs here: turning a core::Ledger into lines, and the scroll window
// over them. The view is a projection of the ledger and holds no text of its
// own, so a run's log cannot drift from the run's actual history: there is only
// one copy, and it is the ledger's.
//
// What does not belong here: writing to the ledger. The log view is read-only
// by construction, which is why it takes a const reference and returns values.

#include <cstddef>
#include <string>
#include <vector>

#include <obscura/core/ledger.hpp>

namespace obscura::render {

// Renders the newest `rows` entries, oldest first, so the caller can print top
// to bottom and have the newest at the bottom — which is where a terminal
// reader looks. Fewer than `rows` entries yields a shorter vector, not a padded
// one; padding is the band's job.
[[nodiscard]] auto tail(const core::Ledger& ledger, std::size_t rows)
    -> std::vector<std::string>;

// One entry, formatted. Split out because it is the part with a decision in it
// — everything in tail() above is windowing.
[[nodiscard]] auto format_entry(const core::Entry& entry) -> std::string;

} // namespace obscura::render
