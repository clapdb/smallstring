#include <iterator>
#include <sstream>
#include <string>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

namespace {
template <class String, bool NullTerminated>
void check_iterator_assignment() {
    const size_t lengths[] = {0, 1, 3, 6, 7, 100, 255, 256, 16384};
    for (size_t length : lengths) {
        std::string input(length, 'a');
        for (size_t i = 0; i < length; ++i) input[i] = static_cast<char>(i % 251);
        for (bool spare : {false, true}) {
            for (int operation = 0; operation < 5; ++operation) {
                CAPTURE(length);
                CAPTURE(spare);
                CAPTURE(operation);
                String value(input.data(), input.size());
                if (spare) value.reserve(length * 2 + 8);
                std::string expected = input;
                switch (operation) {
                    case 0:
                        expected.assign(input.rbegin(), input.rend());
                        value.assign(value.rbegin(), value.rend());
                        break;
                    case 1:
                        expected.assign(input.rbegin() + static_cast<std::ptrdiff_t>(length / 2), input.rend());
                        value.assign(value.rbegin() + static_cast<std::ptrdiff_t>(length / 2), value.rend());
                        break;
                    case 2:
                        expected.assign(input.begin() + static_cast<std::ptrdiff_t>(length / 2), input.end());
                        value.assign(value.begin() + static_cast<std::ptrdiff_t>(length / 2), value.end());
                        break;
                    case 3: value.assign(value.cbegin(), value.cend()); break;
                    case 4: expected.clear(); value.assign(value.rbegin(), value.rbegin()); break;
                }
                REQUIRE(value.size() == expected.size());
                CHECK(std::string(value.data(), value.size()) == expected);
                if constexpr (NullTerminated) CHECK(value.c_str()[value.size()] == '\0');
            }
        }
        const int converted[] = {65, 0, 66};
        String from_converted;
        from_converted.assign(std::begin(converted), std::end(converted));
        CHECK(std::string(from_converted.data(), from_converted.size()) == std::string("A\0B", 3));
        std::istringstream stream(input);
        String value("previous contents");
        value.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        REQUIRE(value.size() == input.size());
        CHECK(std::string(value.data(), value.size()) == input);
        if constexpr (NullTerminated) CHECK(value.c_str()[value.size()] == '\0');
    }
}
}

TEST_CASE("iterator assignment snapshots aliased and single-pass ranges") {
    check_iterator_assignment<small::small_string, true>();
    check_iterator_assignment<small::small_byte_string, false>();
    check_iterator_assignment<small::pmr::small_string, true>();
    check_iterator_assignment<small::pmr::small_byte_string, false>();
}
