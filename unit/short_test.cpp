#include <algorithm>
#include <cstring>
#include <iterator>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

TEST_CASE("smallstring short string edge cases") {
    SUBCASE("single character strings") {
        // Test all printable ASCII characters
        for (char c = 32; c <= 126; ++c) {
            small::small_string str(1, c);
            CHECK(str.size() == 1);
            CHECK(str.length() == 1);
            CHECK(!str.empty());
            CHECK(str[0] == c);
            CHECK(str.front() == c);
            CHECK(str.back() == c);
            CHECK(str.at(0) == c);
            CHECK(str.data()[0] == c);
            CHECK(str.c_str()[0] == c);
            CHECK(str.c_str()[1] == '\0');

            // Iterator tests
            CHECK(*str.begin() == c);
            CHECK(str.begin() + 1 == str.end());
            CHECK(std::distance(str.begin(), str.end()) == 1);
        }
    }

    SUBCASE("internal storage capacity boundary (7 chars)") {
        // Test strings at the internal storage boundary
        small::small_string str6("123456");    // 6 chars - should be internal
        small::small_string str7("1234567");   // 7 chars - should be internal
        small::small_string str8("12345678");  // 8 chars - might be external

        CHECK(str6.size() == 6);
        CHECK(str7.size() == 7);
        CHECK(str8.size() == 8);

        // All should work correctly regardless of internal/external storage
        CHECK(str6 == "123456");
        CHECK(str7 == "1234567");
        CHECK(str8 == "12345678");

        // Test that they're null terminated
        CHECK(str6.c_str()[6] == '\0');
        CHECK(str7.c_str()[7] == '\0');
        CHECK(str8.c_str()[8] == '\0');

        // Test character access
        for (size_t i = 0; i < str6.size(); ++i) {
            CHECK(str6[i] == ('1' + i));
        }
        for (size_t i = 0; i < str7.size(); ++i) {
            CHECK(str7[i] == ('1' + i));
        }
        for (size_t i = 0; i < str8.size(); ++i) {
            CHECK(str8[i] == ('1' + i));
        }
    }

    SUBCASE("short string operations") {
        small::small_string short_str("abc");

        // Substring operations
        CHECK(short_str.substr(0, 1) == "a");
        CHECK(short_str.substr(1, 1) == "b");
        CHECK(short_str.substr(2, 1) == "c");
        CHECK(short_str.substr(0, 2) == "ab");
        CHECK(short_str.substr(1, 2) == "bc");
        CHECK(short_str.substr(0, 3) == "abc");
        CHECK(short_str.substr(0) == "abc");

        // Find operations
        CHECK(short_str.find('a') == 0);
        CHECK(short_str.find('b') == 1);
        CHECK(short_str.find('c') == 2);
        CHECK(short_str.find('d') == small::small_string::npos);
        CHECK(short_str.find("a") == 0);
        CHECK(short_str.find("b") == 1);
        CHECK(short_str.find("c") == 2);
        CHECK(short_str.find("ab") == 0);
        CHECK(short_str.find("bc") == 1);
        CHECK(short_str.find("abc") == 0);
        CHECK(short_str.find("d") == small::small_string::npos);

        // Reverse find operations
        CHECK(short_str.rfind('a') == 0);
        CHECK(short_str.rfind('b') == 1);
        CHECK(short_str.rfind('c') == 2);
        CHECK(short_str.rfind('d') == small::small_string::npos);
    }

    SUBCASE("short string modification") {
        small::small_string str("abc");

        // Append to short string
        str += 'd';
        CHECK(str == "abcd");

        // Append string to short string
        str += "ef";
        CHECK(str == "abcdef");

        // Insert into short string
        small::small_string str2("ac");
        str2.insert(1, "b");
        CHECK(str2 == "abc");

        // Replace in short string
        small::small_string str3("abc");
        str3.replace(1, 1, "xy");
        CHECK(str3 == "axyc");

        // Erase from short string
        small::small_string str4("abcde");
        str4.erase(1, 3);
        CHECK(str4 == "ae");
    }

    SUBCASE("short string to empty transitions") {
        small::small_string str("a");
        CHECK(str.size() == 1);
        CHECK(!str.empty());

        // Pop back to empty
        str.pop_back();
        CHECK(str.empty());
        CHECK(str.size() == 0);

        // Add back
        str.push_back('b');
        CHECK(str == "b");
        CHECK(str.size() == 1);

        // Clear to empty
        str.clear();
        CHECK(str.empty());
        CHECK(str.size() == 0);

        // Erase to empty
        str = "x";
        str.erase(0, 1);
        CHECK(str.empty());
        CHECK(str.size() == 0);

        // Resize to empty
        str = "xyz";
        str.resize(0);
        CHECK(str.empty());
        CHECK(str.size() == 0);
    }

    SUBCASE("short string iterators") {
        small::small_string str("hello");

        // Forward iteration
        std::string result;
        for (auto it = str.begin(); it != str.end(); ++it) {
            result += *it;
        }
        CHECK(result == "hello");

        // Reverse iteration
        result.clear();
        for (auto it = str.rbegin(); it != str.rend(); ++it) {
            result += *it;
        }
        CHECK(result == "olleh");

        // Const iterators
        const auto& const_str = str;
        result.clear();
        for (auto it = const_str.cbegin(); it != const_str.cend(); ++it) {
            result += *it;
        }
        CHECK(result == "hello");

        // Algorithm support
        auto vowel_count = std::count_if(
          str.begin(), str.end(), [](char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; });
        CHECK(vowel_count == 2);  // 'e' and 'o'
    }

    SUBCASE("short string comparison edge cases") {
        small::small_string a("a");
        small::small_string b("b");
        small::small_string aa("aa");
        small::small_string ab("ab");

        // Single char comparisons
        CHECK(a < b);
        CHECK(!(b < a));
        CHECK(a != b);
        CHECK(a == a);

        // Length vs lexicographic ordering
        CHECK(a < aa);  // shorter comes first
        CHECK(a < ab);
        CHECK(aa < ab);  // same length, lexicographic
        CHECK(ab > aa);

        // Three-way comparison
        CHECK(a.compare(b) < 0);
        CHECK(b.compare(a) > 0);
        CHECK(a.compare(a) == 0);
    }
}