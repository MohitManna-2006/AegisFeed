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
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::ConflictingStockLocate) == 15);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::InvalidAddOrder) == 16);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::DuplicateOrderReference) == 17);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::BookArithmeticOverflow) == 18);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::InvalidOrderReduction) == 21);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::UnknownOrderReference) == 22);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::OverExecution) == 23);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::OverCancel) == 24);
static_assert(
    std::is_same_v<decltype(std::declval<aegis::OrderBook&>().execute(
                       std::declval<const aegis::OrderExecuted&>())),
                   aegis::Result<void>>);
static_assert(
    std::is_same_v<decltype(std::declval<aegis::OrderBook&>().execute_with_price(
                       std::declval<const aegis::OrderExecutedWithPrice&>())),
                   aegis::Result<void>>);
static_assert(
    std::is_same_v<decltype(std::declval<aegis::OrderBook&>().cancel(
                       std::declval<const aegis::OrderCancel&>())),
                   aegis::Result<void>>);

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

[[nodiscard]] aegis::AddOrder add_order(
    const aegis::OrderId order_id,
    const aegis::StockLocate stock_locate,
    const aegis::Side side,
    const aegis::Shares shares,
    const aegis::Price4 price,
    const std::optional<aegis::Attribution> attribution = std::nullopt)
{
    aegis::AddOrder message{};
    message.header.type = attribution.has_value() ? 'F' : 'A';
    message.header.stock_locate = stock_locate;
    message.order_reference = order_id;
    message.side = side;
    message.shares = shares;
    message.price_1e4 = price;
    message.attribution = attribution;
    return message;
}

[[nodiscard]] aegis::OrderExecuted execution(
    const aegis::OrderId order_id,
    const aegis::StockLocate stock_locate,
    const aegis::Shares shares)
{
    aegis::OrderExecuted message{};
    message.header.type = 'E';
    message.header.stock_locate = stock_locate;
    message.order_reference = order_id;
    message.executed_shares = shares;
    message.match_number = 0x0102'0304'0506'0708ULL;
    return message;
}

[[nodiscard]] aegis::OrderExecutedWithPrice execution_with_price(
    const aegis::OrderId order_id,
    const aegis::StockLocate stock_locate,
    const aegis::Shares shares,
    const aegis::Price4 execution_price)
{
    aegis::OrderExecutedWithPrice message{};
    message.header.type = 'C';
    message.header.stock_locate = stock_locate;
    message.order_reference = order_id;
    message.executed_shares = shares;
    message.match_number = 0x1112'1314'1516'1718ULL;
    message.printable = true;
    message.execution_price_1e4 = execution_price;
    return message;
}

[[nodiscard]] aegis::OrderCancel cancellation(
    const aegis::OrderId order_id,
    const aegis::StockLocate stock_locate,
    const aegis::Shares shares)
{
    aegis::OrderCancel message{};
    message.header.type = 'X';
    message.header.stock_locate = stock_locate;
    message.order_reference = order_id;
    message.cancelled_shares = shares;
    return message;
}

void check_error(
    const aegis::Error* error,
    const aegis::ErrorCategory category,
    const aegis::ErrorCode code)
{
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->category == category);
        CHECK(error->code == code);
    }
}

void check_invariants(const aegis::OrderBook& book)
{
    CHECK(book.validate_invariants().has_value());
}

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

void test_basic_buy()
{
    aegis::OrderBook book{42};
    const auto result = book.add(add_order(1, 42, aegis::Side::Buy, 100, 10'000));
    CHECK(result.has_value());

    CHECK(book.active_order_count() == 1);
    CHECK(book.bid_level_count() == 1);
    CHECK(book.ask_level_count() == 0);
    CHECK(book.best_bid() == std::optional<aegis::Price4>{10'000});
    CHECK(book.best_ask() == std::nullopt);

    const auto level = book.bid_level(10'000);
    CHECK(level.has_value());
    if (level) {
        CHECK(level->aggregate_shares == 100);
        CHECK(level->order_count == 1);
    }
    CHECK(book.ask_level(10'000) == std::nullopt);

    const auto order = book.order(1);
    CHECK(order.has_value());
    if (order) {
        CHECK(order->id == 1);
        CHECK(order->stock_locate == 42);
        CHECK(order->side == aegis::Side::Buy);
        CHECK(order->price == 10'000);
        CHECK(order->remaining == 100);
        CHECK(!order->has_attribution);
        CHECK(order->attribution == aegis::Attribution{});
    }
    CHECK(book.order(999) == std::nullopt);
    check_invariants(book);
}

void test_basic_sell()
{
    aegis::OrderBook book{42};
    const auto result = book.add(add_order(2, 42, aegis::Side::Sell, 75, 11'000));
    CHECK(result.has_value());

    CHECK(book.active_order_count() == 1);
    CHECK(book.bid_level_count() == 0);
    CHECK(book.ask_level_count() == 1);
    CHECK(book.best_bid() == std::nullopt);
    CHECK(book.best_ask() == std::optional<aegis::Price4>{11'000});

    const auto level = book.ask_level(11'000);
    CHECK(level.has_value());
    if (level) {
        CHECK(level->aggregate_shares == 75);
        CHECK(level->order_count == 1);
    }
    CHECK(book.bid_level(11'000) == std::nullopt);
    check_invariants(book);
}

void test_same_price_aggregation()
{
    aegis::OrderBook book{7};
    CHECK(book.add(add_order(1, 7, aegis::Side::Buy, 10, 100)).has_value());
    CHECK(book.add(add_order(2, 7, aegis::Side::Buy, 25, 100)).has_value());

    CHECK(book.active_order_count() == 2);
    CHECK(book.bid_level_count() == 1);
    const auto level = book.bid_level(100);
    CHECK(level.has_value());
    if (level) {
        CHECK(level->aggregate_shares == 35);
        CHECK(level->order_count == 2);
    }
    CHECK(book.order(1)->remaining == 10);
    CHECK(book.order(2)->remaining == 25);
    check_invariants(book);
}

void test_multiple_prices_and_both_sides()
{
    aegis::OrderBook book{9};
    CHECK(book.add(add_order(1, 9, aegis::Side::Buy, 1, 100)).has_value());
    CHECK(book.add(add_order(2, 9, aegis::Side::Buy, 2, 105)).has_value());
    CHECK(book.add(add_order(3, 9, aegis::Side::Buy, 3, 103)).has_value());
    CHECK(book.add(add_order(4, 9, aegis::Side::Sell, 4, 110)).has_value());
    CHECK(book.add(add_order(5, 9, aegis::Side::Sell, 5, 107)).has_value());
    CHECK(book.add(add_order(6, 9, aegis::Side::Sell, 6, 115)).has_value());

    CHECK(book.active_order_count() == 6);
    CHECK(book.bid_level_count() == 3);
    CHECK(book.ask_level_count() == 3);
    CHECK(book.best_bid() == std::optional<aegis::Price4>{105});
    CHECK(book.best_ask() == std::optional<aegis::Price4>{107});
    CHECK(book.bid_level(105)->aggregate_shares == 2);
    CHECK(book.ask_level(107)->aggregate_shares == 5);
    check_invariants(book);

    const auto active_before = book.active_order_count();
    const auto bids_before = book.bid_level_count();
    const auto asks_before = book.ask_level_count();
    const auto best_bid_before = book.best_bid();
    const auto best_ask_before = book.best_ask();
    CHECK(book.validate_invariants().has_value());
    CHECK(book.active_order_count() == active_before);
    CHECK(book.bid_level_count() == bids_before);
    CHECK(book.ask_level_count() == asks_before);
    CHECK(book.best_bid() == best_bid_before);
    CHECK(book.best_ask() == best_ask_before);
}

void test_crossed_book_is_accepted()
{
    aegis::OrderBook book{11};
    CHECK(book.add(add_order(1, 11, aegis::Side::Buy, 10, 120)).has_value());
    CHECK(book.add(add_order(2, 11, aegis::Side::Sell, 10, 110)).has_value());
    CHECK(book.best_bid() == std::optional<aegis::Price4>{120});
    CHECK(book.best_ask() == std::optional<aegis::Price4>{110});
    check_invariants(book);
}

void test_attribution_metadata()
{
    aegis::OrderBook book{12};
    CHECK(book.add(add_order(1, 12, aegis::Side::Sell, 20, 500)).has_value());
    CHECK(book.add(add_order(
              2,
              12,
              aegis::Side::Sell,
              30,
              500,
              aegis::Attribution{'M', 'P', 'I', 'D'}))
              .has_value());

    const auto plain = book.order(1);
    const auto attributed = book.order(2);
    CHECK(plain.has_value());
    CHECK(attributed.has_value());
    if (plain) {
        CHECK(!plain->has_attribution);
        CHECK(plain->attribution == aegis::Attribution{});
    }
    if (attributed) {
        CHECK(attributed->has_attribution);
        CHECK(attributed->attribution == aegis::Attribution({'M', 'P', 'I', 'D'}));
    }

    const auto level = book.ask_level(500);
    CHECK(level.has_value());
    if (level) {
        CHECK(level->aggregate_shares == 50);
        CHECK(level->order_count == 2);
    }
    check_invariants(book);
}

void test_price_boundaries()
{
    aegis::OrderBook book{13};
    CHECK(book.add(add_order(1, 13, aegis::Side::Buy, 1, 0)).has_value());
    CHECK(book.add(add_order(2, 13, aegis::Side::Buy, 1, aegis::kMaxPrice4)).has_value());
    CHECK(book.bid_level(0).has_value());
    CHECK(book.bid_level(aegis::kMaxPrice4).has_value());
    CHECK(book.best_bid() == std::optional<aegis::Price4>{aegis::kMaxPrice4});
    check_invariants(book);

    const auto rejected = book.add(
        add_order(3, 13, aegis::Side::Buy, 1, aegis::kMaxPrice4 + 1U));
    CHECK(!rejected.has_value());
    check_error(
        rejected.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::InvalidAddOrder);
    CHECK(book.active_order_count() == 2);
    CHECK(book.bid_level_count() == 2);
    CHECK(book.order(3) == std::nullopt);
    CHECK(book.bid_level(aegis::kMaxPrice4 + 1U) == std::nullopt);
    CHECK(book.best_bid() == std::optional<aegis::Price4>{aegis::kMaxPrice4});
    check_invariants(book);
}

void test_zero_shares_rejected()
{
    aegis::OrderBook book{14};
    const auto result = book.add(add_order(1, 14, aegis::Side::Buy, 0, 100));
    CHECK(!result.has_value());
    check_error(
        result.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::InvalidAddOrder);
    check_logically_empty(book);
    CHECK(book.order(1) == std::nullopt);
    check_invariants(book);
}

void test_wrong_locate_rejected()
{
    aegis::OrderBook book{0};
    const auto result = book.add(add_order(1, 1, aegis::Side::Buy, 10, 100));
    CHECK(!result.has_value());
    check_error(
        result.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::InvalidAddOrder);
    check_logically_empty(book);
    check_invariants(book);

    CHECK(book.add(add_order(2, 0, aegis::Side::Buy, 10, 100)).has_value());
    CHECK(book.order(2)->stock_locate == 0);
    check_invariants(book);
}

void test_invalid_side_rejected()
{
    aegis::OrderBook book{15};
    const auto invalid_side = static_cast<aegis::Side>(static_cast<std::uint8_t>('X'));
    const auto result = book.add(add_order(1, 15, invalid_side, 10, 100));
    CHECK(!result.has_value());
    check_error(
        result.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::InvalidAddOrder);
    check_logically_empty(book);
    check_invariants(book);
}

void test_duplicate_order_is_transactional()
{
    aegis::OrderBook book{16};
    CHECK(book.add(add_order(1, 16, aegis::Side::Buy, 100, 10'000)).has_value());

    const auto result = book.add(add_order(1, 16, aegis::Side::Sell, 500, 20'000));
    CHECK(!result.has_value());
    check_error(
        result.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::DuplicateOrderReference);
    if (result.error() != nullptr) {
        CHECK(result.error()->observed_value == 1);
    }

    CHECK(book.active_order_count() == 1);
    CHECK(book.bid_level_count() == 1);
    CHECK(book.ask_level_count() == 0);
    CHECK(book.best_bid() == std::optional<aegis::Price4>{10'000});
    CHECK(book.best_ask() == std::nullopt);
    const auto existing_order = book.order(1);
    CHECK(existing_order.has_value());
    if (existing_order) {
        CHECK(existing_order->side == aegis::Side::Buy);
        CHECK(existing_order->remaining == 100);
        CHECK(existing_order->price == 10'000);
    }
    const auto existing_level = book.bid_level(10'000);
    CHECK(existing_level.has_value());
    if (existing_level) {
        CHECK(existing_level->aggregate_shares == 100);
        CHECK(existing_level->order_count == 1);
    }
    CHECK(book.ask_level(20'000) == std::nullopt);
    check_invariants(book);
}

void test_partial_and_full_execution()
{
    aegis::OrderBook book{42};
    CHECK(book.add(add_order(1, 42, aegis::Side::Buy, 100, 10'000)).has_value());

    CHECK(book.execute(execution(1, 42, 40)).has_value());
    CHECK(book.active_order_count() == 1);
    CHECK(book.order(1)->remaining == 60);
    CHECK(book.order(1)->price == 10'000);
    CHECK(book.bid_level(10'000)->aggregate_shares == 60);
    CHECK(book.bid_level(10'000)->order_count == 1);
    CHECK(book.best_bid() == std::optional<aegis::Price4>{10'000});
    check_invariants(book);

    CHECK(book.execute(execution(1, 42, 60)).has_value());
    CHECK(book.order(1) == std::nullopt);
    check_logically_empty(book);
    check_invariants(book);
}

void test_full_execution_preserves_same_level_and_updates_best_bid()
{
    aegis::OrderBook book{43};
    CHECK(book.add(add_order(1, 43, aegis::Side::Buy, 50, 10'500)).has_value());
    CHECK(book.add(add_order(2, 43, aegis::Side::Buy, 100, 10'500)).has_value());
    CHECK(book.add(add_order(3, 43, aegis::Side::Buy, 75, 10'000)).has_value());
    CHECK(book.add(add_order(4, 43, aegis::Side::Sell, 80, 10'700)).has_value());

    CHECK(book.execute(execution(1, 43, 50)).has_value());
    CHECK(book.order(1) == std::nullopt);
    CHECK(book.order(2)->remaining == 100);
    CHECK(book.bid_level(10'500)->aggregate_shares == 100);
    CHECK(book.bid_level(10'500)->order_count == 1);
    CHECK(book.best_bid() == std::optional<aegis::Price4>{10'500});
    CHECK(book.ask_level(10'700)->aggregate_shares == 80);

    CHECK(book.execute(execution(2, 43, 100)).has_value());
    CHECK(book.bid_level(10'500) == std::nullopt);
    CHECK(book.best_bid() == std::optional<aegis::Price4>{10'000});
    CHECK(book.ask_level(10'700)->aggregate_shares == 80);
    CHECK(book.best_ask() == std::optional<aegis::Price4>{10'700});
    check_invariants(book);
}

void test_execution_failures_are_transactional()
{
    aegis::OrderBook book{44};
    CHECK(book.add(add_order(1, 44, aegis::Side::Buy, 100, 10'000)).has_value());
    CHECK(book.add(add_order(2, 44, aegis::Side::Sell, 70, 11'000)).has_value());

    const auto check_unchanged = [&] {
        CHECK(book.active_order_count() == 2);
        CHECK(book.bid_level_count() == 1);
        CHECK(book.ask_level_count() == 1);
        CHECK(book.order(1)->remaining == 100);
        CHECK(book.order(2)->remaining == 70);
        CHECK(book.bid_level(10'000)->aggregate_shares == 100);
        CHECK(book.bid_level(10'000)->order_count == 1);
        CHECK(book.ask_level(11'000)->aggregate_shares == 70);
        CHECK(book.ask_level(11'000)->order_count == 1);
        CHECK(book.best_bid() == std::optional<aegis::Price4>{10'000});
        CHECK(book.best_ask() == std::optional<aegis::Price4>{11'000});
        check_invariants(book);
    };

    const auto wrong_locate = book.execute(execution(1, 45, 10));
    CHECK(!wrong_locate.has_value());
    check_error(
        wrong_locate.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::InvalidOrderReduction);
    check_unchanged();

    const auto zero = book.execute(execution(1, 44, 0));
    CHECK(!zero.has_value());
    check_error(
        zero.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::InvalidOrderReduction);
    check_unchanged();

    const auto unknown = book.execute(execution(999, 44, 10));
    CHECK(!unknown.has_value());
    check_error(
        unknown.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::UnknownOrderReference);
    if (unknown.error() != nullptr) {
        CHECK(unknown.error()->observed_value == 999);
    }
    check_unchanged();

    const auto over = book.execute(execution(1, 44, 101));
    CHECK(!over.has_value());
    check_error(
        over.error(), aegis::ErrorCategory::BookInvariant, aegis::ErrorCode::OverExecution);
    if (over.error() != nullptr) {
        CHECK(over.error()->observed_value == 101);
        CHECK(over.error()->limit_value == 100);
    }
    check_unchanged();
}

void test_execute_with_price_uses_resting_price()
{
    aegis::OrderBook book{45};
    CHECK(book.add(add_order(1, 45, aegis::Side::Buy, 100, 10'000)).has_value());

    CHECK(book.execute_with_price(execution_with_price(1, 45, 40, 12'345)).has_value());
    CHECK(book.order(1)->remaining == 60);
    CHECK(book.order(1)->price == 10'000);
    CHECK(book.bid_level(10'000)->aggregate_shares == 60);
    CHECK(book.bid_level(10'000)->order_count == 1);
    CHECK(book.bid_level(12'345) == std::nullopt);
    CHECK(book.best_bid() == std::optional<aegis::Price4>{10'000});
    check_invariants(book);

    const auto over =
        book.execute_with_price(execution_with_price(1, 45, 61, 12'345));
    CHECK(!over.has_value());
    check_error(
        over.error(), aegis::ErrorCategory::BookInvariant, aegis::ErrorCode::OverExecution);
    CHECK(book.order(1)->remaining == 60);
    CHECK(book.bid_level(10'000)->aggregate_shares == 60);
    CHECK(book.bid_level(12'345) == std::nullopt);

    CHECK(book.execute_with_price(execution_with_price(1, 45, 60, 9'999)).has_value());
    check_logically_empty(book);
    check_invariants(book);
}

void test_partial_and_full_cancel()
{
    aegis::OrderBook book{46};
    CHECK(book.add(add_order(1, 46, aegis::Side::Sell, 100, 10'700)).has_value());
    CHECK(book.add(add_order(2, 46, aegis::Side::Sell, 50, 10'700)).has_value());
    CHECK(book.add(add_order(3, 46, aegis::Side::Sell, 80, 11'000)).has_value());
    CHECK(book.add(add_order(4, 46, aegis::Side::Buy, 90, 10'000)).has_value());

    CHECK(book.cancel(cancellation(1, 46, 40)).has_value());
    CHECK(book.order(1)->remaining == 60);
    CHECK(book.ask_level(10'700)->aggregate_shares == 110);
    CHECK(book.ask_level(10'700)->order_count == 2);
    CHECK(book.bid_level(10'000)->aggregate_shares == 90);

    CHECK(book.cancel(cancellation(1, 46, 60)).has_value());
    CHECK(book.order(1) == std::nullopt);
    CHECK(book.ask_level(10'700)->aggregate_shares == 50);
    CHECK(book.ask_level(10'700)->order_count == 1);
    CHECK(book.best_ask() == std::optional<aegis::Price4>{10'700});

    CHECK(book.cancel(cancellation(2, 46, 50)).has_value());
    CHECK(book.ask_level(10'700) == std::nullopt);
    CHECK(book.best_ask() == std::optional<aegis::Price4>{11'000});
    CHECK(book.bid_level(10'000)->aggregate_shares == 90);
    CHECK(book.best_bid() == std::optional<aegis::Price4>{10'000});
    check_invariants(book);
}

void test_cancel_failures_are_transactional()
{
    aegis::OrderBook book{47};
    CHECK(book.add(add_order(1, 47, aegis::Side::Sell, 100, 11'000)).has_value());

    const auto check_unchanged = [&] {
        CHECK(book.active_order_count() == 1);
        CHECK(book.order(1)->remaining == 100);
        CHECK(book.ask_level(11'000)->aggregate_shares == 100);
        CHECK(book.ask_level(11'000)->order_count == 1);
        CHECK(book.best_ask() == std::optional<aegis::Price4>{11'000});
        CHECK(book.best_bid() == std::nullopt);
        check_invariants(book);
    };

    const auto wrong_locate = book.cancel(cancellation(1, 48, 10));
    CHECK(!wrong_locate.has_value());
    check_error(
        wrong_locate.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::InvalidOrderReduction);
    check_unchanged();

    const auto zero = book.cancel(cancellation(1, 47, 0));
    CHECK(!zero.has_value());
    check_error(
        zero.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::InvalidOrderReduction);
    check_unchanged();

    const auto unknown = book.cancel(cancellation(999, 47, 10));
    CHECK(!unknown.has_value());
    check_error(
        unknown.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::UnknownOrderReference);
    check_unchanged();

    const auto over = book.cancel(cancellation(1, 47, 101));
    CHECK(!over.has_value());
    check_error(
        over.error(), aegis::ErrorCategory::BookInvariant, aegis::ErrorCode::OverCancel);
    if (over.error() != nullptr) {
        CHECK(over.error()->observed_value == 101);
        CHECK(over.error()->limit_value == 100);
    }
    check_unchanged();
}

void test_attribution_survives_partial_reduction()
{
    aegis::OrderBook book{48};
    constexpr aegis::Attribution attribution{'M', 'P', 'I', 'D'};
    CHECK(book.add(add_order(
              1, 48, aegis::Side::Buy, 100, 10'000, attribution))
              .has_value());

    CHECK(book.execute(execution(1, 48, 25)).has_value());
    const auto after_execution = book.order(1);
    CHECK(after_execution.has_value());
    if (after_execution) {
        CHECK(after_execution->id == 1);
        CHECK(after_execution->stock_locate == 48);
        CHECK(after_execution->side == aegis::Side::Buy);
        CHECK(after_execution->price == 10'000);
        CHECK(after_execution->remaining == 75);
        CHECK(after_execution->has_attribution);
        CHECK(after_execution->attribution == attribution);
    }

    CHECK(book.cancel(cancellation(1, 48, 25)).has_value());
    const auto after_cancel = book.order(1);
    CHECK(after_cancel.has_value());
    if (after_cancel) {
        CHECK(after_cancel->remaining == 50);
        CHECK(after_cancel->has_attribution);
        CHECK(after_cancel->attribution == attribution);
    }
    CHECK(book.bid_level(10'000)->aggregate_shares == 50);
    CHECK(book.bid_level(10'000)->order_count == 1);
    check_invariants(book);
}

}  // namespace

int main()
{
    test_order_record_model();
    test_empty_book_and_identity();
    test_capacity_preparation();
    test_empty_invariants_are_stable_and_const();
    test_basic_buy();
    test_basic_sell();
    test_same_price_aggregation();
    test_multiple_prices_and_both_sides();
    test_crossed_book_is_accepted();
    test_attribution_metadata();
    test_price_boundaries();
    test_zero_shares_rejected();
    test_wrong_locate_rejected();
    test_invalid_side_rejected();
    test_duplicate_order_is_transactional();
    test_partial_and_full_execution();
    test_full_execution_preserves_same_level_and_updates_best_bid();
    test_execution_failures_are_transactional();
    test_execute_with_price_uses_resting_price();
    test_partial_and_full_cancel();
    test_cancel_failures_are_transactional();
    test_attribution_survives_partial_reduction();
    return failure_count == 0 ? 0 : 1;
}
