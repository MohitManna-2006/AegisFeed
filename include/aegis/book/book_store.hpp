#pragma once

#include "aegis/book/order_book.hpp"
#include "aegis/book/symbol_directory.hpp"
#include "aegis/common/result.hpp"
#include "aegis/itch/itch_types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>

namespace aegis {

enum class BookRouteStatus : std::uint8_t {
    Applied,
    KnownUnselected,
};

class BookStore {
public:
    [[nodiscard]] static Result<BookStore> create(
        std::span<const std::string_view> requested_symbols,
        std::size_t expected_active_order_capacity_per_book = 0);

    [[nodiscard]] Result<void> observe(const StockDirectory& directory);
    [[nodiscard]] Result<BookRouteStatus> add(const AddOrder& message);

    [[nodiscard]] std::size_t book_count() const noexcept;
    [[nodiscard]] bool has_book(StockLocate stock_locate) const noexcept;
    [[nodiscard]] const OrderBook* book(StockLocate stock_locate) const noexcept;
    [[nodiscard]] const SymbolDirectory& symbol_directory() const noexcept;

private:
    using BooksByLocate = std::unordered_map<StockLocate, OrderBook>;

    BookStore(
        SymbolDirectory symbol_directory,
        std::size_t expected_active_order_capacity_per_book);

    SymbolDirectory symbol_directory_;
    BooksByLocate books_by_locate_{};
    std::size_t expected_active_order_capacity_per_book_{};
};

}  // namespace aegis
