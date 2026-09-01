#include "aegis/book/symbol_directory.hpp"

#include "aegis/common/error.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace aegis {
namespace {

constexpr std::size_t kStockSymbolWidth = 8;

[[nodiscard]] constexpr char ascii_uppercase(const char character) noexcept
{
    if (character >= 'a' && character <= 'z') {
        return static_cast<char>(character - 'a' + 'A');
    }
    return character;
}

[[nodiscard]] std::size_t right_trimmed_size(const std::string_view symbol) noexcept
{
    std::size_t size = symbol.size();
    while (size != 0 && symbol[size - 1] == ' ') {
        --size;
    }
    return size;
}

}  // namespace

Result<std::string> normalize_feed_symbol(const StockSymbol& symbol)
{
    for (std::size_t index = 0; index < symbol.size(); ++index) {
        if (symbol[index] == '\0') {
            return Result<std::string>::failure(
                Error::invalid_stock_symbol("feed stock symbol contains an embedded null", index));
        }
    }

    const std::string_view symbol_view{symbol.data(), symbol.size()};
    const auto normalized_size = right_trimmed_size(symbol_view);
    if (normalized_size == 0) {
        return Result<std::string>::failure(
            Error::invalid_stock_symbol("feed stock symbol is empty after normalization"));
    }

    return Result<std::string>::success(std::string{symbol_view.substr(0, normalized_size)});
}

Result<std::string> normalize_requested_symbol(const std::string_view symbol)
{
    if (symbol.size() > kStockSymbolWidth) {
        return Result<std::string>::failure(Error::invalid_requested_symbol(
            "requested symbol exceeds the ITCH stock-symbol width", symbol.size()));
    }

    for (std::size_t index = 0; index < symbol.size(); ++index) {
        if (symbol[index] == '\0') {
            return Result<std::string>::failure(Error::invalid_requested_symbol(
                "requested symbol contains an embedded null", symbol.size()));
        }
    }

    const auto normalized_size = right_trimmed_size(symbol);
    if (normalized_size == 0) {
        return Result<std::string>::failure(Error::invalid_requested_symbol(
            "requested symbol is empty after normalization", symbol.size()));
    }

    std::string normalized;
    normalized.reserve(normalized_size);
    for (std::size_t index = 0; index < normalized_size; ++index) {
        normalized.push_back(ascii_uppercase(symbol[index]));
    }
    return Result<std::string>::success(std::move(normalized));
}

Result<SymbolDirectory> SymbolDirectory::create(
    const std::span<const std::string_view> requested_symbols)
{
    SymbolDirectory directory;
    for (const auto requested_symbol : requested_symbols) {
        auto normalized = normalize_requested_symbol(requested_symbol);
        if (!normalized) {
            return Result<SymbolDirectory>::failure(*normalized.error());
        }
        directory.requested_symbols_.try_emplace(
            std::move(*normalized.value()), std::nullopt);
    }
    return Result<SymbolDirectory>::success(std::move(directory));
}

Result<void> SymbolDirectory::observe(const StockDirectory& directory)
{
    auto normalized = normalize_feed_symbol(directory.stock);
    if (!normalized) {
        return Result<void>::failure(*normalized.error());
    }

    const auto stock_locate = static_cast<StockLocate>(directory.header.stock_locate);
    const auto existing = symbols_by_locate_.find(stock_locate);
    if (existing != symbols_by_locate_.end()) {
        if (existing->second == *normalized.value()) {
            return Result<void>::success();
        }
        return Result<void>::failure(Error::conflicting_stock_locate(stock_locate));
    }

    const auto requested = requested_symbols_.find(*normalized.value());
    const bool is_requested = requested != requested_symbols_.end();

    symbols_by_locate_.emplace(stock_locate, std::move(*normalized.value()));
    if (is_requested) {
        if (!requested->second.has_value()) {
            requested->second = stock_locate;
            ++discovered_requested_symbol_count_;
        }
    }

    return Result<void>::success();
}

std::size_t SymbolDirectory::known_locate_count() const noexcept
{
    return symbols_by_locate_.size();
}

std::size_t SymbolDirectory::requested_symbol_count() const noexcept
{
    return requested_symbols_.size();
}

std::size_t SymbolDirectory::discovered_requested_symbol_count() const noexcept
{
    return discovered_requested_symbol_count_;
}

std::optional<std::string> SymbolDirectory::symbol_for(const StockLocate stock_locate) const
{
    const auto found = symbols_by_locate_.find(stock_locate);
    if (found == symbols_by_locate_.end()) {
        return std::nullopt;
    }
    return found->second;
}

bool SymbolDirectory::is_known(const StockLocate stock_locate) const noexcept
{
    return symbols_by_locate_.contains(stock_locate);
}

bool SymbolDirectory::is_selected(const StockLocate stock_locate) const noexcept
{
    const auto known = symbols_by_locate_.find(stock_locate);
    return known != symbols_by_locate_.end() && requested_symbols_.contains(known->second);
}

Result<bool> SymbolDirectory::requested_symbol_discovered(
    const std::string_view requested_symbol) const
{
    auto normalized = normalize_requested_symbol(requested_symbol);
    if (!normalized) {
        return Result<bool>::failure(*normalized.error());
    }

    const auto found = requested_symbols_.find(*normalized.value());
    return Result<bool>::success(
        found != requested_symbols_.end() && found->second.has_value());
}

Result<std::optional<StockLocate>> SymbolDirectory::discovered_locate(
    const std::string_view requested_symbol) const
{
    auto normalized = normalize_requested_symbol(requested_symbol);
    if (!normalized) {
        return Result<std::optional<StockLocate>>::failure(*normalized.error());
    }

    const auto found = requested_symbols_.find(*normalized.value());
    if (found == requested_symbols_.end()) {
        return Result<std::optional<StockLocate>>::success(std::nullopt);
    }
    return Result<std::optional<StockLocate>>::success(found->second);
}

bool SymbolDirectory::all_requested_symbols_discovered() const noexcept
{
    return discovered_requested_symbol_count_ == requested_symbols_.size();
}

}  // namespace aegis
