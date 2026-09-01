#include "aegis/itch/itch_lengths.hpp"
#include "aegis/itch/itch_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

namespace {

static_assert(std::is_same_v<aegis::Price4, std::uint32_t>);
static_assert(!std::is_floating_point_v<aegis::Price4>);
static_assert(aegis::kMaxPrice4 == 2'000'000'000U);
static_assert(std::numeric_limits<aegis::Price4>::max() >= aegis::kMaxPrice4);
static_assert(std::is_same_v<decltype(aegis::ItchCommonHeader{}.stock_locate), std::uint16_t>);
static_assert(std::is_same_v<decltype(aegis::ItchCommonHeader{}.tracking_number), std::uint16_t>);
static_assert(std::is_same_v<decltype(aegis::ItchCommonHeader{}.timestamp_ns), std::uint64_t>);
static_assert(std::is_same_v<decltype(aegis::AddOrder{}.stock), std::array<char, 8>>);
static_assert(
    std::is_same_v<decltype(aegis::AddOrder{}.attribution),
                   std::optional<std::array<char, 4>>>);
static_assert(std::is_same_v<decltype(aegis::StockDirectory{}.issue_subtype),
                             std::array<char, 2>>);
static_assert(std::is_same_v<decltype(aegis::OrderExecuted{}.order_reference), std::uint64_t>);
static_assert(std::is_same_v<decltype(aegis::OrderExecuted{}.match_number), std::uint64_t>);
static_assert(std::is_same_v<decltype(aegis::OrderCancel{}.cancelled_shares), std::uint32_t>);
static_assert(std::is_same_v<decltype(aegis::OrderReplace{}.price_1e4), aegis::Price4>);
static_assert(std::is_copy_constructible_v<aegis::AddOrder>);
static_assert(std::is_nothrow_move_constructible_v<aegis::AddOrder>);
static_assert(std::is_nothrow_move_constructible_v<aegis::KnownBookNeutral>);

struct ExpectedEntry {
    char type;
    std::size_t length;
};

constexpr std::array required_entries{
    ExpectedEntry{'S', 12},
    ExpectedEntry{'R', 39},
    ExpectedEntry{'A', 36},
    ExpectedEntry{'F', 40},
    ExpectedEntry{'E', 31},
    ExpectedEntry{'C', 36},
    ExpectedEntry{'X', 23},
    ExpectedEntry{'D', 19},
    ExpectedEntry{'U', 35},
};

constexpr std::array neutral_entries{
    ExpectedEntry{'H', 25},
    ExpectedEntry{'Y', 20},
    ExpectedEntry{'L', 26},
    ExpectedEntry{'V', 35},
    ExpectedEntry{'W', 12},
    ExpectedEntry{'K', 28},
    ExpectedEntry{'J', 35},
    ExpectedEntry{'h', 21},
    ExpectedEntry{'P', 44},
    ExpectedEntry{'Q', 40},
    ExpectedEntry{'B', 19},
    ExpectedEntry{'I', 50},
    ExpectedEntry{'N', 20},
    ExpectedEntry{'O', 48},
};

static_assert(aegis::expected_itch_length('A') == 36);
static_assert(aegis::classify_itch_type('A') == aegis::ItchTypeClass::Required);
static_assert(
    aegis::classify_itch_type('h') == aegis::ItchTypeClass::KnownBookNeutral);
static_assert(aegis::classify_itch_type('?') == aegis::ItchTypeClass::Unknown);
static_assert(!aegis::expected_itch_length('?').has_value());

std::size_t failure_count = 0;

void check(const bool condition, const std::string_view expression, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << "itch_types_test.cpp:" << line << ": check failed: " << expression << '\n';
    ++failure_count;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

[[nodiscard]] constexpr char protocol_code(const std::uint8_t value) noexcept
{
    return static_cast<char>(value);
}

void test_required_registry()
{
    for (const auto& entry : required_entries) {
        CHECK(aegis::classify_itch_type(entry.type) == aegis::ItchTypeClass::Required);

        const auto length = aegis::expected_itch_length(entry.type);
        CHECK(length.has_value());
        if (length.has_value()) {
            CHECK(*length == entry.length);
        }
    }
}

void test_known_book_neutral_registry()
{
    for (const auto& entry : neutral_entries) {
        CHECK(
            aegis::classify_itch_type(entry.type) == aegis::ItchTypeClass::KnownBookNeutral);

        const auto length = aegis::expected_itch_length(entry.type);
        CHECK(length.has_value());
        if (length.has_value()) {
            CHECK(*length == entry.length);
        }
    }
}

void test_unknown_types()
{
    constexpr std::array unknown_types{
        '\0',
        '?',
        'Z',
        static_cast<char>(0x7F),
        static_cast<char>(0x80),
        static_cast<char>(0xFF),
    };

    for (const char type : unknown_types) {
        CHECK(aegis::classify_itch_type(type) == aegis::ItchTypeClass::Unknown);
        CHECK(!aegis::expected_itch_length(type).has_value());
    }
}

void test_price_representation()
{
    const aegis::Price4 maximum = aegis::kMaxPrice4;
    CHECK(maximum == 2'000'000'000U);
    CHECK(maximum < std::numeric_limits<aegis::Price4>::max());
}

void test_side_codes()
{
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::Side::Buy)) == 'B');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::Side::Sell)) == 'S');
}

void test_system_event_codes()
{
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::SystemEventCode::StartOfMessages)) ==
          'O');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::SystemEventCode::StartOfSystemHours)) ==
          'S');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::SystemEventCode::StartOfMarketHours)) ==
          'Q');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::SystemEventCode::EndOfMarketHours)) ==
          'M');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::SystemEventCode::EndOfSystemHours)) ==
          'E');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::SystemEventCode::EndOfMessages)) ==
          'C');
    CHECK(protocol_code(static_cast<std::uint8_t>(
              aegis::SystemEventCode::EmergencyMarketConditionHalt)) == 'A');
    CHECK(protocol_code(static_cast<std::uint8_t>(
              aegis::SystemEventCode::EmergencyMarketConditionQuoteOnly)) == 'R');
    CHECK(protocol_code(static_cast<std::uint8_t>(
              aegis::SystemEventCode::EmergencyMarketConditionResumption)) == 'B');
}

void test_stock_directory_codes()
{
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::MarketCategory::InvestorsExchange)) ==
          'V');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::MarketCategory::NyseTexas)) == 'M');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::MarketCategory::TexasStockExchange)) ==
          'F');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::IpoFlag::NotAvailable)) == ' ');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::IpoFlag::NotIpo)) == 'N');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::IpoFlag::Ipo)) == 'Y');
    CHECK(protocol_code(static_cast<std::uint8_t>(aegis::IpoFlag::NonIpoNewIssue)) == 'Z');
}

}  // namespace

int main()
{
    test_required_registry();
    test_known_book_neutral_registry();
    test_unknown_types();
    test_price_representation();
    test_side_codes();
    test_system_event_codes();
    test_stock_directory_codes();
    return failure_count == 0 ? 0 : 1;
}
