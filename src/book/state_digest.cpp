#include "aegis/book/state_digest.hpp"

#include "aegis/common/error.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace aegis {
namespace {

constexpr std::string_view kCanonicalDomain{"AEGISBK"};
constexpr std::uint8_t kCanonicalVersion = 1;
constexpr std::uint64_t kFnv1aOffsetBasis = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t kFnv1aPrime = 1'099'511'628'211ULL;

void append_u8(std::vector<std::byte>& bytes, const std::uint8_t value)
{
    bytes.push_back(static_cast<std::byte>(value));
}

void append_u16_be(std::vector<std::byte>& bytes, const std::uint16_t value)
{
    append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
    append_u8(bytes, static_cast<std::uint8_t>(value));
}

void append_u32_be(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    append_u8(bytes, static_cast<std::uint8_t>(value >> 24U));
    append_u8(bytes, static_cast<std::uint8_t>(value >> 16U));
    append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
    append_u8(bytes, static_cast<std::uint8_t>(value));
}

void append_u64_be(std::vector<std::byte>& bytes, const std::uint64_t value)
{
    for (std::size_t index = 0; index < 8; ++index) {
        const auto shift = static_cast<unsigned>(56U - index * 8U);
        append_u8(bytes, static_cast<std::uint8_t>(value >> shift));
    }
}

void append_chars(std::vector<std::byte>& bytes, const std::span<const char> chars)
{
    for (const char value : chars) {
        append_u8(bytes, static_cast<std::uint8_t>(static_cast<unsigned char>(value)));
    }
}

[[nodiscard]] Result<std::vector<std::byte>> encode_snapshot(
    const CanonicalBookStore& snapshot,
    const std::uint64_t last_mold_sequence)
{
    if (snapshot.books.size() > std::numeric_limits<std::uint32_t>::max()) {
        return Result<std::vector<std::byte>>::failure(Error::book_arithmetic_overflow(
            "canonical symbol count exceeds its fixed-width field",
            snapshot.books.size(),
            std::numeric_limits<std::uint32_t>::max()));
    }

    std::vector<std::byte> bytes;
    append_chars(bytes, std::span<const char>{kCanonicalDomain.data(), kCanonicalDomain.size()});
    append_u8(bytes, kCanonicalVersion);
    append_u64_be(bytes, last_mold_sequence);
    append_u32_be(bytes, static_cast<std::uint32_t>(snapshot.books.size()));

    for (const auto& canonical_book : snapshot.books) {
        if (canonical_book.symbol.size() > std::numeric_limits<std::uint8_t>::max()) {
            return Result<std::vector<std::byte>>::failure(
                Error::book_arithmetic_overflow(
                    "canonical symbol length exceeds its fixed-width field",
                    canonical_book.symbol.size(),
                    std::numeric_limits<std::uint8_t>::max()));
        }
        if (canonical_book.book.bids.size() >
            std::numeric_limits<std::uint32_t>::max()) {
            return Result<std::vector<std::byte>>::failure(
                Error::book_arithmetic_overflow(
                    "canonical bid-level count exceeds its fixed-width field",
                    canonical_book.book.bids.size(),
                    std::numeric_limits<std::uint32_t>::max()));
        }
        if (canonical_book.book.asks.size() >
            std::numeric_limits<std::uint32_t>::max()) {
            return Result<std::vector<std::byte>>::failure(
                Error::book_arithmetic_overflow(
                    "canonical ask-level count exceeds its fixed-width field",
                    canonical_book.book.asks.size(),
                    std::numeric_limits<std::uint32_t>::max()));
        }

        append_u8(bytes, static_cast<std::uint8_t>(canonical_book.symbol.size()));
        append_chars(bytes, std::span<const char>{
                                canonical_book.symbol.data(),
                                canonical_book.symbol.size()});
        append_u16_be(bytes, canonical_book.book.stock_locate);

        const auto append_levels = [&](const auto& levels) {
            append_u32_be(bytes, static_cast<std::uint32_t>(levels.size()));
            for (const auto& level : levels) {
                append_u32_be(bytes, level.price);
                append_u64_be(bytes, level.aggregate_shares);
                append_u32_be(bytes, level.order_count);
            }
        };
        append_levels(canonical_book.book.bids);
        append_levels(canonical_book.book.asks);

        append_u64_be(
            bytes, static_cast<std::uint64_t>(canonical_book.book.orders.size()));
        for (const auto& order : canonical_book.book.orders) {
            append_u64_be(bytes, order.id);
            append_u16_be(bytes, order.stock_locate);
            append_u8(bytes, static_cast<std::uint8_t>(order.side));
            append_u32_be(bytes, order.price);
            append_u32_be(bytes, order.remaining);
            append_u8(bytes, order.has_attribution ? 1U : 0U);
            append_chars(bytes, order.attribution);
        }
    }

    return Result<std::vector<std::byte>>::success(std::move(bytes));
}

[[nodiscard]] std::uint64_t fnv1a64(const std::span<const std::byte> bytes) noexcept
{
    std::uint64_t hash = kFnv1aOffsetBasis;
    for (const auto byte : bytes) {
        hash ^= std::to_integer<std::uint8_t>(byte);
        hash *= kFnv1aPrime;
    }
    return hash;
}

[[nodiscard]] bool add_size(
    std::uint64_t& total,
    const std::size_t value) noexcept
{
    if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
        if (value > std::numeric_limits<std::uint64_t>::max()) {
            return false;
        }
    }
    const auto converted = static_cast<std::uint64_t>(value);
    if (converted > std::numeric_limits<std::uint64_t>::max() - total) {
        return false;
    }
    total += converted;
    return true;
}

[[nodiscard]] bool add_value(
    std::uint64_t& total,
    const std::uint64_t value) noexcept
{
    if (value > std::numeric_limits<std::uint64_t>::max() - total) {
        return false;
    }
    total += value;
    return true;
}

[[nodiscard]] Result<StateDigest> summarize(
    const CanonicalBookStore& snapshot,
    const std::uint64_t hash,
    const std::uint64_t last_mold_sequence)
{
    StateDigest digest;
    digest.hash = hash;
    digest.last_mold_sequence = last_mold_sequence;

    if (!add_size(digest.symbol_count, snapshot.books.size())) {
        return Result<StateDigest>::failure(Error::book_arithmetic_overflow(
            "canonical symbol summary count overflowed",
            snapshot.books.size(),
            std::numeric_limits<std::uint64_t>::max()));
    }

    for (const auto& canonical_book : snapshot.books) {
        if (!add_size(
                digest.active_order_count, canonical_book.book.orders.size()) ||
            !add_size(digest.bid_level_count, canonical_book.book.bids.size()) ||
            !add_size(digest.ask_level_count, canonical_book.book.asks.size())) {
            return Result<StateDigest>::failure(Error::book_arithmetic_overflow(
                "canonical structural summary count overflowed",
                canonical_book.book.orders.size(),
                std::numeric_limits<std::uint64_t>::max()));
        }

        for (const auto& level : canonical_book.book.bids) {
            if (!add_value(digest.aggregate_displayed_shares, level.aggregate_shares)) {
                return Result<StateDigest>::failure(Error::book_arithmetic_overflow(
                    "canonical displayed-share summary overflowed",
                    level.aggregate_shares,
                    std::numeric_limits<std::uint64_t>::max() -
                        digest.aggregate_displayed_shares));
            }
        }
        for (const auto& level : canonical_book.book.asks) {
            if (!add_value(digest.aggregate_displayed_shares, level.aggregate_shares)) {
                return Result<StateDigest>::failure(Error::book_arithmetic_overflow(
                    "canonical displayed-share summary overflowed",
                    level.aggregate_shares,
                    std::numeric_limits<std::uint64_t>::max() -
                        digest.aggregate_displayed_shares));
            }
        }
    }

    return Result<StateDigest>::success(digest);
}

}  // namespace

Result<std::vector<std::byte>> canonical_state_bytes(
    const BookStore& store,
    const std::uint64_t last_mold_sequence)
{
    auto snapshot = store.canonical_snapshot();
    if (!snapshot) {
        return Result<std::vector<std::byte>>::failure(*snapshot.error());
    }
    return encode_snapshot(*snapshot.value(), last_mold_sequence);
}

Result<StateDigest> compute_state_digest(
    const BookStore& store,
    const std::uint64_t last_mold_sequence)
{
    auto snapshot = store.canonical_snapshot();
    if (!snapshot) {
        return Result<StateDigest>::failure(*snapshot.error());
    }

    auto bytes = encode_snapshot(*snapshot.value(), last_mold_sequence);
    if (!bytes) {
        return Result<StateDigest>::failure(*bytes.error());
    }
    return summarize(
        *snapshot.value(), fnv1a64(*bytes.value()), last_mold_sequence);
}

}  // namespace aegis
