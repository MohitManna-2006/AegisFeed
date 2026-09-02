#include "aegis/book/order_book.hpp"

#include "aegis/common/error.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <string_view>

namespace aegis {
namespace {

[[nodiscard]] Result<void> invariant_failure(
    const std::string_view reason,
    const std::uint64_t observed_value = 0,
    const std::uint64_t limit_value = 0) noexcept
{
    return Result<void>::failure(
        Error::book_invariant_violation(reason, observed_value, limit_value));
}

template <typename LevelMap, typename OrderMap>
[[nodiscard]] Result<void> validate_levels(
    const LevelMap& levels,
    const OrderMap& active_orders,
    const Side expected_side) noexcept
{
    for (const auto& [price, level] : levels) {
        if (level.order_count == 0) {
            return invariant_failure("active price level has zero orders");
        }
        if (level.aggregate_shares == 0) {
            return invariant_failure("active price level has zero aggregate shares");
        }

        std::uint64_t recomputed_shares = 0;
        std::uint64_t recomputed_order_count = 0;
        for (const auto& [order_id, order] : active_orders) {
            static_cast<void>(order_id);
            if (order.side != expected_side || order.price != price) {
                continue;
            }

            if (order.remaining >
                std::numeric_limits<std::uint64_t>::max() - recomputed_shares) {
                return invariant_failure("recomputed aggregate shares overflowed");
            }
            recomputed_shares += static_cast<std::uint64_t>(order.remaining);

            if (recomputed_order_count == std::numeric_limits<std::uint64_t>::max()) {
                return invariant_failure("recomputed price-level order count overflowed");
            }
            ++recomputed_order_count;
        }

        if (recomputed_shares != level.aggregate_shares) {
            return invariant_failure(
                "price-level aggregate shares do not match active orders",
                level.aggregate_shares,
                recomputed_shares);
        }
        if (recomputed_order_count != static_cast<std::uint64_t>(level.order_count)) {
            return invariant_failure(
                "price-level order count does not match active orders",
                level.order_count,
                recomputed_order_count);
        }
    }

    return Result<void>::success();
}

}  // namespace

OrderBook::OrderBook(
    const StockLocate stock_locate,
    const std::size_t expected_active_order_capacity)
    : stock_locate_{stock_locate}
{
    if (expected_active_order_capacity != 0) {
        active_orders_.reserve(expected_active_order_capacity);
    }
    debug_validate_invariants();
}

StockLocate OrderBook::stock_locate() const noexcept
{
    return stock_locate_;
}

std::size_t OrderBook::active_order_count() const noexcept
{
    return active_orders_.size();
}

std::size_t OrderBook::bid_level_count() const noexcept
{
    return bids_.size();
}

std::size_t OrderBook::ask_level_count() const noexcept
{
    return asks_.size();
}

std::optional<Price4> OrderBook::best_bid() const noexcept
{
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.begin()->first;
}

std::optional<Price4> OrderBook::best_ask() const noexcept
{
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.begin()->first;
}

std::optional<OrderRecord> OrderBook::order(const OrderId order_id) const noexcept
{
    const auto found = active_orders_.find(order_id);
    if (found == active_orders_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<PriceLevel> OrderBook::bid_level(const Price4 price) const noexcept
{
    const auto found = bids_.find(price);
    if (found == bids_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<PriceLevel> OrderBook::ask_level(const Price4 price) const noexcept
{
    const auto found = asks_.find(price);
    if (found == asks_.end()) {
        return std::nullopt;
    }
    return found->second;
}

Result<void> OrderBook::add(const AddOrder& message)
{
    if (message.header.stock_locate != stock_locate_) {
        return Result<void>::failure(Error::invalid_add_order(
            "add stock locate does not match containing book",
            message.header.stock_locate,
            stock_locate_));
    }
    if (message.side != Side::Buy && message.side != Side::Sell) {
        return Result<void>::failure(Error::invalid_add_order(
            "add order has an invalid side",
            static_cast<std::uint8_t>(message.side),
            0));
    }
    if (message.shares == 0) {
        return Result<void>::failure(
            Error::invalid_add_order("add order must have positive shares", 0, 1));
    }
    if (message.price_1e4 > kMaxPrice4) {
        return Result<void>::failure(Error::invalid_add_order(
            "add order price exceeds the supported Price4 range",
            message.price_1e4,
            kMaxPrice4));
    }
    if (active_orders_.contains(message.order_reference)) {
        return Result<void>::failure(
            Error::duplicate_order_reference(message.order_reference));
    }

    const OrderRecord record{
        message.order_reference,
        static_cast<StockLocate>(message.header.stock_locate),
        message.side,
        message.price_1e4,
        message.shares,
        message.attribution.value_or(Attribution{}),
        message.attribution.has_value(),
    };

    const auto add_to_side = [&](auto& levels) -> Result<void> {
        const auto existing = levels.find(message.price_1e4);
        if (existing != levels.end()) {
            if (message.shares >
                std::numeric_limits<std::uint64_t>::max() -
                    existing->second.aggregate_shares) {
                return Result<void>::failure(Error::book_arithmetic_overflow(
                    "adding shares would overflow the price-level aggregate",
                    message.shares,
                    std::numeric_limits<std::uint64_t>::max() -
                        existing->second.aggregate_shares));
            }
            if (existing->second.order_count ==
                std::numeric_limits<std::uint32_t>::max()) {
                return Result<void>::failure(Error::book_arithmetic_overflow(
                    "adding an order would overflow the price-level order count",
                    existing->second.order_count,
                    std::numeric_limits<std::uint32_t>::max()));
            }

            active_orders_.emplace(message.order_reference, record);
            existing->second.aggregate_shares += static_cast<std::uint64_t>(message.shares);
            ++existing->second.order_count;
            return Result<void>::success();
        }

        active_orders_.emplace(message.order_reference, record);
        levels.emplace(
            message.price_1e4,
            PriceLevel{static_cast<std::uint64_t>(message.shares), 1});
        return Result<void>::success();
    };

    auto result = message.side == Side::Buy ? add_to_side(bids_) : add_to_side(asks_);
    if (!result) {
        return result;
    }

    debug_validate_invariants();
    return Result<void>::success();
}

Result<void> OrderBook::execute(const OrderExecuted& message)
{
    return reduce(
        message.header.stock_locate,
        message.order_reference,
        message.executed_shares,
        ReductionKind::Execution);
}

Result<void> OrderBook::execute_with_price(const OrderExecutedWithPrice& message)
{
    return reduce(
        message.header.stock_locate,
        message.order_reference,
        message.executed_shares,
        ReductionKind::Execution);
}

Result<void> OrderBook::cancel(const OrderCancel& message)
{
    return reduce(
        message.header.stock_locate,
        message.order_reference,
        message.cancelled_shares,
        ReductionKind::Cancel);
}

Result<void> OrderBook::reduce(
    const StockLocate stock_locate,
    const OrderId order_reference,
    const Shares shares,
    const ReductionKind kind)
{
    if (stock_locate != stock_locate_) {
        return Result<void>::failure(Error::invalid_order_reduction(
            "order reduction stock locate does not match containing book",
            stock_locate,
            stock_locate_));
    }
    if (shares == 0) {
        return Result<void>::failure(Error::invalid_order_reduction(
            "order reduction must have positive shares", shares, 1));
    }

    const auto active_order = active_orders_.find(order_reference);
    if (active_order == active_orders_.end()) {
        return Result<void>::failure(Error::unknown_order_reference(order_reference));
    }
    if (active_order->second.id != order_reference) {
        return invariant_failure("active-order key does not match order record id");
    }
    if (active_order->second.stock_locate != stock_locate_) {
        return invariant_failure(
            "active order stock locate does not match containing book",
            active_order->second.stock_locate,
            stock_locate_);
    }
    if (active_order->second.remaining == 0) {
        return invariant_failure("active order has zero remaining shares");
    }
    if (shares > active_order->second.remaining) {
        const auto error = kind == ReductionKind::Execution
                               ? Error::over_execution(shares, active_order->second.remaining)
                               : Error::over_cancel(shares, active_order->second.remaining);
        return Result<void>::failure(error);
    }

    const auto reduce_from_side = [&](auto& levels) -> Result<void> {
        const auto level = levels.find(active_order->second.price);
        if (level == levels.end()) {
            return invariant_failure("active order has no resting price level");
        }
        if (level->second.order_count == 0) {
            return invariant_failure("resting price level has zero orders");
        }
        if (level->second.aggregate_shares < static_cast<std::uint64_t>(shares)) {
            return invariant_failure(
                "resting price level has insufficient aggregate shares",
                level->second.aggregate_shares,
                shares);
        }

        const bool removes_order = shares == active_order->second.remaining;
        const auto remaining_level_shares =
            level->second.aggregate_shares - static_cast<std::uint64_t>(shares);
        if (removes_order) {
            const bool level_becomes_empty = level->second.order_count == 1;
            if (level_becomes_empty != (remaining_level_shares == 0)) {
                return invariant_failure(
                    "resting price-level count and aggregate disagree on full removal",
                    level->second.order_count,
                    remaining_level_shares);
            }
        } else if (remaining_level_shares == 0) {
            return invariant_failure(
                "partial reduction would leave an active zero-share price level");
        }

        level->second.aggregate_shares = remaining_level_shares;
        if (!removes_order) {
            active_order->second.remaining -= shares;
        } else {
            --level->second.order_count;
            active_orders_.erase(active_order);
            if (level->second.order_count == 0) {
                levels.erase(level);
            }
        }

        debug_validate_invariants();
        return Result<void>::success();
    };

    if (active_order->second.side == Side::Buy) {
        return reduce_from_side(bids_);
    }
    if (active_order->second.side == Side::Sell) {
        return reduce_from_side(asks_);
    }
    return invariant_failure("active order has an invalid side");
}

Result<void> OrderBook::delete_order(const OrderDelete& message)
{
    if (message.header.stock_locate != stock_locate_) {
        return Result<void>::failure(Error::invalid_order_delete(
            "delete stock locate does not match containing book",
            message.header.stock_locate,
            stock_locate_));
    }

    const auto active_order = active_orders_.find(message.order_reference);
    if (active_order == active_orders_.end()) {
        return Result<void>::failure(
            Error::unknown_order_reference(message.order_reference));
    }
    if (active_order->second.id != message.order_reference) {
        return invariant_failure("active-order key does not match order record id");
    }
    if (active_order->second.stock_locate != stock_locate_) {
        return invariant_failure(
            "active order stock locate does not match containing book",
            active_order->second.stock_locate,
            stock_locate_);
    }
    if (active_order->second.remaining == 0) {
        return invariant_failure("active order has zero remaining shares");
    }

    const auto delete_from_side = [&](auto& levels) -> Result<void> {
        const auto level = levels.find(active_order->second.price);
        if (level == levels.end()) {
            return invariant_failure("active order has no resting price level");
        }
        if (level->second.order_count == 0) {
            return invariant_failure("resting price level has zero orders");
        }
        if (level->second.aggregate_shares < active_order->second.remaining) {
            return invariant_failure(
                "resting price level has insufficient aggregate shares",
                level->second.aggregate_shares,
                active_order->second.remaining);
        }

        const auto remaining_level_shares =
            level->second.aggregate_shares - active_order->second.remaining;
        const bool level_becomes_empty = level->second.order_count == 1;
        if (level_becomes_empty != (remaining_level_shares == 0)) {
            return invariant_failure(
                "resting price-level count and aggregate disagree on full removal",
                level->second.order_count,
                remaining_level_shares);
        }

        level->second.aggregate_shares = remaining_level_shares;
        --level->second.order_count;
        active_orders_.erase(active_order);
        if (level->second.order_count == 0) {
            levels.erase(level);
        }

        debug_validate_invariants();
        return Result<void>::success();
    };

    if (active_order->second.side == Side::Buy) {
        return delete_from_side(bids_);
    }
    if (active_order->second.side == Side::Sell) {
        return delete_from_side(asks_);
    }
    return invariant_failure("active order has an invalid side");
}

Result<void> OrderBook::replace(const OrderReplace& message)
{
    if (message.header.stock_locate != stock_locate_) {
        return Result<void>::failure(Error::invalid_order_replace(
            "replace stock locate does not match containing book",
            message.header.stock_locate,
            stock_locate_));
    }
    if (message.shares == 0) {
        return Result<void>::failure(Error::invalid_order_replace(
            "replacement order must have positive shares", message.shares, 1));
    }
    if (message.price_1e4 > kMaxPrice4) {
        return Result<void>::failure(Error::invalid_order_replace(
            "replacement price exceeds the supported Price4 range",
            message.price_1e4,
            kMaxPrice4));
    }

    const auto original = active_orders_.find(message.original_order_reference);
    if (original == active_orders_.end()) {
        return Result<void>::failure(
            Error::unknown_order_reference(message.original_order_reference));
    }
    if (original->second.id != message.original_order_reference) {
        return invariant_failure("active-order key does not match order record id");
    }
    if (original->second.stock_locate != stock_locate_) {
        return invariant_failure(
            "active order stock locate does not match containing book",
            original->second.stock_locate,
            stock_locate_);
    }
    if (original->second.remaining == 0) {
        return invariant_failure("active order has zero remaining shares");
    }
    if (active_orders_.contains(message.new_order_reference)) {
        return Result<void>::failure(
            Error::duplicate_order_reference(message.new_order_reference));
    }

    const auto replace_on_side = [&](auto& levels) -> Result<void> {
        const auto source_level = levels.find(original->second.price);
        if (source_level == levels.end()) {
            return invariant_failure("active order has no resting price level");
        }
        if (source_level->second.order_count == 0) {
            return invariant_failure("resting price level has zero orders");
        }
        if (source_level->second.aggregate_shares < original->second.remaining) {
            return invariant_failure(
                "resting price level has insufficient aggregate shares",
                source_level->second.aggregate_shares,
                original->second.remaining);
        }

        const auto source_shares_after_removal =
            source_level->second.aggregate_shares - original->second.remaining;
        const bool source_level_becomes_empty = source_level->second.order_count == 1;
        if (source_level_becomes_empty != (source_shares_after_removal == 0)) {
            return invariant_failure(
                "resting price-level count and aggregate disagree on full removal",
                source_level->second.order_count,
                source_shares_after_removal);
        }

        const bool same_price = message.price_1e4 == original->second.price;
        auto destination_level = levels.find(message.price_1e4);
        if (same_price) {
            if (message.shares >
                std::numeric_limits<std::uint64_t>::max() -
                    source_shares_after_removal) {
                return Result<void>::failure(Error::book_arithmetic_overflow(
                    "same-price replacement would overflow the price-level aggregate",
                    message.shares,
                    std::numeric_limits<std::uint64_t>::max() -
                        source_shares_after_removal));
            }
        } else if (destination_level != levels.end()) {
            if (destination_level->second.order_count == 0) {
                return invariant_failure("destination price level has zero orders");
            }
            if (destination_level->second.aggregate_shares == 0) {
                return invariant_failure(
                    "destination price level has zero aggregate shares");
            }
            if (message.shares >
                std::numeric_limits<std::uint64_t>::max() -
                    destination_level->second.aggregate_shares) {
                return Result<void>::failure(Error::book_arithmetic_overflow(
                    "replacement would overflow the destination-level aggregate",
                    message.shares,
                    std::numeric_limits<std::uint64_t>::max() -
                        destination_level->second.aggregate_shares));
            }
            if (destination_level->second.order_count ==
                std::numeric_limits<std::uint32_t>::max()) {
                return Result<void>::failure(Error::book_arithmetic_overflow(
                    "replacement would overflow the destination-level order count",
                    destination_level->second.order_count,
                    std::numeric_limits<std::uint32_t>::max()));
            }
        }

        const OrderRecord replacement{
            message.new_order_reference,
            original->second.stock_locate,
            original->second.side,
            message.price_1e4,
            message.shares,
            original->second.attribution,
            original->second.has_attribution,
        };

        if (same_price) {
            source_level->second.aggregate_shares =
                source_shares_after_removal + message.shares;
            active_orders_.erase(original);
            active_orders_.emplace(message.new_order_reference, replacement);
        } else {
            source_level->second.aggregate_shares = source_shares_after_removal;
            --source_level->second.order_count;
            if (source_level->second.order_count == 0) {
                levels.erase(source_level);
            }

            active_orders_.erase(original);
            active_orders_.emplace(message.new_order_reference, replacement);

            if (destination_level != levels.end()) {
                destination_level->second.aggregate_shares += message.shares;
                ++destination_level->second.order_count;
            } else {
                levels.emplace(
                    message.price_1e4,
                    PriceLevel{static_cast<std::uint64_t>(message.shares), 1});
            }
        }

        debug_validate_invariants();
        return Result<void>::success();
    };

    if (original->second.side == Side::Buy) {
        return replace_on_side(bids_);
    }
    if (original->second.side == Side::Sell) {
        return replace_on_side(asks_);
    }
    return invariant_failure("active order has an invalid side");
}

CanonicalOrderBook OrderBook::canonical_snapshot() const
{
    CanonicalOrderBook snapshot;
    snapshot.stock_locate = stock_locate_;
    snapshot.bids.reserve(bids_.size());
    snapshot.asks.reserve(asks_.size());
    snapshot.orders.reserve(active_orders_.size());

    for (const auto& [price, level] : bids_) {
        snapshot.bids.push_back(
            CanonicalPriceLevel{price, level.aggregate_shares, level.order_count});
    }
    for (const auto& [price, level] : asks_) {
        snapshot.asks.push_back(
            CanonicalPriceLevel{price, level.aggregate_shares, level.order_count});
    }
    for (const auto& [order_id, order] : active_orders_) {
        static_cast<void>(order_id);
        snapshot.orders.push_back(CanonicalOrder{
            order.id,
            order.stock_locate,
            order.side,
            order.price,
            order.remaining,
            order.attribution,
            order.has_attribution,
        });
    }

    std::ranges::sort(snapshot.orders, {}, &CanonicalOrder::id);
    return snapshot;
}

Result<void> OrderBook::validate_invariants() const noexcept
{
    const auto bid_result = validate_levels(bids_, active_orders_, Side::Buy);
    if (!bid_result) {
        return bid_result;
    }

    const auto ask_result = validate_levels(asks_, active_orders_, Side::Sell);
    if (!ask_result) {
        return ask_result;
    }

    for (const auto& [order_id, order] : active_orders_) {
        if (order.id != order_id) {
            return invariant_failure("active-order key does not match order record id");
        }
        if (order.remaining == 0) {
            return invariant_failure("active order has zero remaining shares");
        }
        if (order.stock_locate != stock_locate_) {
            return invariant_failure(
                "active order stock locate does not match containing book",
                order.stock_locate,
                stock_locate_);
        }

        if (order.side == Side::Buy) {
            if (bids_.find(order.price) == bids_.end()) {
                return invariant_failure("active buy order has no bid price level");
            }
        } else if (order.side == Side::Sell) {
            if (asks_.find(order.price) == asks_.end()) {
                return invariant_failure("active sell order has no ask price level");
            }
        } else {
            return invariant_failure("active order has an invalid side");
        }
    }

    return Result<void>::success();
}

void OrderBook::debug_validate_invariants() const noexcept
{
#ifndef NDEBUG
    assert(validate_invariants().has_value());
#endif
}

}  // namespace aegis
