#pragma once

#include "aegis/common/byte_reader.hpp"
#include "aegis/common/byte_writer.hpp"
#include "aegis/common/error.hpp"
#include "aegis/common/result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace aegis {

inline constexpr std::size_t kMoldSessionSize = 10;
using MoldSession = std::array<std::byte, kMoldSessionSize>;

static_assert(sizeof(MoldSession) == kMoldSessionSize);
static_assert(alignof(MoldSession) == alignof(std::byte));
static_assert(std::is_standard_layout_v<MoldSession>);
static_assert(std::is_trivially_copyable_v<MoldSession>);

struct MoldSessionDisplay {
    std::array<char, kMoldSessionSize> characters{};
    std::size_t size{0};

    [[nodiscard]] constexpr std::string_view view() const noexcept
    {
        return std::string_view{characters.data(), size};
    }
};

[[nodiscard]] constexpr Result<MoldSession> make_mold_session(
    const std::string_view text) noexcept
{
    if (text.empty() || text.size() > kMoldSessionSize) {
        return Result<MoldSession>::failure(
            Error::invalid_session_length(text.size(), kMoldSessionSize));
    }

    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto character = static_cast<unsigned char>(text[index]);
        if (character < 0x20U || character > 0x7EU) {
            return Result<MoldSession>::failure(
                Error::invalid_session_character(index, character));
        }
    }

    MoldSession session{};
    session.fill(std::byte{0x20});
    for (std::size_t index = 0; index < text.size(); ++index) {
        session[index] = static_cast<std::byte>(static_cast<unsigned char>(text[index]));
    }
    return Result<MoldSession>::success(session);
}

[[nodiscard]] constexpr Result<void> read_mold_session(
    ByteReader& reader,
    MoldSession& destination) noexcept
{
    const auto bytes = reader.read_bytes(kMoldSessionSize);
    if (!bytes.has_value()) {
        const Error* const error = bytes.error();
        return error != nullptr ? Result<void>::failure(*error)
                                : Result<void>::failure(Error{});
    }

    const std::span<const std::byte>* const source = bytes.value();
    if (source == nullptr) {
        return Result<void>::failure(Error{});
    }

    MoldSession parsed{};
    for (std::size_t index = 0; index < kMoldSessionSize; ++index) {
        parsed[index] = (*source)[index];
    }
    destination = parsed;
    return Result<void>::success();
}

[[nodiscard]] inline Result<void> write_mold_session(
    ByteWriter& writer,
    const MoldSession& session) noexcept
{
    return writer.write_bytes(std::span<const std::byte>{session});
}

[[nodiscard]] constexpr MoldSessionDisplay format_mold_session(
    const MoldSession& session) noexcept
{
    MoldSessionDisplay display{};
    display.size = kMoldSessionSize;
    for (std::size_t index = 0; index < kMoldSessionSize; ++index) {
        display.characters[index] =
            static_cast<char>(std::to_integer<unsigned char>(session[index]));
    }

    while (display.size > 0 && session[display.size - 1] == std::byte{0x20}) {
        --display.size;
    }
    return display;
}

}  // namespace aegis
