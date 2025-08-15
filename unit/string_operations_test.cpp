#include <iterator>
#include <algorithm>
#include <string>
#include <string_view>
#include "include/smallstring.hpp"
#include "doctest/doctest/doctest.h"

TEST_CASE("smallstring substring and copy operations") {
    SUBCASE("substr comprehensive testing") {
        small::small_string str("hello world");
        
        // Basic substring operations
        CHECK(str.substr() == "hello world");
        CHECK(str.substr(0) == "hello world");
        CHECK(str.substr(6) == "world");
        CHECK(str.substr(0, 5) == "hello");
        CHECK(str.substr(6, 5) == "world");
        
        // Edge cases
        CHECK(str.substr(str.size()) == "");
        CHECK(str.substr(0, 0) == "");
        CHECK(str.substr(5, 0) == "");
        
        // Length longer than available
        CHECK(str.substr(6, 100) == "world");
        CHECK(str.substr(0, 100) == "hello world");
        
        // Single character substrings
        CHECK(str.substr(0, 1) == "h");
        CHECK(str.substr(4, 1) == "o");
        CHECK(str.substr(5, 1) == " ");
        CHECK(str.substr(10, 1) == "d");
        
        // Middle substrings
        CHECK(str.substr(2, 3) == "llo");
        CHECK(str.substr(1, 9) == "ello worl");
        
        // Out of range position should throw
        CHECK_THROWS_AS(str.substr(100), std::out_of_range);
        CHECK_THROWS_AS(str.substr(str.size() + 1), std::out_of_range);
        
        // Large string substr
        small::small_string large(100, 'x');
        large += "target";
        large += std::string(100, 'y');
        CHECK(large.substr(100, 6) == "target");
        CHECK(large.substr(0, 10) == std::string(10, 'x'));
        CHECK(large.substr(196) == std::string(100, 'y'));
        
        // Empty string substr
        small::small_string empty;
        CHECK(empty.substr() == "");
        CHECK(empty.substr(0) == "");
        CHECK(empty.substr(0, 0) == "");
        CHECK_THROWS_AS(empty.substr(1), std::out_of_range);
    }
    
    SUBCASE("copy method comprehensive testing") {
        small::small_string str("hello world testing");
        char buffer[50];
        
        // Basic copy operations
        auto copied = str.copy(buffer, 5, 0);
        CHECK(copied == 5);
        CHECK(std::string(buffer, 5) == "hello");
        
        // Copy from middle
        copied = str.copy(buffer, 5, 6);
        CHECK(copied == 5);
        CHECK(std::string(buffer, 5) == "world");
        
        // Copy entire string
        copied = str.copy(buffer, str.size(), 0);
        CHECK(copied == str.size());
        CHECK(std::string(buffer, copied) == "hello world testing");
        
        // Copy with default parameters
        copied = str.copy(buffer);
        CHECK(copied == str.size());
        CHECK(std::string(buffer, copied) == "hello world testing");
        
        // Copy more than available
        copied = str.copy(buffer, 100, 6);
        CHECK(copied == str.size() - 6);
        CHECK(std::string(buffer, copied) == "world testing");
        
        // Copy from end
        copied = str.copy(buffer, 10, str.size() - 7);
        CHECK(copied == 7);
        CHECK(std::string(buffer, 7) == "testing");
        
        // Copy zero characters
        copied = str.copy(buffer, 0, 0);
        CHECK(copied == 0);
        
        copied = str.copy(buffer, 0, 5);
        CHECK(copied == 0);
        
        // Copy from exact end position
        copied = str.copy(buffer, 10, str.size());
        CHECK(copied == 0);
        
        // Out of range position should throw
        CHECK_THROWS_AS(str.copy(buffer, 5, 100), std::out_of_range);
        
        // Test with large string
        small::small_string large(1000, 'A');
        char large_buffer[100];
        copied = large.copy(large_buffer, 50, 500);
        CHECK(copied == 50);
        CHECK(std::string(large_buffer, 50) == std::string(50, 'A'));
        
        // Test with empty string
        small::small_string empty;
        copied = empty.copy(buffer, 10, 0);
        CHECK(copied == 0);
    }
}

TEST_CASE("smallstring resize operations") {
    SUBCASE("resize with character fill comprehensive") {
        small::small_string str;
        
        // Resize empty string
        str.resize(5, 'a');
        CHECK(str == "aaaaa");
        CHECK(str.size() == 5);
        
        // Resize to larger
        str.resize(10, 'b');
        CHECK(str == "aaaaabbbbb");
        CHECK(str.size() == 10);
        
        // Resize to smaller (should truncate)
        str.resize(7);
        CHECK(str == "aaaaaaa");  // Note: resize without char defaults to truncation
        CHECK(str.size() == 7);
        
        // Resize to same size
        str.resize(7, 'c');
        CHECK(str == "aaaaaaa");
        CHECK(str.size() == 7);
        
        // Resize to zero
        str.resize(0);
        CHECK(str.empty());
        CHECK(str.size() == 0);
        
        // Resize across storage boundaries
        small::small_string boundary;
        
        // Small to medium
        boundary.resize(3, 'x');
        CHECK(boundary == "xxx");
        
        // Medium to large
        boundary.resize(20, 'y');
        CHECK(boundary == "xxx" + std::string(17, 'y'));
        CHECK(boundary.size() == 20);
        
        // Large to very large
        boundary.resize(100, 'z');
        CHECK(boundary.size() == 100);
        CHECK(boundary.substr(0, 3) == "xxx");
        CHECK(boundary.substr(3, 17) == std::string(17, 'y'));
        CHECK(boundary.substr(20, 80) == std::string(80, 'z'));
        
        // Very large back to small
        boundary.resize(2);
        CHECK(boundary == "xx");
        CHECK(boundary.size() == 2);
    }
    
    SUBCASE("resize edge cases and performance") {
        small::small_string str("initial");
        
        // Resize with null character
        str.resize(10, '\0');
        CHECK(str.size() == 10);
        CHECK(str[7] == '\0');
        CHECK(str[8] == '\0');
        CHECK(str[9] == '\0');
        CHECK(str.substr(0, 7) == "initial");
        
        // Resize with special characters
        str.clear();
        str.resize(5, '\n');
        CHECK(str.size() == 5);
        CHECK(str[0] == '\n');
        CHECK(str[4] == '\n');
        
        str.resize(8, '\t');
        CHECK(str.size() == 8);
        CHECK(str[5] == '\t');
        CHECK(str[7] == '\t');
        
        // Large resize operations
        str.clear();
        str.resize(1000, 'L');
        CHECK(str.size() == 1000);
        CHECK(str[0] == 'L');
        CHECK(str[999] == 'L');
        CHECK(str[500] == 'L');
        
        // Resize down from large
        str.resize(10);
        CHECK(str.size() == 10);
        CHECK(str == std::string(10, 'L'));
        
        // Multiple rapid resizes
        for (int i = 1; i <= 50; ++i) {
            str.resize(i, static_cast<char>('A' + (i % 26)));
            CHECK(str.size() == i);
            if (i > 1) {
                CHECK(str[i-1] == static_cast<char>('A' + (i % 26)));
            }
        }
        
        // Resize to same size repeatedly
        size_t initial_cap = str.capacity();
        for (int i = 0; i < 10; ++i) {
            str.resize(50, 'R');
            CHECK(str.size() == 50);
            CHECK(str.capacity() >= initial_cap); // Capacity shouldn't decrease
        }
    }
}

TEST_CASE("smallstring replacement operations") {
    SUBCASE("replace with position and count") {
        small::small_string str("hello world");
        
        // Basic replacement
        str.replace(6, 5, "universe");
        CHECK(str == "hello universe");
        
        // Replace with shorter string
        str = "hello world";
        str.replace(6, 5, "moon");
        CHECK(str == "hello moon");
        
        // Replace with longer string
        str = "hello world";
        str.replace(6, 5, "beautiful galaxy");
        CHECK(str == "hello beautiful galaxy");
        
        // Replace at beginning
        str = "hello world";
        str.replace(0, 5, "goodbye");
        CHECK(str == "goodbye world");
        
        // Replace at end
        str = "hello world";
        str.replace(6, 5, "friend");
        CHECK(str == "hello friend");
        
        // Replace entire string
        str = "hello world";
        str.replace(0, str.size(), "new content");
        CHECK(str == "new content");
        
        // Replace with empty string (deletion)
        str = "hello world";
        str.replace(5, 6, "");
        CHECK(str == "hello");
        
        // Replace zero characters (insertion)
        str = "hello world";
        str.replace(5, 0, " beautiful");
        CHECK(str == "hello beautiful world");
    }
    
    SUBCASE("replace with character count") {
        small::small_string str("abcdef");
        
        // Replace with repeated character
        str.replace(1, 3, 4, 'x');
        CHECK(str == "axxxxef");
        
        // Replace with zero characters
        str = "abcdef";
        str.replace(2, 2, 0, 'y');
        CHECK(str == "abef");
        
        // Replace with many characters
        str = "abcdef";
        str.replace(1, 1, 10, 'z');
        CHECK(str == "a" + std::string(10, 'z') + "cdef");
        
        // Replace entire middle
        str = "start middle end";
        str.replace(6, 6, 5, '*');
        CHECK(str == "start ***** end");
    }
    
    SUBCASE("replace with iterators") {
        small::small_string str("hello world");
        
        // Replace using iterators
        auto first = str.begin() + 6;
        auto last = str.end();
        str.replace(first, last, "universe");
        CHECK(str == "hello universe");
        
        // Replace with another string
        str = "hello world";
        small::small_string replacement("beautiful day");
        first = str.begin() + 6;
        last = str.end();
        str.replace(first, last, replacement);
        CHECK(str == "hello beautiful day");
        
        // Replace with character repetition
        str = "hello world";
        first = str.begin() + 1;
        last = str.begin() + 4;
        str.replace(first, last, 3, 'X');
        CHECK(str == "hXXXo world");
        
        // Replace with initializer list
        str = "hello world";
        first = str.begin() + 6;
        last = str.end();
        str.replace(first, last, {'m', 'o', 'o', 'n'});
        CHECK(str == "hello moon");
        
        // Replace with input iterators
        str = "hello world";
        std::string source = "galaxy";
        first = str.begin() + 6;
        last = str.end();
        str.replace(first, last, source.begin(), source.end());
        CHECK(str == "hello galaxy");
    }
    
    SUBCASE("replace boundary and error conditions") {
        small::small_string str("test string");
        
        // Replace at exact boundaries
        str.replace(0, 0, "prefix ");
        CHECK(str == "prefix test string");
        
        str = "test string";
        str.replace(str.size(), 0, " suffix");
        CHECK(str == "test string suffix");
        
        // Replace with count > remaining length
        str = "test string";
        str.replace(5, 1000, "replaced");
        CHECK(str == "test replaced");
        
        // Out of range position
        CHECK_THROWS_AS(str.replace(100, 1, "x"), std::out_of_range);
        
        // Replace across storage boundaries
        small::small_string small_str("abc");
        small_str.replace(1, 1, "very long replacement that exceeds internal storage");
        CHECK(small_str.size() > 10); // Should transition to external storage
        CHECK(small_str[0] == 'a');
        CHECK(small_str[1] == 'v'); // Start of replacement
        CHECK(small_str.back() == 'c'); // Original last char preserved
        
        // Large string replace
        small::small_string large(100, 'x');
        large.replace(50, 10, "replacement");
        CHECK(large.size() == 100 - 10 + 11); // 101
        CHECK(large.substr(50, 11) == "replacement");
        CHECK(large.substr(0, 50) == std::string(50, 'x'));
    }
}

TEST_CASE("smallstring advanced insert operations") {
    SUBCASE("insert with string_view and custom types") {
        small::small_string str("hello world");
        
        // Insert string_view
        std::string_view sv("beautiful ");
        str.insert(6, sv);
        CHECK(str == "hello beautiful world");
        
        // Insert another small_string
        str = "hello world";
        small::small_string insert_str("amazing ");
        str.insert(6, insert_str);
        CHECK(str == "hello amazing world");
        
        // Insert PMR small_string
        str = "hello world";
        small::pmr::small_string pmr_str("wonderful ");
        str.insert(6, pmr_str);
        CHECK(str == "hello wonderful world");
        
        // Insert with position from source string
        str = "hello world";
        small::small_string source("the beautiful moon");
        str.insert(6, source, 4, 9); // "beautiful"
        CHECK(str == "hello beautiful world");
    }
    
    SUBCASE("insert at iterator positions") {
        small::small_string str("ac");
        
        // Insert single character
        auto it = str.insert(str.begin() + 1, 'b');
        CHECK(str == "abc");
        CHECK(*it == 'b');
        CHECK(it == str.begin() + 1);
        
        // Insert multiple characters
        str = "ae";
        str.insert(str.begin() + 1, 3, 'x');
        CHECK(str == "axxxe");
        
        // Insert range
        str = "start end";
        std::string middle = " middle";
        str.insert(str.begin() + 5, middle.begin(), middle.end());
        CHECK(str == "start middle end");
        
        // Insert initializer list
        str = "begin finish";
        str.insert(str.begin() + 6, {'m', 'i', 'd', 'd', 'l', 'e', ' '});
        CHECK(str == "begin middle finish");
        
        // Insert at beginning
        str = "world";
        str.insert(str.begin(), "hello ");
        CHECK(str == "hello world");
        
        // Insert at end
        str = "hello";
        str.insert(str.end(), " world");
        CHECK(str == "hello world");
    }
    
    SUBCASE("insert boundary conditions and performance") {
        small::small_string str;
        
        // Insert into empty string
        str.insert(0, "first");
        CHECK(str == "first");
        
        // Multiple inserts at different positions
        str.insert(0, "the ");        // "the first"
        CHECK(str == "the first");
        
        str.insert(str.size(), " word");  // "the first word"
        CHECK(str == "the first word");
        
        str.insert(4, "very ");      // "the very first word"
        CHECK(str == "the very first word");
        
        // Insert across storage boundaries
        small::small_string boundary("ab");
        std::string long_insert(50, 'x');
        boundary.insert(1, long_insert);
        CHECK(boundary.size() == 52);
        CHECK(boundary[0] == 'a');
        CHECK(boundary[1] == 'x');
        CHECK(boundary[50] == 'x');
        CHECK(boundary[51] == 'b');
        
        // Large string insert
        small::small_string large(100, 'L');
        large.insert(50, "INSERTED");
        CHECK(large.size() == 108);
        CHECK(large.substr(50, 8) == "INSERTED");
        CHECK(large.substr(0, 50) == std::string(50, 'L'));
        CHECK(large.substr(58, 50) == std::string(50, 'L'));
        
        // Out of range insert
        small::small_string test("test");
        CHECK_THROWS_AS(test.insert(100, "invalid"), std::out_of_range);
        
        // Insert with very large count
        try {
            str.clear();
            str.insert(0, 1000000, 'M');  // Might throw or succeed
            if (str.size() == 1000000) {
                CHECK(str[0] == 'M');
                CHECK(str[999999] == 'M');
            }
        } catch (const std::exception&) {
            // Memory allocation failure is acceptable
        }
    }
}