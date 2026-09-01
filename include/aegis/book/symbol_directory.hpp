#pragma once

#include "aegis/book/order_book.hpp"
#include "aegis/common/result.hpp"
#include "aegis/itch/itch_types.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace aegis {

[[nodiscard]] Result<std::string> normalize_feed_symbol(const StockSymbol& symbol);
[[nodiscard]] Result<std::string> normalize_requested_symbol(std::string_view symbol);

class SymbolDirectory {
public:
    [[nodiscard]] static Result<SymbolDirectory> create(
        std::span<const std::string_view> requested_symbols);

    [[nodiscard]] Result<void> observe(const StockDirectory& directory);

    [[nodiscard]] std::size_t known_locate_count() const noexcept;
    [[nodiscard]] std::size_t requested_symbol_count() const noexcept;
    [[nodiscard]] std::size_t discovered_requested_symbol_count() const noexcept;

    [[nodiscard]] std::optional<std::string> symbol_for(StockLocate stock_locate) const;
    [[nodiscard]] bool is_known(StockLocate stock_locate) const noexcept;
    [[nodiscard]] bool is_selected(StockLocate stock_locate) const noexcept;

    [[nodiscard]] Result<bool> requested_symbol_discovered(
        std::string_view requested_symbol) const;
    [[nodiscard]] Result<std::optional<StockLocate>> discovered_locate(
        std::string_view requested_symbol) const;
    [[nodiscard]] bool all_requested_symbols_discovered() const noexcept;

private:
    using LocateSymbols = std::map<StockLocate, std::string>;
    using RequestedSymbols =
        std::map<std::string, std::optional<StockLocate>, std::less<>>;

    SymbolDirectory() = default;

    LocateSymbols symbols_by_locate_{};
    RequestedSymbols requested_symbols_{};
    std::size_t discovered_requested_symbol_count_{};
};

}  // namespace aegis
