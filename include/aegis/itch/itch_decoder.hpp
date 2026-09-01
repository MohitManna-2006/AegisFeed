#pragma once

#include "aegis/common/result.hpp"
#include "aegis/itch/itch_types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace aegis {

struct ItchDecodeContext {
    std::uint64_t sequence{};
};

[[nodiscard]] Result<SystemEvent> decode_system_event(
    std::span<const std::byte> payload,
    ItchDecodeContext context = {}) noexcept;

[[nodiscard]] Result<StockDirectory> decode_stock_directory(
    std::span<const std::byte> payload,
    ItchDecodeContext context = {}) noexcept;

}  // namespace aegis
