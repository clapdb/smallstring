#include <iterator>
#include <sstream>
#include <string>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

namespace {
template <class String, bool NullTerminated>
void check_iterator_replacement() {
    const size_t lengths[] = {0, 1, 3, 6, 7, 100, 255, 256, 16384};
    for (size_t length : lengths) {
        std::string input(length, 'a');
        for (size_t i = 0; i < length; ++i) input[i] = static_cast<char>(i % 251);
        const size_t positions[] = {0, length / 3, length};
        const size_t counts[] = {0, length / 3, length};
        for (bool spare : {false, true}) {
            for (size_t pos : positions) {
                for (size_t count : counts) {
                    count = std::min(count, length - pos);
                    for (int operation = 0; operation < 4; ++operation) {
                        CAPTURE(length);
                        CAPTURE(spare);
                        CAPTURE(pos);
                        CAPTURE(count);
                        CAPTURE(operation);
                        String value(input.data(), input.size());
                        if (spare) value.reserve(length * 3 + 8);
                        const auto offset = static_cast<std::ptrdiff_t>(length / 2);
                        std::string source;
                        auto first = value.cbegin() + static_cast<std::ptrdiff_t>(pos);
                        auto last = first + static_cast<std::ptrdiff_t>(count);
                        String* result = nullptr;
                        switch (operation) {
                            case 0:
                                source.assign(input.rbegin(), input.rend());
                                result = &value.replace(first, last, value.rbegin(), value.rend());
                                break;
                            case 1:
                                source.assign(input.rbegin() + offset, input.rend());
                                result = &value.replace(first, last, value.rbegin() + offset, value.rend());
                                break;
                            case 2:
                                source.assign(input.begin() + offset, input.end());
                                result = &value.replace(first, last, value.cbegin() + offset, value.cend());
                                break;
                            case 3:
                                result = &value.replace(first, last, value.rbegin(), value.rbegin());
                                break;
                        }
                        std::string expected = input;
                        expected.replace(pos, count, source);
                        CHECK(result == &value);
                        REQUIRE(value.size() == expected.size());
                        CHECK(std::string(value.data(), value.size()) == expected);
                        if constexpr (NullTerminated) CHECK(value.c_str()[value.size()] == '\0');
                    }
                }
            }
        }
        std::istringstream stream(input);
        String value("left-right");
        value.replace(value.cbegin() + 4, value.cbegin() + 5,
                      std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        const std::string expected = "left" + input + "right";
        CHECK(std::string(value.data(), value.size()) == expected);
        if constexpr (NullTerminated) CHECK(value.c_str()[value.size()] == '\0');
    }
}
}

TEST_CASE("iterator replacement snapshots aliased and single-pass ranges") {
    check_iterator_replacement<small::small_string, true>();
    check_iterator_replacement<small::small_byte_string, false>();
    check_iterator_replacement<small::pmr::small_string, true>();
    check_iterator_replacement<small::pmr::small_byte_string, false>();
}
