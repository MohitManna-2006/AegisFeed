#include "aegis/book/book_store.hpp"

#include "aegis/book/order_book.hpp"
#include "aegis/book/symbol_directory.hpp"
#include "aegis/common/error.hpp"
#include "aegis/common/result.hpp"
#include "aegis/itch/itch_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

static_assert(sizeof(aegis::BookRouteStatus) == sizeof(std::uint8_t));
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::BookArithmeticOverflow) == 18);
static_assert(
    static_cast<std::uint16_t>(aegis::ErrorCode::ConflictingSelectedSymbolLocate) == 19);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::UnknownStockLocate) == 20);
static_assert(
    std::is_same_v<decltype(std::declval<const aegis::BookStore&>().book(0)),
                   const aegis::OrderBook*>);
static_assert(
    std::is_same_v<decltype(std::declval<const aegis::BookStore&>().symbol_directory()),
                   const aegis::SymbolDirectory&>);

std::size_t failure_count = 0;

void check(const bool condition, const std::string_view expression, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << "book_store_test.cpp:" << line << ": check failed: " << expression << '\n';
    ++failure_count;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

[[nodiscard]] aegis::StockDirectory stock_directory(
    const aegis::StockLocate stock_locate,
    const aegis::StockSymbol& symbol)
{
    aegis::StockDirectory directory{};
    directory.header.stock_locate = stock_locate;
    directory.stock = symbol;
    return directory;
}

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

template <std::size_t Size>
[[nodiscard]] std::optional<aegis::BookStore> configured_store(
    const std::array<std::string_view, Size>& requested_symbols,
    const std::size_t expected_active_order_capacity_per_book = 0)
{
    auto result = aegis::BookStore::create(
        std::span<const std::string_view>{requested_symbols},
        expected_active_order_capacity_per_book);
    CHECK(result.has_value());
    if (!result) {
        return std::nullopt;
    }
    return std::move(*result.value());
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

void check_empty_book(const aegis::OrderBook* book, const aegis::StockLocate stock_locate)
{
    CHECK(book != nullptr);
    if (book == nullptr) {
        return;
    }
    CHECK(book->stock_locate() == stock_locate);
    CHECK(book->active_order_count() == 0);
    CHECK(book->bid_level_count() == 0);
    CHECK(book->ask_level_count() == 0);
    CHECK(book->best_bid() == std::nullopt);
    CHECK(book->best_ask() == std::nullopt);
    CHECK(book->validate_invariants().has_value());
}

void test_creation_and_directory_observation()
{
    constexpr std::array requested{std::string_view{"AAPL"}, std::string_view{"MSFT"}};
    auto store = configured_store(requested, 256);
    if (!store) {
        return;
    }

    CHECK(store->book_count() == 0);
    CHECK(store->symbol_directory().requested_symbol_count() == 2);

    CHECK(store->observe(stock_directory(
              20, {'N', 'V', 'D', 'A', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(store->symbol_directory().is_known(20));
    CHECK(!store->symbol_directory().is_selected(20));
    CHECK(!store->has_book(20));
    CHECK(store->book(20) == nullptr);
    CHECK(store->book_count() == 0);

    const auto aapl_directory = stock_directory(
        10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '});
    CHECK(store->observe(aapl_directory).has_value());
    CHECK(store->symbol_directory().is_selected(10));
    CHECK(store->has_book(10));
    CHECK(store->book_count() == 1);
    check_empty_book(store->book(10), 10);

    CHECK(store->observe(stock_directory(
              30, {'M', 'S', 'F', 'T', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(store->has_book(30));
    CHECK(store->book_count() == 2);
    check_empty_book(store->book(30), 30);

    const auto* const original_aapl_book = store->book(10);
    CHECK(store->observe(aapl_directory).has_value());
    CHECK(store->book_count() == 2);
    CHECK(store->book(10) == original_aapl_book);
}

void test_selected_adds_and_book_isolation()
{
    constexpr std::array requested{std::string_view{"AAPL"}, std::string_view{"MSFT"}};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->observe(stock_directory(
              10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(store->observe(stock_directory(
              30, {'M', 'S', 'F', 'T', ' ', ' ', ' ', ' '}))
              .has_value());

    const auto aapl_result =
        store->add(add_order(1, 10, aegis::Side::Buy, 100, 10'000));
    CHECK(aapl_result.has_value());
    if (aapl_result) {
        CHECK(*aapl_result.value() == aegis::BookRouteStatus::Applied);
    }

    const auto* const aapl_book = store->book(10);
    const auto* const msft_book = store->book(30);
    CHECK(aapl_book != nullptr);
    CHECK(msft_book != nullptr);
    if (aapl_book != nullptr) {
        CHECK(aapl_book->active_order_count() == 1);
        CHECK(aapl_book->best_bid() == std::optional<aegis::Price4>{10'000});
        CHECK(aapl_book->order(1)->remaining == 100);
        CHECK(aapl_book->bid_level(10'000)->aggregate_shares == 100);
        CHECK(aapl_book->validate_invariants().has_value());
    }
    check_empty_book(msft_book, 30);

    const auto msft_result = store->add(add_order(
        2,
        30,
        aegis::Side::Sell,
        75,
        11'000,
        aegis::Attribution{'M', 'P', 'I', 'D'}));
    CHECK(msft_result.has_value());
    if (msft_result) {
        CHECK(*msft_result.value() == aegis::BookRouteStatus::Applied);
    }

    if (aapl_book != nullptr) {
        CHECK(aapl_book->active_order_count() == 1);
        CHECK(aapl_book->ask_level_count() == 0);
    }
    if (msft_book != nullptr) {
        CHECK(msft_book->active_order_count() == 1);
        CHECK(msft_book->best_ask() == std::optional<aegis::Price4>{11'000});
        const auto order = msft_book->order(2);
        CHECK(order.has_value());
        if (order) {
            CHECK(order->has_attribution);
            CHECK(order->attribution == aegis::Attribution({'M', 'P', 'I', 'D'}));
        }
        CHECK(msft_book->ask_level(11'000)->aggregate_shares == 75);
        CHECK(msft_book->validate_invariants().has_value());
    }
}

void test_known_unselected_and_unknown_adds()
{
    constexpr std::array requested{std::string_view{"AAPL"}};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->observe(stock_directory(
              10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(store->observe(stock_directory(
              20, {'N', 'V', 'D', 'A', ' ', ' ', ' ', ' '}))
              .has_value());

    const auto skipped = store->add(add_order(1, 20, aegis::Side::Buy, 50, 9'000));
    CHECK(skipped.has_value());
    if (skipped) {
        CHECK(*skipped.value() == aegis::BookRouteStatus::KnownUnselected);
    }
    CHECK(store->book_count() == 1);
    CHECK(!store->has_book(20));
    check_empty_book(store->book(10), 10);

    const auto unknown = store->add(add_order(2, 99, aegis::Side::Buy, 50, 9'000));
    CHECK(!unknown.has_value());
    check_error(
        unknown.error(), aegis::ErrorCategory::Session, aegis::ErrorCode::UnknownStockLocate);
    if (unknown.error() != nullptr) {
        CHECK(unknown.error()->observed_value == 99);
    }
    CHECK(store->book_count() == 1);
    CHECK(!store->symbol_directory().is_known(99));
    check_empty_book(store->book(10), 10);
}

void test_order_book_error_propagation()
{
    constexpr std::array requested{std::string_view{"AAPL"}};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->observe(stock_directory(
              10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());

    const auto rejected = store->add(add_order(1, 10, aegis::Side::Buy, 0, 10'000));
    CHECK(!rejected.has_value());
    check_error(
        rejected.error(),
        aegis::ErrorCategory::BookInvariant,
        aegis::ErrorCode::InvalidAddOrder);
    check_empty_book(store->book(10), 10);
    CHECK(store->book_count() == 1);
}

void test_directory_conflicts_do_not_create_books()
{
    constexpr std::array requested{std::string_view{"AAPL"}, std::string_view{"MSFT"}};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->observe(stock_directory(
              10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());

    const auto same_locate_conflict = store->observe(stock_directory(
        10, {'M', 'S', 'F', 'T', ' ', ' ', ' ', ' '}));
    CHECK(!same_locate_conflict.has_value());
    check_error(
        same_locate_conflict.error(),
        aegis::ErrorCategory::Session,
        aegis::ErrorCode::ConflictingStockLocate);

    const auto selected_symbol_conflict = store->observe(stock_directory(
        20, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}));
    CHECK(!selected_symbol_conflict.has_value());
    check_error(
        selected_symbol_conflict.error(),
        aegis::ErrorCategory::Session,
        aegis::ErrorCode::ConflictingSelectedSymbolLocate);

    CHECK(store->book_count() == 1);
    CHECK(store->has_book(10));
    CHECK(!store->has_book(20));
    CHECK(!store->symbol_directory().is_known(20));
    check_empty_book(store->book(10), 10);
}

void test_locate_zero()
{
    constexpr std::array requested{std::string_view{"ZERO"}};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->observe(stock_directory(
              0, {'Z', 'E', 'R', 'O', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(store->has_book(0));
    CHECK(store->book(0) != nullptr);
    CHECK(store->book(0)->stock_locate() == 0);

    const auto routed = store->add(add_order(1, 0, aegis::Side::Buy, 10, 100));
    CHECK(routed.has_value());
    if (routed) {
        CHECK(*routed.value() == aegis::BookRouteStatus::Applied);
    }
    CHECK(store->book(0)->order(1)->stock_locate == 0);
    CHECK(store->book(0)->validate_invariants().has_value());
}

void test_empty_requested_set()
{
    constexpr std::array<std::string_view, 0> requested{};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->book_count() == 0);
    CHECK(store->symbol_directory().all_requested_symbols_discovered());
}

}  // namespace

int main()
{
    test_creation_and_directory_observation();
    test_selected_adds_and_book_isolation();
    test_known_unselected_and_unknown_adds();
    test_order_book_error_propagation();
    test_directory_conflicts_do_not_create_books();
    test_locate_zero();
    test_empty_requested_set();
    return failure_count == 0 ? 0 : 1;
}
