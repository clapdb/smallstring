#include <forward_list>
#include <iterator>
#include <sstream>
#include <string>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

namespace {
template <class String, bool NullTerminated>
void check_single_pass_construction() {
    const size_t lengths[] = {0, 1, 3, 6, 7, 9, 100, 255, 256, 16384};
    for (size_t length : lengths) {
        CAPTURE(length);
        std::string input(length, 'a');
        for (size_t i = 0; i < length; ++i) input[i] = static_cast<char>(i % 251);
        auto check = [&](const String& value) {
            REQUIRE(value.size() == input.size());
            CHECK(std::string(value.data(), value.size()) == input);
            if constexpr (NullTerminated) CHECK(value.c_str()[value.size()] == '\0');
        };
        std::istringstream stream(input);
        String value{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        check(value);
        CHECK(stream.rdbuf()->sgetc() == std::char_traits<char>::eof());

        std::istringstream formatted(input);
        formatted >> std::noskipws;
        String from_input{std::istream_iterator<char>(formatted), std::istream_iterator<char>()};
        check(from_input);

        // The forward-iterator and contiguous-iterator paths retain two-pass construction.
        std::forward_list<char> forward(input.begin(), input.end());
        check(String(forward.begin(), forward.end()));
        check(String(input.begin(), input.end()));

        // Non-contiguous append/insert use the range constructor to snapshot input.
        std::istringstream append_stream(input);
        String appended;
        appended.append(std::istreambuf_iterator<char>(append_stream), std::istreambuf_iterator<char>());
        check(appended);
        std::istringstream insert_stream(input);
        String inserted;
        inserted.insert(inserted.cbegin(), std::istreambuf_iterator<char>(insert_stream),
                        std::istreambuf_iterator<char>());
        check(inserted);
    }
}
}

TEST_CASE("single-pass iterator construction") {
    check_single_pass_construction<small::small_string, true>();
    check_single_pass_construction<small::small_byte_string, false>();
    check_single_pass_construction<small::pmr::small_string, true>();
    check_single_pass_construction<small::pmr::small_byte_string, false>();
}
