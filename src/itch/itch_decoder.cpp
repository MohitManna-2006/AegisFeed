#include "aegis/itch/itch_decoder.hpp"

#include "aegis/common/byte_reader.hpp"
#include "aegis/common/error.hpp"
#include "aegis/itch/itch_lengths.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

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

    [[nodiscard]] bool read_u64_be(std::uint64_t& destination) noexcept
    {
        return take(reader_.read_u64_be(), destination);
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

[[nodiscard]] constexpr bool is_valid_side(const std::uint8_t value) noexcept
{
    return value == enum_value(Side::Buy) || value == enum_value(Side::Sell);
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

[[nodiscard]] bool read_printable_flag(
    ItchReadCursor& reader,
    const std::size_t offset,
    const ItchDecodeContext context,
    const std::uint8_t message_type,
    bool& destination,
    Error& error) noexcept
{
    std::uint8_t value{};
    if (!reader.read_u8(value)) {
        error = reader.error();
        return false;
    }

    if (value == byte_value('Y')) {
        destination = true;
        return true;
    }
    if (value == byte_value('N')) {
        destination = false;
        return true;
    }

    error = Error::invalid_itch_enum(offset, value, context.sequence, message_type);
    return false;
}

struct ExecutionFields {
    ItchCommonHeader header{};
    std::uint64_t order_reference{};
    std::uint32_t executed_shares{};
    std::uint64_t match_number{};
};

[[nodiscard]] Result<ExecutionFields> decode_execution_fields(
    ItchReadCursor& reader,
    const ItchDecodeContext context,
    const std::uint8_t message_type) noexcept
{
    auto header_result = decode_common_header(reader, message_type, context);
    if (!header_result.has_value()) {
        return Result<ExecutionFields>::failure(*header_result.error());
    }

    std::uint64_t order_reference{};
    std::uint32_t executed_shares{};
    std::uint64_t match_number{};
    if (!reader.read_u64_be(order_reference) || !reader.read_u32_be(executed_shares) ||
        !reader.read_u64_be(match_number)) {
        return Result<ExecutionFields>::failure(reader.error());
    }

    return Result<ExecutionFields>::success(ExecutionFields{
        *header_result.value(),
        order_reference,
        executed_shares,
        match_number,
    });
}

[[nodiscard]] Result<AddOrder> decode_add_order_fields(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context,
    const std::uint8_t message_type,
    const std::size_t expected_size,
    const bool has_attribution) noexcept
{
    ItchReadCursor reader{payload, context, message_type, expected_size};
    auto header_result = decode_common_header(reader, message_type, context);
    if (!header_result.has_value()) {
        return Result<AddOrder>::failure(*header_result.error());
    }

    std::uint64_t order_reference{};
    Side side{};
    std::uint32_t shares{};
    StockSymbol stock{};
    Price4 price_1e4{};
    Error error{};
    if (!reader.read_u64_be(order_reference) ||
        !read_enum_field(
            reader, 19, context, message_type, is_valid_side, side, error) ||
        !reader.read_u32_be(shares) ||
        !read_ascii_field(reader, 24, context, message_type, stock, error) ||
        !reader.read_u32_be(price_1e4)) {
        if (error.code == ErrorCode::None) {
            error = reader.error();
        }
        return Result<AddOrder>::failure(error);
    }

    if (price_1e4 > kMaxPrice4) {
        return Result<AddOrder>::failure(Error::invalid_itch_value(
            32, price_1e4, kMaxPrice4, context.sequence, message_type));
    }

    std::optional<Attribution> attribution{};
    if (has_attribution) {
        Attribution decoded_attribution{};
        if (!read_ascii_field(
                reader,
                36,
                context,
                message_type,
                decoded_attribution,
                error)) {
            return Result<AddOrder>::failure(error);
        }
        attribution = decoded_attribution;
    }

    return Result<AddOrder>::success(AddOrder{
        *header_result.value(),
        order_reference,
        side,
        shares,
        stock,
        price_1e4,
        attribution,
    });
}

template <typename Message>
[[nodiscard]] Result<DecodedItchMessage> lift_decoded_result(
    Result<Message> result) noexcept
{
    Message* const message = result.value();
    if (message != nullptr) {
        return Result<DecodedItchMessage>::success(DecodedItchMessage{
            std::in_place_type<Message>,
            std::move(*message),
        });
    }

    return Result<DecodedItchMessage>::failure(*result.error());
}

[[nodiscard]] Result<DecodedItchMessage> decode_known_book_neutral(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context,
    const std::uint8_t message_type,
    const std::size_t expected_size) noexcept
{
    ItchReadCursor reader{payload, context, message_type, expected_size};
    auto header_result = decode_common_header(reader, message_type, context);
    if (!header_result.has_value()) {
        return Result<DecodedItchMessage>::failure(*header_result.error());
    }

    // Registered book-neutral bodies are intentionally opaque in MVP; their exact
    // length and common header provide the required structural recognition.
    return Result<DecodedItchMessage>::success(DecodedItchMessage{
        std::in_place_type<KnownBookNeutral>,
        KnownBookNeutral{*header_result.value()},
    });
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

Result<AddOrder> decode_add_order(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    constexpr auto message_type = byte_value('A');
    if (payload.size() != kAddOrderLength) {
        return Result<AddOrder>::failure(Error::invalid_itch_length(
            kAddOrderLength, payload.size(), context.sequence, message_type));
    }

    return decode_add_order_fields(
        payload, context, message_type, kAddOrderLength, false);
}

Result<AddOrder> decode_add_order_with_attribution(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    constexpr auto message_type = byte_value('F');
    if (payload.size() != kAddOrderWithAttributionLength) {
        return Result<AddOrder>::failure(Error::invalid_itch_length(
            kAddOrderWithAttributionLength,
            payload.size(),
            context.sequence,
            message_type));
    }

    return decode_add_order_fields(
        payload, context, message_type, kAddOrderWithAttributionLength, true);
}

Result<OrderExecuted> decode_order_executed(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    constexpr auto message_type = byte_value('E');
    if (payload.size() != kOrderExecutedLength) {
        return Result<OrderExecuted>::failure(Error::invalid_itch_length(
            kOrderExecutedLength, payload.size(), context.sequence, message_type));
    }

    ItchReadCursor reader{payload, context, message_type, kOrderExecutedLength};
    auto fields_result = decode_execution_fields(reader, context, message_type);
    if (!fields_result.has_value()) {
        return Result<OrderExecuted>::failure(*fields_result.error());
    }

    const ExecutionFields& fields = *fields_result.value();
    return Result<OrderExecuted>::success(OrderExecuted{
        fields.header,
        fields.order_reference,
        fields.executed_shares,
        fields.match_number,
    });
}

Result<OrderExecutedWithPrice> decode_order_executed_with_price(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    constexpr auto message_type = byte_value('C');
    if (payload.size() != kOrderExecutedWithPriceLength) {
        return Result<OrderExecutedWithPrice>::failure(Error::invalid_itch_length(
            kOrderExecutedWithPriceLength,
            payload.size(),
            context.sequence,
            message_type));
    }

    ItchReadCursor reader{payload, context, message_type, kOrderExecutedWithPriceLength};
    auto fields_result = decode_execution_fields(reader, context, message_type);
    if (!fields_result.has_value()) {
        return Result<OrderExecutedWithPrice>::failure(*fields_result.error());
    }

    bool printable{};
    Price4 execution_price_1e4{};
    Error error{};
    if (!read_printable_flag(reader, 31, context, message_type, printable, error) ||
        !reader.read_u32_be(execution_price_1e4)) {
        if (error.code == ErrorCode::None) {
            error = reader.error();
        }
        return Result<OrderExecutedWithPrice>::failure(error);
    }
    if (execution_price_1e4 > kMaxPrice4) {
        return Result<OrderExecutedWithPrice>::failure(Error::invalid_itch_value(
            32,
            execution_price_1e4,
            kMaxPrice4,
            context.sequence,
            message_type));
    }

    const ExecutionFields& fields = *fields_result.value();
    return Result<OrderExecutedWithPrice>::success(OrderExecutedWithPrice{
        fields.header,
        fields.order_reference,
        fields.executed_shares,
        fields.match_number,
        printable,
        execution_price_1e4,
    });
}

Result<OrderCancel> decode_order_cancel(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    constexpr auto message_type = byte_value('X');
    if (payload.size() != kOrderCancelLength) {
        return Result<OrderCancel>::failure(Error::invalid_itch_length(
            kOrderCancelLength, payload.size(), context.sequence, message_type));
    }

    ItchReadCursor reader{payload, context, message_type, kOrderCancelLength};
    auto header_result = decode_common_header(reader, message_type, context);
    if (!header_result.has_value()) {
        return Result<OrderCancel>::failure(*header_result.error());
    }

    std::uint64_t order_reference{};
    std::uint32_t cancelled_shares{};
    if (!reader.read_u64_be(order_reference) || !reader.read_u32_be(cancelled_shares)) {
        return Result<OrderCancel>::failure(reader.error());
    }

    return Result<OrderCancel>::success(OrderCancel{
        *header_result.value(),
        order_reference,
        cancelled_shares,
    });
}

Result<OrderDelete> decode_order_delete(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    constexpr auto message_type = byte_value('D');
    if (payload.size() != kOrderDeleteLength) {
        return Result<OrderDelete>::failure(Error::invalid_itch_length(
            kOrderDeleteLength, payload.size(), context.sequence, message_type));
    }

    ItchReadCursor reader{payload, context, message_type, kOrderDeleteLength};
    auto header_result = decode_common_header(reader, message_type, context);
    if (!header_result.has_value()) {
        return Result<OrderDelete>::failure(*header_result.error());
    }

    std::uint64_t order_reference{};
    if (!reader.read_u64_be(order_reference)) {
        return Result<OrderDelete>::failure(reader.error());
    }

    return Result<OrderDelete>::success(
        OrderDelete{*header_result.value(), order_reference});
}

Result<OrderReplace> decode_order_replace(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    constexpr auto message_type = byte_value('U');
    if (payload.size() != kOrderReplaceLength) {
        return Result<OrderReplace>::failure(Error::invalid_itch_length(
            kOrderReplaceLength, payload.size(), context.sequence, message_type));
    }

    ItchReadCursor reader{payload, context, message_type, kOrderReplaceLength};
    auto header_result = decode_common_header(reader, message_type, context);
    if (!header_result.has_value()) {
        return Result<OrderReplace>::failure(*header_result.error());
    }

    std::uint64_t original_order_reference{};
    std::uint64_t new_order_reference{};
    std::uint32_t shares{};
    Price4 price_1e4{};
    if (!reader.read_u64_be(original_order_reference) ||
        !reader.read_u64_be(new_order_reference) || !reader.read_u32_be(shares) ||
        !reader.read_u32_be(price_1e4)) {
        return Result<OrderReplace>::failure(reader.error());
    }
    if (price_1e4 > kMaxPrice4) {
        return Result<OrderReplace>::failure(Error::invalid_itch_value(
            31, price_1e4, kMaxPrice4, context.sequence, message_type));
    }

    return Result<OrderReplace>::success(OrderReplace{
        *header_result.value(),
        original_order_reference,
        new_order_reference,
        shares,
        price_1e4,
    });
}

Result<DecodedItchMessage> decode_itch(
    const std::span<const std::byte> payload,
    const ItchDecodeContext context) noexcept
{
    if (payload.empty()) {
        return Result<DecodedItchMessage>::failure(
            Error::invalid_itch_length(1, 0, context.sequence, 0));
    }

    const std::uint8_t raw_type = std::to_integer<std::uint8_t>(payload.front());
    const char message_type = static_cast<char>(raw_type);
    const ItchTypeClass classification = classify_itch_type(message_type);
    if (classification == ItchTypeClass::Unknown) {
        return Result<DecodedItchMessage>::failure(
            Error::unknown_itch_type(raw_type, context.sequence));
    }

    const auto expected_size = expected_itch_length(message_type);
    if (!expected_size.has_value()) {
        return Result<DecodedItchMessage>::failure(
            Error::unknown_itch_type(raw_type, context.sequence));
    }
    if (payload.size() != *expected_size) {
        return Result<DecodedItchMessage>::failure(Error::invalid_itch_length(
            *expected_size, payload.size(), context.sequence, raw_type));
    }

    if (classification == ItchTypeClass::KnownBookNeutral) {
        return decode_known_book_neutral(payload, context, raw_type, *expected_size);
    }

    switch (message_type) {
    case 'S':
        return lift_decoded_result(decode_system_event(payload, context));
    case 'R':
        return lift_decoded_result(decode_stock_directory(payload, context));
    case 'A':
        return lift_decoded_result(decode_add_order(payload, context));
    case 'F':
        return lift_decoded_result(decode_add_order_with_attribution(payload, context));
    case 'E':
        return lift_decoded_result(decode_order_executed(payload, context));
    case 'C':
        return lift_decoded_result(decode_order_executed_with_price(payload, context));
    case 'X':
        return lift_decoded_result(decode_order_cancel(payload, context));
    case 'D':
        return lift_decoded_result(decode_order_delete(payload, context));
    case 'U':
        return lift_decoded_result(decode_order_replace(payload, context));
    default:
        return Result<DecodedItchMessage>::failure(
            Error::unknown_itch_type(raw_type, context.sequence));
    }
}

}  // namespace aegis
