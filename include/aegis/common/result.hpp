#pragma once

#include "aegis/common/error.hpp"

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace aegis {

template <typename T>
class [[nodiscard]] Result {
    static_assert(!std::is_reference_v<T>, "Result<T> cannot hold a reference");

public:
    [[nodiscard]] static constexpr Result success(T value) noexcept(
        std::is_nothrow_move_constructible_v<T>)
    {
        return Result{SuccessTag{}, std::move(value)};
    }

    [[nodiscard]] static constexpr Result failure(const Error error) noexcept
    {
        return Result{error};
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return value_.has_value();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] constexpr T* value() noexcept
    {
        return value_ ? std::addressof(*value_) : nullptr;
    }

    [[nodiscard]] constexpr const T* value() const noexcept
    {
        return value_ ? std::addressof(*value_) : nullptr;
    }

    [[nodiscard]] constexpr Error* error() noexcept
    {
        return value_ ? nullptr : std::addressof(error_);
    }

    [[nodiscard]] constexpr const Error* error() const noexcept
    {
        return value_ ? nullptr : std::addressof(error_);
    }

private:
    struct SuccessTag {};

    constexpr Result(SuccessTag, T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : value_{std::move(value)}
    {
    }

    constexpr explicit Result(const Error error) noexcept : value_{std::nullopt}, error_{error} {}

    std::optional<T> value_;
    Error error_{};
};

}  // namespace aegis
