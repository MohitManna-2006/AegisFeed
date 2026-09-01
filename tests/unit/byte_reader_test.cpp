#include "aegis/common/byte_reader.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>

namespace {

std::size_t failure_count = 0;

void check(const bool condition, const std::string_view expression, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << "byte_reader_test.cpp:" << line << ": check failed: " << expression << '\n';
    ++failure_count;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename T>
[[nodiscard]] bool result_equals(const aegis::Result<T>& result, const T expected)
{
    const T* const value = result.value();
    return value != nullptr && *value == expected;
}

template <typename T>
void check_truncated_result(
    const aegis::Result<T>& result,
    const aegis::ByteReader& reader,
    const std::size_t requested,
    const std::size_t available)
{
    CHECK(!result.has_value());
    CHECK(result.value() == nullptr);
    CHECK(reader.position() == 0);
    CHECK(reader.remaining() == available);

    const aegis::Error* const error = result.error();
    CHECK(error != nullptr);
    if (error == nullptr) {
        return;
    }

    CHECK(error->category == aegis::ErrorCategory::InputFraming);
    CHECK(error->code == aegis::ErrorCode::ReadPastEnd);
    CHECK(error->offset == 0);
    CHECK(error->requested_size == requested);
    CHECK(error->available_size == available);
    CHECK(!error->message.empty());
}

void test_empty_input()
{
    const std::array<std::byte, 0> bytes{};
    aegis::ByteReader reader{bytes};

    CHECK(reader.position() == 0);
    CHECK(reader.remaining() == 0);
    CHECK(reader.empty());

    const auto result = reader.read_u8();
    check_truncated_result(result, reader, 1, 0);
}

void test_exact_boundary_reads()
{
    {
        const std::array bytes{std::byte{0xAB}};
        aegis::ByteReader reader{bytes};
        const auto result = reader.read_u8();
        CHECK(result_equals(result, std::uint8_t{0xAB}));
        CHECK(result.error() == nullptr);
        CHECK(reader.position() == bytes.size());
        CHECK(reader.remaining() == 0);
    }

    {
        const std::array bytes{std::byte{0x12}, std::byte{0x34}};
        aegis::ByteReader reader{bytes};
        const auto result = reader.read_u16_be();
        CHECK(result_equals(result, std::uint16_t{0x1234}));
        CHECK(reader.position() == bytes.size());
        CHECK(reader.empty());
    }

    {
        const std::array bytes{
            std::byte{0x01}, std::byte{0x23}, std::byte{0x45}, std::byte{0x67}};
        aegis::ByteReader reader{bytes};
        const auto result = reader.read_u32_be();
        CHECK(result_equals(result, std::uint32_t{0x01234567}));
        CHECK(reader.position() == bytes.size());
        CHECK(reader.empty());
    }

    {
        const std::array bytes{
            std::byte{0x01},
            std::byte{0x23},
            std::byte{0x45},
            std::byte{0x67},
            std::byte{0x89},
            std::byte{0xAB},
        };
        aegis::ByteReader reader{bytes};
        const auto result = reader.read_u48_be();
        CHECK(result_equals(result, std::uint64_t{0x0123456789ABULL}));
        CHECK(reader.position() == bytes.size());
        CHECK(reader.empty());
    }

    {
        const std::array bytes{
            std::byte{0x01},
            std::byte{0x23},
            std::byte{0x45},
            std::byte{0x67},
            std::byte{0x89},
            std::byte{0xAB},
            std::byte{0xCD},
            std::byte{0xEF},
        };
        aegis::ByteReader reader{bytes};
        const auto result = reader.read_u64_be();
        CHECK(result_equals(result, std::uint64_t{0x0123456789ABCDEFULL}));
        CHECK(reader.position() == bytes.size());
        CHECK(reader.empty());
    }
}

void test_sequential_reads_and_positions()
{
    const std::array bytes{
        std::byte{0x7F},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
        std::byte{0x06},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
        std::byte{0x06},
        std::byte{0x07},
        std::byte{0x08},
    };
    aegis::ByteReader reader{bytes};

    CHECK(reader.position() == 0);
    CHECK(reader.remaining() == bytes.size());

    CHECK(result_equals(reader.read_u8(), std::uint8_t{0x7F}));
    CHECK(reader.position() == 1);
    CHECK(reader.remaining() == 20);

    CHECK(result_equals(reader.read_u16_be(), std::uint16_t{0x0102}));
    CHECK(reader.position() == 3);

    CHECK(result_equals(reader.read_u32_be(), std::uint32_t{0x01020304}));
    CHECK(reader.position() == 7);

    CHECK(result_equals(reader.read_u48_be(), std::uint64_t{0x010203040506ULL}));
    CHECK(reader.position() == 13);

    CHECK(result_equals(reader.read_u64_be(), std::uint64_t{0x0102030405060708ULL}));
    CHECK(reader.position() == bytes.size());
    CHECK(reader.remaining() == 0);
    CHECK(reader.empty());
}

void test_zero_values()
{
    const std::array<std::byte, 21> bytes{};
    aegis::ByteReader reader{bytes};

    CHECK(result_equals(reader.read_u8(), std::uint8_t{0}));
    CHECK(result_equals(reader.read_u16_be(), std::uint16_t{0}));
    CHECK(result_equals(reader.read_u32_be(), std::uint32_t{0}));
    CHECK(result_equals(reader.read_u48_be(), std::uint64_t{0}));
    CHECK(result_equals(reader.read_u64_be(), std::uint64_t{0}));
    CHECK(reader.empty());
}

void test_truncation_for_every_width()
{
    const std::array<std::byte, 7> storage{};

    aegis::ByteReader u8_reader{std::span{storage}.first(0)};
    check_truncated_result(u8_reader.read_u8(), u8_reader, 1, 0);

    aegis::ByteReader u16_reader{std::span{storage}.first(1)};
    check_truncated_result(u16_reader.read_u16_be(), u16_reader, 2, 1);

    aegis::ByteReader u32_reader{std::span{storage}.first(3)};
    check_truncated_result(u32_reader.read_u32_be(), u32_reader, 4, 3);

    aegis::ByteReader u48_reader{std::span{storage}.first(5)};
    check_truncated_result(u48_reader.read_u48_be(), u48_reader, 6, 5);

    aegis::ByteReader u64_reader{std::span{storage}.first(7)};
    check_truncated_result(u64_reader.read_u64_be(), u64_reader, 8, 7);
}

void test_bounded_byte_spans()
{
    const std::array bytes{
        std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40}};
    aegis::ByteReader reader{bytes};

    CHECK(result_equals(reader.read_u8(), std::uint8_t{0x10}));
    CHECK(reader.position() == 1);

    const auto span_result = reader.read_bytes(2);
    CHECK(span_result.has_value());
    const std::span<const std::byte>* const byte_span = span_result.value();
    CHECK(byte_span != nullptr);
    if (byte_span != nullptr) {
        CHECK(byte_span->data() == bytes.data() + 1);
        CHECK(byte_span->size() == 2);
        CHECK((*byte_span)[0] == std::byte{0x20});
        CHECK((*byte_span)[1] == std::byte{0x30});
    }
    CHECK(reader.position() == 3);
    CHECK(reader.remaining() == 1);

    const auto oversized = reader.read_bytes(2);
    CHECK(!oversized.has_value());
    CHECK(reader.position() == 3);
    CHECK(reader.remaining() == 1);
    const aegis::Error* const error = oversized.error();
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->offset == 3);
        CHECK(error->requested_size == 2);
        CHECK(error->available_size == 1);
    }

    const auto final_byte = reader.read_bytes(1);
    CHECK(final_byte.has_value());
    CHECK(reader.position() == bytes.size());
    CHECK(reader.empty());
}

void test_oversized_request_does_not_overflow()
{
    const std::array bytes{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    aegis::ByteReader reader{bytes};

    const auto result = reader.read_bytes(std::numeric_limits<std::size_t>::max());
    CHECK(!result.has_value());
    CHECK(reader.position() == 0);
    CHECK(reader.remaining() == bytes.size());

    const aegis::Error* const error = result.error();
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->requested_size == std::numeric_limits<std::size_t>::max());
        CHECK(error->available_size == bytes.size());
    }
}

void test_cursor_stability_after_failed_read()
{
    const std::array bytes{std::byte{0xAA}, std::byte{0xBB}};
    aegis::ByteReader reader{bytes};

    CHECK(result_equals(reader.read_u8(), std::uint8_t{0xAA}));
    CHECK(reader.position() == 1);

    const auto result = reader.read_u16_be();
    CHECK(!result.has_value());
    CHECK(reader.position() == 1);
    CHECK(reader.remaining() == 1);

    const aegis::Error* const error = result.error();
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->offset == 1);
        CHECK(error->requested_size == 2);
        CHECK(error->available_size == 1);
    }
}

void test_high_bit_bytes()
{
    const std::array<std::byte, 21> bytes{
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
    };
    aegis::ByteReader reader{bytes};

    CHECK(result_equals(reader.read_u8(), std::numeric_limits<std::uint8_t>::max()));
    CHECK(result_equals(reader.read_u16_be(), std::numeric_limits<std::uint16_t>::max()));
    CHECK(result_equals(reader.read_u32_be(), std::numeric_limits<std::uint32_t>::max()));
    CHECK(result_equals(reader.read_u48_be(), std::uint64_t{0x0000FFFFFFFFFFFFULL}));
    CHECK(result_equals(reader.read_u64_be(), std::numeric_limits<std::uint64_t>::max()));
    CHECK(reader.empty());
}

void test_offset_primitive_api()
{
    const std::array bytes{
        std::byte{0xEE},
        std::byte{0x80},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
        std::byte{0x06},
        std::byte{0x07},
    };

    std::uint8_t u8 = 0;
    std::uint16_t u16 = 0;
    std::uint32_t u32 = 0;
    std::uint64_t u48 = 0;
    std::uint64_t u64 = 0;

    CHECK(aegis::read_u8(bytes, 1, u8));
    CHECK(u8 == std::uint8_t{0x80});
    CHECK(aegis::read_u16_be(bytes, 1, u16));
    CHECK(u16 == std::uint16_t{0x8001});
    CHECK(aegis::read_u32_be(bytes, 1, u32));
    CHECK(u32 == std::uint32_t{0x80010203});
    CHECK(aegis::read_u48_be(bytes, 1, u48));
    CHECK(u48 == std::uint64_t{0x800102030405ULL});
    CHECK(aegis::read_u64_be(bytes, 1, u64));
    CHECK(u64 == std::uint64_t{0x8001020304050607ULL});

    u64 = std::uint64_t{0xDEADBEEF};
    CHECK(!aegis::read_u64_be(bytes, 2, u64));
    CHECK(u64 == std::uint64_t{0xDEADBEEF});

    CHECK(!aegis::read_u8(bytes, std::numeric_limits<std::size_t>::max(), u8));
    CHECK(u8 == std::uint8_t{0x80});
}

}  // namespace

int main()
{
    test_empty_input();
    test_exact_boundary_reads();
    test_sequential_reads_and_positions();
    test_zero_values();
    test_truncation_for_every_width();
    test_bounded_byte_spans();
    test_oversized_request_does_not_overflow();
    test_cursor_stability_after_failed_read();
    test_high_bit_bytes();
    test_offset_primitive_api();

    if (failure_count != 0) {
        std::cerr << failure_count << " ByteReader check(s) failed\n";
        return 1;
    }

    return 0;
}
