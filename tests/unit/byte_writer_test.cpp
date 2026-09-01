#include "aegis/common/byte_reader.hpp"
#include "aegis/common/byte_writer.hpp"

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

    std::cerr << "byte_writer_test.cpp:" << line << ": check failed: " << expression << '\n';
    ++failure_count;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

void check_success(const aegis::Result<void>& result)
{
    CHECK(result.has_value());
    CHECK(static_cast<bool>(result));
    CHECK(result.error() == nullptr);
}

void check_capacity_failure(
    const aegis::Result<void>& result,
    const aegis::ByteWriter& writer,
    const std::size_t expected_position,
    const std::size_t requested,
    const std::size_t available)
{
    CHECK(!result.has_value());
    CHECK(!static_cast<bool>(result));
    CHECK(writer.position() == expected_position);
    CHECK(writer.remaining() == available);

    const aegis::Error* const error = result.error();
    CHECK(error != nullptr);
    if (error == nullptr) {
        return;
    }

    CHECK(error->category == aegis::ErrorCategory::ResourceLimit);
    CHECK(error->code == aegis::ErrorCode::WritePastEnd);
    CHECK(error->offset == expected_position);
    CHECK(error->requested_size == requested);
    CHECK(error->available_size == available);
    CHECK(!error->message.empty());
}

template <std::size_t Size, typename WriteOperation>
void check_truncated_integer_write(
    const std::size_t requested,
    WriteOperation write_operation)
{
    std::array<std::byte, Size> destination{};
    destination.fill(std::byte{0xA5});
    const auto original = destination;
    aegis::ByteWriter writer{destination};

    const auto result = write_operation(writer);
    check_capacity_failure(result, writer, 0, requested, Size);
    CHECK(destination == original);
}

void test_known_integer_output_and_exact_capacity()
{
    {
        std::array<std::byte, 1> destination{};
        aegis::ByteWriter writer{destination};
        check_success(writer.write_u8(std::uint8_t{0xAB}));
        CHECK(destination == std::array{std::byte{0xAB}});
        CHECK(writer.position() == destination.size());
        CHECK(writer.remaining() == 0);
    }

    {
        std::array<std::byte, 2> destination{};
        aegis::ByteWriter writer{destination};
        check_success(writer.write_u16_be(std::uint16_t{0x1234}));
        CHECK(destination == (std::array{std::byte{0x12}, std::byte{0x34}}));
        CHECK(writer.position() == destination.size());
        CHECK(writer.remaining() == 0);
    }

    {
        std::array<std::byte, 4> destination{};
        aegis::ByteWriter writer{destination};
        check_success(writer.write_u32_be(std::uint32_t{0x01234567}));
        CHECK(destination == (std::array{
                                 std::byte{0x01},
                                 std::byte{0x23},
                                 std::byte{0x45},
                                 std::byte{0x67},
                             }));
        CHECK(writer.position() == destination.size());
        CHECK(writer.remaining() == 0);
    }

    {
        std::array<std::byte, 6> destination{};
        aegis::ByteWriter writer{destination};
        check_success(writer.write_u48_be(std::uint64_t{0x0123456789ABULL}));
        CHECK(destination == (std::array{
                                 std::byte{0x01},
                                 std::byte{0x23},
                                 std::byte{0x45},
                                 std::byte{0x67},
                                 std::byte{0x89},
                                 std::byte{0xAB},
                             }));
        CHECK(writer.position() == destination.size());
        CHECK(writer.remaining() == 0);
    }

    {
        std::array<std::byte, 8> destination{};
        aegis::ByteWriter writer{destination};
        check_success(writer.write_u64_be(std::uint64_t{0x0123456789ABCDEFULL}));
        CHECK(destination == (std::array{
                                 std::byte{0x01},
                                 std::byte{0x23},
                                 std::byte{0x45},
                                 std::byte{0x67},
                                 std::byte{0x89},
                                 std::byte{0xAB},
                                 std::byte{0xCD},
                                 std::byte{0xEF},
                             }));
        CHECK(writer.position() == destination.size());
        CHECK(writer.remaining() == 0);
    }
}

void test_zero_and_maximum_values()
{
    std::array<std::byte, 21> zero_destination{};
    zero_destination.fill(std::byte{0xA5});
    aegis::ByteWriter zero_writer{zero_destination};

    check_success(zero_writer.write_u8(0));
    check_success(zero_writer.write_u16_be(0));
    check_success(zero_writer.write_u32_be(0));
    check_success(zero_writer.write_u48_be(0));
    check_success(zero_writer.write_u64_be(0));
    CHECK(zero_destination == (std::array<std::byte, 21>{}));
    CHECK(zero_writer.position() == zero_destination.size());

    std::array<std::byte, 21> max_destination{};
    aegis::ByteWriter max_writer{max_destination};

    check_success(max_writer.write_u8(std::numeric_limits<std::uint8_t>::max()));
    check_success(max_writer.write_u16_be(std::numeric_limits<std::uint16_t>::max()));
    check_success(max_writer.write_u32_be(std::numeric_limits<std::uint32_t>::max()));
    check_success(max_writer.write_u48_be(std::uint64_t{0x0000FFFFFFFFFFFFULL}));
    check_success(max_writer.write_u64_be(std::numeric_limits<std::uint64_t>::max()));

    std::array<std::byte, 21> expected_max{};
    expected_max.fill(std::byte{0xFF});
    CHECK(max_destination == expected_max);
    CHECK(max_writer.position() == max_destination.size());
}

void test_high_bit_values()
{
    std::array<std::byte, 21> destination{};
    aegis::ByteWriter writer{destination};

    check_success(writer.write_u8(std::uint8_t{0x80}));
    check_success(writer.write_u16_be(std::uint16_t{0x8001}));
    check_success(writer.write_u32_be(std::uint32_t{0x80010203}));
    check_success(writer.write_u48_be(std::uint64_t{0x800102030405ULL}));
    check_success(writer.write_u64_be(std::uint64_t{0x8001020304050607ULL}));

    const std::array expected{
        std::byte{0x80},
        std::byte{0x80},
        std::byte{0x01},
        std::byte{0x80},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x80},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
        std::byte{0x80},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
        std::byte{0x06},
        std::byte{0x07},
    };
    CHECK(destination == expected);
}

void test_sequential_writes_and_round_trip()
{
    const std::array trailing_bytes{std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}};
    std::array<std::byte, 24> destination{};
    destination.fill(std::byte{0x5A});
    aegis::ByteWriter writer{destination};

    CHECK(writer.position() == 0);
    CHECK(writer.remaining() == destination.size());

    check_success(writer.write_u8(std::uint8_t{0x7F}));
    CHECK(writer.position() == 1);
    check_success(writer.write_u16_be(std::uint16_t{0x0102}));
    CHECK(writer.position() == 3);
    check_success(writer.write_u32_be(std::uint32_t{0x01020304}));
    CHECK(writer.position() == 7);
    check_success(writer.write_u48_be(std::uint64_t{0x010203040506ULL}));
    CHECK(writer.position() == 13);
    check_success(writer.write_u64_be(std::uint64_t{0x0102030405060708ULL}));
    CHECK(writer.position() == 21);
    check_success(writer.write_bytes(trailing_bytes));
    CHECK(writer.position() == destination.size());
    CHECK(writer.remaining() == 0);

    const std::array expected{
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
        std::byte{0xAA},
        std::byte{0xBB},
        std::byte{0xCC},
    };
    CHECK(destination == expected);

    aegis::ByteReader reader{destination};
    const auto u8 = reader.read_u8();
    const auto u16 = reader.read_u16_be();
    const auto u32 = reader.read_u32_be();
    const auto u48 = reader.read_u48_be();
    const auto u64 = reader.read_u64_be();
    const auto bytes = reader.read_bytes(trailing_bytes.size());

    CHECK(u8.value() != nullptr && *u8.value() == std::uint8_t{0x7F});
    CHECK(u16.value() != nullptr && *u16.value() == std::uint16_t{0x0102});
    CHECK(u32.value() != nullptr && *u32.value() == std::uint32_t{0x01020304});
    CHECK(u48.value() != nullptr && *u48.value() == std::uint64_t{0x010203040506ULL});
    CHECK(u64.value() != nullptr && *u64.value() == std::uint64_t{0x0102030405060708ULL});
    CHECK(bytes.value() != nullptr);
    if (bytes.value() != nullptr) {
        CHECK(bytes.value()->size() == trailing_bytes.size());
        CHECK((*bytes.value())[0] == trailing_bytes[0]);
        CHECK((*bytes.value())[1] == trailing_bytes[1]);
        CHECK((*bytes.value())[2] == trailing_bytes[2]);
    }
    CHECK(reader.position() == destination.size());
    CHECK(reader.empty());
}

void test_truncation_for_every_width()
{
    check_truncated_integer_write<0>(1, [](aegis::ByteWriter& writer) {
        return writer.write_u8(std::uint8_t{0x12});
    });
    check_truncated_integer_write<1>(2, [](aegis::ByteWriter& writer) {
        return writer.write_u16_be(std::uint16_t{0x1234});
    });
    check_truncated_integer_write<3>(4, [](aegis::ByteWriter& writer) {
        return writer.write_u32_be(std::uint32_t{0x12345678});
    });
    check_truncated_integer_write<5>(6, [](aegis::ByteWriter& writer) {
        return writer.write_u48_be(std::uint64_t{0x123456789ABCULL});
    });
    check_truncated_integer_write<7>(8, [](aegis::ByteWriter& writer) {
        return writer.write_u64_be(std::uint64_t{0x123456789ABCDEF0ULL});
    });
}

void test_cursor_and_buffer_stability_after_failure()
{
    std::array<std::byte, 3> destination{};
    destination.fill(std::byte{0xA5});
    aegis::ByteWriter writer{destination};

    check_success(writer.write_u8(std::uint8_t{0x11}));
    CHECK(writer.position() == 1);
    const auto before_failure = destination;

    const auto result = writer.write_u32_be(std::uint32_t{0x22334455});
    check_capacity_failure(result, writer, 1, 4, 2);
    CHECK(destination == before_failure);
}

void test_zero_length_and_oversized_byte_writes()
{
    std::array<std::byte, 3> destination{};
    destination.fill(std::byte{0xA5});
    const auto original = destination;
    const std::array<std::byte, 0> empty_source{};
    aegis::ByteWriter writer{destination};

    check_success(writer.write_bytes(empty_source));
    CHECK(writer.position() == 0);
    CHECK(writer.remaining() == destination.size());
    CHECK(destination == original);

    const std::array oversized_source{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}};
    const auto oversized_result = writer.write_bytes(oversized_source);
    check_capacity_failure(
        oversized_result, writer, 0, oversized_source.size(), destination.size());
    CHECK(destination == original);

    const std::array exact_source{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}};
    check_success(writer.write_bytes(exact_source));
    CHECK(destination == exact_source);
    CHECK(writer.position() == destination.size());
    CHECK(writer.remaining() == 0);
}

void test_rejects_values_exceeding_48_bits()
{
    std::array<std::byte, 7> destination{};
    destination.fill(std::byte{0xA5});
    aegis::ByteWriter writer{destination};

    check_success(writer.write_u8(std::uint8_t{0x11}));
    const auto before_failure = destination;
    constexpr std::uint64_t oversized_value = 0x0001000000000000ULL;

    const auto result = writer.write_u48_be(oversized_value);
    CHECK(!result.has_value());
    CHECK(writer.position() == 1);
    CHECK(writer.remaining() == 6);
    CHECK(destination == before_failure);

    const aegis::Error* const error = result.error();
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->category == aegis::ErrorCategory::InputFraming);
        CHECK(error->code == aegis::ErrorCode::ValueOutOfRange);
        CHECK(error->offset == 1);
        CHECK(error->observed_value == oversized_value);
        CHECK(error->limit_value == std::uint64_t{0x0000FFFFFFFFFFFFULL});
        CHECK(!error->message.empty());
    }

    std::array<std::byte, 6> offset_destination{};
    offset_destination.fill(std::byte{0xCC});
    const auto offset_original = offset_destination;
    CHECK(!aegis::write_u48_be(offset_destination, 0, oversized_value));
    CHECK(offset_destination == offset_original);
}

void test_offset_write_helpers()
{
    std::array<std::byte, 29> destination{};
    destination.fill(std::byte{0xEE});

    CHECK(aegis::write_u8(destination, 1, std::uint8_t{0x80}));
    CHECK(aegis::write_u16_be(destination, 3, std::uint16_t{0x8001}));
    CHECK(aegis::write_u32_be(destination, 6, std::uint32_t{0x80010203}));
    CHECK(aegis::write_u48_be(destination, 11, std::uint64_t{0x800102030405ULL}));
    CHECK(aegis::write_u64_be(destination, 18, std::uint64_t{0x8001020304050607ULL}));
    const std::array source{std::byte{0x11}, std::byte{0x22}};
    CHECK(aegis::write_bytes(destination, 27, source));

    CHECK(destination[0] == std::byte{0xEE});
    CHECK(destination[1] == std::byte{0x80});
    CHECK(destination[2] == std::byte{0xEE});
    CHECK(destination[3] == std::byte{0x80});
    CHECK(destination[4] == std::byte{0x01});
    CHECK(destination[5] == std::byte{0xEE});
    CHECK(destination[6] == std::byte{0x80});
    CHECK(destination[7] == std::byte{0x01});
    CHECK(destination[8] == std::byte{0x02});
    CHECK(destination[9] == std::byte{0x03});
    CHECK(destination[10] == std::byte{0xEE});
    CHECK(destination[11] == std::byte{0x80});
    CHECK(destination[12] == std::byte{0x01});
    CHECK(destination[13] == std::byte{0x02});
    CHECK(destination[14] == std::byte{0x03});
    CHECK(destination[15] == std::byte{0x04});
    CHECK(destination[16] == std::byte{0x05});
    CHECK(destination[17] == std::byte{0xEE});
    CHECK(destination[18] == std::byte{0x80});
    CHECK(destination[19] == std::byte{0x01});
    CHECK(destination[20] == std::byte{0x02});
    CHECK(destination[21] == std::byte{0x03});
    CHECK(destination[22] == std::byte{0x04});
    CHECK(destination[23] == std::byte{0x05});
    CHECK(destination[24] == std::byte{0x06});
    CHECK(destination[25] == std::byte{0x07});
    CHECK(destination[26] == std::byte{0xEE});
    CHECK(destination[27] == std::byte{0x11});
    CHECK(destination[28] == std::byte{0x22});

    const auto before_failure = destination;
    CHECK(!aegis::write_u64_be(destination, destination.size() - 7, 0));
    CHECK(destination == before_failure);
    CHECK(!aegis::write_u8(
        destination, std::numeric_limits<std::size_t>::max(), std::uint8_t{0x01}));
    CHECK(destination == before_failure);
    CHECK(!aegis::write_bytes(destination, destination.size(), source));
    CHECK(destination == before_failure);
}

void test_overlapping_byte_span_write()
{
    std::array destination{
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
    };
    const std::span<const std::byte> source{destination.data(), 4};

    CHECK(aegis::write_bytes(destination, 1, source));
    CHECK(destination == (std::array{
                             std::byte{0x01},
                             std::byte{0x01},
                             std::byte{0x02},
                             std::byte{0x03},
                             std::byte{0x04},
                         }));
}

}  // namespace

int main()
{
    test_known_integer_output_and_exact_capacity();
    test_zero_and_maximum_values();
    test_high_bit_values();
    test_sequential_writes_and_round_trip();
    test_truncation_for_every_width();
    test_cursor_and_buffer_stability_after_failure();
    test_zero_length_and_oversized_byte_writes();
    test_rejects_values_exceeding_48_bits();
    test_offset_write_helpers();
    test_overlapping_byte_span_write();

    if (failure_count != 0) {
        std::cerr << failure_count << " ByteWriter check(s) failed\n";
        return 1;
    }

    return 0;
}
