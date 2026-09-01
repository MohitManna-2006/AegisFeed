#pragma once

#include <cerrno>
#include <type_traits>
#include <utility>

#include <unistd.h>

namespace aegis {

class UniqueFd {
public:
    static constexpr int kInvalidDescriptor = -1;

    constexpr UniqueFd() noexcept = default;

    constexpr explicit UniqueFd(const int descriptor) noexcept
        : descriptor_{normalize(descriptor)}
    {
    }

    ~UniqueFd() noexcept
    {
        close_preserving_errno(release());
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    constexpr UniqueFd(UniqueFd&& other) noexcept : descriptor_{other.release()} {}

    UniqueFd& operator=(UniqueFd&& other) noexcept
    {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] constexpr int get() const noexcept
    {
        return descriptor_;
    }

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return descriptor_ >= 0;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return valid();
    }

    [[nodiscard]] constexpr int release() noexcept
    {
        return std::exchange(descriptor_, kInvalidDescriptor);
    }

    void reset(const int descriptor = kInvalidDescriptor) noexcept
    {
        const int normalized = normalize(descriptor);
        if (descriptor_ == normalized) {
            return;
        }

        const int previous = std::exchange(descriptor_, kInvalidDescriptor);
        close_preserving_errno(previous);
        descriptor_ = normalized;
    }

    constexpr void swap(UniqueFd& other) noexcept
    {
        std::swap(descriptor_, other.descriptor_);
    }

private:
    [[nodiscard]] static constexpr int normalize(const int descriptor) noexcept
    {
        return descriptor >= 0 ? descriptor : kInvalidDescriptor;
    }

    static void close_preserving_errno(const int descriptor) noexcept
    {
        if (descriptor < 0) {
            return;
        }

        const int saved_errno = errno;
        static_cast<void>(::close(descriptor));
        errno = saved_errno;
    }

    int descriptor_{kInvalidDescriptor};
};

constexpr void swap(UniqueFd& lhs, UniqueFd& rhs) noexcept
{
    lhs.swap(rhs);
}

static_assert(sizeof(UniqueFd) == sizeof(int));
static_assert(!std::is_copy_constructible_v<UniqueFd>);
static_assert(!std::is_copy_assignable_v<UniqueFd>);
static_assert(std::is_nothrow_move_constructible_v<UniqueFd>);
static_assert(std::is_nothrow_move_assignable_v<UniqueFd>);
static_assert(std::is_nothrow_destructible_v<UniqueFd>);

}  // namespace aegis
