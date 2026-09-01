#include "aegis/itch/itch_decoder.hpp"

#include "aegis/common/byte_reader.hpp"
#include "aegis/common/error.hpp"
#include "aegis/itch/itch_lengths.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace aegis {
namespace {

[[nodiscard]] constexpr std::uint8_t byte_value(const char value) noexcept
{
    return static_cast<std::uint8_t>(static_cast<unsigned char>(value));
}

template <typename Enum>
[[nodiscard]] constexpr std::uint8_t enum_value(const Enum value) noexcept
{
    return static_cast<std::uint8_t>(value);
}

class ItchReadCursor {
public:
    ItchReadCursor(
        const std::span<const std::byte> payload,
        const ItchDecodeContext context,
        const std::uint8_t message_type,
        const std::size_t expected_size) noexcept
        : reader_{payload},
          context_{context},
          message_type_{message_type},
          error_{Error::invalid_itch_length(
              expected_size, payload.size(), context.sequence, message_type)}
    {
    }

    [[nodiscard]] bool read_u8(std::uint8_t& destination) noexcept
    {
        return take(reader_.read_u8(), destination);
    }

    [[nodiscard]] bool read_u16_be(std::uint16_t& destination) noexcept
    {
        return take(reader_.read_u16_be(), destination);
    }

    [[nodiscard]] bool read_u32_be(std::uint32_t& destination) noexcept
    {
        return take(reader_.read_u32_be(), destination);
    }

    [[nodiscard]] bool read_u48_be(std::uint64_t& destination) noexcept
    {
        return take(reader_.read_u48_be(), destination);
    }

    [[nodiscard]] bool read_bytes(
        const std::size_t count,
        std::span<const std::byte>& destination) noexcept
    {
        return take(reader_.read_bytes(count), destination);
    }

    [[nodiscard]] Error error() const noexcept
    {
        return error_;
    }

private:
    template <typename T>
    [[nodiscard]] bool take(Result<T> result, T& destination) noexcept
    {
        const T* const value = result.value();
        if (value != nullptr) {
            destination = *value;
            return true;
        }

        const Error* const source = result.error();
        if (source != nullptr) {
            error_ = *source;
        }
        error_.category = ErrorCategory::ItchDecode;
        error_.code = ErrorCode::InvalidItchLength;
        error_.message = "ITCH field exceeds payload";
        error_.sequence = context_.sequence;
        error_.message_type = message_type_;
        return false;
    }

    ByteReader reader_;
    ItchDecodeContext context_;
    std::uint8_t message_type_;
    Error error_;
};

[[nodiscard]] Result<ItchCommonHeader> decode_common_header(
    ItchReadCursor& reader,
    const std::uint8_t expected_type,
    const ItchDecodeContext context) noexcept
{
    std::uint8_t raw_type{};
    if (!reader.read_u8(raw_type)) {
        return Result<ItchCommonHeader>::failure(reader.error());
    }
    if (raw_type != expected_type) {
        return Result<ItchCommonHeader>::failure(
            Error::unexpected_itch_type(expected_type, raw_type, context.sequence));
    }

    std::uint16_t stock_locate{};
    std::uint16_t tracking_number{};
    std::uint64_t timestamp_ns{};
    if (!reader.read_u16_be(stock_locate) || !reader.read_u16_be(tracking_number) ||
        !reader.read_u48_be(timestamp_ns)) {
        return Result<ItchCommonHeader>::failure(reader.error());
    }

    return Result<ItchCommonHeader>::success(ItchCommonHeader{
        static_cast<char>(raw_type),
        stock_locate,
        tracking_number,
        timestamp_ns,
    });
}

[[nodiscard]] constexpr bool is_valid_system_event_code(const std::uint8_t value) noexcept
{
    switch (value) {
    case enum_value(SystemEventCode::StartOfMessages):
    case enum_value(SystemEventCode::StartOfSystemHours):
    case enum_value(SystemEventCode::StartOfMarketHours):
    case enum_value(SystemEventCode::EndOfMarketHours):
    case enum_value(SystemEventCode::EndOfSystemHours):
    case enum_value(SystemEventCode::EndOfMessages):
    case enum_value(SystemEventCode::EmergencyMarketConditionHalt):
    case enum_value(SystemEventCode::EmergencyMarketConditionQuoteOnly):
    case enum_value(SystemEventCode::EmergencyMarketConditionResumption):
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool is_valid_market_category(const std::uint8_t value) noexcept
{
    switch (value) {
    case enum_value(MarketCategory::NotAvailable):
    case enum_value(MarketCategory::NasdaqGlobalSelect):
    case enum_value(MarketCategory::NasdaqGlobalMarket):
    case enum_value(MarketCategory::NasdaqCapitalMarket):
    case enum_value(MarketCategory::NewYorkStockExchange):
    case enum_value(MarketCategory::NyseTexas):
    case enum_value(MarketCategory::TexasStockExchange):
    case enum_value(MarketCategory::NyseAmerican):
    case enum_value(MarketCategory::NyseArca):
    case enum_value(MarketCategory::CboeBzx):
    case enum_value(MarketCategory::InvestorsExchange):
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool is_valid_financial_status(const std::uint8_t value) noexcept
{
    switch (value) {
    case enum_value(FinancialStatusIndicator::NotAvailable):
    case enum_value(FinancialStatusIndicator::Deficient):
    case enum_value(FinancialStatusIndicator::Delinquent):
    case enum_value(FinancialStatusIndicator::Bankrupt):
    case enum_value(FinancialStatusIndicator::Suspended):
    case enum_value(FinancialStatusIndicator::DeficientAndBankrupt):
    case enum_value(FinancialStatusIndicator::DeficientAndDelinquent):
    case enum_value(FinancialStatusIndicator::DelinquentAndBankrupt):
    case enum_value(FinancialStatusIndicator::DeficientDelinquentAndBankrupt):
    case enum_value(FinancialStatusIndicator::CreationsOrRedemptionsSuspended):
    case enum_value(FinancialStatusIndicator::Normal):
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool is_valid_round_lots_only(const std::uint8_t value) noexcept
{
    return value == enum_value(RoundLotsOnly::No) || value == enum_value(RoundLotsOnly::Yes);
}

[[nodiscard]] constexpr bool is_valid_authenticity(const std::uint8_t value) noexcept
{
    return value == enum_value(Authenticity::Production) ||
           value == enum_value(Authenticity::Test);
}

[[nodiscard]] constexpr bool is_valid_short_sale_threshold(const std::uint8_t value) noexcept
{
    switch (value) {
    case enum_value(ShortSaleThresholdIndicator::NotAvailable):
    case enum_value(ShortSaleThresholdIndicator::NotRestricted):
    case enum_value(ShortSaleThresholdIndicator::Restricted):
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool is_valid_ipo_flag(const std::uint8_t value) noexcept
{
    switch (value) {
    case enum_value(IpoFlag::NotAvailable):
    case enum_value(IpoFlag::NotIpo):
    case enum_value(IpoFlag::Ipo):
    case enum_value(IpoFlag::NonIpoNewIssue):
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool is_valid_luld_tier(const std::uint8_t value) noexcept
{
    switch (value) {
    case enum_value(LuldReferencePriceTier::NotAvailable):
    case enum_value(LuldReferencePriceTier::TierOne):
    case enum_value(LuldReferencePriceTier::TierTwo):
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool is_valid_etp_flag(const std::uint8_t value) noexcept
{
    switch (value) {
    case enum_value(EtpFlag::NotAvailable):
    case enum_value(EtpFlag::NotEtp):
    case enum_value(EtpFlag::Etp):
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool is_valid_inverse_indicator(const std::uint8_t value) noexcept
{
    return value == enum_value(InverseIndicator::NotInverse) ||
           value == enum_value(InverseIndicator::Inverse);
}

[[nodiscard]] constexpr bool is_printable_ascii(const std::uint8_t value) noexcept
{
    return value >= 0x20U && value <= 0x7EU;
}

template <typename Enum>
[[nodiscard]] bool read_enum_field(
    ItchReadCursor& reader,
    const std::size_t offset,
    const ItchDecodeContext context,
    const std::uint8_t message_type,
    bool (*const validator)(std::uint8_t) noexcept,
    Enum& destination,
    Error& error) noexcept
{
    std::uint8_t value{};
    if (!reader.read_u8(value)) {
        error = reader.error();
        return false;
    }
    if (!validator(value)) {
        error = Error::invalid_itch_enum(offset, value, context.sequence, message_type);
        return false;
    }

    destination = static_cast<Enum>(value);
    return true;
}

template <std::size_t Size>
[[nodiscard]] bool read_ascii_field(
    ItchReadCursor& reader,
    const std::size_t offset,
    const ItchDecodeContext context,
    const std::uint8_t message_type,
    std::array<char, Size>& destination,
    Error& error) noexcept
{
    std::span<const std::byte> bytes{};
    if (!reader.read_bytes(Size, bytes)) {
        error = reader.error();
        return false;
    }

    for (std::size_t index = 0; index < Size; ++index) {
        const auto value = std::to_integer<std::uint8_t>(bytes[index]);
        if (!is_printable_ascii(value)) {
            error = Error::invalid_itch_ascii(
                offset + index, value, context.sequence, message_type);
            return false;
        }
        destination[index] = static_cast<char>(value);
    }
    return true;
}

[[nodiscard]] bool read_ascii_character(
    ItchReadCursor& reader,
    const std::size_t offset,
    const ItchDecodeContext context,
    const std::uint8_t message_type,
    char& destination,
    Error& error) noexcept
{
    std::uint8_t value{};
    if (!reader.read_u8(value)) {
        error = reader.error();
        return false;
    }
    if (!is_printable_ascii(value)) {
        error = Error::invalid_itch_ascii(offset, value, context.sequence, message_type);
        return false;
    }

    destination = static_cast<char>(value);
    return true;
}

}  // namespace

Result<SystemEvent> decode_system_event(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    constexpr auto message_type = byte_value('S');
    if (payload.size() != kSystemEventLength) {
        return Result<SystemEvent>::failure(Error::invalid_itch_length(
            kSystemEventLength, payload.size(), context.sequence, message_type));
    }

    ItchReadCursor reader{payload, context, message_type, kSystemEventLength};
    auto header_result = decode_common_header(reader, message_type, context);
    if (!header_result.has_value()) {
        return Result<SystemEvent>::failure(*header_result.error());
    }

    SystemEventCode event_code{};
    Error error{};
    if (!read_enum_field(
            reader,
            kItchCommonHeaderLength,
            context,
            message_type,
            is_valid_system_event_code,
            event_code,
            error)) {
        return Result<SystemEvent>::failure(error);
    }

    return Result<SystemEvent>::success(SystemEvent{*header_result.value(), event_code});
}

Result<StockDirectory> decode_stock_directory(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    constexpr auto message_type = byte_value('R');
    if (payload.size() != kStockDirectoryLength) {
        return Result<StockDirectory>::failure(Error::invalid_itch_length(
            kStockDirectoryLength, payload.size(), context.sequence, message_type));
    }

    ItchReadCursor reader{payload, context, message_type, kStockDirectoryLength};
    auto header_result = decode_common_header(reader, message_type, context);
    if (!header_result.has_value()) {
        return Result<StockDirectory>::failure(*header_result.error());
    }

    StockSymbol stock{};
    MarketCategory market_category{};
    FinancialStatusIndicator financial_status{};
    std::uint32_t round_lot_size{};
    RoundLotsOnly round_lots_only{};
    char issue_classification{};
    IssueSubtype issue_subtype{};
    Authenticity authenticity{};
    ShortSaleThresholdIndicator short_sale_threshold{};
    IpoFlag ipo_flag{};
    LuldReferencePriceTier luld_tier{};
    EtpFlag etp_flag{};
    std::uint32_t etp_leverage_factor{};
    InverseIndicator inverse_indicator{};
    Error error{};

    if (!read_ascii_field(reader, 11, context, message_type, stock, error) ||
        !read_enum_field(
            reader,
            19,
            context,
            message_type,
            is_valid_market_category,
            market_category,
            error) ||
        !read_enum_field(
            reader,
            20,
            context,
            message_type,
            is_valid_financial_status,
            financial_status,
            error) ||
        !reader.read_u32_be(round_lot_size) ||
        !read_enum_field(
            reader,
            25,
            context,
            message_type,
            is_valid_round_lots_only,
            round_lots_only,
            error) ||
        !read_ascii_character(
            reader, 26, context, message_type, issue_classification, error) ||
        !read_ascii_field(reader, 27, context, message_type, issue_subtype, error) ||
        !read_enum_field(
            reader,
            29,
            context,
            message_type,
            is_valid_authenticity,
            authenticity,
            error) ||
        !read_enum_field(
            reader,
            30,
            context,
            message_type,
            is_valid_short_sale_threshold,
            short_sale_threshold,
            error) ||
        !read_enum_field(
            reader, 31, context, message_type, is_valid_ipo_flag, ipo_flag, error) ||
        !read_enum_field(
            reader, 32, context, message_type, is_valid_luld_tier, luld_tier, error) ||
        !read_enum_field(
            reader, 33, context, message_type, is_valid_etp_flag, etp_flag, error) ||
        !reader.read_u32_be(etp_leverage_factor) ||
        !read_enum_field(
            reader,
            38,
            context,
            message_type,
            is_valid_inverse_indicator,
            inverse_indicator,
            error)) {
        if (error.code == ErrorCode::None) {
            error = reader.error();
        }
        return Result<StockDirectory>::failure(error);
    }

    return Result<StockDirectory>::success(StockDirectory{
        *header_result.value(),
        stock,
        market_category,
        financial_status,
        round_lot_size,
        round_lots_only,
        issue_classification,
        issue_subtype,
        authenticity,
        short_sale_threshold,
        ipo_flag,
        luld_tier,
        etp_flag,
        etp_leverage_factor,
        inverse_indicator,
    });
}

}  // namespace aegis
