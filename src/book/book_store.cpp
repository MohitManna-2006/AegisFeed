#include "aegis/book/book_store.hpp"

#include "aegis/common/error.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

namespace aegis {

BookStore::BookStore(
    SymbolDirectory symbol_directory,
    const std::size_t expected_active_order_capacity_per_book)
    : symbol_directory_{std::move(symbol_directory)},
      expected_active_order_capacity_per_book_{expected_active_order_capacity_per_book}
{
    books_by_locate_.reserve(symbol_directory_.requested_symbol_count());
}

Result<BookStore> BookStore::create(
    const std::span<const std::string_view> requested_symbols,
    const std::size_t expected_active_order_capacity_per_book)
{
    auto symbol_directory = SymbolDirectory::create(requested_symbols);
    if (!symbol_directory) {
        return Result<BookStore>::failure(*symbol_directory.error());
    }

    return Result<BookStore>::success(BookStore{
        std::move(*symbol_directory.value()), expected_active_order_capacity_per_book});
}

Result<void> BookStore::observe(const StockDirectory& directory)
{
    const auto observation = symbol_directory_.observe(directory);
    if (!observation) {
        return observation;
    }

    const auto stock_locate = static_cast<StockLocate>(directory.header.stock_locate);
    if (!symbol_directory_.is_selected(stock_locate)) {
        return Result<void>::success();
    }

    books_by_locate_.try_emplace(
        stock_locate, stock_locate, expected_active_order_capacity_per_book_);
    return Result<void>::success();
}

Result<BookRouteStatus> BookStore::add(const AddOrder& message)
{
    auto selected_book = resolve_book(message.header.stock_locate);
    if (!selected_book) {
        return Result<BookRouteStatus>::failure(*selected_book.error());
    }
    if (*selected_book.value() == nullptr) {
        return Result<BookRouteStatus>::success(BookRouteStatus::KnownUnselected);
    }

    const auto result = (*selected_book.value())->add(message);
    if (!result) {
        return Result<BookRouteStatus>::failure(*result.error());
    }
    return Result<BookRouteStatus>::success(BookRouteStatus::Applied);
}

Result<BookRouteStatus> BookStore::execute(const OrderExecuted& message)
{
    auto selected_book = resolve_book(message.header.stock_locate);
    if (!selected_book) {
        return Result<BookRouteStatus>::failure(*selected_book.error());
    }
    if (*selected_book.value() == nullptr) {
        return Result<BookRouteStatus>::success(BookRouteStatus::KnownUnselected);
    }

    const auto result = (*selected_book.value())->execute(message);
    if (!result) {
        return Result<BookRouteStatus>::failure(*result.error());
    }
    return Result<BookRouteStatus>::success(BookRouteStatus::Applied);
}

Result<BookRouteStatus> BookStore::execute_with_price(
    const OrderExecutedWithPrice& message)
{
    auto selected_book = resolve_book(message.header.stock_locate);
    if (!selected_book) {
        return Result<BookRouteStatus>::failure(*selected_book.error());
    }
    if (*selected_book.value() == nullptr) {
        return Result<BookRouteStatus>::success(BookRouteStatus::KnownUnselected);
    }

    const auto result = (*selected_book.value())->execute_with_price(message);
    if (!result) {
        return Result<BookRouteStatus>::failure(*result.error());
    }
    return Result<BookRouteStatus>::success(BookRouteStatus::Applied);
}

Result<BookRouteStatus> BookStore::cancel(const OrderCancel& message)
{
    auto selected_book = resolve_book(message.header.stock_locate);
    if (!selected_book) {
        return Result<BookRouteStatus>::failure(*selected_book.error());
    }
    if (*selected_book.value() == nullptr) {
        return Result<BookRouteStatus>::success(BookRouteStatus::KnownUnselected);
    }

    const auto result = (*selected_book.value())->cancel(message);
    if (!result) {
        return Result<BookRouteStatus>::failure(*result.error());
    }
    return Result<BookRouteStatus>::success(BookRouteStatus::Applied);
}

Result<BookRouteStatus> BookStore::delete_order(const OrderDelete& message)
{
    auto selected_book = resolve_book(message.header.stock_locate);
    if (!selected_book) {
        return Result<BookRouteStatus>::failure(*selected_book.error());
    }
    if (*selected_book.value() == nullptr) {
        return Result<BookRouteStatus>::success(BookRouteStatus::KnownUnselected);
    }

    const auto result = (*selected_book.value())->delete_order(message);
    if (!result) {
        return Result<BookRouteStatus>::failure(*result.error());
    }
    return Result<BookRouteStatus>::success(BookRouteStatus::Applied);
}

Result<BookRouteStatus> BookStore::replace(const OrderReplace& message)
{
    auto selected_book = resolve_book(message.header.stock_locate);
    if (!selected_book) {
        return Result<BookRouteStatus>::failure(*selected_book.error());
    }
    if (*selected_book.value() == nullptr) {
        return Result<BookRouteStatus>::success(BookRouteStatus::KnownUnselected);
    }

    const auto result = (*selected_book.value())->replace(message);
    if (!result) {
        return Result<BookRouteStatus>::failure(*result.error());
    }
    return Result<BookRouteStatus>::success(BookRouteStatus::Applied);
}

Result<OrderBook*> BookStore::resolve_book(const StockLocate stock_locate)
{
    if (!symbol_directory_.is_known(stock_locate)) {
        return Result<OrderBook*>::failure(Error::unknown_stock_locate(stock_locate));
    }
    if (!symbol_directory_.is_selected(stock_locate)) {
        return Result<OrderBook*>::success(nullptr);
    }

    const auto selected_book = books_by_locate_.find(stock_locate);
    if (selected_book == books_by_locate_.end()) {
        return Result<OrderBook*>::failure(Error::book_invariant_violation(
            "selected stock locate has no owned order book", stock_locate, 0));
    }
    return Result<OrderBook*>::success(&selected_book->second);
}

Result<CanonicalBookStore> BookStore::canonical_snapshot() const
{
    CanonicalBookStore snapshot;
    snapshot.books.reserve(books_by_locate_.size());

    for (const auto& [stock_locate, book] : books_by_locate_) {
        const auto validity = book.validate_invariants();
        if (!validity) {
            return Result<CanonicalBookStore>::failure(*validity.error());
        }
        if (book.stock_locate() != stock_locate) {
            return Result<CanonicalBookStore>::failure(Error::book_invariant_violation(
                "owned book key does not match book stock locate",
                book.stock_locate(),
                stock_locate));
        }
        if (!symbol_directory_.is_selected(stock_locate)) {
            return Result<CanonicalBookStore>::failure(Error::book_invariant_violation(
                "owned book stock locate is not selected", stock_locate, 0));
        }

        auto symbol = symbol_directory_.symbol_for(stock_locate);
        if (!symbol.has_value()) {
            return Result<CanonicalBookStore>::failure(Error::book_invariant_violation(
                "owned book stock locate has no directory symbol", stock_locate, 0));
        }
        snapshot.books.push_back(
            CanonicalBook{std::move(*symbol), book.canonical_snapshot()});
    }

    std::ranges::sort(snapshot.books, {}, &CanonicalBook::symbol);
    return Result<CanonicalBookStore>::success(std::move(snapshot));
}

std::size_t BookStore::book_count() const noexcept
{
    return books_by_locate_.size();
}

bool BookStore::has_book(const StockLocate stock_locate) const noexcept
{
    return books_by_locate_.contains(stock_locate);
}

const OrderBook* BookStore::book(const StockLocate stock_locate) const noexcept
{
    const auto found = books_by_locate_.find(stock_locate);
    if (found == books_by_locate_.end()) {
        return nullptr;
    }
    return &found->second;
}

const SymbolDirectory& BookStore::symbol_directory() const noexcept
{
    return symbol_directory_;
}

}  // namespace aegis
