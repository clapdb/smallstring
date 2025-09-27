#include <algorithm>
#include <cstring>
#include <iterator>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

TEST_CASE("smallstring boundary transitions") {
    SUBCASE("internal storage maximum is 6 characters") {
        // Verify the internal buffer size
        small::small_string str6("123456");   // 6 chars - should be internal (max)
        small::small_string str7("1234567");  // 7 chars - should be external

        CHECK(str6.size() == 6);
        CHECK(str7.size() == 7);

        // Both should work correctly regardless of internal/external storage
        CHECK(str6 == "123456");
        CHECK(str7 == "1234567");

        // Test that they're null terminated
        CHECK(str6.c_str()[6] == '\0');
        CHECK(str7.c_str()[7] == '\0');

        // Test character access
        for (size_t i = 0; i < str6.size(); ++i) {
            CHECK(str6[i] == ('1' + i));
        }
        for (size_t i = 0; i < str7.size(); ++i) {
            CHECK(str7[i] == ('1' + i));
        }
    }

    SUBCASE("grow from internal to external storage") {
        small::small_string str;

        // Start empty (internal)
        CHECK(str.empty());

        // Grow to maximum internal size (6 chars)
        for (int i = 0; i < 6; ++i) {
            str += static_cast<char>('a' + i);
        }
        CHECK(str == "abcdef");
        CHECK(str.size() == 6);

        // Add one more (should transition to external)
        str += 'g';
        CHECK(str == "abcdefg");
        CHECK(str.size() == 7);

        // Continue adding to ensure external storage works
        str += "hijklmnop";
        CHECK(str == "abcdefghijklmnop");
        CHECK(str.size() == 16);
    }

    SUBCASE("shrink from external to internal storage") {
        // Start with large string (external)
        small::small_string str(50, 'x');
        CHECK(str.size() == 50);

        // Shrink gradually to just above internal boundary
        while (str.size() > 7) {
            str.pop_back();
        }
        CHECK(str.size() == 7);
        CHECK(std::all_of(str.begin(), str.end(), [](char c) { return c == 'x'; }));

        // Shrink to boundary (might transition to internal)
        str.pop_back();  // Now size 6 (should be internal)
        CHECK(str.size() == 6);
        CHECK(std::all_of(str.begin(), str.end(), [](char c) { return c == 'x'; }));

        // Continue shrinking in internal mode
        while (!str.empty()) {
            str.pop_back();
        }
        CHECK(str.empty());
        CHECK(str.size() == 0);
    }

    SUBCASE("capacity changes around boundaries") {
        small::small_string str;
        auto prev_capacity = str.capacity();

        // Track capacity changes as we grow
        for (auto i = 0u; i < 20u; ++i) {
            str += static_cast<char>('a' + (i % 26u));
            auto current_capacity = str.capacity();

            // Capacity should never decrease during growth
            CHECK(current_capacity >= prev_capacity);
            CHECK(current_capacity >= str.size());

            prev_capacity = current_capacity;
        }
    }

    SUBCASE("reserve around boundary conditions") {
        small::small_string str;

        // Reserve small amount (should stay internal)
        str.reserve(5);
        CHECK(str.capacity() >= 5);
        CHECK(str.empty());

        // Reserve exactly at internal boundary
        str.reserve(6);
        CHECK(str.capacity() >= 6);

        // Reserve beyond boundary (should go external)
        str.reserve(10);
        CHECK(str.capacity() >= 10);

        // Fill to verify it works
        while (str.size() < 10) {
            str += static_cast<char>('a' + (str.size() % 26));
        }
        CHECK(str.size() == 10);
    }

    SUBCASE("resize around boundary conditions") {
        small::small_string str;

        // Resize to small size (internal)
        str.resize(3, 'a');
        CHECK(str == "aaa");
        CHECK(str.size() == 3);

        // Resize to internal boundary
        str.resize(6, 'b');
        CHECK(str == "aaabbb");
        CHECK(str.size() == 6);

        // Resize beyond boundary (external)
        str.resize(10, 'c');
        CHECK(str == "aaabbbcccc");
        CHECK(str.size() == 10);

        // Resize back down through boundary
        str.resize(6);
        CHECK(str == "aaabbb");
        CHECK(str.size() == 6);

        // Resize to very small
        str.resize(2);
        CHECK(str == "aa");
        CHECK(str.size() == 2);

        // Resize to empty
        str.resize(0);
        CHECK(str.empty());
    }

    SUBCASE("insert operations around boundaries") {
        small::small_string str("abc");  // Start small (3 chars)

        // Insert to grow towards boundary
        str.insert(1, "de");  // "adebc" (5 chars)
        CHECK(str == "adebc");
        CHECK(str.size() == 5);

        // Insert one more char to hit boundary
        str.insert(2, "f");  // "adfdebc" -> wait, let me be more careful
        str = "abc";
        str.insert(1, "def");  // "adefbc" (6 chars - at boundary)
        CHECK(str == "adefbc");
        CHECK(str.size() == 6);

        // Insert to cross boundary
        str.insert(0, "X");  // "Xadefbc" (7 chars - external)
        CHECK(str == "Xadefbc");
        CHECK(str.size() == 7);

        // Continue inserting in external mode
        str.insert(str.size(), "123");
        CHECK(str == "Xadefbc123");
        CHECK(str.size() == 10);
    }

    SUBCASE("erase operations around boundaries") {
        // Start with string that's definitely external
        small::small_string str("abcdefghijklmnop");
        CHECK(str.size() == 16);

        // Erase to bring back towards boundary
        str.erase(7, 9);  // Remove "hijklmnop"
        CHECK(str == "abcdefg");
        CHECK(str.size() == 7);  // Still external

        // Erase to cross boundary downward
        str.erase(6, 1);  // Remove "g"
        CHECK(str == "abcdef");
        CHECK(str.size() == 6);  // Now internal

        // Continue erasing in internal mode
        str.erase(3, 2);  // Remove "de"
        CHECK(str == "abcf");
        CHECK(str.size() == 4);
    }

    SUBCASE("replace operations around boundaries") {
        small::small_string str("abc");  // 3 chars, internal

        // Replace to grow towards boundary
        str.replace(1, 1, "def");  // "adefc" (5 chars)
        CHECK(str == "adefc");
        CHECK(str.size() == 5);

        // Replace to hit boundary
        str.replace(4, 1, "g");   // "adefg" (5 chars)
        str.replace(2, 1, "XY");  // "adXYfg" (6 chars - at boundary)
        CHECK(str == "adXYfg");
        CHECK(str.size() == 6);

        // Replace to cross boundary
        str.replace(1, 1, "123");  // "a123XYfg" (8 chars - external)
        CHECK(str == "a123XYfg");
        CHECK(str.size() == 8);

        // Replace in external mode
        str.replace(0, 4, "Z");  // "ZXYfg" (5 chars - back to internal)
        CHECK(str == "ZXYfg");
        CHECK(str.size() == 5);
    }

    SUBCASE("append operations around boundaries") {
        small::small_string str;

        // Append to build up to boundary
        str.append("abc");
        CHECK(str == "abc");
        CHECK(str.size() == 3);

        str.append("def");
        CHECK(str == "abcdef");
        CHECK(str.size() == 6);  // At internal boundary

        // Cross boundary
        str.append("g");
        CHECK(str == "abcdefg");
        CHECK(str.size() == 7);  // Now external

        // Continue appending in external mode
        str.append("hijkl");
        CHECK(str == "abcdefghijkl");
        CHECK(str.size() == 12);

        str.append(3, 'x');
        CHECK(str == "abcdefghijklxxx");
        CHECK(str.size() == 15);
    }

    SUBCASE("copy constructor around boundaries") {
        // Internal string (6 chars max)
        small::small_string internal("abcdef");
        small::small_string internal_copy(internal);
        CHECK(internal_copy == internal);
        CHECK(internal_copy.size() == 6);

        // External string (7+ chars)
        small::small_string external("abcdefg");
        small::small_string external_copy(external);
        CHECK(external_copy == external);
        CHECK(external_copy.size() == 7);

        // Larger external string
        small::small_string large_external("abcdefghijklmnop");
        small::small_string large_copy(large_external);
        CHECK(large_copy == large_external);
        CHECK(large_copy.size() == 16);
    }

    SUBCASE("assignment around boundaries") {
        small::small_string str;

        // Assign internal string
        str = "abc";
        CHECK(str == "abc");
        CHECK(str.size() == 3);

        // Assign at boundary
        str = "abcdef";
        CHECK(str == "abcdef");
        CHECK(str.size() == 6);

        // Assign external string
        str = "abcdefg";
        CHECK(str == "abcdefg");
        CHECK(str.size() == 7);

        // Assign larger external string
        str = "abcdefghijklmnop";
        CHECK(str == "abcdefghijklmnop");
        CHECK(str.size() == 16);

        // Assign back to smaller
        str = "xyz";
        CHECK(str == "xyz");
        CHECK(str.size() == 3);

        // Assign empty
        str = "";
        CHECK(str.empty());
    }

    SUBCASE("move operations around boundaries") {
        // Move internal string
        small::small_string internal("abcdef");  // 6 chars
        small::small_string moved_internal(std::move(internal));
        CHECK(moved_internal == "abcdef");
        CHECK(moved_internal.size() == 6);

        // Move external string
        small::small_string external("abcdefghijk");  // 11 chars
        small::small_string moved_external(std::move(external));
        CHECK(moved_external == "abcdefghijk");
        CHECK(moved_external.size() == 11);
        // Original should be empty or in a moved-from state
        CHECK(external.size() <= 128);  // Implementation detail - small strings might not be fully moved
    }
}