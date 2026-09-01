#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aegis {

enum class ErrorCategory : std::uint8_t {
    Configuration = 1,
    InputIo = 2,
    InputFraming = 3,
    SocketIo = 4,
    MoldFraming = 5,
    Session = 6,
    Sequence = 7,
    Recovery = 8,
    ItchDecode = 9,
    BookInvariant = 10,
    ResourceLimit = 11,
    Verification = 12,
    Internal = 13,
};

enum class ErrorCode : std::uint16_t {
    None = 0,
    ReadPastEnd = 1,
    WritePastEnd = 2,
    ValueOutOfRange = 3,
    InvalidSessionLength = 4,
    InvalidSessionCharacter = 5,
    InvalidItchLength = 6,
    UnexpectedItchType = 7,
    InvalidItchEnum = 8,
    InvalidItchAscii = 9,
    InvalidItchValue = 10,
    UnknownItchType = 11,
    BookInvariantViolation = 12,
    InvalidRequestedSymbol = 13,
    InvalidStockSymbol = 14,
    ConflictingStockLocate = 15,
    InvalidAddOrder = 16,
    DuplicateOrderReference = 17,
    BookArithmeticOverflow = 18,
    ConflictingSelectedSymbolLocate = 19,
    UnknownStockLocate = 20,
    InvalidOrderReduction = 21,
    UnknownOrderReference = 22,
    OverExecution = 23,
    OverCancel = 24,
};

struct Error {
    ErrorCategory category{ErrorCategory::Internal};
    ErrorCode code{ErrorCode::None};
    std::size_t offset{0};
    std::size_t requested_size{0};
    std::size_t available_size{0};
    std::uint64_t observed_value{0};
    std::uint64_t limit_value{0};
    std::string_view message{};
    std::uint64_t sequence{0};
    std::uint8_t message_type{0};

    [[nodiscard]] static constexpr Error read_past_end(
        const std::size_t offset,
        const std::size_t requested_size,
        const std::size_t available_size) noexcept
    {
        return Error{
            ErrorCategory::InputFraming,
            ErrorCode::ReadPastEnd,
            offset,
            requested_size,
            available_size,
            0,
            0,
            "read exceeds remaining buffer",
        };
    }

    [[nodiscard]] static constexpr Error write_past_end(
        const std::size_t offset,
        const std::size_t requested_size,
        const std::size_t available_size) noexcept
    {
        return Error{
            ErrorCategory::ResourceLimit,
            ErrorCode::WritePastEnd,
            offset,
            requested_size,
            available_size,
            0,
            0,
            "write exceeds remaining buffer",
        };
    }

    [[nodiscard]] static constexpr Error value_out_of_range(
        const std::size_t offset,
        const std::uint64_t observed_value,
        const std::uint64_t limit_value) noexcept
    {
        return Error{
            ErrorCategory::InputFraming,
            ErrorCode::ValueOutOfRange,
            offset,
            0,
            0,
            observed_value,
            limit_value,
            "value does not fit field width",
        };
    }

    [[nodiscard]] static constexpr Error invalid_session_length(
        const std::size_t observed_length,
        const std::size_t maximum_length) noexcept
    {
        return Error{
            ErrorCategory::Session,
            ErrorCode::InvalidSessionLength,
            0,
            observed_length,
            maximum_length,
            observed_length,
            maximum_length,
            "session text must contain 1 to 10 printable ASCII characters",
        };
    }

    [[nodiscard]] static constexpr Error invalid_session_character(
        const std::size_t offset,
        const std::uint8_t observed_character) noexcept
    {
        return Error{
            ErrorCategory::Session,
            ErrorCode::InvalidSessionCharacter,
            offset,
            1,
            1,
            observed_character,
            0x7E,
            "session text contains a non-printable ASCII character",
        };
    }

    [[nodiscard]] static constexpr Error invalid_itch_length(
        const std::size_t expected_size,
        const std::size_t observed_size,
        const std::uint64_t sequence,
        const std::uint8_t message_type) noexcept
    {
        return Error{
            ErrorCategory::ItchDecode,
            ErrorCode::InvalidItchLength,
            0,
            expected_size,
            observed_size,
            observed_size,
            expected_size,
            "ITCH payload length does not match message type",
            sequence,
            message_type,
        };
    }

    [[nodiscard]] static constexpr Error unexpected_itch_type(
        const std::uint8_t expected_type,
        const std::uint8_t observed_type,
        const std::uint64_t sequence) noexcept
    {
        return Error{
            ErrorCategory::ItchDecode,
            ErrorCode::UnexpectedItchType,
            0,
            1,
            1,
            observed_type,
            expected_type,
            "unexpected ITCH message type",
            sequence,
            observed_type,
        };
    }

    [[nodiscard]] static constexpr Error unknown_itch_type(
        const std::uint8_t observed_type,
        const std::uint64_t sequence) noexcept
    {
        return Error{
            ErrorCategory::ItchDecode,
            ErrorCode::UnknownItchType,
            0,
            1,
            1,
            observed_type,
            0,
            "unknown ITCH message type",
            sequence,
            observed_type,
        };
    }

    [[nodiscard]] static constexpr Error invalid_itch_enum(
        const std::size_t offset,
        const std::uint8_t observed_value,
        const std::uint64_t sequence,
        const std::uint8_t message_type) noexcept
    {
        return Error{
            ErrorCategory::ItchDecode,
            ErrorCode::InvalidItchEnum,
            offset,
            1,
            1,
            observed_value,
            0,
            "invalid ITCH enum value",
            sequence,
            message_type,
        };
    }

    [[nodiscard]] static constexpr Error invalid_itch_ascii(
        const std::size_t offset,
        const std::uint8_t observed_value,
        const std::uint64_t sequence,
        const std::uint8_t message_type) noexcept
    {
        return Error{
            ErrorCategory::ItchDecode,
            ErrorCode::InvalidItchAscii,
            offset,
            1,
            1,
            observed_value,
            0x7E,
            "ITCH alpha field contains non-printable ASCII",
            sequence,
            message_type,
        };
    }

    [[nodiscard]] static constexpr Error invalid_itch_value(
        const std::size_t offset,
        const std::uint64_t observed_value,
        const std::uint64_t limit_value,
        const std::uint64_t sequence,
        const std::uint8_t message_type) noexcept
    {
        return Error{
            ErrorCategory::ItchDecode,
            ErrorCode::InvalidItchValue,
            offset,
            0,
            0,
            observed_value,
            limit_value,
            "ITCH value exceeds permitted range",
            sequence,
            message_type,
        };
    }

    [[nodiscard]] static constexpr Error book_invariant_violation(
        const std::string_view reason,
        const std::uint64_t observed_value = 0,
        const std::uint64_t limit_value = 0) noexcept
    {
        return Error{
            ErrorCategory::BookInvariant,
            ErrorCode::BookInvariantViolation,
            0,
            0,
            0,
            observed_value,
            limit_value,
            reason,
        };
    }

    [[nodiscard]] static constexpr Error invalid_requested_symbol(
        const std::string_view reason,
        const std::size_t observed_size = 0) noexcept
    {
        return Error{
            ErrorCategory::Configuration,
            ErrorCode::InvalidRequestedSymbol,
            0,
            observed_size,
            8,
            observed_size,
            8,
            reason,
        };
    }

    [[nodiscard]] static constexpr Error invalid_stock_symbol(
        const std::string_view reason,
        const std::size_t offset = 0) noexcept
    {
        return Error{
            ErrorCategory::Session,
            ErrorCode::InvalidStockSymbol,
            offset,
            1,
            8,
            0,
            0,
            reason,
        };
    }

    [[nodiscard]] static constexpr Error conflicting_stock_locate(
        const std::uint16_t stock_locate) noexcept
    {
        return Error{
            ErrorCategory::Session,
            ErrorCode::ConflictingStockLocate,
            0,
            0,
            0,
            stock_locate,
            stock_locate,
            "stock locate is already mapped to a different symbol",
        };
    }

    [[nodiscard]] static constexpr Error invalid_add_order(
        const std::string_view reason,
        const std::uint64_t observed_value = 0,
        const std::uint64_t limit_value = 0) noexcept
    {
        return Error{
            ErrorCategory::BookInvariant,
            ErrorCode::InvalidAddOrder,
            0,
            0,
            0,
            observed_value,
            limit_value,
            reason,
        };
    }

    [[nodiscard]] static constexpr Error duplicate_order_reference(
        const std::uint64_t order_reference) noexcept
    {
        return Error{
            ErrorCategory::BookInvariant,
            ErrorCode::DuplicateOrderReference,
            0,
            0,
            0,
            order_reference,
            0,
            "order reference is already active",
        };
    }

    [[nodiscard]] static constexpr Error book_arithmetic_overflow(
        const std::string_view reason,
        const std::uint64_t observed_value,
        const std::uint64_t limit_value) noexcept
    {
        return Error{
            ErrorCategory::ResourceLimit,
            ErrorCode::BookArithmeticOverflow,
            0,
            0,
            0,
            observed_value,
            limit_value,
            reason,
        };
    }

    [[nodiscard]] static constexpr Error conflicting_selected_symbol_locate(
        const std::uint16_t existing_stock_locate,
        const std::uint16_t observed_stock_locate) noexcept
    {
        return Error{
            ErrorCategory::Session,
            ErrorCode::ConflictingSelectedSymbolLocate,
            0,
            0,
            0,
            observed_stock_locate,
            existing_stock_locate,
            "requested symbol is already discovered at a different stock locate",
        };
    }

    [[nodiscard]] static constexpr Error unknown_stock_locate(
        const std::uint16_t stock_locate) noexcept
    {
        return Error{
            ErrorCategory::Session,
            ErrorCode::UnknownStockLocate,
            0,
            0,
            0,
            stock_locate,
            0,
            "order event references an unknown stock locate",
        };
    }

    [[nodiscard]] static constexpr Error invalid_order_reduction(
        const std::string_view reason,
        const std::uint64_t observed_value = 0,
        const std::uint64_t limit_value = 0) noexcept
    {
        return Error{
            ErrorCategory::BookInvariant,
            ErrorCode::InvalidOrderReduction,
            0,
            0,
            0,
            observed_value,
            limit_value,
            reason,
        };
    }

    [[nodiscard]] static constexpr Error unknown_order_reference(
        const std::uint64_t order_reference) noexcept
    {
        return Error{
            ErrorCategory::BookInvariant,
            ErrorCode::UnknownOrderReference,
            0,
            0,
            0,
            order_reference,
            0,
            "order reduction references an inactive order",
        };
    }

    [[nodiscard]] static constexpr Error over_execution(
        const std::uint64_t executed_shares,
        const std::uint64_t remaining_shares) noexcept
    {
        return Error{
            ErrorCategory::BookInvariant,
            ErrorCode::OverExecution,
            0,
            0,
            0,
            executed_shares,
            remaining_shares,
            "executed shares exceed remaining order shares",
        };
    }

    [[nodiscard]] static constexpr Error over_cancel(
        const std::uint64_t cancelled_shares,
        const std::uint64_t remaining_shares) noexcept
    {
        return Error{
            ErrorCategory::BookInvariant,
            ErrorCode::OverCancel,
            0,
            0,
            0,
            cancelled_shares,
            remaining_shares,
            "cancelled shares exceed remaining order shares",
        };
    }
};

}  // namespace aegis
