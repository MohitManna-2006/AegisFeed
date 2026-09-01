#include "aegis/mold/mold_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace {

static_assert(sizeof(aegis::MoldSession) == 10);
static_assert(alignof(aegis::MoldSession) == alignof(std::byte));
static_assert(std::is_same_v<aegis::MoldSession, std::array<std::byte, 10>>);
static_assert(std::is_standard_layout_v<aegis::MoldSession>);
static_assert(std::is_trivially_copyable_v<aegis::MoldSession>);

std::size_t failure_count = 0;

void check(const bool condition, const std::string_view expression, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << "mold_session_test.cpp:" << line << ": check failed: " << expression << '\n';
    ++failure_count;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

void check_success(const aegis::Result<void>& result)
{
    CHECK(result.has_value());
    CHECK(static_cast<bool>(result));
    CHECK(result.error() == nullptr);
}

void test_exact_ten_byte_text_construction()
{
    const auto result = aegis::make_mold_session("AEGIS00001");
    CHECK(result.has_value());
    CHECK(result.error() == nullptr);

    const aegis::MoldSession expected{
        std::byte{'A'},
        std::byte{'E'},
        std::byte{'G'},
        std::byte{'I'},
        std::byte{'S'},
        std::byte{'0'},
        std::byte{'0'},
        std::byte{'0'},
        std::byte{'0'},
        std::byte{'1'},
    };
    const aegis::MoldSession* const session = result.value();
    CHECK(session != nullptr);
    if (session != nullptr) {
        CHECK(*session == expected);
        CHECK(aegis::format_mold_session(*session).view() == "AEGIS00001");
    }
}

void test_short_text_is_right_padded()
{
    const auto result = aegis::make_mold_session("ABC");
    const aegis::MoldSession expected{
        std::byte{'A'},
        std::byte{'B'},
        std::byte{'C'},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
    };

    CHECK(result.has_value());
    const aegis::MoldSession* const session = result.value();
    CHECK(session != nullptr);
    if (session != nullptr) {
        CHECK(*session == expected);
        CHECK(aegis::format_mold_session(*session).view() == "ABC");
    }

    const auto explicit_trailing_space = aegis::make_mold_session("ABC ");
    CHECK(explicit_trailing_space.has_value());
    CHECK(explicit_trailing_space.value() != nullptr);
    if (session != nullptr && explicit_trailing_space.value() != nullptr) {
        CHECK(*session == *explicit_trailing_space.value());
    }

    const auto one_space = aegis::make_mold_session(" ");
    CHECK(one_space.has_value());
    CHECK(one_space.value() != nullptr);
    if (one_space.value() != nullptr) {
        CHECK(aegis::format_mold_session(*one_space.value()).view().empty());
    }
}

void test_empty_and_oversize_text_are_rejected()
{
    const auto empty = aegis::make_mold_session("");
    CHECK(!empty.has_value());
    CHECK(empty.value() == nullptr);
    const aegis::Error* const empty_error = empty.error();
    CHECK(empty_error != nullptr);
    if (empty_error != nullptr) {
        CHECK(empty_error->category == aegis::ErrorCategory::Session);
        CHECK(empty_error->code == aegis::ErrorCode::InvalidSessionLength);
        CHECK(empty_error->observed_value == 0);
        CHECK(empty_error->limit_value == 10);
    }

    const auto exact = aegis::make_mold_session("ABCDEFGHIJ");
    const auto oversized = aegis::make_mold_session("ABCDEFGHIJK");
    CHECK(exact.has_value());
    CHECK(!oversized.has_value());
    CHECK(oversized.value() == nullptr);
    const aegis::Error* const oversized_error = oversized.error();
    CHECK(oversized_error != nullptr);
    if (oversized_error != nullptr) {
        CHECK(oversized_error->category == aegis::ErrorCategory::Session);
        CHECK(oversized_error->code == aegis::ErrorCode::InvalidSessionLength);
        CHECK(oversized_error->observed_value == 11);
        CHECK(oversized_error->limit_value == 10);
    }
}

void test_non_printable_text_is_rejected()
{
    const std::array<char, 3> text{'A', '\0', 'B'};
    const auto result =
        aegis::make_mold_session(std::string_view{text.data(), text.size()});

    CHECK(!result.has_value());
    const aegis::Error* const error = result.error();
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->category == aegis::ErrorCategory::Session);
        CHECK(error->code == aegis::ErrorCode::InvalidSessionCharacter);
        CHECK(error->offset == 1);
        CHECK(error->observed_value == 0);
    }
}

void test_equality_and_lexicographic_order_use_all_bytes()
{
    const auto first_result = aegis::make_mold_session("ABCDEFGHIJ");
    const auto second_result = aegis::make_mold_session("ABCDEFGHIK");
    CHECK(first_result.value() != nullptr);
    CHECK(second_result.value() != nullptr);
    if (first_result.value() == nullptr || second_result.value() == nullptr) {
        return;
    }

    const aegis::MoldSession first = *first_result.value();
    const aegis::MoldSession first_copy = first;
    const aegis::MoldSession second = *second_result.value();

    CHECK(first == first_copy);
    CHECK(first != second);
    CHECK(first < second);

    aegis::MoldSession padded = first;
    padded[9] = std::byte{' '};
    CHECK(first != padded);
    CHECK(padded < first);
}

void test_raw_protocol_bytes_preserve_embedded_zero()
{
    const aegis::MoldSession raw{
        std::byte{'A'},
        std::byte{0x00},
        std::byte{'B'},
        std::byte{' '},
        std::byte{'C'},
        std::byte{0xFF},
        std::byte{'D'},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
    };

    const auto display = aegis::format_mold_session(raw);
    CHECK(display.view().size() == 7);
    CHECK(display.view()[0] == 'A');
    CHECK(display.view()[1] == '\0');
    CHECK(display.view()[2] == 'B');
    CHECK(display.view()[3] == ' ');
    CHECK(display.view()[4] == 'C');
    CHECK(static_cast<unsigned char>(display.view()[5]) == 0xFFU);
    CHECK(display.view()[6] == 'D');
}

void test_exact_serialization_and_known_bytes()
{
    const auto session_result = aegis::make_mold_session("ABC");
    CHECK(session_result.value() != nullptr);
    if (session_result.value() == nullptr) {
        return;
    }

    std::array<std::byte, 10> output{};
    output.fill(std::byte{0xA5});
    aegis::ByteWriter writer{output};
    check_success(aegis::write_mold_session(writer, *session_result.value()));

    const std::array expected{
        std::byte{'A'},
        std::byte{'B'},
        std::byte{'C'},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
        std::byte{' '},
    };
    CHECK(output == expected);
    CHECK(writer.position() == 10);
    CHECK(writer.remaining() == 0);
}

void test_parse_serialize_round_trip()
{
    const aegis::MoldSession original{
        std::byte{0x00},
        std::byte{0x01},
        std::byte{0x7F},
        std::byte{0x80},
        std::byte{0xFF},
        std::byte{'A'},
        std::byte{' '},
        std::byte{'B'},
        std::byte{' '},
        std::byte{0x00},
    };
    std::array<std::byte, 10> wire{};
    aegis::ByteWriter writer{wire};
    check_success(aegis::write_mold_session(writer, original));

    aegis::MoldSession parsed{};
    parsed.fill(std::byte{0xA5});
    aegis::ByteReader reader{wire};
    check_success(aegis::read_mold_session(reader, parsed));

    CHECK(wire == original);
    CHECK(parsed == original);
    CHECK(reader.position() == 10);
    CHECK(reader.remaining() == 0);
}

void test_reader_truncation_is_transactional()
{
    std::array<std::byte, 10> input{};
    input.fill(std::byte{0x11});
    aegis::ByteReader reader{input};
    const auto prefix = reader.read_u8();
    CHECK(prefix.has_value());
    CHECK(reader.position() == 1);

    aegis::MoldSession destination{};
    destination.fill(std::byte{0xA5});
    const auto original_destination = destination;

    const auto result = aegis::read_mold_session(reader, destination);
    CHECK(!result.has_value());
    CHECK(destination == original_destination);
    CHECK(reader.position() == 1);
    CHECK(reader.remaining() == 9);

    const aegis::Error* const error = result.error();
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->code == aegis::ErrorCode::ReadPastEnd);
        CHECK(error->offset == 1);
        CHECK(error->requested_size == 10);
        CHECK(error->available_size == 9);
    }
}

void test_writer_capacity_failure_is_transactional()
{
    const auto session_result = aegis::make_mold_session("AEGIS00001");
    CHECK(session_result.value() != nullptr);
    if (session_result.value() == nullptr) {
        return;
    }

    std::array<std::byte, 10> output{};
    output.fill(std::byte{0xA5});
    aegis::ByteWriter writer{output};
    check_success(writer.write_u8(std::uint8_t{0x11}));
    const auto before_failure = output;

    const auto result = aegis::write_mold_session(writer, *session_result.value());
    CHECK(!result.has_value());
    CHECK(output == before_failure);
    CHECK(writer.position() == 1);
    CHECK(writer.remaining() == 9);

    const aegis::Error* const error = result.error();
    CHECK(error != nullptr);
    if (error != nullptr) {
        CHECK(error->code == aegis::ErrorCode::WritePastEnd);
        CHECK(error->offset == 1);
        CHECK(error->requested_size == 10);
        CHECK(error->available_size == 9);
    }
}

}  // namespace

int main()
{
    test_exact_ten_byte_text_construction();
    test_short_text_is_right_padded();
    test_empty_and_oversize_text_are_rejected();
    test_non_printable_text_is_rejected();
    test_equality_and_lexicographic_order_use_all_bytes();
    test_raw_protocol_bytes_preserve_embedded_zero();
    test_exact_serialization_and_known_bytes();
    test_parse_serialize_round_trip();
    test_reader_truncation_is_transactional();
    test_writer_capacity_failure_is_transactional();

    if (failure_count != 0) {
        std::cerr << failure_count << " MoldSession check(s) failed\n";
        return 1;
    }

    return 0;
}
