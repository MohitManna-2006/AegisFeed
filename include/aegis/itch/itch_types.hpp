#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace aegis {

using Price4 = std::uint32_t;
inline constexpr Price4 kMaxPrice4 = 2'000'000'000U;

using StockSymbol = std::array<char, 8>;
using Attribution = std::array<char, 4>;
using IssueSubtype = std::array<char, 2>;

struct ItchCommonHeader {
    char type{};
    std::uint16_t stock_locate{};
    std::uint16_t tracking_number{};
    std::uint64_t timestamp_ns{};
};

using ItchHeader = ItchCommonHeader;

enum class Side : std::uint8_t {
    Buy = static_cast<std::uint8_t>('B'),
    Sell = static_cast<std::uint8_t>('S'),
};

enum class SystemEventCode : std::uint8_t {
    StartOfMessages = static_cast<std::uint8_t>('O'),
    StartOfSystemHours = static_cast<std::uint8_t>('S'),
    StartOfMarketHours = static_cast<std::uint8_t>('Q'),
    EndOfMarketHours = static_cast<std::uint8_t>('M'),
    EndOfSystemHours = static_cast<std::uint8_t>('E'),
    EndOfMessages = static_cast<std::uint8_t>('C'),
    EmergencyMarketConditionHalt = static_cast<std::uint8_t>('A'),
    EmergencyMarketConditionQuoteOnly = static_cast<std::uint8_t>('R'),
    EmergencyMarketConditionResumption = static_cast<std::uint8_t>('B'),
};

enum class MarketCategory : std::uint8_t {
    NotAvailable = static_cast<std::uint8_t>(' '),
    NasdaqGlobalSelect = static_cast<std::uint8_t>('Q'),
    NasdaqGlobalMarket = static_cast<std::uint8_t>('G'),
    NasdaqCapitalMarket = static_cast<std::uint8_t>('S'),
    NewYorkStockExchange = static_cast<std::uint8_t>('N'),
    NyseTexas = static_cast<std::uint8_t>('M'),
    TexasStockExchange = static_cast<std::uint8_t>('F'),
    NyseAmerican = static_cast<std::uint8_t>('A'),
    NyseArca = static_cast<std::uint8_t>('P'),
    CboeBzx = static_cast<std::uint8_t>('Z'),
    InvestorsExchange = static_cast<std::uint8_t>('V'),
};

enum class FinancialStatusIndicator : std::uint8_t {
    NotAvailable = static_cast<std::uint8_t>(' '),
    Deficient = static_cast<std::uint8_t>('D'),
    Delinquent = static_cast<std::uint8_t>('E'),
    Bankrupt = static_cast<std::uint8_t>('Q'),
    Suspended = static_cast<std::uint8_t>('S'),
    DeficientAndBankrupt = static_cast<std::uint8_t>('G'),
    DeficientAndDelinquent = static_cast<std::uint8_t>('H'),
    DelinquentAndBankrupt = static_cast<std::uint8_t>('J'),
    DeficientDelinquentAndBankrupt = static_cast<std::uint8_t>('K'),
    CreationsOrRedemptionsSuspended = static_cast<std::uint8_t>('C'),
    Normal = static_cast<std::uint8_t>('N'),
};

enum class RoundLotsOnly : std::uint8_t {
    No = static_cast<std::uint8_t>('N'),
    Yes = static_cast<std::uint8_t>('Y'),
};

enum class Authenticity : std::uint8_t {
    Production = static_cast<std::uint8_t>('P'),
    Test = static_cast<std::uint8_t>('T'),
};

enum class ShortSaleThresholdIndicator : std::uint8_t {
    NotAvailable = static_cast<std::uint8_t>(' '),
    NotRestricted = static_cast<std::uint8_t>('N'),
    Restricted = static_cast<std::uint8_t>('Y'),
};

enum class IpoFlag : std::uint8_t {
    NotAvailable = static_cast<std::uint8_t>(' '),
    NotIpo = static_cast<std::uint8_t>('N'),
    Ipo = static_cast<std::uint8_t>('Y'),
    NonIpoNewIssue = static_cast<std::uint8_t>('Z'),
};

enum class LuldReferencePriceTier : std::uint8_t {
    NotAvailable = static_cast<std::uint8_t>(' '),
    TierOne = static_cast<std::uint8_t>('1'),
    TierTwo = static_cast<std::uint8_t>('2'),
};

enum class EtpFlag : std::uint8_t {
    NotAvailable = static_cast<std::uint8_t>(' '),
    NotEtp = static_cast<std::uint8_t>('N'),
    Etp = static_cast<std::uint8_t>('Y'),
};

enum class InverseIndicator : std::uint8_t {
    NotInverse = static_cast<std::uint8_t>('N'),
    Inverse = static_cast<std::uint8_t>('Y'),
};

struct SystemEvent {
    ItchCommonHeader header{};
    SystemEventCode event_code{};
};

struct StockDirectory {
    ItchCommonHeader header{};
    StockSymbol stock{};
    MarketCategory market_category{};
    FinancialStatusIndicator financial_status_indicator{};
    std::uint32_t round_lot_size{};
    RoundLotsOnly round_lots_only{};
    char issue_classification{};
    IssueSubtype issue_subtype{};
    Authenticity authenticity{};
    ShortSaleThresholdIndicator short_sale_threshold_indicator{};
    IpoFlag ipo_flag{};
    LuldReferencePriceTier luld_reference_price_tier{};
    EtpFlag etp_flag{};
    std::uint32_t etp_leverage_factor{};
    InverseIndicator inverse_indicator{};
};

struct AddOrder {
    ItchCommonHeader header{};
    std::uint64_t order_reference{};
    Side side{};
    std::uint32_t shares{};
    StockSymbol stock{};
    Price4 price_1e4{};
    std::optional<Attribution> attribution{};
};

struct OrderExecuted {
    ItchCommonHeader header{};
    std::uint64_t order_reference{};
    std::uint32_t executed_shares{};
    std::uint64_t match_number{};
};

struct OrderExecutedWithPrice {
    ItchCommonHeader header{};
    std::uint64_t order_reference{};
    std::uint32_t executed_shares{};
    std::uint64_t match_number{};
    bool printable{};
    Price4 execution_price_1e4{};
};

struct OrderCancel {
    ItchCommonHeader header{};
    std::uint64_t order_reference{};
    std::uint32_t cancelled_shares{};
};

struct OrderDelete {
    ItchCommonHeader header{};
    std::uint64_t order_reference{};
};

struct OrderReplace {
    ItchCommonHeader header{};
    std::uint64_t original_order_reference{};
    std::uint64_t new_order_reference{};
    std::uint32_t shares{};
    Price4 price_1e4{};
};

struct KnownBookNeutral {
    ItchCommonHeader header{};
};

static_assert(sizeof(StockSymbol) == 8);
static_assert(sizeof(Attribution) == 4);
static_assert(sizeof(IssueSubtype) == 2);
static_assert(sizeof(Side) == sizeof(std::uint8_t));
static_assert(sizeof(SystemEventCode) == sizeof(std::uint8_t));
static_assert(std::is_same_v<Price4, std::uint32_t>);
static_assert(std::is_trivially_copyable_v<ItchCommonHeader>);
static_assert(std::is_nothrow_move_constructible_v<AddOrder>);
static_assert(std::is_nothrow_move_constructible_v<OrderExecuted>);
static_assert(std::is_nothrow_move_constructible_v<OrderExecutedWithPrice>);
static_assert(std::is_nothrow_move_constructible_v<OrderCancel>);
static_assert(std::is_nothrow_move_constructible_v<OrderDelete>);
static_assert(std::is_nothrow_move_constructible_v<OrderReplace>);
static_assert(std::is_nothrow_move_constructible_v<KnownBookNeutral>);

}  // namespace aegis
