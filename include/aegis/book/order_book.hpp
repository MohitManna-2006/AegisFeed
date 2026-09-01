#pragma once

#include "aegis/common/result.hpp"
#include "aegis/itch/itch_types.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>

namespace aegis {

using OrderId = std::uint64_t;
using Shares = std::uint32_t;
using StockLocate = std::uint16_t;

struct OrderRecord {
    OrderId id{};
    StockLocate stock_locate{};
    Side side{};
    Price4 price{};
    Shares remaining{};
    Attribution attribution{};
    bool has_attribution{};
};

struct PriceLevel {
    std::uint64_t aggregate_shares{};
    std::uint32_t order_count{};
};

class OrderBook {
public:
    using BidLevels = std::map<Price4, PriceLevel, std::greater<>>;
    using AskLevels = std::map<Price4, PriceLevel, std::less<>>;

    explicit OrderBook(
        StockLocate stock_locate,
        std::size_t expected_active_order_capacity = 0);

    [[nodiscard]] StockLocate stock_locate() const noexcept;
    [[nodiscard]] std::size_t active_order_count() const noexcept;
    [[nodiscard]] std::size_t bid_level_count() const noexcept;
    [[nodiscard]] std::size_t ask_level_count() const noexcept;
    [[nodiscard]] std::optional<Price4> best_bid() const noexcept;
    [[nodiscard]] std::optional<Price4> best_ask() const noexcept;
    [[nodiscard]] std::optional<OrderRecord> order(OrderId order_id) const noexcept;
    [[nodiscard]] std::optional<PriceLevel> bid_level(Price4 price) const noexcept;
    [[nodiscard]] std::optional<PriceLevel> ask_level(Price4 price) const noexcept;

    [[nodiscard]] Result<void> add(const AddOrder& message);

    [[nodiscard]] Result<void> validate_invariants() const noexcept;

private:
    using ActiveOrders = std::unordered_map<OrderId, OrderRecord>;

    void debug_validate_invariants() const noexcept;

    StockLocate stock_locate_{};
    ActiveOrders active_orders_{};
    BidLevels bids_{};
    AskLevels asks_{};
};

}  // namespace aegis
