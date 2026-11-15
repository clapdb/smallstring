#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

namespace {
template <class String>
void check_view_replace() {
    const std::string source("X\0YZW", 5);
    const std::string_view view(source.data(), source.size());
    const size_t destinations[] = {0, 1, 4, 8, 10};
    const size_t counts[] = {0, 2, 30};
    const size_t positions[] = {0, 1, 4, 5};
    const size_t source_counts[] = {0, 1, 2, 20, static_cast<size_t>(String::npos),
                                    std::numeric_limits<size_t>::max()};
    for (size_t destination : destinations) {
        for (size_t count : counts) {
            for (size_t position : positions) {
                for (size_t source_count : source_counts) {
                    CAPTURE(destination);
                    CAPTURE(count);
                    CAPTURE(position);
                    CAPTURE(source_count);
                    String actual("abcdefghij");
                    std::string expected("abcdefghij");
                    expected.replace(destination, count, source, position, source_count);
                    actual.replace(destination, count, view, position, source_count);
                    CHECK(std::string(actual.data(), actual.size()) == expected);
                }
            }
        }
    }
    String default_count("abcdef");
    default_count.replace(1, 2, view, 2);
    CHECK(std::string(default_count.data(), default_count.size()) == "aYZWdef");
    String empty_view("abcdef");
    empty_view.replace(1, 2, std::string_view{}, 0);
    CHECK(std::string(empty_view.data(), empty_view.size()) == "adef");
    String invalid("abcdef");
    CHECK_THROWS_AS(invalid.replace(1, 2, view, view.size() + 1, 0), std::out_of_range);
    CHECK_THROWS_AS(invalid.replace(1, 2, view, std::numeric_limits<size_t>::max(), 1), std::out_of_range);
    CHECK(std::string(invalid.data(), invalid.size()) == "abcdef");

    const size_t lengths[] = {6, 100, 16384};
    for (size_t length : lengths) {
        std::string before(length, 'a');
        for (size_t i = 0; i < length; ++i) before[i] = static_cast<char>('a' + i % 23);
        for (bool spare_capacity : {false, true}) {
            for (int operation = 0; operation < 5; ++operation) {
                CAPTURE(length);
                CAPTURE(spare_capacity);
                CAPTURE(operation);
                String actual(before.data(), before.size());
                if (spare_capacity) actual.reserve(length * 2 + 16);
                const size_t destination = operation == 0 || operation == 2 ? 1 :
                    operation == 1 ? 3 : operation == 3 ? length : 0;
                const size_t count = operation == 0 ? 2 : operation == 2 ? 1 :
                    operation == 3 ? 0 : std::string::npos;
                const size_t position = operation == 1 || operation == 4 ? 1 : 0;
                const size_t source_count = operation == 0 ? 2 : operation == 1 ? 4 :
                    operation == 4 ? std::string::npos : length;
                const auto replacement = before.substr(position, source_count);
                std::string expected = before;
                expected.replace(destination, count, replacement);
                const std::string_view own(actual.data(), actual.size());
                actual.replace(destination, count, own, position, source_count);
                CHECK(std::string(actual.data(), actual.size()) == expected);
            }
        }
    }
}
}  // namespace

TEST_CASE("string-view replacement uses source offsets and preserves overlap") {
    small::small_string probe("abcdef");
    probe.replace(1, 2, std::string_view("XYZ"), 0, 2);
    REQUIRE(std::string(probe.data(), probe.size()) == "aXYdef");
    check_view_replace<small::small_string>();
    check_view_replace<small::small_byte_string>();
    check_view_replace<small::pmr::small_string>();
    check_view_replace<small::pmr::small_byte_string>();
}
