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

[[nodiscard]] Result<AddOrder> decode_add_order(
    std::span<const std::byte> payload,
    ItchDecodeContext context = {}) noexcept;

[[nodiscard]] Result<AddOrder> decode_add_order_with_attribution(
    std::span<const std::byte> payload,
    ItchDecodeContext context = {}) noexcept;

[[nodiscard]] Result<OrderExecuted> decode_order_executed(
    std::span<const std::byte> payload,
    ItchDecodeContext context = {}) noexcept;

[[nodiscard]] Result<OrderExecutedWithPrice> decode_order_executed_with_price(
    std::span<const std::byte> payload,
    ItchDecodeContext context = {}) noexcept;

[[nodiscard]] Result<OrderCancel> decode_order_cancel(
    std::span<const std::byte> payload,
    ItchDecodeContext context = {}) noexcept;

[[nodiscard]] Result<OrderDelete> decode_order_delete(
    std::span<const std::byte> payload,
    ItchDecodeContext context = {}) noexcept;

[[nodiscard]] Result<OrderReplace> decode_order_replace(
    std::span<const std::byte> payload,
    ItchDecodeContext context = {}) noexcept;

}  // namespace aegis
