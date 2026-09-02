#pragma once

// The complete M1 charge schedule, in one table. Positive values grant charge;
// negative values spend it. Keeping the breached move as its own row avoids a
// hidden multiplier at the call site when balance tuning begins.

#include <array>
#include <cstddef>
#include <cstdint>

namespace obscura::core {

enum class ChargeAction : std::uint8_t {
  Start,
  Move,
  MoveBreached,
  Survey,
  Examine,
  Reread,
  Abort,
  BatchCorrect,
  BatchWrong,
  Count,
};

inline constexpr std::array<std::int32_t,
                            static_cast<std::size_t>(ChargeAction::Count)>
    kChargeEconomy{{120, -1, -3, -8, -3, -2, 0, 15, -20}};

[[nodiscard]] constexpr auto charge_delta(ChargeAction action) -> std::int32_t {
  return kChargeEconomy.at(static_cast<std::size_t>(action));
}

} // namespace obscura::core
