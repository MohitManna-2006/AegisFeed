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
#include <string>
#include <string_view>
#include <utility>

namespace {

static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::BookInvariantViolation) == 12);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::InvalidRequestedSymbol) == 13);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::InvalidStockSymbol) == 14);
static_assert(static_cast<std::uint16_t>(aegis::ErrorCode::ConflictingStockLocate) == 15);
static_assert(
    static_cast<std::uint16_t>(aegis::ErrorCode::ConflictingSelectedSymbolLocate) == 19);

std::size_t failure_count = 0;

void check(const bool condition, const std::string_view expression, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << "symbol_directory_test.cpp:" << line
              << ": check failed: " << expression << '\n';
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

template <std::size_t Size>
[[nodiscard]] std::optional<aegis::SymbolDirectory> configured_directory(
    const std::array<std::string_view, Size>& requested_symbols)
{
    auto result = aegis::SymbolDirectory::create(
        std::span<const std::string_view>{requested_symbols});
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

void test_feed_normalization()
{
    const auto padded = aegis::normalize_feed_symbol({'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '});
    CHECK(padded.has_value());
    if (padded) {
        CHECK(*padded.value() == "AAPL");
    }

    const auto preserved_case =
        aegis::normalize_feed_symbol({'A', 'b', 'C', ' ', ' ', ' ', ' ', ' '});
    CHECK(preserved_case.has_value());
    if (preserved_case) {
        CHECK(*preserved_case.value() == "AbC");
    }

    const auto punctuation_and_digits =
        aegis::normalize_feed_symbol({'B', 'R', 'K', '.', 'B', '1', ' ', ' '});
    CHECK(punctuation_and_digits.has_value());
    if (punctuation_and_digits) {
        CHECK(*punctuation_and_digits.value() == "BRK.B1");
    }

    const auto internal_space =
        aegis::normalize_feed_symbol({'A', ' ', 'B', ' ', ' ', ' ', ' ', ' '});
    CHECK(internal_space.has_value());
    if (internal_space) {
        CHECK(*internal_space.value() == "A B");
    }

    const auto exact_width =
        aegis::normalize_feed_symbol({'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'});
    CHECK(exact_width.has_value());
    if (exact_width) {
        CHECK(*exact_width.value() == "ABCDEFGH");
        CHECK(exact_width.value()->size() == 8);
    }

    const auto all_spaces =
        aegis::normalize_feed_symbol({' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '});
    CHECK(!all_spaces.has_value());
    check_error(
        all_spaces.error(),
        aegis::ErrorCategory::Session,
        aegis::ErrorCode::InvalidStockSymbol);

    const auto embedded_null =
        aegis::normalize_feed_symbol({'A', 'A', '\0', 'L', ' ', ' ', ' ', ' '});
    CHECK(!embedded_null.has_value());
    check_error(
        embedded_null.error(),
        aegis::ErrorCategory::Session,
        aegis::ErrorCode::InvalidStockSymbol);
    if (embedded_null.error() != nullptr) {
        CHECK(embedded_null.error()->offset == 2);
    }
}

void test_requested_normalization()
{
    const auto lowercase = aegis::normalize_requested_symbol("aapl");
    CHECK(lowercase.has_value());
    if (lowercase) {
        CHECK(*lowercase.value() == "AAPL");
    }

    const auto mixed_case = aegis::normalize_requested_symbol("Aapl");
    CHECK(mixed_case.has_value());
    if (mixed_case) {
        CHECK(*mixed_case.value() == "AAPL");
    }

    const auto uppercase = aegis::normalize_requested_symbol("AAPL");
    CHECK(uppercase.has_value());
    if (uppercase) {
        CHECK(*uppercase.value() == "AAPL");
    }

    const auto punctuation_and_digits = aegis::normalize_requested_symbol("brk.b1");
    CHECK(punctuation_and_digits.has_value());
    if (punctuation_and_digits) {
        CHECK(*punctuation_and_digits.value() == "BRK.B1");
    }

    const auto trailing_spaces = aegis::normalize_requested_symbol("aapl   ");
    CHECK(trailing_spaces.has_value());
    if (trailing_spaces) {
        CHECK(*trailing_spaces.value() == "AAPL");
    }

    const auto internal_space = aegis::normalize_requested_symbol("a b");
    CHECK(internal_space.has_value());
    if (internal_space) {
        CHECK(*internal_space.value() == "A B");
    }

    const std::string embedded_null_text{'A', 'A', '\0', 'P', 'L'};
    const auto embedded_null = aegis::normalize_requested_symbol(
        std::string_view{embedded_null_text.data(), embedded_null_text.size()});
    CHECK(!embedded_null.has_value());
    check_error(
        embedded_null.error(),
        aegis::ErrorCategory::Configuration,
        aegis::ErrorCode::InvalidRequestedSymbol);

    const auto overlength = aegis::normalize_requested_symbol("ABCDEFGHI");
    CHECK(!overlength.has_value());
    check_error(
        overlength.error(),
        aegis::ErrorCategory::Configuration,
        aegis::ErrorCode::InvalidRequestedSymbol);

    const auto empty = aegis::normalize_requested_symbol("");
    CHECK(!empty.has_value());
    check_error(
        empty.error(),
        aegis::ErrorCategory::Configuration,
        aegis::ErrorCode::InvalidRequestedSymbol);
}

void test_requested_case_variants_deduplicate()
{
    constexpr std::array requested{std::string_view{"AAPL"}, std::string_view{"aapl"},
                                   std::string_view{"Aapl"}};
    auto directory = configured_directory(requested);
    if (!directory) {
        return;
    }

    CHECK(directory->requested_symbol_count() == 1);
    CHECK(directory->discovered_requested_symbol_count() == 0);
    CHECK(!directory->all_requested_symbols_discovered());
}

void test_locate_mapping_and_selection()
{
    constexpr std::array requested{std::string_view{"aapl"}, std::string_view{"MSFT"}};
    auto directory = configured_directory(requested);
    if (!directory) {
        return;
    }

    const auto aapl_result = directory->observe(stock_directory(
        10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}));
    CHECK(aapl_result.has_value());
    CHECK(directory->known_locate_count() == 1);
    CHECK(directory->is_known(10));
    CHECK(directory->is_selected(10));
    CHECK(directory->symbol_for(10) == std::optional<std::string>{"AAPL"});

    const auto nvda_result = directory->observe(stock_directory(
        20, {'N', 'V', 'D', 'A', ' ', ' ', ' ', ' '}));
    CHECK(nvda_result.has_value());
    CHECK(directory->known_locate_count() == 2);
    CHECK(directory->is_known(20));
    CHECK(!directory->is_selected(20));
    CHECK(directory->symbol_for(20) == std::optional<std::string>{"NVDA"});
    CHECK(!directory->is_known(99));
    CHECK(!directory->is_selected(99));
    CHECK(directory->symbol_for(99) == std::nullopt);

    const auto aapl_discovered = directory->requested_symbol_discovered("aapl");
    CHECK(aapl_discovered.has_value());
    if (aapl_discovered) {
        CHECK(*aapl_discovered.value());
    }
    const auto aapl_locate = directory->discovered_locate("Aapl");
    CHECK(aapl_locate.has_value());
    if (aapl_locate) {
        CHECK(*aapl_locate.value() == std::optional<aegis::StockLocate>{10});
    }

    const auto msft_discovered = directory->requested_symbol_discovered("MSFT");
    CHECK(msft_discovered.has_value());
    if (msft_discovered) {
        CHECK(!*msft_discovered.value());
    }
    const auto msft_locate_before = directory->discovered_locate("msft");
    CHECK(msft_locate_before.has_value());
    if (msft_locate_before) {
        CHECK(*msft_locate_before.value() == std::nullopt);
    }
    CHECK(directory->discovered_requested_symbol_count() == 1);
    CHECK(!directory->all_requested_symbols_discovered());

    const auto msft_result = directory->observe(stock_directory(
        30, {'M', 'S', 'F', 'T', ' ', ' ', ' ', ' '}));
    CHECK(msft_result.has_value());
    CHECK(directory->is_selected(30));
    CHECK(directory->discovered_requested_symbol_count() == 2);
    CHECK(directory->all_requested_symbols_discovered());
    const auto msft_locate_after = directory->discovered_locate("MSFT");
    CHECK(msft_locate_after.has_value());
    if (msft_locate_after) {
        CHECK(*msft_locate_after.value() == std::optional<aegis::StockLocate>{30});
    }
}

void test_locate_zero_and_distinct_locates()
{
    constexpr std::array requested{std::string_view{"ZERO"}};
    auto directory = configured_directory(requested);
    if (!directory) {
        return;
    }

    CHECK(directory->observe(stock_directory(
              0, {'Z', 'E', 'R', 'O', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(directory->observe(stock_directory(
              1, {'O', 'T', 'H', 'R', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(directory->known_locate_count() == 2);
    CHECK(directory->is_known(0));
    CHECK(directory->is_selected(0));
    const auto locate = directory->discovered_locate("zero");
    CHECK(locate.has_value());
    if (locate) {
        CHECK(*locate.value() == std::optional<aegis::StockLocate>{0});
    }
}

void test_idempotent_observation()
{
    constexpr std::array requested{std::string_view{"AAPL"}};
    auto directory = configured_directory(requested);
    if (!directory) {
        return;
    }

    const auto message = stock_directory(
        42, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '});
    CHECK(directory->observe(message).has_value());
    CHECK(directory->observe(message).has_value());
    CHECK(directory->known_locate_count() == 1);
    CHECK(directory->requested_symbol_count() == 1);
    CHECK(directory->discovered_requested_symbol_count() == 1);
    CHECK(directory->all_requested_symbols_discovered());
}

void test_conflict_is_transactional()
{
    constexpr std::array requested{std::string_view{"AAPL"}, std::string_view{"MSFT"}};
    auto directory = configured_directory(requested);
    if (!directory) {
        return;
    }

    CHECK(directory->observe(stock_directory(
              42, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());
    const auto conflict = directory->observe(stock_directory(
        42, {'M', 'S', 'F', 'T', ' ', ' ', ' ', ' '}));
    CHECK(!conflict.has_value());
    check_error(
        conflict.error(),
        aegis::ErrorCategory::Session,
        aegis::ErrorCode::ConflictingStockLocate);
    if (conflict.error() != nullptr) {
        CHECK(conflict.error()->observed_value == 42);
    }

    CHECK(directory->known_locate_count() == 1);
    CHECK(directory->symbol_for(42) == std::optional<std::string>{"AAPL"});
    CHECK(directory->is_selected(42));
    CHECK(directory->discovered_requested_symbol_count() == 1);
    CHECK(!directory->all_requested_symbols_discovered());

    const auto aapl_locate = directory->discovered_locate("AAPL");
    CHECK(aapl_locate.has_value());
    if (aapl_locate) {
        CHECK(*aapl_locate.value() == std::optional<aegis::StockLocate>{42});
    }
    const auto msft_locate = directory->discovered_locate("MSFT");
    CHECK(msft_locate.has_value());
    if (msft_locate) {
        CHECK(*msft_locate.value() == std::nullopt);
    }
}

void test_selected_symbol_locate_conflict_is_transactional()
{
    constexpr std::array requested{std::string_view{"AAPL"}};
    auto directory = configured_directory(requested);
    if (!directory) {
        return;
    }

    CHECK(directory->observe(stock_directory(
              10, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}))
              .has_value());
    const auto conflict = directory->observe(stock_directory(
        20, {'A', 'A', 'P', 'L', ' ', ' ', ' ', ' '}));
    CHECK(!conflict.has_value());
    check_error(
        conflict.error(),
        aegis::ErrorCategory::Session,
        aegis::ErrorCode::ConflictingSelectedSymbolLocate);
    if (conflict.error() != nullptr) {
        CHECK(conflict.error()->observed_value == 20);
        CHECK(conflict.error()->limit_value == 10);
    }

    CHECK(directory->known_locate_count() == 1);
    CHECK(directory->discovered_requested_symbol_count() == 1);
    CHECK(directory->is_known(10));
    CHECK(directory->is_selected(10));
    CHECK(!directory->is_known(20));
    CHECK(!directory->is_selected(20));
    CHECK(directory->all_requested_symbols_discovered());
    const auto discovered = directory->discovered_locate("AAPL");
    CHECK(discovered.has_value());
    if (discovered) {
        CHECK(*discovered.value() == std::optional<aegis::StockLocate>{10});
    }
}

void test_duplicate_unrequested_symbol_locates_are_allowed()
{
    constexpr std::array<std::string_view, 0> requested{};
    auto directory = configured_directory(requested);
    if (!directory) {
        return;
    }

    CHECK(directory->observe(stock_directory(
              10, {'N', 'V', 'D', 'A', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(directory->observe(stock_directory(
              20, {'N', 'V', 'D', 'A', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(directory->known_locate_count() == 2);
    CHECK(directory->is_known(10));
    CHECK(directory->is_known(20));
    CHECK(!directory->is_selected(10));
    CHECK(!directory->is_selected(20));
}

void test_empty_requested_set()
{
    constexpr std::array<std::string_view, 0> requested{};
    auto directory = configured_directory(requested);
    if (!directory) {
        return;
    }

    CHECK(directory->requested_symbol_count() == 0);
    CHECK(directory->discovered_requested_symbol_count() == 0);
    CHECK(directory->all_requested_symbols_discovered());

    CHECK(directory->observe(stock_directory(
              7, {'N', 'V', 'D', 'A', ' ', ' ', ' ', ' '}))
              .has_value());
    CHECK(directory->is_known(7));
    CHECK(!directory->is_selected(7));
    CHECK(directory->all_requested_symbols_discovered());
}

void test_invalid_configuration_is_structured()
{
    constexpr std::array requested{std::string_view{"AAPL"},
                                   std::string_view{"TOO-LONG-1"}};
    const auto result = aegis::SymbolDirectory::create(
        std::span<const std::string_view>{requested});
    CHECK(!result.has_value());
    check_error(
        result.error(),
        aegis::ErrorCategory::Configuration,
        aegis::ErrorCode::InvalidRequestedSymbol);
}

}  // namespace

int main()
{
    test_feed_normalization();
    test_requested_normalization();
    test_requested_case_variants_deduplicate();
    test_locate_mapping_and_selection();
    test_locate_zero_and_distinct_locates();
    test_idempotent_observation();
    test_conflict_is_transactional();
    test_selected_symbol_locate_conflict_is_transactional();
    test_duplicate_unrequested_symbol_locates_are_allowed();
    test_empty_requested_set();
    test_invalid_configuration_is_structured();
    return failure_count == 0 ? 0 : 1;
}
