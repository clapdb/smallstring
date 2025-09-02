#include <iterator>
#include <algorithm>
#include <numeric>
#include <sstream>
#include "include/smallstring.hpp"
#include "doctest/doctest/doctest.h"

TEST_CASE("smallstring manipulation operations") {
    SUBCASE("comprehensive insert operations") {
        small::small_string str("hello");
        
        // Insert character at beginning
        str.insert(str.begin(), 'A');
        CHECK(str == "Ahello");
        
        // Insert string in middle
        str.insert(1, "BC");
        CHECK(str == "ABChello");
        
        // Insert at end
        str.insert(str.size(), "XYZ");
        CHECK(str == "ABChelloXYZ");
        
        // Insert substring
        small::small_string other("123456");
        str.insert(3, other, 1, 3); // Insert "234" at position 3
        CHECK(str == "ABC234helloXYZ");
        
        // Insert multiple characters  
        str.insert(str.begin() + 6, 2, '!');
        CHECK(str == "ABC234!!helloXYZ");
    }
    
    SUBCASE("comprehensive erase operations") {
        small::small_string str("abcdefghijk");
        
        // Erase single character
        str.erase(5, 1); // Remove 'f'
        CHECK(str == "abcdeghijk");
        
        // Erase range
        str.erase(3, 3); // Remove "deg"
        CHECK(str == "abchijk");
        
        // Erase from position to end
        str.erase(4);
        CHECK(str == "abch");
        
        // Erase using iterators
        str.erase(str.begin() + 1, str.begin() + 3); // Remove "bc"
        CHECK(str == "ah");
        
        // Erase single character using iterator
        str.erase(str.begin()); // Remove 'a'
        CHECK(str == "h");
    }
    
    SUBCASE("comprehensive replace operations") {
        small::small_string str("hello world");
        
        // Replace with string literal
        str.replace(6, 5, "universe");
        CHECK(str == "hello universe");
        
        // Replace with single character
        str.replace(0, 5, "hi");
        CHECK(str == "hi universe");
        
        // Replace with repeated character - "hi universe" -> replace pos 3 to 11 with 5 X's
        // "hi " + "XXXXX" + remaining = "hi XXXXX" + "" = "hi XXXXX" 
        str.replace(str.begin() + 3, str.begin() + 11, 5, 'X');
        CHECK(str == "hi XXXXX");
        
        // Replace using iterators - "hi XXXXX" -> replace from pos 2 to end-1 with "test"
        // "hi" + "test" + last char = "hi" + "test" + "X" = "hitestX"
        str.replace(str.begin() + 2, str.end() - 1, "test");
        CHECK(str == "hitestX");
        
        // Replace with substring of another string - "hitestX" -> replace pos 2, len 4 with "bcd"
        // "hi" + "bcd" + remaining = "hi" + "bcd" + "X" = "hibcdX"
        small::small_string other("abcdef");
        str.replace(2, 4, other, 1, 3); // Replace with "bcd"
        CHECK(str == "hibcdX");
    }
    
    SUBCASE("find operations comprehensive") {
        small::small_string str("hello world hello");
        
        // Find character
        CHECK(str.find('l') == 2);
        CHECK(str.find('l', 3) == 3);
        CHECK(str.find('l', 4) == 9);
        CHECK(str.find('z') == small::small_string::npos);
        
        // Find string
        CHECK(str.find("hello") == 0);
        CHECK(str.find("hello", 1) == 12);
        CHECK(str.find("world") == 6);
        CHECK(str.find("xyz") == small::small_string::npos);
        
        // Find with count
        CHECK(str.find("ell", 0, 3) == 1);
        CHECK(str.find("ell", 0, 2) == 1); // "el" matches at position 1 of "hello"
        
        // Reverse find
        CHECK(str.rfind('l') == 15);
        CHECK(str.rfind('l', 14) == 14);
        CHECK(str.rfind('h') == 12);
        CHECK(str.rfind("hello") == 12);
        CHECK(str.rfind("world") == 6);
    }
    
    SUBCASE("find_first_of and find_last_of") {
        small::small_string str("hello, world!");
        
        // Find first of character set
        CHECK(str.find_first_of("aeiou") == 1); // 'e'
        CHECK(str.find_first_of("xyz") == small::small_string::npos);
        CHECK(str.find_first_of("wo") == 4); // 'o' in "hello"
        
        // Find last of character set
        CHECK(str.find_last_of("aeiou") == 8); // 'o'
        CHECK(str.find_last_of("l") == 10); // last 'l'
        CHECK(str.find_last_of("xyz") == small::small_string::npos);
        
        // Find first not of
        CHECK(str.find_first_not_of("hel") == 4); // ','
        CHECK(str.find_first_not_of("abcdefghijklmnopqrstuvwxyz") == 5); // ','
        
        // Find last not of
        CHECK(str.find_last_not_of("!") == 11); // 'd'
        CHECK(str.find_last_not_of("d!") == 10); // 'l'
    }
    
    SUBCASE("prefix and suffix checking") {
        small::small_string str("hello world");
        
        // Manual implementation for testing prefix/suffix functionality
        auto manual_starts_with = [](const small::small_string& s, const char* prefix) {
            auto prefix_len = strlen(prefix);
            if (prefix_len > s.size()) return false;
            return s.substr(0, prefix_len) == prefix;
        };
        
        auto manual_ends_with = [](const small::small_string& s, const char* suffix) {
            auto suffix_len = strlen(suffix);
            if (suffix_len > s.size()) return false;
            return s.substr(s.size() - suffix_len) == suffix;
        };
        
        CHECK(manual_starts_with(str, "hello"));
        CHECK(!manual_starts_with(str, "world"));
        CHECK(manual_ends_with(str, "world"));
        CHECK(!manual_ends_with(str, "hello"));
        
        // Test with single characters by comparing first/last
        CHECK(str.front() == 'h');
        CHECK(str.back() == 'd');
        CHECK(str.front() != 'w');
        CHECK(str.back() != 'h');
    }
}

TEST_CASE("smallstring algorithm compatibility") {
    SUBCASE("standard algorithms work with smallstring") {
        small::small_string str("hello");
        
        // std::transform
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        CHECK(str == "HELLO");
        
        // std::reverse
        std::reverse(str.begin(), str.end());
        CHECK(str == "OLLEH");
        
        // std::sort
        std::sort(str.begin(), str.end());
        CHECK(str == "EHLLO");
        
        // std::count
        auto l_count = std::count(str.begin(), str.end(), 'L');
        CHECK(l_count == 2);
        
        // std::find
        auto found = std::find(str.begin(), str.end(), 'H');
        CHECK(found != str.end());
        CHECK(*found == 'H');
        
        // std::accumulate (with char to int conversion)
        auto sum = std::accumulate(str.begin(), str.end(), 0,
            [](int acc, char c) { return acc + static_cast<int>(c); });
        CHECK(sum > 0); // Should be sum of ASCII values
    }
    
    SUBCASE("range-based algorithms") {
        small::small_string str("programming");
        
        // Count specific characters
        auto m_count = std::count(str.begin(), str.end(), 'm');
        CHECK(m_count == 2);
        
        // Find if any character satisfies condition
        auto has_vowel = std::any_of(str.begin(), str.end(), [](char c) {
            return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
        });
        CHECK(has_vowel);
        
        // Check if all characters are lowercase
        auto all_lower = std::all_of(str.begin(), str.end(), [](char c) {
            return c >= 'a' && c <= 'z';
        });
        CHECK(all_lower);
        
        // None of condition
        auto no_digits = std::none_of(str.begin(), str.end(), [](char c) {
            return c >= '0' && c <= '9';
        });
        CHECK(no_digits);
    }
    
    SUBCASE("iterator arithmetic and random access") {
        small::small_string str("0123456789");
        
        auto it = str.begin();
        auto end = str.end();
        
        // Iterator arithmetic
        CHECK(*(it + 5) == '5');
        CHECK(*(end - 1) == '9');
        CHECK(*(end - 3) == '7');
        
        // Iterator comparison
        CHECK(it < end);
        CHECK(it <= end);
        CHECK(end > it);
        CHECK(end >= it);
        CHECK((it + 5) < end);
        CHECK((it + 5) <= (it + 7));
        
        // Iterator difference
        CHECK((end - it) == 10);
        CHECK(((it + 7) - (it + 3)) == 4);
        
        // Random access
        CHECK(it[3] == '3');
        CHECK(it[9] == '9');
    }
}

TEST_CASE("smallstring stream operations") {
    SUBCASE("output stream operator") {
        small::small_string str("test output");
        std::ostringstream oss;
        oss << str;
        CHECK(oss.str() == "test output");
        
        // Test with empty string
        small::small_string empty;
        std::ostringstream oss2;
        oss2 << empty;
        CHECK(oss2.str() == "");
        
        // Test with string containing spaces and special chars
        small::small_string special("hello\tworld\n");
        std::ostringstream oss3;
        oss3 << special;
        CHECK(oss3.str() == "hello\tworld\n");
    }
    
    SUBCASE("input stream operator") {
        std::istringstream iss("input test");
        small::small_string str;
        iss >> str;
        CHECK(str == "input"); // Should stop at whitespace
        
        iss >> str; // Read next word
        CHECK(str == "test");
        
        // Test with empty input
        std::istringstream empty_iss("");
        small::small_string empty_str;
        empty_iss >> empty_str; // This should fail
        bool result = empty_iss.fail() || empty_str.empty();
        CHECK(result);
    }
    
    SUBCASE("string_view operations") {
        small::small_string str("test string");
        
        // Test conversion to string_view
        std::string_view sv = str;
        CHECK(sv == "test string");
        CHECK(sv.size() == str.size());
        
        // Test operations using string_view
        CHECK(sv.substr(5) == "string");
        CHECK(sv.find("test") == 0);
        CHECK(sv.find("string") == 5);
    }
}

TEST_CASE("smallstring concatenation operations") {
    SUBCASE("string concatenation operators") {
        small::small_string str1("hello");
        small::small_string str2("world");
        
        // String + String
        auto result1 = str1 + str2;
        CHECK(result1 == "helloworld");
        
        // String + const char*
        auto result2 = str1 + " there";
        CHECK(result2 == "hello there");
        
        // const char* + String
        auto result3 = "hi " + str2;
        CHECK(result3 == "hi world");
        
        // String + char
        auto result4 = str1 + '!';
        CHECK(result4 == "hello!");
        
        // char + String
        auto result5 = '>' + str1;
        CHECK(result5 == ">hello");
    }
    
    SUBCASE("move semantics in concatenation") {
        small::small_string str1("hello");
        small::small_string str2("world");
        
        // Test move + copy
        auto result1 = std::move(str1) + str2;
        CHECK(result1 == "helloworld");
        // str1 is now in moved-from state
        
        // Test copy + move
        small::small_string str3("test");
        auto result2 = str3 + std::move(str2);
        CHECK(result2 == "testworld");
        CHECK(str3 == "test"); // str3 should be unchanged
        
        // Test move + move
        small::small_string str4("a");
        small::small_string str5("b");
        auto result3 = std::move(str4) + std::move(str5);
        CHECK(result3 == "ab");
    }
    
    SUBCASE("compound assignment operators") {
        small::small_string str("base");
        
        // += with string
        str += " case";
        CHECK(str == "base case");
        
        // += with const char*
        str += " test";
        CHECK(str == "base case test");
        
        // += with char
        str += '!';
        CHECK(str == "base case test!");
        
        // += with another small_string
        small::small_string suffix(" end");
        str += suffix;
        CHECK(str == "base case test! end");
        
        // += with initializer list
        str += {'#', '#'};
        CHECK(str == "base case test! end##");
    }
}