#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

TEST_CASE("smallstring core methods") {
    SUBCASE("data() method functionality") {
        small::small_string str("hello world");

        // Test const data()
        const auto& const_str = str;
        const char* const_ptr = const_str.data();
        CHECK(const_ptr != nullptr);
        CHECK(strcmp(const_ptr, "hello world") == 0);

        // Test mutable data()
        char* mut_ptr = str.data();
        CHECK(mut_ptr != nullptr);
        CHECK(strcmp(mut_ptr, "hello world") == 0);

        // Verify data() points to actual string content
        CHECK(const_ptr == mut_ptr);

        // Test data() with empty string
        small::small_string empty;
        CHECK(empty.data() != nullptr);  // Should not be null even when empty

        // Test data() modification through pointer
        mut_ptr[0] = 'H';
        CHECK(str[0] == 'H');
        CHECK(str == "Hello world");
    }

    SUBCASE("c_str() method functionality") {
        small::small_string str("test string");

        const char* cstr = str.c_str();
        CHECK(cstr != nullptr);
        CHECK(strcmp(cstr, "test string") == 0);

        // Verify null termination
        CHECK(cstr[11] == '\0');
        CHECK(strlen(cstr) == str.size());

        // Test with empty string
        small::small_string empty;
        const char* empty_cstr = empty.c_str();
        CHECK(empty_cstr != nullptr);
        CHECK(strlen(empty_cstr) == 0);
        CHECK(empty_cstr[0] == '\0');

        // Test c_str() with string containing null characters
        small::small_string null_str;
        null_str.push_back('a');
        null_str.push_back('\0');
        null_str.push_back('b');
        const char* null_cstr = null_str.c_str();
        CHECK(null_cstr[0] == 'a');
        CHECK(null_cstr[1] == '\0');
        CHECK(null_cstr[2] == 'b');
        CHECK(null_cstr[3] == '\0');  // Final null termination
    }

    SUBCASE("length() method functionality") {
        small::small_string str("example");
        CHECK(str.length() == 7);
        CHECK(str.length() == str.size());

        // Test with empty string
        small::small_string empty;
        CHECK(empty.length() == 0);

        // Test length consistency after modifications
        str.push_back('!');
        CHECK(str.length() == 8);

        str.pop_back();
        CHECK(str.length() == 7);

        str.clear();
        CHECK(str.length() == 0);
    }

    SUBCASE("max_size() method functionality") {
        small::small_string str;
        auto max_sz = str.max_size();

        // Should be a reasonable large number
        CHECK(max_sz > 1000);
        CHECK(max_sz > str.size());
        CHECK(max_sz <= std::numeric_limits<small::small_string::size_type>::max());

        // max_size should be consistent
        small::small_string str2("different content");
        CHECK(str2.max_size() == max_sz);

        // PMR version should have same max_size
        small::pmr::small_string pmr_str;
        CHECK(pmr_str.max_size() == max_sz);
    }

    SUBCASE("empty() method functionality") {
        small::small_string str;
        CHECK(str.empty());

        str.push_back('a');
        CHECK(!str.empty());

        str.pop_back();
        CHECK(str.empty());

        str = "test";
        CHECK(!str.empty());

        str.clear();
        CHECK(str.empty());

        // Test with various operations
        str.assign(5, 'x');
        CHECK(!str.empty());

        str.resize(0);
        CHECK(str.empty());
    }

    SUBCASE("clear() method functionality") {
        small::small_string str("some content to clear");
        CHECK(!str.empty());
        CHECK(str.size() > 0);

        str.clear();
        CHECK(str.empty());
        CHECK(str.size() == 0);
        CHECK(str.length() == 0);

        // Capacity might remain unchanged
        auto cap_after_clear = str.capacity();
        CHECK(cap_after_clear >= 0);

        // Should be able to use string after clear
        str.push_back('a');
        CHECK(str.size() == 1);
        CHECK(str == "a");

        // Multiple clears should be safe
        str.clear();
        str.clear();
        CHECK(str.empty());
    }
}

TEST_CASE("smallstring capacity management") {
    SUBCASE("reserve() method functionality") {
        small::small_string str;
        (void)str.capacity();  // Suppress unused variable warning

        // Reserve larger capacity
        str.reserve(100);
        CHECK(str.capacity() >= 100);
        CHECK(str.empty());  // Content should remain unchanged

        // Fill up to reserved capacity
        for (int i = 0; i < 50; ++i) {
            str.push_back(static_cast<char>('a' + (i % 26)));
        }
        CHECK(str.size() == 50);
        CHECK(str.capacity() >= 100);

        // Reserve smaller than current capacity (should be no-op)
        auto cap_before = str.capacity();
        str.reserve(10);
        CHECK(str.capacity() >= cap_before);

        // Reserve exactly current size
        str.reserve(str.size());
        CHECK(str.capacity() >= str.size());

        // Test reserve with PMR version
        small::pmr::small_string pmr_str;
        pmr_str.reserve(50);
        CHECK(pmr_str.capacity() >= 50);
        CHECK(pmr_str.empty());
    }

    SUBCASE("shrink_to_fit() method functionality") {
        small::small_string str;

        // Create large capacity
        str.reserve(1000);
        auto large_cap = str.capacity();
        CHECK(large_cap >= 1000);

        // Add small amount of content
        str = "small";
        CHECK(str.size() == 5);

        // Shrink to fit
        str.shrink_to_fit();
        auto new_cap = str.capacity();
        CHECK(new_cap >= str.size());
        // Note: shrink_to_fit is non-binding, so we can't guarantee it shrinks
        CHECK(str == "small");  // Content should be preserved

        // Test shrink_to_fit on empty string
        small::small_string empty;
        empty.reserve(100);
        empty.shrink_to_fit();
        CHECK(empty.empty());
        CHECK(empty.capacity() >= 0);

        // Test shrink_to_fit doesn't break functionality
        str.shrink_to_fit();
        str.push_back('!');
        CHECK(str == "small!");
    }
}

TEST_CASE("smallstring modifiers") {
    SUBCASE("push_back() method functionality") {
        small::small_string str;

        // Push single character
        str.push_back('a');
        CHECK(str.size() == 1);
        CHECK(str[0] == 'a');
        CHECK(str == "a");

        // Push multiple characters
        str.push_back('b');
        str.push_back('c');
        CHECK(str.size() == 3);
        CHECK(str == "abc");

        // Push special characters
        str.push_back('\n');
        str.push_back('\0');
        str.push_back('d');
        CHECK(str.size() == 6);
        CHECK(str[3] == '\n');
        CHECK(str[4] == '\0');
        CHECK(str[5] == 'd');

        // Test push_back beyond small string optimization boundary
        while (str.size() < 20) {
            str.push_back('x');
        }
        CHECK(str.size() == 20);
        CHECK(str.back() == 'x');

        // Test with high ASCII values
        str.clear();
        str.push_back(char(200));
        str.push_back(char(255));
        CHECK(str.size() == 2);
        CHECK(str[0] == char(200));
        CHECK(str[1] == char(255));
    }

    SUBCASE("pop_back() method functionality") {
        small::small_string str("hello");
        CHECK(str.size() == 5);

        str.pop_back();
        CHECK(str.size() == 4);
        CHECK(str == "hell");

        // Pop multiple characters
        str.pop_back();
        str.pop_back();
        CHECK(str.size() == 2);
        CHECK(str == "he");

        // Pop down to single character
        str.pop_back();
        CHECK(str.size() == 1);
        CHECK(str == "h");

        // Pop last character
        str.pop_back();
        CHECK(str.empty());
        CHECK(str.size() == 0);

        // Test pop_back after push_back
        str.push_back('A');
        str.push_back('B');
        str.pop_back();
        CHECK(str == "A");
    }

    SUBCASE("append() method with count and character") {
        small::small_string str("base");

        // Append multiple copies of character
        str.append(3, 'x');
        CHECK(str == "basexxx");
        CHECK(str.size() == 7);

        // Append zero characters (should be no-op)
        str.append(0, 'y');
        CHECK(str == "basexxx");

        // Append single character
        str.append(1, '!');
        CHECK(str == "basexxx!");

        // Append to empty string
        small::small_string empty;
        empty.append(4, 'z');
        CHECK(empty == "zzzz");

        // Append large number of characters
        small::small_string large;
        large.append(100, 'A');
        CHECK(large.size() == 100);
        CHECK(large[0] == 'A');
        CHECK(large[99] == 'A');
    }

    SUBCASE("append() method with other string") {
        small::small_string str("hello");
        small::small_string other(" world");

        str.append(other);
        CHECK(str == "hello world");
        CHECK(str.size() == 11);

        // Append empty string (should be no-op)
        small::small_string empty;
        str.append(empty);
        CHECK(str == "hello world");

        // Append to empty string
        small::small_string target;
        target.append(str);
        CHECK(target == str);

        // Chain appends
        small::small_string chain("a");
        small::small_string b("b");
        small::small_string c("c");
        chain.append(b).append(c);
        CHECK(chain == "abc");

        // Append with PMR strings
        small::pmr::small_string pmr1("pmr");
        small::pmr::small_string pmr2("test");
        pmr1.append(pmr2);
        CHECK(pmr1 == "pmrtest");
    }
}

TEST_CASE("smallstring C++20 string methods") {
    SUBCASE("starts_with() method functionality") {
        small::small_string str("hello world");

        // Test with string literals
        CHECK(str.starts_with("hello"));
        CHECK(str.starts_with("h"));
        CHECK(!str.starts_with("world"));
        CHECK(!str.starts_with("Hello"));  // Case sensitive

        // Test with empty prefix
        CHECK(str.starts_with(""));

        // Test with prefix longer than string
        CHECK(!str.starts_with("hello world!"));

        // Test with exact match
        CHECK(str.starts_with("hello world"));

        // Test with string_view
        std::string_view sv("hello");
        CHECK(str.starts_with(sv));

        // Test with character
        CHECK(str.starts_with('h'));
        CHECK(!str.starts_with('w'));

        // Test with empty string
        small::small_string empty;
        CHECK(empty.starts_with(""));
        CHECK(!empty.starts_with("a"));
        CHECK(!empty.starts_with('a'));
    }

    SUBCASE("ends_with() method functionality") {
        small::small_string str("hello world");

        // Test with string literals
        CHECK(str.ends_with("world"));
        CHECK(str.ends_with("d"));
        CHECK(!str.ends_with("hello"));
        CHECK(!str.ends_with("World"));  // Case sensitive

        // Test with empty suffix
        CHECK(str.ends_with(""));

        // Test with suffix longer than string
        CHECK(!str.ends_with("hello world!"));

        // Test with exact match
        CHECK(str.ends_with("hello world"));

        // Test with string_view
        std::string_view sv("world");
        CHECK(str.ends_with(sv));

        // Test with character
        CHECK(str.ends_with('d'));
        CHECK(!str.ends_with('h'));

        // Test with empty string
        small::small_string empty;
        CHECK(empty.ends_with(""));
        CHECK(!empty.ends_with("a"));
        CHECK(!empty.ends_with('a'));
    }

    SUBCASE("contains() method functionality") {
        small::small_string str("hello world");

        // Test with substrings
        CHECK(str.contains("hello"));
        CHECK(str.contains("world"));
        CHECK(str.contains("lo wo"));
        CHECK(str.contains(""));
        CHECK(!str.contains("xyz"));
        CHECK(!str.contains("Hello"));  // Case sensitive

        // Test with characters
        CHECK(str.contains('h'));
        CHECK(str.contains('w'));
        CHECK(str.contains(' '));
        CHECK(!str.contains('x'));

        // Test with string_view
        std::string_view sv("llo");
        CHECK(str.contains(sv));

        // Test with empty string
        small::small_string empty;
        CHECK(empty.contains(""));
        CHECK(!empty.contains("a"));
        CHECK(!empty.contains('a'));

        // Test edge cases
        small::small_string single("a");
        CHECK(single.contains("a"));
        CHECK(single.contains('a'));
        CHECK(!single.contains("aa"));
    }
}

TEST_CASE("smallstring string view conversion") {
    SUBCASE("implicit conversion to string_view") {
        small::small_string str("test conversion!");

        // Implicit conversion
        std::string_view sv = str;
        CHECK(sv.data() == str.data());
        CHECK(sv.size() == str.size());
        CHECK(sv == "test conversion!");

        // Use in function expecting string_view
        auto test_func = [](std::string_view view) { return view.size(); };

        CHECK(test_func(str) == str.size());

        // Test with empty string
        small::small_string empty;
        std::string_view empty_sv = empty;
        CHECK(empty_sv.empty());
        CHECK(empty_sv.size() == 0);

        // Test modification doesn't affect original conversion
        std::string_view sv_before = str;
        str.push_back('?');
        // sv_before now points to modified data, but size is old
        // This is expected behavior for string_view
        CHECK_EQ(sv_before.data(), str.data());
    }

    SUBCASE("explicit string_view operations") {
        small::small_string str("string view test");

        // Create string_view explicitly
        auto sv = static_cast<std::string_view>(str);
        CHECK(sv == "string view test");

        // String_view operations
        auto sub_sv = sv.substr(7, 4);
        CHECK(sub_sv == "view");

        // Find operations through string_view
        auto pos = sv.find("view");
        CHECK(pos == 7);

        // Compare with other string_views
        std::string_view other("string view test");
        CHECK(sv == other);
    }
}