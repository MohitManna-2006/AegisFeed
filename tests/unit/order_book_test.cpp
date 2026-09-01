#include "aegis/book/order_book.hpp"

#include "aegis/common/error.hpp"
#include "aegis/itch/itch_types.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

static_assert(std::is_same_v<aegis::OrderId, std::uint64_t>);
static_assert(std::is_same_v<aegis::Shares, std::uint32_t>);
static_assert(std::is_same_v<aegis::StockLocate, std::uint16_t>);
static_assert(std::is_same_v<decltype(aegis::OrderRecord{}.id), aegis::OrderId>);
static_assert(
    std::is_same_v<decltype(aegis::OrderRecord{}.stock_locate), aegis::StockLocate>);
static_assert(std::is_same_v<decltype(aegis::OrderRecord{}.side), aegis::Side>);
static_assert(std::is_same_v<decltype(aegis::OrderRecord{}.price), aegis::Price4>);
static_assert(std::is_same_v<decltype(aegis::OrderRecord{}.remaining), aegis::Shares>);
static_assert(
    std::is_same_v<decltype(aegis::OrderRecord{}.attribution), aegis::Attribution>);
static_assert(std::is_same_v<decltype(aegis::OrderRecord{}.has_attribution), bool>);
static_assert(
    std::is_same_v<decltype(aegis::PriceLevel{}.aggregate_shares), std::uint64_t>);
static_assert(std::is_same_v<decltype(aegis::PriceLevel{}.order_count), std::uint32_t>);
static_assert(sizeof(aegis::PriceLevel{}.aggregate_shares) >= sizeof(std::uint64_t));
static_assert(
    std::is_same_v<typename aegis::OrderBook::BidLevels::key_type, aegis::Price4>);
static_assert(
    std::is_same_v<typename aegis::OrderBook::AskLevels::key_type, aegis::Price4>);
static_assert(std::is_same_v<typename aegis::OrderBook::BidLevels::key_compare,
                             std::greater<void>>);
static_assert(std::is_same_v<typename aegis::OrderBook::AskLevels::key_compare,
                             std::less<void>>);
static_assert(
    std::is_same_v<decltype(std::declval<const aegis::OrderBook&>().best_bid()),
                   std::optional<aegis::Price4>>);
static_assert(
    std::is_same_v<decltype(std::declval<const aegis::OrderBook&>().best_ask()),
                   std::optional<aegis::Price4>>);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::BookInvariantViolation) == 12);

std::size_t failure_count = 0;

void check(const bool condition, const std::string_view expression, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << "order_book_test.cpp:" << line << ": check failed: " << expression << '\n';
    ++failure_count;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

void check_logically_empty(const aegis::OrderBook& book)
{
    CHECK(book.active_order_count() == 0);
    CHECK(book.bid_level_count() == 0);
    CHECK(book.ask_level_count() == 0);
    CHECK(book.best_bid() == std::nullopt);
    CHECK(book.best_ask() == std::nullopt);
}

void test_order_record_model()
{
    const aegis::OrderRecord order{
        0x0102'0304'0506'0708ULL,
        0x1234,
        aegis::Side::Sell,
        123'456U,
        4'000'000'000U,
        {'A', 'E', 'G', 'S'},
        true,
    };

    CHECK(order.id == 0x0102'0304'0506'0708ULL);
    CHECK(order.stock_locate == 0x1234);
    CHECK(order.side == aegis::Side::Sell);
    CHECK(order.price == 123'456U);
    CHECK(order.remaining == 4'000'000'000U);
    CHECK(order.attribution == aegis::Attribution({'A', 'E', 'G', 'S'}));
    CHECK(order.has_attribution);
}

void test_empty_book_and_identity()
{
    const aegis::OrderBook zero_locate_book{0};
    CHECK(zero_locate_book.stock_locate() == 0);
    check_logically_empty(zero_locate_book);

    const aegis::OrderBook located_book{0xBEEF};
    CHECK(located_book.stock_locate() == 0xBEEF);
    check_logically_empty(located_book);
}

void test_capacity_preparation()
{
    const aegis::OrderBook zero_capacity{17, 0};
    CHECK(zero_capacity.stock_locate() == 17);
    check_logically_empty(zero_capacity);

    const aegis::OrderBook reserved_capacity{23, 4'096};
    CHECK(reserved_capacity.stock_locate() == 23);
    check_logically_empty(reserved_capacity);
}

void test_empty_invariants_are_stable_and_const()
{
    const aegis::OrderBook book{42, 128};
    const auto active_orders_before = book.active_order_count();
    const auto bid_levels_before = book.bid_level_count();
    const auto ask_levels_before = book.ask_level_count();

    const auto first_result = book.validate_invariants();
    const auto second_result = book.validate_invariants();

    CHECK(first_result.has_value());
    CHECK(second_result.has_value());
    CHECK(book.active_order_count() == active_orders_before);
    CHECK(book.bid_level_count() == bid_levels_before);
    CHECK(book.ask_level_count() == ask_levels_before);
    CHECK(book.best_bid() == std::nullopt);
    CHECK(book.best_ask() == std::nullopt);
}

}  // namespace

int main()
{
    test_order_record_model();
    test_empty_book_and_identity();
    test_capacity_preparation();
    test_empty_invariants_are_stable_and_const();
    return failure_count == 0 ? 0 : 1;
}
