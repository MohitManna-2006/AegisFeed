#include "aegis/common/socket.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

static_assert(!std::is_copy_constructible_v<aegis::UniqueFd>);
static_assert(!std::is_copy_assignable_v<aegis::UniqueFd>);
static_assert(std::is_nothrow_move_constructible_v<aegis::UniqueFd>);
static_assert(std::is_nothrow_move_assignable_v<aegis::UniqueFd>);
static_assert(std::is_nothrow_destructible_v<aegis::UniqueFd>);
static_assert(noexcept(std::declval<aegis::UniqueFd&>().swap(
    std::declval<aegis::UniqueFd&>())));

std::size_t failure_count = 0;

void check(const bool condition, const std::string_view expression, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << "unique_fd_test.cpp:" << line << ": check failed: " << expression << '\n';
    ++failure_count;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

[[nodiscard]] bool make_pipe(std::array<int, 2>& descriptors)
{
    if (::pipe(descriptors.data()) == 0) {
        return true;
    }

    CHECK(false);
    return false;
}

[[nodiscard]] bool descriptor_is_open(const int descriptor)
{
    errno = 0;
    return ::fcntl(descriptor, F_GETFD) != -1;
}

[[nodiscard]] bool descriptor_is_closed(const int descriptor)
{
    errno = 0;
    const int result = ::fcntl(descriptor, F_GETFD);
    return result == -1 && errno == EBADF;
}

void close_raw(const int descriptor)
{
    if (descriptor >= 0) {
        static_cast<void>(::close(descriptor));
    }
}

void test_default_invalid_state()
{
    const aegis::UniqueFd descriptor{};
    CHECK(descriptor.get() == aegis::UniqueFd::kInvalidDescriptor);
    CHECK(!descriptor.valid());
    CHECK(!static_cast<bool>(descriptor));
}

void test_adoption_and_descriptor_zero_semantics()
{
    std::array<int, 2> pipe_descriptors{};
    if (!make_pipe(pipe_descriptors)) {
        return;
    }

    const int read_descriptor = pipe_descriptors[0];
    {
        aegis::UniqueFd owner{read_descriptor};
        CHECK(owner.valid());
        CHECK(static_cast<bool>(owner));
        CHECK(owner.get() == read_descriptor);
        CHECK(owner.release() == read_descriptor);
        CHECK(!owner.valid());
    }
    CHECK(descriptor_is_open(read_descriptor));
    close_raw(read_descriptor);
    close_raw(pipe_descriptors[1]);

    aegis::UniqueFd zero{0};
    CHECK(zero.valid());
    CHECK(static_cast<bool>(zero));
    CHECK(zero.get() == 0);
    CHECK(zero.release() == 0);
    CHECK(!zero.valid());
}

void test_automatic_close_and_errno_preservation()
{
    std::array<int, 2> pipe_descriptors{};
    if (!make_pipe(pipe_descriptors)) {
        return;
    }

    const int owned_descriptor = pipe_descriptors[0];
    errno = EDOM;
    {
        aegis::UniqueFd owner{owned_descriptor};
        CHECK(owner.get() == owned_descriptor);
        errno = EDOM;
    }
    const int errno_after_destruction = errno;

    CHECK(errno_after_destruction == EDOM);
    CHECK(descriptor_is_closed(owned_descriptor));
    close_raw(pipe_descriptors[1]);
}

void test_move_construction()
{
    std::array<int, 2> pipe_descriptors{};
    if (!make_pipe(pipe_descriptors)) {
        return;
    }

    const int owned_descriptor = pipe_descriptors[0];
    {
        aegis::UniqueFd source{owned_descriptor};
        aegis::UniqueFd destination{std::move(source)};

        CHECK(!source.valid());
        CHECK(source.get() == aegis::UniqueFd::kInvalidDescriptor);
        CHECK(destination.valid());
        CHECK(destination.get() == owned_descriptor);
        CHECK(descriptor_is_open(owned_descriptor));
    }

    CHECK(descriptor_is_closed(owned_descriptor));
    close_raw(pipe_descriptors[1]);
}

void test_move_assignment_closes_previous_owner()
{
    std::array<int, 2> first_pipe{};
    std::array<int, 2> second_pipe{};
    if (!make_pipe(first_pipe)) {
        return;
    }
    if (!make_pipe(second_pipe)) {
        close_raw(first_pipe[0]);
        close_raw(first_pipe[1]);
        return;
    }

    const int transferred_descriptor = first_pipe[0];
    const int replaced_descriptor = second_pipe[0];
    {
        aegis::UniqueFd source{transferred_descriptor};
        aegis::UniqueFd destination{replaced_descriptor};
        destination = std::move(source);

        CHECK(!source.valid());
        CHECK(destination.get() == transferred_descriptor);
        CHECK(descriptor_is_closed(replaced_descriptor));
        CHECK(descriptor_is_open(transferred_descriptor));
    }

    CHECK(descriptor_is_closed(transferred_descriptor));
    close_raw(first_pipe[1]);
    close_raw(second_pipe[1]);
}

void test_release_prevents_automatic_close()
{
    std::array<int, 2> pipe_descriptors{};
    if (!make_pipe(pipe_descriptors)) {
        return;
    }

    const int owned_descriptor = pipe_descriptors[0];
    int released_descriptor = aegis::UniqueFd::kInvalidDescriptor;
    {
        aegis::UniqueFd owner{owned_descriptor};
        released_descriptor = owner.release();
        CHECK(released_descriptor == owned_descriptor);
        CHECK(!owner.valid());
    }

    CHECK(descriptor_is_open(released_descriptor));
    close_raw(released_descriptor);
    CHECK(descriptor_is_closed(released_descriptor));
    close_raw(pipe_descriptors[1]);
}

void test_reset_closes_old_and_adopts_new()
{
    std::array<int, 2> first_pipe{};
    std::array<int, 2> second_pipe{};
    if (!make_pipe(first_pipe)) {
        return;
    }
    if (!make_pipe(second_pipe)) {
        close_raw(first_pipe[0]);
        close_raw(first_pipe[1]);
        return;
    }

    const int old_descriptor = first_pipe[0];
    const int new_descriptor = second_pipe[0];
    {
        aegis::UniqueFd owner{old_descriptor};
        owner.reset(new_descriptor);

        CHECK(owner.get() == new_descriptor);
        CHECK(descriptor_is_closed(old_descriptor));
        CHECK(descriptor_is_open(new_descriptor));
    }

    CHECK(descriptor_is_closed(new_descriptor));
    close_raw(first_pipe[1]);
    close_raw(second_pipe[1]);
}

void test_reset_to_invalid()
{
    std::array<int, 2> pipe_descriptors{};
    if (!make_pipe(pipe_descriptors)) {
        return;
    }

    const int owned_descriptor = pipe_descriptors[0];
    aegis::UniqueFd owner{owned_descriptor};
    owner.reset();

    CHECK(!owner.valid());
    CHECK(owner.get() == aegis::UniqueFd::kInvalidDescriptor);
    CHECK(descriptor_is_closed(owned_descriptor));
    close_raw(pipe_descriptors[1]);
}

void test_reset_to_same_descriptor()
{
    std::array<int, 2> pipe_descriptors{};
    if (!make_pipe(pipe_descriptors)) {
        return;
    }

    const int owned_descriptor = pipe_descriptors[0];
    {
        aegis::UniqueFd owner{owned_descriptor};
        owner.reset(owner.get());

        CHECK(owner.valid());
        CHECK(owner.get() == owned_descriptor);
        CHECK(descriptor_is_open(owned_descriptor));
    }

    CHECK(descriptor_is_closed(owned_descriptor));
    close_raw(pipe_descriptors[1]);
}

void test_self_move_assignment()
{
    std::array<int, 2> pipe_descriptors{};
    if (!make_pipe(pipe_descriptors)) {
        return;
    }

    const int owned_descriptor = pipe_descriptors[0];
    {
        aegis::UniqueFd owner{owned_descriptor};
        aegis::UniqueFd* const alias = &owner;
        owner = std::move(*alias);

        CHECK(owner.valid());
        CHECK(owner.get() == owned_descriptor);
        CHECK(descriptor_is_open(owned_descriptor));
    }

    CHECK(descriptor_is_closed(owned_descriptor));
    close_raw(pipe_descriptors[1]);
}

void test_swap_behavior()
{
    std::array<int, 2> first_pipe{};
    std::array<int, 2> second_pipe{};
    if (!make_pipe(first_pipe)) {
        return;
    }
    if (!make_pipe(second_pipe)) {
        close_raw(first_pipe[0]);
        close_raw(first_pipe[1]);
        return;
    }

    const int first_descriptor = first_pipe[0];
    const int second_descriptor = second_pipe[0];
    {
        aegis::UniqueFd first{first_descriptor};
        aegis::UniqueFd second{second_descriptor};
        swap(first, second);

        CHECK(first.get() == second_descriptor);
        CHECK(second.get() == first_descriptor);
        CHECK(descriptor_is_open(first_descriptor));
        CHECK(descriptor_is_open(second_descriptor));
    }

    CHECK(descriptor_is_closed(first_descriptor));
    CHECK(descriptor_is_closed(second_descriptor));
    close_raw(first_pipe[1]);
    close_raw(second_pipe[1]);
}

void test_socketpair_ownership()
{
    std::array<int, 2> socket_descriptors{};
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, socket_descriptors.data()) != 0) {
        CHECK(false);
        return;
    }

    const int first_descriptor = socket_descriptors[0];
    const int second_descriptor = socket_descriptors[1];
    {
        aegis::UniqueFd first{first_descriptor};
        aegis::UniqueFd second{second_descriptor};
        CHECK(first.valid());
        CHECK(second.valid());
        CHECK(descriptor_is_open(first_descriptor));
        CHECK(descriptor_is_open(second_descriptor));
    }

    CHECK(descriptor_is_closed(first_descriptor));
    CHECK(descriptor_is_closed(second_descriptor));
}

}  // namespace

int main()
{
    test_default_invalid_state();
    test_adoption_and_descriptor_zero_semantics();
    test_automatic_close_and_errno_preservation();
    test_move_construction();
    test_move_assignment_closes_previous_owner();
    test_release_prevents_automatic_close();
    test_reset_closes_old_and_adopts_new();
    test_reset_to_invalid();
    test_reset_to_same_descriptor();
    test_self_move_assignment();
    test_swap_behavior();
    test_socketpair_ownership();

    if (failure_count != 0) {
        std::cerr << failure_count << " UniqueFd check(s) failed\n";
        return 1;
    }

    return 0;
}
