#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace aegis {

inline constexpr std::size_t kItchCommonHeaderLength = 11;

inline constexpr std::size_t kSystemEventLength = 12;
inline constexpr std::size_t kStockDirectoryLength = 39;
inline constexpr std::size_t kAddOrderLength = 36;
inline constexpr std::size_t kAddOrderWithAttributionLength = 40;
inline constexpr std::size_t kOrderExecutedLength = 31;
inline constexpr std::size_t kOrderExecutedWithPriceLength = 36;
inline constexpr std::size_t kOrderCancelLength = 23;
inline constexpr std::size_t kOrderDeleteLength = 19;
inline constexpr std::size_t kOrderReplaceLength = 35;

inline constexpr std::size_t kStockTradingActionLength = 25;
inline constexpr std::size_t kRegShoRestrictionLength = 20;
inline constexpr std::size_t kMarketParticipantPositionLength = 26;
inline constexpr std::size_t kMwcbDeclineLevelLength = 35;
inline constexpr std::size_t kMwcbStatusLength = 12;
inline constexpr std::size_t kIpoQuotingPeriodUpdateLength = 28;
inline constexpr std::size_t kLuldAuctionCollarLength = 35;
inline constexpr std::size_t kOperationalHaltLength = 21;
inline constexpr std::size_t kTradeLength = 44;
inline constexpr std::size_t kCrossTradeLength = 40;
inline constexpr std::size_t kBrokenTradeLength = 19;
inline constexpr std::size_t kNetOrderImbalanceIndicatorLength = 50;
inline constexpr std::size_t kRetailPriceImprovementIndicatorLength = 20;
inline constexpr std::size_t kDirectListingPriceDiscoveryLength = 48;

enum class ItchTypeClass : std::uint8_t {
    Required,
    KnownBookNeutral,
    Unknown,
};

namespace detail {

struct ItchTypeDefinition {
    char type;
    std::size_t length;
    ItchTypeClass classification;
};

inline constexpr std::array<ItchTypeDefinition, 23> itch_type_definitions{{
    {'S', kSystemEventLength, ItchTypeClass::Required},
    {'R', kStockDirectoryLength, ItchTypeClass::Required},
    {'A', kAddOrderLength, ItchTypeClass::Required},
    {'F', kAddOrderWithAttributionLength, ItchTypeClass::Required},
    {'E', kOrderExecutedLength, ItchTypeClass::Required},
    {'C', kOrderExecutedWithPriceLength, ItchTypeClass::Required},
    {'X', kOrderCancelLength, ItchTypeClass::Required},
    {'D', kOrderDeleteLength, ItchTypeClass::Required},
    {'U', kOrderReplaceLength, ItchTypeClass::Required},
    {'H', kStockTradingActionLength, ItchTypeClass::KnownBookNeutral},
    {'Y', kRegShoRestrictionLength, ItchTypeClass::KnownBookNeutral},
    {'L', kMarketParticipantPositionLength, ItchTypeClass::KnownBookNeutral},
    {'V', kMwcbDeclineLevelLength, ItchTypeClass::KnownBookNeutral},
    {'W', kMwcbStatusLength, ItchTypeClass::KnownBookNeutral},
    {'K', kIpoQuotingPeriodUpdateLength, ItchTypeClass::KnownBookNeutral},
    {'J', kLuldAuctionCollarLength, ItchTypeClass::KnownBookNeutral},
    {'h', kOperationalHaltLength, ItchTypeClass::KnownBookNeutral},
    {'P', kTradeLength, ItchTypeClass::KnownBookNeutral},
    {'Q', kCrossTradeLength, ItchTypeClass::KnownBookNeutral},
    {'B', kBrokenTradeLength, ItchTypeClass::KnownBookNeutral},
    {'I', kNetOrderImbalanceIndicatorLength, ItchTypeClass::KnownBookNeutral},
    {'N', kRetailPriceImprovementIndicatorLength, ItchTypeClass::KnownBookNeutral},
    {'O', kDirectListingPriceDiscoveryLength, ItchTypeClass::KnownBookNeutral},
}};

}  // namespace detail

[[nodiscard]] constexpr ItchTypeClass classify_itch_type(const char type) noexcept
{
    for (const auto& definition : detail::itch_type_definitions) {
        if (definition.type == type) {
            return definition.classification;
        }
    }
    return ItchTypeClass::Unknown;
}

[[nodiscard]] constexpr std::optional<std::size_t> expected_itch_length(
    const char type) noexcept
{
    for (const auto& definition : detail::itch_type_definitions) {
        if (definition.type == type) {
            return definition.length;
        }
    }
    return std::nullopt;
}

static_assert(kItchCommonHeaderLength == 11);
static_assert(classify_itch_type('S') == ItchTypeClass::Required);
static_assert(classify_itch_type('H') == ItchTypeClass::KnownBookNeutral);
static_assert(classify_itch_type('\0') == ItchTypeClass::Unknown);
static_assert(expected_itch_length('S') == kSystemEventLength);
static_assert(!expected_itch_length('\0').has_value());

}  // namespace aegis
