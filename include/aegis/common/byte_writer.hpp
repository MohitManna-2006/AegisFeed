#pragma once

#include "aegis/common/error.hpp"
#include "aegis/common/result.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace aegis {
namespace detail {

inline constexpr std::uint64_t max_u48_value = 0x0000FFFFFFFFFFFFULL;

[[nodiscard]] constexpr bool can_write(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::size_t width) noexcept
{
    return offset <= bytes.size() && width <= bytes.size() - offset;
}

template <typename UInt>
[[nodiscard]] constexpr bool write_unsigned_be(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::size_t width,
    UInt value) noexcept
{
    if (!can_write(bytes, offset, width)) {
        return false;
    }

    for (std::size_t index = 0; index < width; ++index) {
        bytes[offset + width - index - 1] =
            static_cast<std::byte>(value & static_cast<UInt>(0xFFU));
        value = static_cast<UInt>(value >> 8U);
    }

    return true;
}

}  // namespace detail

[[nodiscard]] constexpr bool write_u8(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint8_t value) noexcept
{
    return detail::write_unsigned_be(bytes, offset, 1, value);
}

[[nodiscard]] constexpr bool write_u16_be(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint16_t value) noexcept
{
    return detail::write_unsigned_be(bytes, offset, 2, value);
}

[[nodiscard]] constexpr bool write_u32_be(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint32_t value) noexcept
{
    return detail::write_unsigned_be(bytes, offset, 4, value);
}

[[nodiscard]] constexpr bool write_u48_be(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint64_t value) noexcept
{
    return value <= detail::max_u48_value &&
           detail::write_unsigned_be(bytes, offset, 6, value);
}

[[nodiscard]] constexpr bool write_u64_be(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint64_t value) noexcept
{
    return detail::write_unsigned_be(bytes, offset, 8, value);
}

[[nodiscard]] inline bool write_bytes(
    const std::span<std::byte> destination,
    const std::size_t offset,
    const std::span<const std::byte> source) noexcept
{
    if (!detail::can_write(destination, offset, source.size())) {
        return false;
    }

    if (!source.empty()) {
        std::memmove(destination.data() + offset, source.data(), source.size());
    }
    return true;
}

class ByteWriter {
public:
    constexpr explicit ByteWriter(const std::span<std::byte> bytes) noexcept : bytes_{bytes} {}

    [[nodiscard]] constexpr std::size_t position() const noexcept
    {
        return position_;
    }

    [[nodiscard]] constexpr std::size_t remaining() const noexcept
    {
        return bytes_.size() - position_;
    }

    [[nodiscard]] constexpr Result<void> write_u8(const std::uint8_t value) noexcept
    {
        return write_integer(value, 1);
    }

    [[nodiscard]] constexpr Result<void> write_u16_be(const std::uint16_t value) noexcept
    {
        return write_integer(value, 2);
    }

    [[nodiscard]] constexpr Result<void> write_u32_be(const std::uint32_t value) noexcept
    {
        return write_integer(value, 4);
    }

    [[nodiscard]] constexpr Result<void> write_u48_be(const std::uint64_t value) noexcept
    {
        if (value > detail::max_u48_value) {
            return Result<void>::failure(
                Error::value_out_of_range(position_, value, detail::max_u48_value));
        }
        return write_integer(value, 6);
    }

    [[nodiscard]] constexpr Result<void> write_u64_be(const std::uint64_t value) noexcept
    {
        return write_integer(value, 8);
    }

    [[nodiscard]] Result<void> write_bytes(const std::span<const std::byte> source) noexcept
    {
        if (!detail::can_write(bytes_, position_, source.size())) {
            return Result<void>::failure(
                Error::write_past_end(position_, source.size(), remaining()));
        }

        if (!aegis::write_bytes(bytes_, position_, source)) {
            return Result<void>::failure(
                Error::write_past_end(position_, source.size(), remaining()));
        }

        position_ += source.size();
        return Result<void>::success();
    }

private:
    template <typename UInt>
    [[nodiscard]] constexpr Result<void> write_integer(
        const UInt value,
        const std::size_t width) noexcept
    {
        if (!detail::can_write(bytes_, position_, width)) {
            return Result<void>::failure(Error::write_past_end(position_, width, remaining()));
        }

        if (!detail::write_unsigned_be(bytes_, position_, width, value)) {
            return Result<void>::failure(Error::write_past_end(position_, width, remaining()));
        }

        position_ += width;
        return Result<void>::success();
    }

    std::span<std::byte> bytes_;
    std::size_t position_{0};
};

}  // namespace aegis
