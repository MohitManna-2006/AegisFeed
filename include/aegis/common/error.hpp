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
};

struct Error {
    ErrorCategory category{ErrorCategory::Internal};
    ErrorCode code{ErrorCode::None};
    std::size_t offset{0};
    std::size_t requested_size{0};
    std::size_t available_size{0};
    std::string_view message{};

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
            "read exceeds remaining buffer",
        };
    }
};

}  // namespace aegis
