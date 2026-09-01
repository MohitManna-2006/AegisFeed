#include "aegis/itch/itch_decoder.hpp"

#include "aegis/common/error.hpp"
#include "aegis/common/result.hpp"
#include "aegis/itch/itch_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <type_traits>

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

static_assert(system_event_fixture.size() == 12);
static_assert(stock_directory_fixture.size() == 39);
static_assert(std::is_trivially_copyable_v<aegis::ItchDecodeContext>);
static_assert(std::is_nothrow_move_constructible_v<aegis::SystemEvent>);
static_assert(std::is_nothrow_move_constructible_v<aegis::StockDirectory>);

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
    return failure_count == 0 ? 0 : 1;
}
