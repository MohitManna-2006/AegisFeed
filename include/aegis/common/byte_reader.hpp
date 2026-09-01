#pragma once

#include "aegis/common/error.hpp"
#include "aegis/common/result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace aegis {
namespace detail {

[[nodiscard]] constexpr bool can_read(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    const std::size_t width) noexcept
{
    return offset <= bytes.size() && width <= bytes.size() - offset;
}

template <typename UInt>
[[nodiscard]] constexpr bool read_unsigned_be(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    const std::size_t width,
    UInt& out) noexcept
{
    if (!can_read(bytes, offset, width)) {
        return false;
    }

    UInt value{0};
    for (std::size_t index = 0; index < width; ++index) {
        value = static_cast<UInt>(
            (value << 8U) |
            static_cast<UInt>(std::to_integer<std::uint8_t>(bytes[offset + index])));
    }

    out = value;
    return true;
}

}  // namespace detail

[[nodiscard]] constexpr bool read_u8(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    std::uint8_t& out) noexcept
{
    return detail::read_unsigned_be(bytes, offset, 1, out);
}

[[nodiscard]] constexpr bool read_u16_be(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    std::uint16_t& out) noexcept
{
    return detail::read_unsigned_be(bytes, offset, 2, out);
}

[[nodiscard]] constexpr bool read_u32_be(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    std::uint32_t& out) noexcept
{
    return detail::read_unsigned_be(bytes, offset, 4, out);
}

[[nodiscard]] constexpr bool read_u48_be(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    std::uint64_t& out) noexcept
{
    return detail::read_unsigned_be(bytes, offset, 6, out);
}

[[nodiscard]] constexpr bool read_u64_be(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    std::uint64_t& out) noexcept
{
    return detail::read_unsigned_be(bytes, offset, 8, out);
}

class ByteReader {
public:
    constexpr explicit ByteReader(const std::span<const std::byte> bytes) noexcept : bytes_{bytes} {}

    [[nodiscard]] constexpr std::size_t position() const noexcept
    {
        return position_;
    }

    [[nodiscard]] constexpr std::size_t remaining() const noexcept
    {
        return bytes_.size() - position_;
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return remaining() == 0;
    }

    [[nodiscard]] constexpr Result<std::uint8_t> read_u8() noexcept
    {
        return read_integer<std::uint8_t>(1);
    }

    [[nodiscard]] constexpr Result<std::uint16_t> read_u16_be() noexcept
    {
        return read_integer<std::uint16_t>(2);
    }

    [[nodiscard]] constexpr Result<std::uint32_t> read_u32_be() noexcept
    {
        return read_integer<std::uint32_t>(4);
    }

    [[nodiscard]] constexpr Result<std::uint64_t> read_u48_be() noexcept
    {
        return read_integer<std::uint64_t>(6);
    }

    [[nodiscard]] constexpr Result<std::uint64_t> read_u64_be() noexcept
    {
        return read_integer<std::uint64_t>(8);
    }

    [[nodiscard]] constexpr Result<std::span<const std::byte>> read_bytes(
        const std::size_t count) noexcept
    {
        if (!detail::can_read(bytes_, position_, count)) {
            return Result<std::span<const std::byte>>::failure(
                Error::read_past_end(position_, count, remaining()));
        }

        const auto result = bytes_.subspan(position_, count);
        position_ += count;
        return Result<std::span<const std::byte>>::success(result);
    }

private:
    template <typename UInt>
    [[nodiscard]] constexpr Result<UInt> read_integer(const std::size_t width) noexcept
    {
        UInt value{0};
        if (!detail::read_unsigned_be(bytes_, position_, width, value)) {
            return Result<UInt>::failure(Error::read_past_end(position_, width, remaining()));
        }

        position_ += width;
        return Result<UInt>::success(value);
    }

    std::span<const std::byte> bytes_;
    std::size_t position_{0};
};

}  // namespace aegis
