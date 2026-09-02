#include "aegis/book/book_store.hpp"
#include "aegis/book/order_book.hpp"
#include "aegis/book/state_digest.hpp"
#include "aegis/common/byte_reader.hpp"
#include "aegis/common/error.hpp"
#include "aegis/itch/itch_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr std::uint64_t kRandomSeed = 0xA3E1'5F00'D123'4B77ULL;
constexpr std::size_t kRandomOperationCount = 3'000;

static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::InvalidOrderReplace) == 26);
static_assert(
    std::is_same_v<decltype(std::declval<const aegis::OrderBook&>().canonical_snapshot()),
                   aegis::CanonicalOrderBook>);
static_assert(
    std::is_same_v<decltype(std::declval<const aegis::BookStore&>().canonical_snapshot()),
                   aegis::Result<aegis::CanonicalBookStore>>);
static_assert(
    std::is_same_v<decltype(aegis::compute_state_digest(
                       std::declval<const aegis::BookStore&>(), 0)),
                   aegis::Result<aegis::StateDigest>>);

std::size_t failure_count = 0;

void check(const bool condition, const std::string_view expression, const int line)
{
    if (condition) {
        return;
    }
    std::cerr << "state_digest_test.cpp:" << line
              << ": check failed: " << expression << '\n';
    ++failure_count;
}

void check_random(
    const bool condition,
    const std::string_view expression,
    const int line,
    const std::size_t operation_index)
{
    if (condition) {
        return;
    }
    std::cerr << "state_digest_test.cpp:" << line
              << ": randomized check failed: " << expression
              << " seed=0x" << std::hex << kRandomSeed << std::dec
              << " operation=" << operation_index << '\n';
    ++failure_count;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
#define CHECK_RANDOM(expression, operation_index) \
    check_random(                               \
        static_cast<bool>(expression), #expression, __LINE__, operation_index)

[[nodiscard]] aegis::StockDirectory stock_directory(
    const aegis::StockLocate stock_locate,
    const aegis::StockSymbol& symbol)
{
    aegis::StockDirectory message{};
    message.header.stock_locate = stock_locate;
    message.stock = symbol;
    return message;
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
    message.match_number = 1001;
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
    message.match_number = 1002;
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

[[nodiscard]] aegis::OrderDelete deletion(
    const aegis::OrderId order_id,
    const aegis::StockLocate stock_locate)
{
    aegis::OrderDelete message{};
    message.header.type = 'D';
    message.header.stock_locate = stock_locate;
    message.order_reference = order_id;
    return message;
}

[[nodiscard]] aegis::OrderReplace replacement(
    const aegis::OrderId original_order_id,
    const aegis::OrderId new_order_id,
    const aegis::StockLocate stock_locate,
    const aegis::Shares shares,
    const aegis::Price4 price)
{
    aegis::OrderReplace message{};
    message.header.type = 'U';
    message.header.stock_locate = stock_locate;
    message.original_order_reference = original_order_id;
    message.new_order_reference = new_order_id;
    message.shares = shares;
    message.price_1e4 = price;
    return message;
}

template <std::size_t Size>
[[nodiscard]] std::optional<aegis::BookStore> configured_store(
    const std::array<std::string_view, Size>& requested_symbols)
{
    auto result = aegis::BookStore::create(
        std::span<const std::string_view>{requested_symbols}, 512);
    CHECK(result.has_value());
    if (!result) {
        return std::nullopt;
    }
    return std::move(*result.value());
}

void check_applied(const aegis::Result<aegis::BookRouteStatus>& result)
{
    CHECK(result.has_value());
    if (result) {
        CHECK(*result.value() == aegis::BookRouteStatus::Applied);
    }
}

void check_error(const aegis::Error* error, const aegis::ErrorCode code)
{
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->code == code);
    }
}

struct CapturedState {
    aegis::CanonicalBookStore snapshot{};
    std::vector<std::byte> bytes{};
    aegis::StateDigest digest{};

    [[nodiscard]] bool operator==(const CapturedState&) const = default;
};

[[nodiscard]] std::optional<CapturedState> capture(
    const aegis::BookStore& store,
    const std::uint64_t sequence)
{
    auto snapshot = store.canonical_snapshot();
    auto bytes = aegis::canonical_state_bytes(store, sequence);
    auto digest = aegis::compute_state_digest(store, sequence);
    CHECK(snapshot.has_value());
    CHECK(bytes.has_value());
    CHECK(digest.has_value());
    if (!snapshot || !bytes || !digest) {
        return std::nullopt;
    }
    return CapturedState{
        std::move(*snapshot.value()),
        std::move(*bytes.value()),
        *digest.value(),
    };
}

[[nodiscard]] aegis::CanonicalOrder canonical_order(
    const aegis::OrderId id,
    const aegis::StockLocate locate,
    const aegis::Side side,
    const aegis::Price4 price,
    const aegis::Shares remaining,
    const std::optional<aegis::Attribution> attribution = std::nullopt)
{
    return aegis::CanonicalOrder{
        id,
        locate,
        side,
        price,
        remaining,
        attribution.value_or(aegis::Attribution{}),
        attribution.has_value(),
    };
}

void test_exact_fixture_and_canonical_ordering()
{
    constexpr std::array requested{std::string_view{"MSFT"}, std::string_view{"AAPL"}};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }

    CHECK(store->observe(stock_directory(
              20, {'M', 'S', 'F', 'T', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(store->observe(stock_directory(
              10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());

    check_applied(store->add(add_order(900, 10, aegis::Side::Buy, 10, 10'000)));
    check_applied(store->add(add_order(100, 10, aegis::Side::Buy, 20, 10'300)));
    check_applied(store->add(add_order(500, 10, aegis::Side::Buy, 30, 10'100)));
    check_applied(store->add(add_order(800, 10, aegis::Side::Sell, 40, 11'000)));
    check_applied(store->add(add_order(200, 10, aegis::Side::Sell, 50, 10'700)));
    check_applied(store->add(add_order(600, 10, aegis::Side::Sell, 60, 10'900)));
    check_applied(store->add(add_order(
        50,
        20,
        aegis::Side::Sell,
        70,
        20'000,
        aegis::Attribution{'M', 'P', 'I', 'D'})));

    const aegis::CanonicalBookStore expected{{
        aegis::CanonicalBook{
            "AAPL",
            aegis::CanonicalOrderBook{
                10,
                {{10'300, 20, 1}, {10'100, 30, 1}, {10'000, 10, 1}},
                {{10'700, 50, 1}, {10'900, 60, 1}, {11'000, 40, 1}},
                {
                    canonical_order(100, 10, aegis::Side::Buy, 10'300, 20),
                    canonical_order(200, 10, aegis::Side::Sell, 10'700, 50),
                    canonical_order(500, 10, aegis::Side::Buy, 10'100, 30),
                    canonical_order(600, 10, aegis::Side::Sell, 10'900, 60),
                    canonical_order(800, 10, aegis::Side::Sell, 11'000, 40),
                    canonical_order(900, 10, aegis::Side::Buy, 10'000, 10),
                },
            },
        },
        aegis::CanonicalBook{
            "MSFT",
            aegis::CanonicalOrderBook{
                20,
                {},
                {{20'000, 70, 1}},
                {canonical_order(
                    50,
                    20,
                    aegis::Side::Sell,
                    20'000,
                    70,
                    aegis::Attribution{'M', 'P', 'I', 'D'})},
            },
        },
    }};

    constexpr std::uint64_t sequence = 123'456;
    auto captured = capture(*store, sequence);
    if (!captured) {
        return;
    }
    CHECK(captured->snapshot == expected);
    CHECK(captured->digest.symbol_count == 2);
    CHECK(captured->digest.active_order_count == 7);
    CHECK(captured->digest.bid_level_count == 3);
    CHECK(captured->digest.ask_level_count == 4);
    CHECK(captured->digest.aggregate_displayed_shares == 280);
    CHECK(captured->digest.last_mold_sequence == sequence);

    CHECK(captured->bytes.size() > 20);
    if (captured->bytes.size() > 20) {
        const std::array expected_domain{'A', 'E', 'G', 'I', 'S', 'B', 'K'};
        for (std::size_t index = 0; index < expected_domain.size(); ++index) {
            CHECK(std::to_integer<char>(captured->bytes[index]) == expected_domain[index]);
        }
        CHECK(std::to_integer<std::uint8_t>(captured->bytes[7]) == 1);
        aegis::ByteReader reader{std::span<const std::byte>{captured->bytes}.subspan(8)};
        const auto encoded_sequence = reader.read_u64_be();
        const auto encoded_books = reader.read_u32_be();
        CHECK(encoded_sequence.has_value());
        CHECK(encoded_books.has_value());
        if (encoded_sequence) {
            CHECK(*encoded_sequence.value() == sequence);
        }
        if (encoded_books) {
            CHECK(*encoded_books.value() == 2);
        }
    }

    constexpr std::uint64_t expected_digest = 0x27DF'B148'D131'63ACULL;
    if (captured->digest.hash != expected_digest) {
        std::cerr << "exact fixture digest: 0x" << std::hex
                  << captured->digest.hash << std::dec << '\n';
    }
    CHECK(captured->digest.hash == expected_digest);
}

void test_empty_and_insertion_order_independence()
{
    constexpr std::array<std::string_view, 0> none{};
    auto empty_a = configured_store(none);
    auto empty_b = configured_store(none);
    if (!empty_a || !empty_b) {
        return;
    }
    CHECK(capture(*empty_a, 7) == capture(*empty_b, 7));

    constexpr std::array requested{std::string_view{"AAPL"}};
    auto first = configured_store(requested);
    auto second = configured_store(requested);
    if (!first || !second) {
        return;
    }
    const auto directory = stock_directory(
        10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '});
    CHECK(first->observe(directory).has_value());
    CHECK(second->observe(directory).has_value());

    check_applied(first->add(add_order(100, 10, aegis::Side::Buy, 10, 10'000)));
    check_applied(first->add(add_order(200, 10, aegis::Side::Sell, 20, 11'000)));
    check_applied(first->add(add_order(300, 10, aegis::Side::Buy, 30, 10'100)));

    check_applied(second->add(add_order(300, 10, aegis::Side::Buy, 30, 10'100)));
    check_applied(second->add(add_order(100, 10, aegis::Side::Buy, 10, 10'000)));
    check_applied(second->add(add_order(200, 10, aegis::Side::Sell, 20, 11'000)));

    CHECK(capture(*first, 88) == capture(*second, 88));
}

void test_history_and_execution_metadata_independence()
{
    constexpr std::array requested{std::string_view{"AAPL"}};
    const auto directory = stock_directory(
        10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '});

    auto reduced = configured_store(requested);
    auto direct = configured_store(requested);
    if (!reduced || !direct) {
        return;
    }
    CHECK(reduced->observe(directory).has_value());
    CHECK(direct->observe(directory).has_value());
    check_applied(reduced->add(add_order(100, 10, aegis::Side::Buy, 100, 10'000)));
    check_applied(reduced->cancel(cancellation(100, 10, 40)));
    check_applied(direct->add(add_order(100, 10, aegis::Side::Buy, 60, 10'000)));
    CHECK(capture(*reduced, 900) == capture(*direct, 900));

    auto price_a = configured_store(requested);
    auto price_b = configured_store(requested);
    if (!price_a || !price_b) {
        return;
    }
    CHECK(price_a->observe(directory).has_value());
    CHECK(price_b->observe(directory).has_value());
    check_applied(price_a->add(add_order(200, 10, aegis::Side::Sell, 100, 11'000)));
    check_applied(price_b->add(add_order(200, 10, aegis::Side::Sell, 100, 11'000)));
    check_applied(price_a->execute_with_price(
        execution_with_price(200, 10, 40, 10'500)));
    check_applied(price_b->execute_with_price(
        execution_with_price(200, 10, 40, 12'500)));
    CHECK(capture(*price_a, 901) == capture(*price_b, 901));
}

void test_attribution_changes_state_identity()
{
    constexpr std::array requested{std::string_view{"AAPL"}};
    const auto directory = stock_directory(
        10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '});

    auto alpha = configured_store(requested);
    auto beta = configured_store(requested);
    auto plain = configured_store(requested);
    auto attributed_zero = configured_store(requested);
    if (!alpha || !beta || !plain || !attributed_zero) {
        return;
    }
    CHECK(alpha->observe(directory).has_value());
    CHECK(beta->observe(directory).has_value());
    CHECK(plain->observe(directory).has_value());
    CHECK(attributed_zero->observe(directory).has_value());

    check_applied(alpha->add(add_order(
        1, 10, aegis::Side::Buy, 10, 10'000, aegis::Attribution{'A', 'A', 'A', 'A'})));
    check_applied(beta->add(add_order(
        1, 10, aegis::Side::Buy, 10, 10'000, aegis::Attribution{'B', 'B', 'B', 'B'})));
    check_applied(plain->add(add_order(1, 10, aegis::Side::Buy, 10, 10'000)));
    check_applied(attributed_zero->add(add_order(
        1, 10, aegis::Side::Buy, 10, 10'000, aegis::Attribution{})));

    const auto alpha_state = capture(*alpha, 1);
    const auto beta_state = capture(*beta, 1);
    const auto plain_state = capture(*plain, 1);
    const auto zero_state = capture(*attributed_zero, 1);
    CHECK(alpha_state.has_value());
    CHECK(beta_state.has_value());
    CHECK(plain_state.has_value());
    CHECK(zero_state.has_value());
    if (alpha_state && beta_state && plain_state && zero_state) {
        CHECK(alpha_state->snapshot != beta_state->snapshot);
        CHECK(alpha_state->digest.hash != beta_state->digest.hash);
        CHECK(plain_state->snapshot != zero_state->snapshot);
        CHECK(plain_state->digest.hash != zero_state->digest.hash);
    }
}

void test_full_a_f_e_c_x_u_d_lifecycle()
{
    constexpr std::array requested{std::string_view{"AAPL"}};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->observe(stock_directory(
              10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());

    const auto check_book = [&] {
        const auto* book = store->book(10);
        CHECK(book != nullptr);
        if (book != nullptr) {
            CHECK(book->validate_invariants().has_value());
        }
    };

    check_applied(store->add(add_order(1, 10, aegis::Side::Buy, 100, 10'000)));
    check_book();
    check_applied(store->add(add_order(
        2,
        10,
        aegis::Side::Sell,
        80,
        11'000,
        aegis::Attribution{'M', 'P', 'I', 'D'})));
    check_book();
    check_applied(store->execute(execution(1, 10, 20)));
    check_book();
    check_applied(store->execute_with_price(
        execution_with_price(2, 10, 30, 12'345)));
    check_book();
    check_applied(store->cancel(cancellation(1, 10, 30)));
    check_book();
    check_applied(store->replace(replacement(1, 3, 10, 70, 10'500)));
    check_book();
    check_applied(store->add(add_order(4, 10, aegis::Side::Buy, 30, 10'500)));
    check_book();
    check_applied(store->delete_order(deletion(4, 10)));
    check_book();
    check_applied(store->execute(execution(2, 10, 50)));
    check_book();
    check_applied(store->cancel(cancellation(3, 10, 20)));
    check_book();

    const aegis::CanonicalBookStore expected{{aegis::CanonicalBook{
        "AAPL",
        aegis::CanonicalOrderBook{
            10,
            {{10'500, 50, 1}},
            {},
            {canonical_order(3, 10, aegis::Side::Buy, 10'500, 50)},
        },
    }}};
    auto state = capture(*store, 5'000);
    CHECK(state.has_value());
    if (state) {
        CHECK(state->snapshot == expected);
        CHECK(state->digest.active_order_count == 1);
        CHECK(state->digest.bid_level_count == 1);
        CHECK(state->digest.ask_level_count == 0);
        CHECK(state->digest.aggregate_displayed_shares == 50);
    }
}

void test_multi_symbol_lifecycle_and_known_unselected_stability()
{
    constexpr std::array requested{std::string_view{"MSFT"}, std::string_view{"AAPL"}};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->observe(stock_directory(
              20, {'N', 'V', 'D', 'A', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(store->observe(stock_directory(
              30, {'M', 'S', 'F', 'T', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(store->observe(stock_directory(
              10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());

    check_applied(store->add(add_order(1, 10, aegis::Side::Buy, 100, 10'000)));
    check_applied(store->add(add_order(2, 30, aegis::Side::Sell, 80, 11'000)));
    check_applied(store->execute(execution(1, 10, 40)));
    check_applied(store->replace(replacement(2, 3, 30, 90, 12'000)));

    auto before_unselected = capture(*store, 700);
    CHECK(before_unselected.has_value());
    if (!before_unselected) {
        return;
    }
    CHECK(before_unselected->snapshot.books.size() == 2);
    if (before_unselected->snapshot.books.size() == 2) {
        CHECK(before_unselected->snapshot.books[0].symbol == "AAPL");
        CHECK(before_unselected->snapshot.books[1].symbol == "MSFT");
        CHECK(before_unselected->snapshot.books[0].book.orders[0].remaining == 60);
        CHECK(before_unselected->snapshot.books[1].book.orders[0].id == 3);
        CHECK(before_unselected->snapshot.books[1].book.orders[0].remaining == 90);
    }

    const auto skipped_add =
        store->add(add_order(99, 20, aegis::Side::Buy, 50, 9'000));
    const auto skipped_execute = store->execute(execution(99, 20, 10));
    const auto skipped_replace = store->replace(replacement(99, 100, 20, 75, 9'500));
    CHECK(skipped_add.has_value());
    CHECK(skipped_execute.has_value());
    CHECK(skipped_replace.has_value());
    if (skipped_add && skipped_execute && skipped_replace) {
        CHECK(*skipped_add.value() == aegis::BookRouteStatus::KnownUnselected);
        CHECK(*skipped_execute.value() == aegis::BookRouteStatus::KnownUnselected);
        CHECK(*skipped_replace.value() == aegis::BookRouteStatus::KnownUnselected);
    }
    CHECK(!store->has_book(20));
    CHECK(capture(*store, 700) == before_unselected);
}

void test_invalid_transitions_preserve_canonical_state()
{
    constexpr std::array requested{std::string_view{"AAPL"}};
    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->observe(stock_directory(
              10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());
    check_applied(store->add(add_order(1, 10, aegis::Side::Buy, 100, 10'000)));
    check_applied(store->add(add_order(2, 10, aegis::Side::Sell, 80, 11'000)));

    const auto baseline = capture(*store, 800);
    CHECK(baseline.has_value());
    if (!baseline) {
        return;
    }
    const auto check_unchanged = [&] {
        CHECK(capture(*store, 800) == baseline);
        CHECK(store->book(10)->validate_invariants().has_value());
    };

    const auto duplicate =
        store->add(add_order(1, 10, aegis::Side::Buy, 10, 10'500));
    CHECK(!duplicate.has_value());
    check_error(duplicate.error(), aegis::ErrorCode::DuplicateOrderReference);
    check_unchanged();

    const auto unknown_execute = store->execute(execution(999, 10, 10));
    CHECK(!unknown_execute.has_value());
    check_error(unknown_execute.error(), aegis::ErrorCode::UnknownOrderReference);
    check_unchanged();

    const auto over_execute = store->execute(execution(1, 10, 101));
    CHECK(!over_execute.has_value());
    check_error(over_execute.error(), aegis::ErrorCode::OverExecution);
    check_unchanged();

    const auto over_cancel = store->cancel(cancellation(2, 10, 81));
    CHECK(!over_cancel.has_value());
    check_error(over_cancel.error(), aegis::ErrorCode::OverCancel);
    check_unchanged();

    const auto unknown_delete = store->delete_order(deletion(999, 10));
    CHECK(!unknown_delete.has_value());
    check_error(unknown_delete.error(), aegis::ErrorCode::UnknownOrderReference);
    check_unchanged();

    const auto duplicate_replacement =
        store->replace(replacement(1, 2, 10, 50, 10'500));
    CHECK(!duplicate_replacement.has_value());
    check_error(
        duplicate_replacement.error(), aegis::ErrorCode::DuplicateOrderReference);
    check_unchanged();

    const auto zero_replacement =
        store->replace(replacement(1, 3, 10, 0, 10'500));
    CHECK(!zero_replacement.has_value());
    check_error(zero_replacement.error(), aegis::ErrorCode::InvalidOrderReplace);
    check_unchanged();

    const auto wrong_locate =
        store->add(add_order(3, 99, aegis::Side::Buy, 10, 10'500));
    CHECK(!wrong_locate.has_value());
    check_error(wrong_locate.error(), aegis::ErrorCode::UnknownStockLocate);
    check_unchanged();
}

struct ModelOrder {
    aegis::OrderId id{};
    aegis::Side side{};
    aegis::Price4 price{};
    aegis::Shares remaining{};
    aegis::Attribution attribution{};
    bool has_attribution{};
};

using ModelOrders = std::map<aegis::OrderId, ModelOrder>;

[[nodiscard]] aegis::CanonicalOrderBook expected_snapshot(
    const ModelOrders& orders,
    const aegis::StockLocate stock_locate)
{
    std::map<aegis::Price4, aegis::PriceLevel, std::greater<>> bids;
    std::map<aegis::Price4, aegis::PriceLevel, std::less<>> asks;

    aegis::CanonicalOrderBook snapshot;
    snapshot.stock_locate = stock_locate;
    snapshot.orders.reserve(orders.size());
    for (const auto& [order_id, order] : orders) {
        static_cast<void>(order_id);
        auto& level = order.side == aegis::Side::Buy ? bids[order.price] : asks[order.price];
        level.aggregate_shares += order.remaining;
        ++level.order_count;
        snapshot.orders.push_back(aegis::CanonicalOrder{
            order.id,
            stock_locate,
            order.side,
            order.price,
            order.remaining,
            order.attribution,
            order.has_attribution,
        });
    }
    for (const auto& [price, level] : bids) {
        snapshot.bids.push_back(
            aegis::CanonicalPriceLevel{price, level.aggregate_shares, level.order_count});
    }
    for (const auto& [price, level] : asks) {
        snapshot.asks.push_back(
            aegis::CanonicalPriceLevel{price, level.aggregate_shares, level.order_count});
    }
    return snapshot;
}

void test_deterministic_randomized_valid_lifecycle()
{
    constexpr aegis::StockLocate stock_locate = 42;
    constexpr std::array requested{std::string_view{"RAND"}};
    constexpr std::array<aegis::Price4, 8> prices{
        0, 9'800, 9'900, 10'000, 10'100, 10'200, 10'300, aegis::kMaxPrice4};

    auto store = configured_store(requested);
    if (!store) {
        return;
    }
    CHECK(store->observe(stock_directory(
              stock_locate, {'R', 'A', 'N', 'D', ' ', ' ', ' ', ' '}))
              .has_value());

    std::mt19937_64 random{kRandomSeed};
    ModelOrders model;
    aegis::OrderId next_order_id = 1;
    std::array<std::size_t, 7> action_counts{};
    std::size_t empty_state_count = 0;

    const auto choose_order = [&](const std::uint64_t selector) {
        auto selected = model.begin();
        const auto offset = static_cast<std::size_t>(selector % model.size());
        std::advance(selected, static_cast<std::ptrdiff_t>(offset));
        return selected;
    };

    for (std::size_t operation_index = 0;
         operation_index < kRandomOperationCount;
         ++operation_index) {
        std::uint64_t action = random() % 7U;
        if (model.empty()) {
            action = 0;
        } else if (model.size() >= 128 && action == 0) {
            action = 1 + random() % 6U;
        }
        ++action_counts[static_cast<std::size_t>(action)];

        if (action == 0) {
            const auto id = next_order_id++;
            const auto side = random() % 2U == 0 ? aegis::Side::Buy : aegis::Side::Sell;
            const auto shares = static_cast<aegis::Shares>(1 + random() % 500U);
            const auto price = prices[static_cast<std::size_t>(random() % prices.size())];
            const bool has_attribution = random() % 4U == 0;
            const aegis::Attribution attribution{
                static_cast<char>('A' + random() % 26U),
                static_cast<char>('A' + random() % 26U),
                static_cast<char>('A' + random() % 26U),
                static_cast<char>('A' + random() % 26U),
            };
            const auto result = store->add(add_order(
                id,
                stock_locate,
                side,
                shares,
                price,
                has_attribution ? std::optional{attribution} : std::nullopt));
            CHECK_RANDOM(result.has_value(), operation_index);
            if (result) {
                CHECK_RANDOM(
                    *result.value() == aegis::BookRouteStatus::Applied,
                    operation_index);
            }
            model.emplace(id, ModelOrder{
                                  id,
                                  side,
                                  price,
                                  shares,
                                  has_attribution ? attribution : aegis::Attribution{},
                                  has_attribution,
                              });
        } else {
            auto selected = choose_order(random());
            const auto id = selected->first;
            const auto remaining = selected->second.remaining;

            if (action >= 1 && action <= 3) {
                const auto reduction = random() % 3U == 0
                                           ? remaining
                                           : static_cast<aegis::Shares>(
                                                 1 + random() % remaining);
                aegis::Result<aegis::BookRouteStatus> result =
                    action == 1
                        ? store->execute(execution(id, stock_locate, reduction))
                        : action == 2
                              ? store->execute_with_price(execution_with_price(
                                    id,
                                    stock_locate,
                                    reduction,
                                    prices[static_cast<std::size_t>(
                                        random() % prices.size())]))
                              : store->cancel(cancellation(id, stock_locate, reduction));
                CHECK_RANDOM(result.has_value(), operation_index);
                if (result) {
                    CHECK_RANDOM(
                        *result.value() == aegis::BookRouteStatus::Applied,
                        operation_index);
                }
                if (reduction == remaining) {
                    model.erase(selected);
                } else {
                    selected->second.remaining -= reduction;
                }
            } else if (action == 4) {
                const auto result = store->delete_order(deletion(id, stock_locate));
                CHECK_RANDOM(result.has_value(), operation_index);
                if (result) {
                    CHECK_RANDOM(
                        *result.value() == aegis::BookRouteStatus::Applied,
                        operation_index);
                }
                model.erase(selected);
            } else {
                const auto new_id = next_order_id++;
                const auto shares = static_cast<aegis::Shares>(1 + random() % 500U);
                const auto price = action == 5
                                       ? selected->second.price
                                       : prices[static_cast<std::size_t>(
                                             random() % prices.size())];
                const auto result = store->replace(
                    replacement(id, new_id, stock_locate, shares, price));
                CHECK_RANDOM(result.has_value(), operation_index);
                if (result) {
                    CHECK_RANDOM(
                        *result.value() == aegis::BookRouteStatus::Applied,
                        operation_index);
                }
                const auto replacement_order = ModelOrder{
                    new_id,
                    selected->second.side,
                    price,
                    shares,
                    selected->second.attribution,
                    selected->second.has_attribution,
                };
                model.erase(selected);
                model.emplace(new_id, replacement_order);
            }
        }

        if (model.empty()) {
            ++empty_state_count;
        }

        const auto* book = store->book(stock_locate);
        CHECK_RANDOM(book != nullptr, operation_index);
        if (book != nullptr) {
            CHECK_RANDOM(book->validate_invariants().has_value(), operation_index);
            if (operation_index % 25U == 0) {
                CHECK_RANDOM(
                    book->canonical_snapshot() ==
                        expected_snapshot(model, stock_locate),
                    operation_index);
            }
        }
    }

    const auto* final_book = store->book(stock_locate);
    CHECK(final_book != nullptr);
    if (final_book != nullptr) {
        CHECK(final_book->canonical_snapshot() == expected_snapshot(model, stock_locate));
        CHECK(final_book->validate_invariants().has_value());
    }

    auto final_state = store->canonical_snapshot();
    CHECK(final_state.has_value());
    if (final_state && final_state.value()->books.size() == 1) {
        CHECK(final_state.value()->books[0].symbol == "RAND");
        CHECK(final_state.value()->books[0].book ==
              expected_snapshot(model, stock_locate));
    }
    CHECK(aegis::compute_state_digest(*store, 9'999).has_value());
    for (const auto count : action_counts) {
        CHECK(count > 0);
    }
    CHECK(empty_state_count > 0);
}

}  // namespace

int main()
{
    test_exact_fixture_and_canonical_ordering();
    test_empty_and_insertion_order_independence();
    test_history_and_execution_metadata_independence();
    test_attribution_changes_state_identity();
    test_full_a_f_e_c_x_u_d_lifecycle();
    test_multi_symbol_lifecycle_and_known_unselected_stability();
    test_invalid_transitions_preserve_canonical_state();
    test_deterministic_randomized_valid_lifecycle();
    return failure_count == 0 ? 0 : 1;
}
