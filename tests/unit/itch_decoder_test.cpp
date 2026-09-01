#include "aegis/itch/itch_decoder.hpp"

#include "aegis/common/error.hpp"
#include "aegis/common/result.hpp"
#include "aegis/itch/itch_lengths.hpp"
#include "aegis/itch/itch_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

namespace {

constexpr std::array<std::byte, 12> system_event_fixture{
    std::byte{'S'},
    std::byte{0x12},
    std::byte{0x34},
    std::byte{0x56},
    std::byte{0x78},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
    std::byte{0x05},
    std::byte{0x06},
    std::byte{'A'},
};

constexpr std::array<std::byte, 39> stock_directory_fixture{
    std::byte{'R'},
    std::byte{0xAB},
    std::byte{0xCD},
    std::byte{0x13},
    std::byte{0x57},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
    std::byte{0x05},
    std::byte{0x06},
    std::byte{'A'},
    std::byte{'E'},
    std::byte{'G'},
    std::byte{'I'},
    std::byte{'S'},
    std::byte{' '},
    std::byte{' '},
    std::byte{' '},
    std::byte{'F'},
    std::byte{'N'},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
    std::byte{'Y'},
    std::byte{'C'},
    std::byte{'E'},
    std::byte{'U'},
    std::byte{'P'},
    std::byte{'N'},
    std::byte{'Z'},
    std::byte{'1'},
    std::byte{'Y'},
    std::byte{0x0A},
    std::byte{0x0B},
    std::byte{0x0C},
    std::byte{0x0D},
    std::byte{'Y'},
};

constexpr std::array<std::byte, 36> add_order_fixture{
    std::byte{'A'},
    std::byte{0x12},
    std::byte{0x34},
    std::byte{0x56},
    std::byte{0x78},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
    std::byte{0x05},
    std::byte{0x06},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
    std::byte{0x05},
    std::byte{0x06},
    std::byte{0x07},
    std::byte{0x08},
    std::byte{'B'},
    std::byte{0x0A},
    std::byte{0x0B},
    std::byte{0x0C},
    std::byte{0x0D},
    std::byte{'A'},
    std::byte{'E'},
    std::byte{'G'},
    std::byte{'I'},
    std::byte{'S'},
    std::byte{' '},
    std::byte{' '},
    std::byte{' '},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
};

constexpr std::array<std::byte, 40> attributed_add_order_fixture{
    std::byte{'F'},
    std::byte{0xAB},
    std::byte{0xCD},
    std::byte{0x13},
    std::byte{0x57},
    std::byte{0x0A},
    std::byte{0x0B},
    std::byte{0x0C},
    std::byte{0x0D},
    std::byte{0x0E},
    std::byte{0x0F},
    std::byte{0x88},
    std::byte{0x77},
    std::byte{0x66},
    std::byte{0x55},
    std::byte{0x44},
    std::byte{0x33},
    std::byte{0x22},
    std::byte{0x11},
    std::byte{'S'},
    std::byte{0x10},
    std::byte{0x20},
    std::byte{0x30},
    std::byte{0x40},
    std::byte{'P'},
    std::byte{'H'},
    std::byte{'A'},
    std::byte{'S'},
    std::byte{'E'},
    std::byte{'2'},
    std::byte{'C'},
    std::byte{' '},
    std::byte{0x07},
    std::byte{0x5B},
    std::byte{0xCD},
    std::byte{0x15},
    std::byte{'A'},
    std::byte{'B'},
    std::byte{'C'},
    std::byte{'D'},
};

constexpr std::array<std::byte, 31> order_executed_fixture{
    std::byte{'E'},
    std::byte{0x12},
    std::byte{0x34},
    std::byte{0x56},
    std::byte{0x78},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
    std::byte{0x05},
    std::byte{0x06},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
    std::byte{0x05},
    std::byte{0x06},
    std::byte{0x07},
    std::byte{0x08},
    std::byte{0x10},
    std::byte{0x20},
    std::byte{0x30},
    std::byte{0x40},
    std::byte{0x88},
    std::byte{0x77},
    std::byte{0x66},
    std::byte{0x55},
    std::byte{0x44},
    std::byte{0x33},
    std::byte{0x22},
    std::byte{0x11},
};

constexpr std::array<std::byte, 36> order_executed_with_price_fixture{
    std::byte{'C'},
    std::byte{0xAB},
    std::byte{0xCD},
    std::byte{0x13},
    std::byte{0x57},
    std::byte{0x0A},
    std::byte{0x0B},
    std::byte{0x0C},
    std::byte{0x0D},
    std::byte{0x0E},
    std::byte{0x0F},
    std::byte{0x11},
    std::byte{0x22},
    std::byte{0x33},
    std::byte{0x44},
    std::byte{0x55},
    std::byte{0x66},
    std::byte{0x77},
    std::byte{0x88},
    std::byte{0x0A},
    std::byte{0x0B},
    std::byte{0x0C},
    std::byte{0x0D},
    std::byte{0x01},
    std::byte{0x23},
    std::byte{0x45},
    std::byte{0x67},
    std::byte{0x89},
    std::byte{0xAB},
    std::byte{0xCD},
    std::byte{0xEF},
    std::byte{'Y'},
    std::byte{0x07},
    std::byte{0x5B},
    std::byte{0xCD},
    std::byte{0x15},
};

constexpr std::array<std::byte, 23> order_cancel_fixture{
    std::byte{'X'},
    std::byte{0x24},
    std::byte{0x68},
    std::byte{0xAC},
    std::byte{0xE0},
    std::byte{0x11},
    std::byte{0x12},
    std::byte{0x13},
    std::byte{0x14},
    std::byte{0x15},
    std::byte{0x16},
    std::byte{0x10},
    std::byte{0x20},
    std::byte{0x30},
    std::byte{0x40},
    std::byte{0x50},
    std::byte{0x60},
    std::byte{0x70},
    std::byte{0x80},
    std::byte{0x01},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
};

constexpr std::array<std::byte, 19> order_delete_fixture{
    std::byte{'D'},
    std::byte{0x0F},
    std::byte{0x1E},
    std::byte{0x2D},
    std::byte{0x3C},
    std::byte{0x21},
    std::byte{0x22},
    std::byte{0x23},
    std::byte{0x24},
    std::byte{0x25},
    std::byte{0x26},
    std::byte{0xA1},
    std::byte{0xA2},
    std::byte{0xA3},
    std::byte{0xA4},
    std::byte{0xA5},
    std::byte{0xA6},
    std::byte{0xA7},
    std::byte{0xA8},
};

constexpr std::array<std::byte, 35> order_replace_fixture{
    std::byte{'U'},
    std::byte{0xBE},
    std::byte{0xEF},
    std::byte{0xCA},
    std::byte{0xFE},
    std::byte{0x31},
    std::byte{0x32},
    std::byte{0x33},
    std::byte{0x34},
    std::byte{0x35},
    std::byte{0x36},
    std::byte{0x0A},
    std::byte{0x1B},
    std::byte{0x2C},
    std::byte{0x3D},
    std::byte{0x4E},
    std::byte{0x5F},
    std::byte{0x60},
    std::byte{0x71},
    std::byte{0x91},
    std::byte{0xA2},
    std::byte{0xB3},
    std::byte{0xC4},
    std::byte{0xD5},
    std::byte{0xE6},
    std::byte{0xF7},
    std::byte{0x08},
    std::byte{0x50},
    std::byte{0x60},
    std::byte{0x70},
    std::byte{0x80},
    std::byte{0x02},
    std::byte{0x03},
    std::byte{0x04},
    std::byte{0x05},
};

struct NeutralDispatchEntry {
    char type;
    std::size_t length;
};

constexpr std::array neutral_dispatch_entries{
    NeutralDispatchEntry{'H', 25},
    NeutralDispatchEntry{'Y', 20},
    NeutralDispatchEntry{'L', 26},
    NeutralDispatchEntry{'V', 35},
    NeutralDispatchEntry{'W', 12},
    NeutralDispatchEntry{'K', 28},
    NeutralDispatchEntry{'J', 35},
    NeutralDispatchEntry{'h', 21},
    NeutralDispatchEntry{'P', 44},
    NeutralDispatchEntry{'Q', 40},
    NeutralDispatchEntry{'B', 19},
    NeutralDispatchEntry{'I', 50},
    NeutralDispatchEntry{'N', 20},
    NeutralDispatchEntry{'O', 48},
};

static_assert(system_event_fixture.size() == 12);
static_assert(stock_directory_fixture.size() == 39);
static_assert(add_order_fixture.size() == 36);
static_assert(attributed_add_order_fixture.size() == 40);
static_assert(order_executed_fixture.size() == 31);
static_assert(order_executed_with_price_fixture.size() == 36);
static_assert(order_cancel_fixture.size() == 23);
static_assert(order_delete_fixture.size() == 19);
static_assert(order_replace_fixture.size() == 35);
static_assert(std::is_trivially_copyable_v<aegis::ItchDecodeContext>);
static_assert(std::is_nothrow_move_constructible_v<aegis::SystemEvent>);
static_assert(std::is_nothrow_move_constructible_v<aegis::StockDirectory>);
static_assert(std::is_nothrow_move_constructible_v<aegis::AddOrder>);
static_assert(std::variant_size_v<aegis::DecodedItchMessage> == 9);
static_assert(std::is_nothrow_move_constructible_v<aegis::DecodedItchMessage>);

std::size_t failure_count = 0;

void check(const bool condition, const std::string_view expression, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << "itch_decoder_test.cpp:" << line << ": check failed: " << expression << '\n';
    ++failure_count;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename T>
[[nodiscard]] const aegis::Error* check_failure(
    const aegis::Result<T>& result,
    const aegis::ErrorCode expected_code)
{
    CHECK(!result.has_value());
    CHECK(!static_cast<bool>(result));
    CHECK(result.value() == nullptr);

    const aegis::Error* const error = result.error();
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->category == aegis::ErrorCategory::ItchDecode);
        CHECK(error->code == expected_code);
        CHECK(!error->message.empty());
    }
    return error;
}

template <typename Enum>
[[nodiscard]] constexpr std::uint8_t protocol_code(const Enum value) noexcept
{
    return static_cast<std::uint8_t>(value);
}

[[nodiscard]] std::array<std::byte, 50> make_neutral_payload(
    const char type,
    const std::size_t length)
{
    std::array<std::byte, 50> payload{};
    payload.fill(std::byte{0xA5});
    payload[0] = static_cast<std::byte>(static_cast<unsigned char>(type));
    payload[1] = std::byte{0x12};
    payload[2] = std::byte{0x34};
    payload[3] = std::byte{0x56};
    payload[4] = std::byte{0x78};
    payload[5] = std::byte{0x01};
    payload[6] = std::byte{0x02};
    payload[7] = std::byte{0x03};
    payload[8] = std::byte{0x04};
    payload[9] = std::byte{0x05};
    payload[10] = std::byte{0x06};
    CHECK(length >= 11);
    CHECK(length <= payload.size());
    return payload;
}

void check_unified_exact_length(
    const std::span<const std::byte> fixture,
    const char expected_type,
    const std::size_t expected_size)
{
    CHECK(fixture.size() == expected_size);
    constexpr std::uint64_t sequence = 0xABCDEF0123456789ULL;

    const auto short_result = aegis::decode_itch(
        fixture.first(expected_size - 1), aegis::ItchDecodeContext{sequence});
    const aegis::Error* const short_error =
        check_failure(short_result, aegis::ErrorCode::InvalidItchLength);
    if (short_error != nullptr) {
        CHECK(short_error->offset == 0);
        CHECK(short_error->requested_size == expected_size);
        CHECK(short_error->available_size == expected_size - 1);
        CHECK(short_error->observed_value == expected_size - 1);
        CHECK(short_error->limit_value == expected_size);
        CHECK(short_error->sequence == sequence);
        CHECK(short_error->message_type == static_cast<std::uint8_t>(expected_type));
    }

    std::array<std::byte, 51> long_payload{};
    for (std::size_t index = 0; index < fixture.size(); ++index) {
        long_payload[index] = fixture[index];
    }
    long_payload[expected_size] = std::byte{0xFF};
    const auto long_result = aegis::decode_itch(
        std::span<const std::byte>{long_payload}.first(expected_size + 1),
        aegis::ItchDecodeContext{sequence});
    const aegis::Error* const long_error =
        check_failure(long_result, aegis::ErrorCode::InvalidItchLength);
    if (long_error != nullptr) {
        CHECK(long_error->offset == 0);
        CHECK(long_error->requested_size == expected_size);
        CHECK(long_error->available_size == expected_size + 1);
        CHECK(long_error->observed_value == expected_size + 1);
        CHECK(long_error->limit_value == expected_size);
        CHECK(long_error->sequence == sequence);
        CHECK(long_error->message_type == static_cast<std::uint8_t>(expected_type));
    }
}

template <typename Expected, std::size_t Size, typename Validator>
void check_required_dispatch(
    const std::array<std::byte, Size>& fixture,
    Validator validate)
{
    const auto result = aegis::decode_itch(fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::DecodedItchMessage* const decoded = result.value();
    CHECK(decoded != nullptr);
    if (decoded == nullptr) {
        return;
    }

    CHECK(std::holds_alternative<Expected>(*decoded));
    const Expected* const message = std::get_if<Expected>(decoded);
    CHECK(message != nullptr);
    if (message != nullptr) {
        validate(*message);
    }
}

void check_unified_validation_error(
    const aegis::Result<aegis::DecodedItchMessage>& result,
    const aegis::ErrorCode expected_code,
    const std::size_t expected_offset,
    const std::uint64_t expected_observed,
    const std::uint64_t expected_limit,
    const char expected_type,
    const std::uint64_t expected_sequence)
{
    const aegis::Error* const error = check_failure(result, expected_code);
    if (error != nullptr) {
        CHECK(error->offset == expected_offset);
        CHECK(error->observed_value == expected_observed);
        CHECK(error->limit_value == expected_limit);
        CHECK(error->message_type == static_cast<std::uint8_t>(expected_type));
        CHECK(error->sequence == expected_sequence);
    }
}

template <typename Message, std::size_t Size>
void check_exact_length_validation(
    const std::array<std::byte, Size>& fixture,
    const char expected_type,
    aegis::Result<Message> (*const decoder)(
        std::span<const std::byte>, aegis::ItchDecodeContext) noexcept)
{
    constexpr std::uint64_t sequence = 0x1020304050607080ULL;
    const auto short_result = decoder(
        std::span<const std::byte>{fixture}.first(Size - 1),
        aegis::ItchDecodeContext{sequence});
    const aegis::Error* const short_error =
        check_failure(short_result, aegis::ErrorCode::InvalidItchLength);
    if (short_error != nullptr) {
        CHECK(short_error->offset == 0);
        CHECK(short_error->requested_size == Size);
        CHECK(short_error->available_size == Size - 1);
        CHECK(short_error->observed_value == Size - 1);
        CHECK(short_error->limit_value == Size);
        CHECK(short_error->sequence == sequence);
        CHECK(short_error->message_type == static_cast<std::uint8_t>(expected_type));
    }

    std::array<std::byte, Size + 1> long_payload{};
    for (std::size_t index = 0; index < fixture.size(); ++index) {
        long_payload[index] = fixture[index];
    }
    long_payload[Size] = std::byte{0xFF};
    const auto long_result = decoder(long_payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const long_error =
        check_failure(long_result, aegis::ErrorCode::InvalidItchLength);
    if (long_error != nullptr) {
        CHECK(long_error->offset == 0);
        CHECK(long_error->requested_size == Size);
        CHECK(long_error->available_size == Size + 1);
        CHECK(long_error->observed_value == Size + 1);
        CHECK(long_error->limit_value == Size);
        CHECK(long_error->sequence == sequence);
        CHECK(long_error->message_type == static_cast<std::uint8_t>(expected_type));
    }
}

template <typename Message, std::size_t Size>
void check_wrong_type_validation(
    const std::array<std::byte, Size>& fixture,
    const char expected_type,
    aegis::Result<Message> (*const decoder)(
        std::span<const std::byte>, aegis::ItchDecodeContext) noexcept)
{
    auto payload = fixture;
    payload[0] = std::byte{'S'};
    constexpr std::uint64_t sequence = 0x8877665544332211ULL;
    const auto result = decoder(payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const error =
        check_failure(result, aegis::ErrorCode::UnexpectedItchType);
    if (error != nullptr) {
        CHECK(error->offset == 0);
        CHECK(error->requested_size == 1);
        CHECK(error->available_size == 1);
        CHECK(error->observed_value == static_cast<std::uint8_t>('S'));
        CHECK(error->limit_value == static_cast<std::uint8_t>(expected_type));
        CHECK(error->sequence == sequence);
        CHECK(error->message_type == static_cast<std::uint8_t>('S'));
    }
}

void test_system_event_golden_fixture()
{
    const auto result = aegis::decode_system_event(system_event_fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::SystemEvent* const message = result.value();
    CHECK(message != nullptr);
    if (message == nullptr) {
        return;
    }

    CHECK(message->header.type == 'S');
    CHECK(message->header.stock_locate == std::uint16_t{0x1234});
    CHECK(message->header.tracking_number == std::uint16_t{0x5678});
    CHECK(message->header.timestamp_ns == std::uint64_t{0x010203040506ULL});
    CHECK(message->event_code == aegis::SystemEventCode::EmergencyMarketConditionHalt);
}

void test_all_documented_system_event_codes()
{
    struct ExpectedEvent {
        std::byte encoded;
        aegis::SystemEventCode decoded;
    };

    constexpr std::array events{
        ExpectedEvent{std::byte{'O'}, aegis::SystemEventCode::StartOfMessages},
        ExpectedEvent{std::byte{'S'}, aegis::SystemEventCode::StartOfSystemHours},
        ExpectedEvent{std::byte{'Q'}, aegis::SystemEventCode::StartOfMarketHours},
        ExpectedEvent{std::byte{'M'}, aegis::SystemEventCode::EndOfMarketHours},
        ExpectedEvent{std::byte{'E'}, aegis::SystemEventCode::EndOfSystemHours},
        ExpectedEvent{std::byte{'C'}, aegis::SystemEventCode::EndOfMessages},
        ExpectedEvent{std::byte{'A'}, aegis::SystemEventCode::EmergencyMarketConditionHalt},
        ExpectedEvent{
            std::byte{'R'}, aegis::SystemEventCode::EmergencyMarketConditionQuoteOnly},
        ExpectedEvent{
            std::byte{'B'}, aegis::SystemEventCode::EmergencyMarketConditionResumption},
    };

    for (const auto& event : events) {
        auto payload = system_event_fixture;
        payload[11] = event.encoded;
        const auto result = aegis::decode_system_event(payload);
        CHECK(result.has_value());
        if (result.value() != nullptr) {
            CHECK(result.value()->event_code == event.decoded);
        }
    }
}

void test_system_event_exact_length_validation()
{
    constexpr std::uint64_t sequence = 0x1122334455667788ULL;
    const auto short_result = aegis::decode_system_event(
        std::span<const std::byte>{system_event_fixture}.first(11),
        aegis::ItchDecodeContext{sequence});
    const aegis::Error* const short_error =
        check_failure(short_result, aegis::ErrorCode::InvalidItchLength);
    if (short_error != nullptr) {
        CHECK(short_error->offset == 0);
        CHECK(short_error->requested_size == 12);
        CHECK(short_error->available_size == 11);
        CHECK(short_error->observed_value == 11);
        CHECK(short_error->limit_value == 12);
        CHECK(short_error->sequence == sequence);
        CHECK(short_error->message_type == static_cast<std::uint8_t>('S'));
        CHECK(short_error->message == "ITCH payload length does not match message type");
    }

    std::array<std::byte, 13> long_payload{};
    for (std::size_t index = 0; index < system_event_fixture.size(); ++index) {
        long_payload[index] = system_event_fixture[index];
    }
    long_payload[12] = std::byte{0xFF};
    const auto long_result = aegis::decode_system_event(long_payload);
    const aegis::Error* const long_error =
        check_failure(long_result, aegis::ErrorCode::InvalidItchLength);
    if (long_error != nullptr) {
        CHECK(long_error->requested_size == 12);
        CHECK(long_error->available_size == 13);
    }

    std::array<std::byte, 11> wrong_type_and_short{};
    for (std::size_t index = 0; index < wrong_type_and_short.size(); ++index) {
        wrong_type_and_short[index] = system_event_fixture[index];
    }
    wrong_type_and_short[0] = std::byte{'R'};
    const auto validation_order = aegis::decode_system_event(wrong_type_and_short);
    CHECK(check_failure(validation_order, aegis::ErrorCode::InvalidItchLength) != nullptr);
}

void test_system_event_wrong_type()
{
    constexpr std::uint64_t sequence = 987'654'321ULL;
    auto payload = system_event_fixture;
    payload[0] = std::byte{'R'};

    const auto result = aegis::decode_system_event(
        payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const error =
        check_failure(result, aegis::ErrorCode::UnexpectedItchType);
    if (error != nullptr) {
        CHECK(error->offset == 0);
        CHECK(error->sequence == sequence);
        CHECK(error->message_type == static_cast<std::uint8_t>('R'));
        CHECK(error->observed_value == static_cast<std::uint8_t>('R'));
        CHECK(error->limit_value == static_cast<std::uint8_t>('S'));
    }
}

void test_system_event_invalid_enum()
{
    auto payload = system_event_fixture;
    payload[11] = std::byte{'?'};

    const auto result = aegis::decode_system_event(
        payload, aegis::ItchDecodeContext{42});
    const aegis::Error* const error =
        check_failure(result, aegis::ErrorCode::InvalidItchEnum);
    if (error != nullptr) {
        CHECK(error->offset == 11);
        CHECK(error->sequence == 42);
        CHECK(error->message_type == static_cast<std::uint8_t>('S'));
        CHECK(error->observed_value == static_cast<std::uint8_t>('?'));
    }
}

void test_stock_directory_golden_fixture()
{
    const auto result = aegis::decode_stock_directory(stock_directory_fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::StockDirectory* const message = result.value();
    CHECK(message != nullptr);
    if (message == nullptr) {
        return;
    }

    constexpr aegis::StockSymbol expected_stock{'A', 'E', 'G', 'I', 'S', ' ', ' ', ' '};
    constexpr aegis::IssueSubtype expected_subtype{'E', 'U'};

    CHECK(message->header.type == 'R');
    CHECK(message->header.stock_locate == std::uint16_t{0xABCD});
    CHECK(message->header.tracking_number == std::uint16_t{0x1357});
    CHECK(message->header.timestamp_ns == std::uint64_t{0x010203040506ULL});
    CHECK(message->stock == expected_stock);
    CHECK(message->market_category == aegis::MarketCategory::TexasStockExchange);
    CHECK(message->financial_status_indicator == aegis::FinancialStatusIndicator::Normal);
    CHECK(message->round_lot_size == std::uint32_t{0x01020304});
    CHECK(message->round_lots_only == aegis::RoundLotsOnly::Yes);
    CHECK(message->issue_classification == 'C');
    CHECK(message->issue_subtype == expected_subtype);
    CHECK(message->authenticity == aegis::Authenticity::Production);
    CHECK(message->short_sale_threshold_indicator ==
          aegis::ShortSaleThresholdIndicator::NotRestricted);
    CHECK(message->ipo_flag == aegis::IpoFlag::NonIpoNewIssue);
    CHECK(message->luld_reference_price_tier == aegis::LuldReferencePriceTier::TierOne);
    CHECK(message->etp_flag == aegis::EtpFlag::Etp);
    CHECK(message->etp_leverage_factor == std::uint32_t{0x0A0B0C0D});
    CHECK(message->inverse_indicator == aegis::InverseIndicator::Inverse);
}

template <std::size_t Size>
void check_accepted_stock_directory_codes(
    const std::size_t offset,
    const std::array<std::byte, Size>& accepted_values)
{
    for (const std::byte value : accepted_values) {
        auto payload = stock_directory_fixture;
        payload[offset] = value;
        const auto result = aegis::decode_stock_directory(payload);
        CHECK(result.has_value());
    }
}

void test_all_documented_stock_directory_enum_codes()
{
    check_accepted_stock_directory_codes(
        19,
        std::array{
            std::byte{' '},
            std::byte{'Q'},
            std::byte{'G'},
            std::byte{'S'},
            std::byte{'N'},
            std::byte{'A'},
            std::byte{'P'},
            std::byte{'Z'},
            std::byte{'V'},
            std::byte{'M'},
            std::byte{'F'},
        });
    check_accepted_stock_directory_codes(
        20,
        std::array{
            std::byte{' '},
            std::byte{'D'},
            std::byte{'E'},
            std::byte{'Q'},
            std::byte{'S'},
            std::byte{'G'},
            std::byte{'H'},
            std::byte{'J'},
            std::byte{'K'},
            std::byte{'C'},
            std::byte{'N'},
        });
    check_accepted_stock_directory_codes(25, std::array{std::byte{'N'}, std::byte{'Y'}});
    check_accepted_stock_directory_codes(29, std::array{std::byte{'P'}, std::byte{'T'}});
    check_accepted_stock_directory_codes(
        30, std::array{std::byte{' '}, std::byte{'N'}, std::byte{'Y'}});
    check_accepted_stock_directory_codes(
        31, std::array{std::byte{' '}, std::byte{'N'}, std::byte{'Y'}, std::byte{'Z'}});
    check_accepted_stock_directory_codes(
        32, std::array{std::byte{' '}, std::byte{'1'}, std::byte{'2'}});
    check_accepted_stock_directory_codes(
        33, std::array{std::byte{' '}, std::byte{'N'}, std::byte{'Y'}});
    check_accepted_stock_directory_codes(38, std::array{std::byte{'N'}, std::byte{'Y'}});
}

void test_stock_directory_exact_length_validation()
{
    const auto short_result = aegis::decode_stock_directory(
        std::span<const std::byte>{stock_directory_fixture}.first(38));
    const aegis::Error* const short_error =
        check_failure(short_result, aegis::ErrorCode::InvalidItchLength);
    if (short_error != nullptr) {
        CHECK(short_error->requested_size == 39);
        CHECK(short_error->available_size == 38);
        CHECK(short_error->message_type == static_cast<std::uint8_t>('R'));
    }

    std::array<std::byte, 40> long_payload{};
    for (std::size_t index = 0; index < stock_directory_fixture.size(); ++index) {
        long_payload[index] = stock_directory_fixture[index];
    }
    long_payload[39] = std::byte{0x00};
    const auto long_result = aegis::decode_stock_directory(long_payload);
    const aegis::Error* const long_error =
        check_failure(long_result, aegis::ErrorCode::InvalidItchLength);
    if (long_error != nullptr) {
        CHECK(long_error->requested_size == 39);
        CHECK(long_error->available_size == 40);
    }
}

void test_stock_directory_wrong_type()
{
    auto payload = stock_directory_fixture;
    payload[0] = std::byte{'S'};

    const auto result = aegis::decode_stock_directory(payload);
    const aegis::Error* const error =
        check_failure(result, aegis::ErrorCode::UnexpectedItchType);
    if (error != nullptr) {
        CHECK(error->offset == 0);
        CHECK(error->message_type == static_cast<std::uint8_t>('S'));
        CHECK(error->observed_value == static_cast<std::uint8_t>('S'));
        CHECK(error->limit_value == static_cast<std::uint8_t>('R'));
    }
}

void test_stock_directory_invalid_enums()
{
    struct InvalidField {
        std::size_t offset;
        std::byte value;
    };

    constexpr std::array invalid_fields{
        InvalidField{19, std::byte{0xFF}},
        InvalidField{20, std::byte{'?'}},
        InvalidField{25, std::byte{'?'}},
        InvalidField{29, std::byte{'?'}},
        InvalidField{30, std::byte{'?'}},
        InvalidField{31, std::byte{'?'}},
        InvalidField{32, std::byte{'?'}},
        InvalidField{33, std::byte{'?'}},
        InvalidField{38, std::byte{'?'}},
    };
    constexpr std::uint64_t sequence = 0x0102030405060708ULL;

    for (const auto& field : invalid_fields) {
        auto payload = stock_directory_fixture;
        payload[field.offset] = field.value;
        const auto result = aegis::decode_stock_directory(
            payload, aegis::ItchDecodeContext{sequence});
        const aegis::Error* const error =
            check_failure(result, aegis::ErrorCode::InvalidItchEnum);
        if (error != nullptr) {
            CHECK(error->offset == field.offset);
            CHECK(error->sequence == sequence);
            CHECK(error->message_type == static_cast<std::uint8_t>('R'));
            CHECK(error->observed_value == std::to_integer<std::uint8_t>(field.value));
        }
    }
}

void test_stock_directory_invalid_ascii()
{
    struct InvalidField {
        std::size_t offset;
        std::byte value;
    };

    constexpr std::array invalid_fields{
        InvalidField{11, std::byte{0x80}},
        InvalidField{14, std::byte{0x00}},
        InvalidField{26, std::byte{0xFF}},
        InvalidField{28, std::byte{0x1F}},
    };

    for (const auto& field : invalid_fields) {
        auto payload = stock_directory_fixture;
        payload[field.offset] = field.value;
        const auto result = aegis::decode_stock_directory(payload);
        const aegis::Error* const error =
            check_failure(result, aegis::ErrorCode::InvalidItchAscii);
        if (error != nullptr) {
            CHECK(error->offset == field.offset);
            CHECK(error->message_type == static_cast<std::uint8_t>('R'));
            CHECK(error->observed_value == std::to_integer<std::uint8_t>(field.value));
        }
    }
}

void test_add_order_golden_fixture()
{
    const auto result = aegis::decode_add_order(add_order_fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::AddOrder* const message = result.value();
    CHECK(message != nullptr);
    if (message == nullptr) {
        return;
    }

    constexpr aegis::StockSymbol expected_stock{'A', 'E', 'G', 'I', 'S', ' ', ' ', ' '};
    CHECK(message->header.type == 'A');
    CHECK(message->header.stock_locate == std::uint16_t{0x1234});
    CHECK(message->header.tracking_number == std::uint16_t{0x5678});
    CHECK(message->header.timestamp_ns == std::uint64_t{0x010203040506ULL});
    CHECK(message->order_reference == std::uint64_t{0x0102030405060708ULL});
    CHECK(message->side == aegis::Side::Buy);
    CHECK(message->shares == std::uint32_t{0x0A0B0C0D});
    CHECK(message->stock == expected_stock);
    CHECK(message->price_1e4 == aegis::Price4{0x01020304});
    CHECK(!message->attribution.has_value());
}

void test_attributed_add_order_golden_fixture()
{
    const auto result =
        aegis::decode_add_order_with_attribution(attributed_add_order_fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::AddOrder* const message = result.value();
    CHECK(message != nullptr);
    if (message == nullptr) {
        return;
    }

    constexpr aegis::StockSymbol expected_stock{'P', 'H', 'A', 'S', 'E', '2', 'C', ' '};
    constexpr aegis::Attribution expected_attribution{'A', 'B', 'C', 'D'};
    CHECK(message->header.type == 'F');
    CHECK(message->header.stock_locate == std::uint16_t{0xABCD});
    CHECK(message->header.tracking_number == std::uint16_t{0x1357});
    CHECK(message->header.timestamp_ns == std::uint64_t{0x0A0B0C0D0E0FULL});
    CHECK(message->order_reference == std::uint64_t{0x8877665544332211ULL});
    CHECK(message->side == aegis::Side::Sell);
    CHECK(message->shares == std::uint32_t{0x10203040});
    CHECK(message->stock == expected_stock);
    CHECK(message->price_1e4 == aegis::Price4{123'456'789});
    CHECK(message->attribution.has_value());
    if (message->attribution.has_value()) {
        CHECK(*message->attribution == expected_attribution);
    }
}

void test_add_order_exact_length_validation()
{
    constexpr std::uint64_t sequence = 0x0102030405060708ULL;
    const auto short_a = aegis::decode_add_order(
        std::span<const std::byte>{add_order_fixture}.first(35),
        aegis::ItchDecodeContext{sequence});
    const aegis::Error* const short_a_error =
        check_failure(short_a, aegis::ErrorCode::InvalidItchLength);
    if (short_a_error != nullptr) {
        CHECK(short_a_error->requested_size == 36);
        CHECK(short_a_error->available_size == 35);
        CHECK(short_a_error->sequence == sequence);
        CHECK(short_a_error->message_type == static_cast<std::uint8_t>('A'));
    }

    std::array<std::byte, 37> long_a{};
    for (std::size_t index = 0; index < add_order_fixture.size(); ++index) {
        long_a[index] = add_order_fixture[index];
    }
    long_a[36] = std::byte{0xFF};
    const auto long_a_result = aegis::decode_add_order(long_a);
    const aegis::Error* const long_a_error =
        check_failure(long_a_result, aegis::ErrorCode::InvalidItchLength);
    if (long_a_error != nullptr) {
        CHECK(long_a_error->requested_size == 36);
        CHECK(long_a_error->available_size == 37);
    }

    const auto short_f = aegis::decode_add_order_with_attribution(
        std::span<const std::byte>{attributed_add_order_fixture}.first(39));
    const aegis::Error* const short_f_error =
        check_failure(short_f, aegis::ErrorCode::InvalidItchLength);
    if (short_f_error != nullptr) {
        CHECK(short_f_error->requested_size == 40);
        CHECK(short_f_error->available_size == 39);
        CHECK(short_f_error->message_type == static_cast<std::uint8_t>('F'));
    }

    std::array<std::byte, 41> long_f{};
    for (std::size_t index = 0; index < attributed_add_order_fixture.size(); ++index) {
        long_f[index] = attributed_add_order_fixture[index];
    }
    long_f[40] = std::byte{0xFF};
    const auto long_f_result = aegis::decode_add_order_with_attribution(long_f);
    const aegis::Error* const long_f_error =
        check_failure(long_f_result, aegis::ErrorCode::InvalidItchLength);
    if (long_f_error != nullptr) {
        CHECK(long_f_error->requested_size == 40);
        CHECK(long_f_error->available_size == 41);
    }
}

void test_add_order_wrong_types()
{
    auto a_payload = add_order_fixture;
    a_payload[0] = std::byte{'F'};
    const auto a_result = aegis::decode_add_order(a_payload);
    const aegis::Error* const a_error =
        check_failure(a_result, aegis::ErrorCode::UnexpectedItchType);
    if (a_error != nullptr) {
        CHECK(a_error->offset == 0);
        CHECK(a_error->message_type == static_cast<std::uint8_t>('F'));
        CHECK(a_error->observed_value == static_cast<std::uint8_t>('F'));
        CHECK(a_error->limit_value == static_cast<std::uint8_t>('A'));
    }

    auto f_payload = attributed_add_order_fixture;
    f_payload[0] = std::byte{'A'};
    constexpr std::uint64_t sequence = 123'456'789ULL;
    const auto f_result = aegis::decode_add_order_with_attribution(
        f_payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const f_error =
        check_failure(f_result, aegis::ErrorCode::UnexpectedItchType);
    if (f_error != nullptr) {
        CHECK(f_error->offset == 0);
        CHECK(f_error->sequence == sequence);
        CHECK(f_error->message_type == static_cast<std::uint8_t>('A'));
        CHECK(f_error->observed_value == static_cast<std::uint8_t>('A'));
        CHECK(f_error->limit_value == static_cast<std::uint8_t>('F'));
    }
}

void test_add_order_invalid_sides()
{
    auto a_payload = add_order_fixture;
    a_payload[19] = std::byte{'?'};
    const auto a_result = aegis::decode_add_order(a_payload);
    const aegis::Error* const a_error =
        check_failure(a_result, aegis::ErrorCode::InvalidItchEnum);
    if (a_error != nullptr) {
        CHECK(a_error->offset == 19);
        CHECK(a_error->message_type == static_cast<std::uint8_t>('A'));
        CHECK(a_error->observed_value == static_cast<std::uint8_t>('?'));
    }

    auto f_payload = attributed_add_order_fixture;
    f_payload[19] = std::byte{0x80};
    constexpr std::uint64_t sequence = 0xA0B0C0D0ULL;
    const auto f_result = aegis::decode_add_order_with_attribution(
        f_payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const f_error =
        check_failure(f_result, aegis::ErrorCode::InvalidItchEnum);
    if (f_error != nullptr) {
        CHECK(f_error->offset == 19);
        CHECK(f_error->sequence == sequence);
        CHECK(f_error->message_type == static_cast<std::uint8_t>('F'));
        CHECK(f_error->observed_value == 0x80U);
    }
}

void test_add_order_invalid_stock_ascii()
{
    struct InvalidStockByte {
        std::size_t offset;
        std::byte value;
    };

    constexpr std::array invalid_a_bytes{
        InvalidStockByte{24, std::byte{0x80}},
        InvalidStockByte{25, std::byte{0x1F}},
        InvalidStockByte{26, std::byte{0x00}},
        InvalidStockByte{31, std::byte{0x7F}},
    };
    for (const auto& invalid : invalid_a_bytes) {
        auto payload = add_order_fixture;
        payload[invalid.offset] = invalid.value;
        const auto result = aegis::decode_add_order(payload);
        const aegis::Error* const error =
            check_failure(result, aegis::ErrorCode::InvalidItchAscii);
        if (error != nullptr) {
            CHECK(error->offset == invalid.offset);
            CHECK(error->message_type == static_cast<std::uint8_t>('A'));
            CHECK(error->observed_value == std::to_integer<std::uint8_t>(invalid.value));
        }
    }

    auto f_payload = attributed_add_order_fixture;
    f_payload[29] = std::byte{0xFF};
    const auto f_result = aegis::decode_add_order_with_attribution(f_payload);
    const aegis::Error* const f_error =
        check_failure(f_result, aegis::ErrorCode::InvalidItchAscii);
    if (f_error != nullptr) {
        CHECK(f_error->offset == 29);
        CHECK(f_error->message_type == static_cast<std::uint8_t>('F'));
        CHECK(f_error->observed_value == 0xFFU);
    }
}

void test_add_order_price_boundaries()
{
    auto zero_payload = add_order_fixture;
    zero_payload[32] = std::byte{0x00};
    zero_payload[33] = std::byte{0x00};
    zero_payload[34] = std::byte{0x00};
    zero_payload[35] = std::byte{0x00};
    const auto zero_result = aegis::decode_add_order(zero_payload);
    CHECK(zero_result.has_value());
    if (zero_result.value() != nullptr) {
        CHECK(zero_result.value()->price_1e4 == aegis::Price4{0});
    }

    auto maximum_payload = add_order_fixture;
    maximum_payload[32] = std::byte{0x77};
    maximum_payload[33] = std::byte{0x35};
    maximum_payload[34] = std::byte{0x94};
    maximum_payload[35] = std::byte{0x00};
    const auto maximum_result = aegis::decode_add_order(maximum_payload);
    CHECK(maximum_result.has_value());
    if (maximum_result.value() != nullptr) {
        CHECK(maximum_result.value()->price_1e4 == aegis::kMaxPrice4);
    }

    auto oversized_payload = add_order_fixture;
    oversized_payload[32] = std::byte{0x77};
    oversized_payload[33] = std::byte{0x35};
    oversized_payload[34] = std::byte{0x94};
    oversized_payload[35] = std::byte{0x01};
    constexpr std::uint64_t sequence = 0xCAFEBABEULL;
    const auto oversized_result = aegis::decode_add_order(
        oversized_payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const error =
        check_failure(oversized_result, aegis::ErrorCode::InvalidItchValue);
    if (error != nullptr) {
        CHECK(error->offset == 32);
        CHECK(error->observed_value == 2'000'000'001ULL);
        CHECK(error->limit_value == 2'000'000'000ULL);
        CHECK(error->sequence == sequence);
        CHECK(error->message_type == static_cast<std::uint8_t>('A'));
        CHECK(error->message == "ITCH value exceeds permitted range");
    }
}

void test_attributed_add_order_invalid_price()
{
    auto payload = attributed_add_order_fixture;
    payload[32] = std::byte{0x77};
    payload[33] = std::byte{0x35};
    payload[34] = std::byte{0x94};
    payload[35] = std::byte{0x01};
    constexpr std::uint64_t sequence = 0x0F0E0D0C0B0A0908ULL;
    const auto result = aegis::decode_add_order_with_attribution(
        payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const error =
        check_failure(result, aegis::ErrorCode::InvalidItchValue);
    if (error != nullptr) {
        CHECK(error->offset == 32);
        CHECK(error->observed_value == 2'000'000'001ULL);
        CHECK(error->limit_value == 2'000'000'000ULL);
        CHECK(error->sequence == sequence);
        CHECK(error->message_type == static_cast<std::uint8_t>('F'));
    }
}

void test_attributed_add_order_invalid_attribution()
{
    auto payload = attributed_add_order_fixture;
    payload[38] = std::byte{0x7F};
    constexpr std::uint64_t sequence = 0x1020304050607080ULL;
    const auto result = aegis::decode_add_order_with_attribution(
        payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const error =
        check_failure(result, aegis::ErrorCode::InvalidItchAscii);
    if (error != nullptr) {
        CHECK(error->offset == 38);
        CHECK(error->observed_value == 0x7FU);
        CHECK(error->sequence == sequence);
        CHECK(error->message_type == static_cast<std::uint8_t>('F'));
    }
}

void test_order_executed_golden_fixture()
{
    const auto result = aegis::decode_order_executed(order_executed_fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::OrderExecuted* const message = result.value();
    CHECK(message != nullptr);
    if (message == nullptr) {
        return;
    }

    CHECK(message->header.type == 'E');
    CHECK(message->header.stock_locate == std::uint16_t{0x1234});
    CHECK(message->header.tracking_number == std::uint16_t{0x5678});
    CHECK(message->header.timestamp_ns == std::uint64_t{0x010203040506ULL});
    CHECK(message->order_reference == std::uint64_t{0x0102030405060708ULL});
    CHECK(message->executed_shares == std::uint32_t{0x10203040});
    CHECK(message->match_number == std::uint64_t{0x8877665544332211ULL});
}

void test_order_executed_with_price_golden_fixture()
{
    const auto result =
        aegis::decode_order_executed_with_price(order_executed_with_price_fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::OrderExecutedWithPrice* const message = result.value();
    CHECK(message != nullptr);
    if (message == nullptr) {
        return;
    }

    CHECK(message->header.type == 'C');
    CHECK(message->header.stock_locate == std::uint16_t{0xABCD});
    CHECK(message->header.tracking_number == std::uint16_t{0x1357});
    CHECK(message->header.timestamp_ns == std::uint64_t{0x0A0B0C0D0E0FULL});
    CHECK(message->order_reference == std::uint64_t{0x1122334455667788ULL});
    CHECK(message->executed_shares == std::uint32_t{0x0A0B0C0D});
    CHECK(message->match_number == std::uint64_t{0x0123456789ABCDEFULL});
    CHECK(message->printable);
    CHECK(message->execution_price_1e4 == aegis::Price4{123'456'789});
}

void test_order_cancel_golden_fixture()
{
    const auto result = aegis::decode_order_cancel(order_cancel_fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::OrderCancel* const message = result.value();
    CHECK(message != nullptr);
    if (message == nullptr) {
        return;
    }

    CHECK(message->header.type == 'X');
    CHECK(message->header.stock_locate == std::uint16_t{0x2468});
    CHECK(message->header.tracking_number == std::uint16_t{0xACE0});
    CHECK(message->header.timestamp_ns == std::uint64_t{0x111213141516ULL});
    CHECK(message->order_reference == std::uint64_t{0x1020304050607080ULL});
    CHECK(message->cancelled_shares == std::uint32_t{0x01020304});
}

void test_order_delete_golden_fixture()
{
    const auto result = aegis::decode_order_delete(order_delete_fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::OrderDelete* const message = result.value();
    CHECK(message != nullptr);
    if (message == nullptr) {
        return;
    }

    CHECK(message->header.type == 'D');
    CHECK(message->header.stock_locate == std::uint16_t{0x0F1E});
    CHECK(message->header.tracking_number == std::uint16_t{0x2D3C});
    CHECK(message->header.timestamp_ns == std::uint64_t{0x212223242526ULL});
    CHECK(message->order_reference == std::uint64_t{0xA1A2A3A4A5A6A7A8ULL});
}

void test_order_replace_golden_fixture()
{
    const auto result = aegis::decode_order_replace(order_replace_fixture);
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::OrderReplace* const message = result.value();
    CHECK(message != nullptr);
    if (message == nullptr) {
        return;
    }

    CHECK(message->header.type == 'U');
    CHECK(message->header.stock_locate == std::uint16_t{0xBEEF});
    CHECK(message->header.tracking_number == std::uint16_t{0xCAFE});
    CHECK(message->header.timestamp_ns == std::uint64_t{0x313233343536ULL});
    CHECK(message->original_order_reference == std::uint64_t{0x0A1B2C3D4E5F6071ULL});
    CHECK(message->new_order_reference == std::uint64_t{0x91A2B3C4D5E6F708ULL});
    CHECK(message->shares == std::uint32_t{0x50607080});
    CHECK(message->price_1e4 == aegis::Price4{0x02030405});
}

void test_order_event_exact_length_validation()
{
    check_exact_length_validation(
        order_executed_fixture, 'E', aegis::decode_order_executed);
    check_exact_length_validation(
        order_executed_with_price_fixture,
        'C',
        aegis::decode_order_executed_with_price);
    check_exact_length_validation(order_cancel_fixture, 'X', aegis::decode_order_cancel);
    check_exact_length_validation(order_delete_fixture, 'D', aegis::decode_order_delete);
    check_exact_length_validation(order_replace_fixture, 'U', aegis::decode_order_replace);
}

void test_order_event_wrong_types()
{
    check_wrong_type_validation(
        order_executed_fixture, 'E', aegis::decode_order_executed);
    check_wrong_type_validation(
        order_executed_with_price_fixture,
        'C',
        aegis::decode_order_executed_with_price);
    check_wrong_type_validation(order_cancel_fixture, 'X', aegis::decode_order_cancel);
    check_wrong_type_validation(order_delete_fixture, 'D', aegis::decode_order_delete);
    check_wrong_type_validation(order_replace_fixture, 'U', aegis::decode_order_replace);
}

void test_order_executed_with_price_printable_values()
{
    const auto yes_result =
        aegis::decode_order_executed_with_price(order_executed_with_price_fixture);
    CHECK(yes_result.has_value());
    if (yes_result.value() != nullptr) {
        CHECK(yes_result.value()->printable);
    }

    auto no_payload = order_executed_with_price_fixture;
    no_payload[31] = std::byte{'N'};
    const auto no_result = aegis::decode_order_executed_with_price(no_payload);
    CHECK(no_result.has_value());
    if (no_result.value() != nullptr) {
        CHECK(!no_result.value()->printable);
    }
}

void test_order_executed_with_price_invalid_printable_values()
{
    constexpr std::array invalid_values{std::byte{'?'}, std::byte{0x80}};
    constexpr std::uint64_t sequence = 0x0F1E2D3C4B5A6978ULL;

    for (const std::byte invalid_value : invalid_values) {
        auto payload = order_executed_with_price_fixture;
        payload[31] = invalid_value;
        const auto result = aegis::decode_order_executed_with_price(
            payload, aegis::ItchDecodeContext{sequence});
        const aegis::Error* const error =
            check_failure(result, aegis::ErrorCode::InvalidItchEnum);
        if (error != nullptr) {
            CHECK(error->offset == 31);
            CHECK(error->observed_value ==
                  std::to_integer<std::uint8_t>(invalid_value));
            CHECK(error->sequence == sequence);
            CHECK(error->message_type == static_cast<std::uint8_t>('C'));
        }
    }
}

void test_order_executed_with_price_boundaries()
{
    auto zero_payload = order_executed_with_price_fixture;
    zero_payload[32] = std::byte{0x00};
    zero_payload[33] = std::byte{0x00};
    zero_payload[34] = std::byte{0x00};
    zero_payload[35] = std::byte{0x00};
    const auto zero_result = aegis::decode_order_executed_with_price(zero_payload);
    CHECK(zero_result.has_value());
    if (zero_result.value() != nullptr) {
        CHECK(zero_result.value()->execution_price_1e4 == aegis::Price4{0});
    }

    auto maximum_payload = order_executed_with_price_fixture;
    maximum_payload[32] = std::byte{0x77};
    maximum_payload[33] = std::byte{0x35};
    maximum_payload[34] = std::byte{0x94};
    maximum_payload[35] = std::byte{0x00};
    const auto maximum_result = aegis::decode_order_executed_with_price(maximum_payload);
    CHECK(maximum_result.has_value());
    if (maximum_result.value() != nullptr) {
        CHECK(maximum_result.value()->execution_price_1e4 == aegis::kMaxPrice4);
    }

    auto oversized_payload = order_executed_with_price_fixture;
    oversized_payload[32] = std::byte{0x77};
    oversized_payload[33] = std::byte{0x35};
    oversized_payload[34] = std::byte{0x94};
    oversized_payload[35] = std::byte{0x01};
    constexpr std::uint64_t sequence = 0xCAFEBABE10203040ULL;
    const auto oversized_result = aegis::decode_order_executed_with_price(
        oversized_payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const error =
        check_failure(oversized_result, aegis::ErrorCode::InvalidItchValue);
    if (error != nullptr) {
        CHECK(error->offset == 32);
        CHECK(error->observed_value == 2'000'000'001ULL);
        CHECK(error->limit_value == 2'000'000'000ULL);
        CHECK(error->sequence == sequence);
        CHECK(error->message_type == static_cast<std::uint8_t>('C'));
        CHECK(error->message == "ITCH value exceeds permitted range");
    }
}

void test_order_replace_price_boundaries()
{
    auto zero_payload = order_replace_fixture;
    zero_payload[31] = std::byte{0x00};
    zero_payload[32] = std::byte{0x00};
    zero_payload[33] = std::byte{0x00};
    zero_payload[34] = std::byte{0x00};
    const auto zero_result = aegis::decode_order_replace(zero_payload);
    CHECK(zero_result.has_value());
    if (zero_result.value() != nullptr) {
        CHECK(zero_result.value()->price_1e4 == aegis::Price4{0});
    }

    auto maximum_payload = order_replace_fixture;
    maximum_payload[31] = std::byte{0x77};
    maximum_payload[32] = std::byte{0x35};
    maximum_payload[33] = std::byte{0x94};
    maximum_payload[34] = std::byte{0x00};
    const auto maximum_result = aegis::decode_order_replace(maximum_payload);
    CHECK(maximum_result.has_value());
    if (maximum_result.value() != nullptr) {
        CHECK(maximum_result.value()->price_1e4 == aegis::kMaxPrice4);
    }

    auto oversized_payload = order_replace_fixture;
    oversized_payload[31] = std::byte{0x77};
    oversized_payload[32] = std::byte{0x35};
    oversized_payload[33] = std::byte{0x94};
    oversized_payload[34] = std::byte{0x01};
    constexpr std::uint64_t sequence = 0x0123456789ABCDEFULL;
    const auto oversized_result = aegis::decode_order_replace(
        oversized_payload, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const error =
        check_failure(oversized_result, aegis::ErrorCode::InvalidItchValue);
    if (error != nullptr) {
        CHECK(error->offset == 31);
        CHECK(error->observed_value == 2'000'000'001ULL);
        CHECK(error->limit_value == 2'000'000'000ULL);
        CHECK(error->sequence == sequence);
        CHECK(error->message_type == static_cast<std::uint8_t>('U'));
        CHECK(error->message == "ITCH value exceeds permitted range");
    }
}

void test_unified_decoder_empty_payload()
{
    constexpr std::uint64_t sequence = 0x0102030405060708ULL;
    const auto result = aegis::decode_itch(
        std::span<const std::byte>{}, aegis::ItchDecodeContext{sequence});
    const aegis::Error* const error =
        check_failure(result, aegis::ErrorCode::InvalidItchLength);
    if (error != nullptr) {
        CHECK(error->offset == 0);
        CHECK(error->requested_size == 1);
        CHECK(error->available_size == 0);
        CHECK(error->observed_value == 0);
        CHECK(error->limit_value == 1);
        CHECK(error->sequence == sequence);
        CHECK(error->message_type == 0);
    }
}

void test_unified_decoder_unknown_types()
{
    constexpr std::array<std::uint8_t, 6> unknown_types{
        static_cast<std::uint8_t>('?'),
        static_cast<std::uint8_t>('Z'),
        0x00,
        0x7F,
        0x80,
        0xFF,
    };
    constexpr std::uint64_t sequence = 0x8877665544332211ULL;

    for (const std::uint8_t type : unknown_types) {
        const std::array payload{static_cast<std::byte>(type)};
        const auto result =
            aegis::decode_itch(payload, aegis::ItchDecodeContext{sequence});
        const aegis::Error* const error =
            check_failure(result, aegis::ErrorCode::UnknownItchType);
        if (error != nullptr) {
            CHECK(error->offset == 0);
            CHECK(error->requested_size == 1);
            CHECK(error->available_size == 1);
            CHECK(error->observed_value == type);
            CHECK(error->limit_value == 0);
            CHECK(error->sequence == sequence);
            CHECK(error->message_type == type);
            CHECK(error->message == "unknown ITCH message type");
        }
    }
}

void test_unified_required_dispatch()
{
    check_required_dispatch<aegis::SystemEvent>(
        system_event_fixture,
        [](const aegis::SystemEvent& message) {
            CHECK(message.header.type == 'S');
            CHECK(message.event_code ==
                  aegis::SystemEventCode::EmergencyMarketConditionHalt);
        });
    check_required_dispatch<aegis::StockDirectory>(
        stock_directory_fixture,
        [](const aegis::StockDirectory& message) {
            CHECK(message.header.type == 'R');
            CHECK(message.stock[0] == 'A');
            CHECK(message.market_category == aegis::MarketCategory::TexasStockExchange);
        });
    check_required_dispatch<aegis::AddOrder>(
        add_order_fixture,
        [](const aegis::AddOrder& message) {
            CHECK(message.header.type == 'A');
            CHECK(message.order_reference == std::uint64_t{0x0102030405060708ULL});
            CHECK(!message.attribution.has_value());
        });
    check_required_dispatch<aegis::AddOrder>(
        attributed_add_order_fixture,
        [](const aegis::AddOrder& message) {
            CHECK(message.header.type == 'F');
            CHECK(message.order_reference == std::uint64_t{0x8877665544332211ULL});
            CHECK(message.attribution.has_value());
        });
    check_required_dispatch<aegis::OrderExecuted>(
        order_executed_fixture,
        [](const aegis::OrderExecuted& message) {
            CHECK(message.header.type == 'E');
            CHECK(message.match_number == std::uint64_t{0x8877665544332211ULL});
        });
    check_required_dispatch<aegis::OrderExecutedWithPrice>(
        order_executed_with_price_fixture,
        [](const aegis::OrderExecutedWithPrice& message) {
            CHECK(message.header.type == 'C');
            CHECK(message.printable);
            CHECK(message.execution_price_1e4 == aegis::Price4{123'456'789});
        });
    check_required_dispatch<aegis::OrderCancel>(
        order_cancel_fixture,
        [](const aegis::OrderCancel& message) {
            CHECK(message.header.type == 'X');
            CHECK(message.cancelled_shares == std::uint32_t{0x01020304});
        });
    check_required_dispatch<aegis::OrderDelete>(
        order_delete_fixture,
        [](const aegis::OrderDelete& message) {
            CHECK(message.header.type == 'D');
            CHECK(message.order_reference == std::uint64_t{0xA1A2A3A4A5A6A7A8ULL});
        });
    check_required_dispatch<aegis::OrderReplace>(
        order_replace_fixture,
        [](const aegis::OrderReplace& message) {
            CHECK(message.header.type == 'U');
            CHECK(message.new_order_reference == std::uint64_t{0x91A2B3C4D5E6F708ULL});
            CHECK(message.price_1e4 == aegis::Price4{0x02030405});
        });
}

void test_unified_required_exact_lengths()
{
    check_unified_exact_length(system_event_fixture, 'S', 12);
    check_unified_exact_length(stock_directory_fixture, 'R', 39);
    check_unified_exact_length(add_order_fixture, 'A', 36);
    check_unified_exact_length(attributed_add_order_fixture, 'F', 40);
    check_unified_exact_length(order_executed_fixture, 'E', 31);
    check_unified_exact_length(order_executed_with_price_fixture, 'C', 36);
    check_unified_exact_length(order_cancel_fixture, 'X', 23);
    check_unified_exact_length(order_delete_fixture, 'D', 19);
    check_unified_exact_length(order_replace_fixture, 'U', 35);
}

void test_unified_known_neutral_dispatch()
{
    for (const auto& entry : neutral_dispatch_entries) {
        const auto registered_length = aegis::expected_itch_length(entry.type);
        CHECK(registered_length.has_value());
        if (registered_length.has_value()) {
            CHECK(*registered_length == entry.length);
        }

        const auto payload = make_neutral_payload(entry.type, entry.length);
        const auto result = aegis::decode_itch(
            std::span<const std::byte>{payload}.first(entry.length));
        CHECK(result.has_value());
        CHECK(result.error() == nullptr);

        const aegis::DecodedItchMessage* const decoded = result.value();
        CHECK(decoded != nullptr);
        if (decoded == nullptr) {
            continue;
        }

        CHECK(std::holds_alternative<aegis::KnownBookNeutral>(*decoded));
        const aegis::KnownBookNeutral* const message =
            std::get_if<aegis::KnownBookNeutral>(decoded);
        CHECK(message != nullptr);
        if (message != nullptr) {
            CHECK(message->header.type == entry.type);
            CHECK(message->header.stock_locate == std::uint16_t{0x1234});
            CHECK(message->header.tracking_number == std::uint16_t{0x5678});
            CHECK(message->header.timestamp_ns == std::uint64_t{0x010203040506ULL});
        }
    }
}

void test_unified_known_neutral_exact_lengths()
{
    for (const auto& entry : neutral_dispatch_entries) {
        const auto payload = make_neutral_payload(entry.type, entry.length);
        check_unified_exact_length(
            std::span<const std::byte>{payload}.first(entry.length),
            entry.type,
            entry.length);
    }
}

void test_unified_validation_error_propagation()
{
    auto system_event_payload = system_event_fixture;
    system_event_payload[11] = std::byte{'?'};
    constexpr std::uint64_t system_event_sequence = 0x1000000000000001ULL;
    check_unified_validation_error(
        aegis::decode_itch(
            system_event_payload, aegis::ItchDecodeContext{system_event_sequence}),
        aegis::ErrorCode::InvalidItchEnum,
        11,
        static_cast<std::uint8_t>('?'),
        0,
        'S',
        system_event_sequence);

    auto stock_directory_payload = stock_directory_fixture;
    stock_directory_payload[19] = std::byte{0xFF};
    constexpr std::uint64_t stock_directory_sequence = 0x2000000000000002ULL;
    check_unified_validation_error(
        aegis::decode_itch(
            stock_directory_payload, aegis::ItchDecodeContext{stock_directory_sequence}),
        aegis::ErrorCode::InvalidItchEnum,
        19,
        0xFF,
        0,
        'R',
        stock_directory_sequence);

    auto add_order_payload = add_order_fixture;
    add_order_payload[19] = std::byte{'?'};
    constexpr std::uint64_t add_order_sequence = 0x3000000000000003ULL;
    check_unified_validation_error(
        aegis::decode_itch(
            add_order_payload, aegis::ItchDecodeContext{add_order_sequence}),
        aegis::ErrorCode::InvalidItchEnum,
        19,
        static_cast<std::uint8_t>('?'),
        0,
        'A',
        add_order_sequence);

    auto attributed_payload = attributed_add_order_fixture;
    attributed_payload[38] = std::byte{0x7F};
    constexpr std::uint64_t attributed_sequence = 0x4000000000000004ULL;
    check_unified_validation_error(
        aegis::decode_itch(
            attributed_payload, aegis::ItchDecodeContext{attributed_sequence}),
        aegis::ErrorCode::InvalidItchAscii,
        38,
        0x7F,
        0x7E,
        'F',
        attributed_sequence);

    auto printable_payload = order_executed_with_price_fixture;
    printable_payload[31] = std::byte{'?'};
    constexpr std::uint64_t printable_sequence = 0x5000000000000005ULL;
    check_unified_validation_error(
        aegis::decode_itch(
            printable_payload, aegis::ItchDecodeContext{printable_sequence}),
        aegis::ErrorCode::InvalidItchEnum,
        31,
        static_cast<std::uint8_t>('?'),
        0,
        'C',
        printable_sequence);

    auto execution_price_payload = order_executed_with_price_fixture;
    execution_price_payload[32] = std::byte{0x77};
    execution_price_payload[33] = std::byte{0x35};
    execution_price_payload[34] = std::byte{0x94};
    execution_price_payload[35] = std::byte{0x01};
    constexpr std::uint64_t execution_price_sequence = 0x6000000000000006ULL;
    check_unified_validation_error(
        aegis::decode_itch(
            execution_price_payload, aegis::ItchDecodeContext{execution_price_sequence}),
        aegis::ErrorCode::InvalidItchValue,
        32,
        2'000'000'001ULL,
        2'000'000'000ULL,
        'C',
        execution_price_sequence);

    auto replace_price_payload = order_replace_fixture;
    replace_price_payload[31] = std::byte{0x77};
    replace_price_payload[32] = std::byte{0x35};
    replace_price_payload[33] = std::byte{0x94};
    replace_price_payload[34] = std::byte{0x01};
    constexpr std::uint64_t replace_price_sequence = 0x7000000000000007ULL;
    check_unified_validation_error(
        aegis::decode_itch(
            replace_price_payload, aegis::ItchDecodeContext{replace_price_sequence}),
        aegis::ErrorCode::InvalidItchValue,
        31,
        2'000'000'001ULL,
        2'000'000'000ULL,
        'U',
        replace_price_sequence);
}

}  // namespace

int main()
{
    test_system_event_golden_fixture();
    test_all_documented_system_event_codes();
    test_system_event_exact_length_validation();
    test_system_event_wrong_type();
    test_system_event_invalid_enum();
    test_stock_directory_golden_fixture();
    test_all_documented_stock_directory_enum_codes();
    test_stock_directory_exact_length_validation();
    test_stock_directory_wrong_type();
    test_stock_directory_invalid_enums();
    test_stock_directory_invalid_ascii();
    test_add_order_golden_fixture();
    test_attributed_add_order_golden_fixture();
    test_add_order_exact_length_validation();
    test_add_order_wrong_types();
    test_add_order_invalid_sides();
    test_add_order_invalid_stock_ascii();
    test_add_order_price_boundaries();
    test_attributed_add_order_invalid_price();
    test_attributed_add_order_invalid_attribution();
    test_order_executed_golden_fixture();
    test_order_executed_with_price_golden_fixture();
    test_order_cancel_golden_fixture();
    test_order_delete_golden_fixture();
    test_order_replace_golden_fixture();
    test_order_event_exact_length_validation();
    test_order_event_wrong_types();
    test_order_executed_with_price_printable_values();
    test_order_executed_with_price_invalid_printable_values();
    test_order_executed_with_price_boundaries();
    test_order_replace_price_boundaries();
    test_unified_decoder_empty_payload();
    test_unified_decoder_unknown_types();
    test_unified_required_dispatch();
    test_unified_required_exact_lengths();
    test_unified_known_neutral_dispatch();
    test_unified_known_neutral_exact_lengths();
    test_unified_validation_error_propagation();
    return failure_count == 0 ? 0 : 1;
}
