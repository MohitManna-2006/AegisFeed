#pragma once

#include "aegis/book/book_store.hpp"
#include "aegis/common/result.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aegis {

struct StateDigest {
    std::uint64_t hash{};
    std::uint64_t symbol_count{};
    std::uint64_t active_order_count{};
    std::uint64_t bid_level_count{};
    std::uint64_t ask_level_count{};
    std::uint64_t aggregate_displayed_shares{};
    std::uint64_t last_mold_sequence{};

    [[nodiscard]] bool operator==(const StateDigest&) const = default;
};

// Canonical format version 1:
//   7 bytes "AEGISBK", u8 version, u64 last Mold sequence, u32 book count;
//   per book: u8 symbol length, symbol bytes, u16 locate, u32 bid count,
//   bid records, u32 ask count, ask records, u64 active-order count, orders.
//   level record: u32 price, u64 aggregate shares, u32 order count.
//   order record: u64 id, u16 locate, u8 side, u32 price, u32 remaining,
//                 u8 has-attribution, 4 attribution bytes.
// Every integer is unsigned and big-endian. Attribution bytes are always
// present so an unattributed order differs from attributed zero bytes.
[[nodiscard]] Result<std::vector<std::byte>> canonical_state_bytes(
    const BookStore& store,
    std::uint64_t last_mold_sequence);

// FNV-1a 64-bit over canonical_state_bytes(), using offset basis
// 14695981039346656037 and prime 1099511628211.
[[nodiscard]] Result<StateDigest> compute_state_digest(
    const BookStore& store,
    std::uint64_t last_mold_sequence);

}  // namespace aegis
