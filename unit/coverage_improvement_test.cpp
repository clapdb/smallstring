#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"
#include <stdexcept>

TEST_CASE("insert function out_of_range exception handling") {
    small::small_string str("test");
    
    // Test insert(index, count, ch) with invalid index
    CHECK_THROWS_AS(str.insert(10, 5, 'c'), std::out_of_range);
    
    // Test insert(index, str) with invalid index  
    CHECK_THROWS_AS(str.insert(10, "invalid"), std::out_of_range);
    
    // Test insert(index, str, count) with invalid index
    CHECK_THROWS_AS(str.insert(10, "test", 4), std::out_of_range);
}

TEST_CASE("insert function with count=0 no-op behavior") {
    small::small_string str("test");
    auto original = str;
    
    // Test insert with count = 0 should do nothing
    str.insert(1, 0, 'x');
    CHECK(str == original);
    
    str.insert(1, "", 0);
    CHECK(str == original);
}

TEST_CASE("erase function out_of_range exception handling") {
    small::small_string str("test");
    
    // Test erase with invalid index
    CHECK_THROWS_AS(str.erase(10, 5), std::out_of_range);
}

TEST_CASE("replace function out_of_range exception handling") {
    small::small_string str("test");
    
    // Test replace(pos, count, str) with invalid position
    CHECK_THROWS_AS(str.replace(10, 1, "replacement"), std::out_of_range);
    
    // Test replace(pos, count, count2, ch) with invalid position
    CHECK_THROWS_AS(str.replace(10, 1, 5, 'x'), std::out_of_range);
    
    // Test replace(pos, count, str, count2) with invalid position
    CHECK_THROWS_AS(str.replace(10, 1, "replacement", 11), std::out_of_range);
}

TEST_CASE("replace function count2=0 erase behavior") {
    small::small_string str("hello world");
    
    // Test replace with count2 = 0 (equivalent to erase)
    str.replace(6, 5, 0, 'x');  // Remove "world"
    CHECK(str == "hello ");
    
    str = "hello world";
    str.replace(6, 5, "", 0);   // Remove "world" with empty string
    CHECK(str == "hello ");
}

TEST_CASE("copy function out_of_range exception handling") {
    small::small_string str("test");
    char buffer[10];
    
    // Test copy with invalid position
    CHECK_THROWS_AS(str.copy(buffer, 0, 10), std::out_of_range);
}

TEST_CASE("copy function boundary conditions") {
    small::small_string str("copy_test_string");
    char buffer[20] = {0};
    
    // Test copy with different positions and counts
    auto copied = str.copy(buffer, 4, 5);  // Copy "test"
    CHECK(copied == 4);
    CHECK(std::string(buffer, 4) == "test");
    
    // Test copy beyond string end
    std::fill(buffer, buffer + 20, 0);
    copied = str.copy(buffer, 100, 10);  // Should copy from pos 10 to end
    CHECK(copied == 6);  // "string"
    CHECK(std::string(buffer, 6) == "string");
}

TEST_CASE("substr function out_of_range exception handling") {
    small::small_string str("test");
    
    // Test substr with invalid position
    CHECK_THROWS_AS([[maybe_unused]] auto result = str.substr(10), std::out_of_range);
    CHECK_THROWS_AS([[maybe_unused]] auto result = str.substr(10, 5), std::out_of_range);
}

TEST_CASE("resize function with custom fill character") {
    small::small_string str("start");
    
    // Test resize larger with custom character
    str.resize(10, '*');
    CHECK(str == "start*****");
    CHECK(str.size() == 10);
    
    // Test resize smaller 
    str.resize(3);
    CHECK(str == "sta");
    CHECK(str.size() == 3);
    
    // Test resize to same size (should be no-op)
    str.resize(3, 'X');
    CHECK(str == "sta");
}

TEST_CASE("reserve function with various capacities") {
    small::small_string str;
    
    // Test reserve with small capacity
    str.reserve(5);
    CHECK(str.capacity() >= 5);
    
    // Test reserve with large capacity to trigger long buffer
    str.reserve(5000);
    CHECK(str.capacity() >= 5000);
}

TEST_CASE("large string operations triggering buffer transitions") {
    // Test transitions between internal, short, median, and long buffers
    small::small_string str;
    
    // Start very small (internal buffer)
    str = "a";
    CHECK(str.size() == 1);
    
    // Grow to trigger external allocation
    str.append(50, 'b');
    CHECK(str.size() == 51);
    CHECK(str[0] == 'a');
    CHECK(str[50] == 'b');
    
    // Continue growing to trigger long buffer
    str.append(2000, 'c');
    CHECK(str.size() == 2051);
    CHECK(str[2050] == 'c');
}

TEST_CASE("large string replace operations") {
    small::small_string large_str(1000, 'L');
    
    // Test replace on large strings
    large_str.replace(0, 100, 50, 'R');
    CHECK(large_str.size() == 950);
    CHECK(large_str[0] == 'R');
    CHECK(large_str[49] == 'R');
    CHECK(large_str[50] == 'L');
}

TEST_CASE("pmr_small_string operations") {
    // Test PMR string operations
    small::pmr::small_string pmr_str("pmr test");
    small::small_string regular_str("regular");
    
    // Test mixed operations between PMR and regular strings
    regular_str.append(pmr_str);
    CHECK(regular_str == "regularpmr test");
    
    // Test PMR string with large allocations
    small::pmr::small_string large_pmr(2000, 'P');
    CHECK(large_pmr.size() == 2000);
    CHECK(large_pmr[0] == 'P');
    CHECK(large_pmr[1999] == 'P');
}

TEST_CASE("iterator insert operations") {
    small::small_string str("original");
    std::string source("insert_me");
    
    // Test insert with iterator range
    str.insert(str.begin() + 3, source.begin(), source.end());
    CHECK(str == "oriinsert_meginal");
}

TEST_CASE("iterator replace operations") {
    small::small_string str("replace_test");
    std::string replacement("NEW");
    
    // Test replace with iterator range  
    str.replace(str.begin() + 8, str.begin() + 12, replacement.begin(), replacement.end());
    CHECK(str == "replace_NEW");
}

TEST_CASE("string_view insert operations") {
    small::small_string str("string_view_test");
    
    // Test insert with string_view at different positions
    std::string_view sv("INSERT");
    str.insert(str.begin(), sv);
    CHECK(str == "INSERTstring_view_test");
    
    str = "string_view_test";
    str.insert(str.end(), sv);
    CHECK(str == "string_view_testINSERT");
    
    // Test with substring of string_view
    str = "base";
    str.insert(str.begin() + 2, sv, 2, 3);  // Insert "SER"
    CHECK(str == "baSERse");
}

TEST_CASE("find function boundary conditions") {
    small::small_string str("boundary_find_test");
    
    // Test find at exact string length
    CHECK(str.find("", str.size()) == str.size());
    CHECK(str.find("x", str.size()) == small::small_string::npos);
    
    // Test find with patterns longer than remaining string
    CHECK(str.find("test_long_pattern", 15) == small::small_string::npos);
}

TEST_CASE("find_last_of function with empty search string") {
    small::small_string str("search_in_this_string");
    
    // Test find_last_of with empty search string
    CHECK(str.find_last_of("") == small::small_string::npos);
}

TEST_CASE("find_last_not_of function with all characters excluded") {
    small::small_string vowels("aeiou");
    
    // Test find_last_not_of with all characters in string excluded
    CHECK(vowels.find_last_not_of("aeiou") == small::small_string::npos);
}

TEST_CASE("find_first_not_of function with all characters excluded") {
    small::small_string vowels("aeiou");
    
    // Test find_first_not_of with all characters in string excluded
    CHECK(vowels.find_first_not_of("aeiou") == small::small_string::npos);
}

TEST_CASE("comparison operators comprehensive coverage") {
    small::small_string str1("compare");
    small::small_string str2("compare");
    small::small_string str3("different");
    
    // Test all comparison operators for coverage
    CHECK(str1 == str2);
    CHECK_FALSE(str1 == str3);
    CHECK_FALSE(str1 != str2);
    CHECK(str1 != str3);
    CHECK_FALSE(str1 < str2);
    CHECK(str1 < str3);
    CHECK(str1 <= str2);
    CHECK(str1 <= str3);
    CHECK_FALSE(str1 > str2);
    CHECK_FALSE(str1 > str3);
    CHECK(str1 >= str2);
    CHECK_FALSE(str1 >= str3);
}

TEST_CASE("append function with empty parameters") {
    small::small_string str("base");
    
    // Test append operations with empty/zero parameters
    str.append("", 0);
    CHECK(str == "base");
    
    str.append(0, 'x');
    CHECK(str == "base");
}

TEST_CASE("empty string operations") {
    small::small_string str;
    
    // Test various operations on empty string
    str.insert(0, "", 0);
    CHECK(str.empty());
    
    // Note: replace on empty string at position 0 with 0 count is valid
    str.append("temp");  // Make non-empty first
    str.replace(0, 4, "", 0);  // Replace all with empty
    CHECK(str.empty());
    
    str.erase(static_cast<small::small_string::size_type>(0), static_cast<small::small_string::size_type>(0));
    CHECK(str.empty());
}