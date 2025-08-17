#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"
#include <cstring>

TEST_CASE("null termination verification for c_str()") {
    small::small_string str("hello world");
    
    // Verify c_str() returns null-terminated string
    const char* cstr = str.c_str();
    CHECK(std::strlen(cstr) == str.size());
    CHECK(cstr[str.size()] == '\0');
    CHECK(std::strcmp(cstr, "hello world") == 0);
}

TEST_CASE("null termination verification for data() method") {
    small::small_string str("test string");
    
    // Verify data() points to null-terminated buffer
    const char* data_ptr = str.data();
    CHECK(data_ptr[str.size()] == '\0');
    CHECK(std::strlen(data_ptr) == str.size());
}

TEST_CASE("null termination after string modifications") {
    small::small_string str("initial");
    
    // Test null termination after append
    str.append(" text");
    CHECK(str.c_str()[str.size()] == '\0');
    CHECK(std::strlen(str.c_str()) == str.size());
    
    // Test null termination after insert
    str.insert(7, " inserted");
    CHECK(str.c_str()[str.size()] == '\0');
    CHECK(std::strlen(str.c_str()) == str.size());
    
    // Test null termination after erase
    str.erase(7, 9);  // Remove " inserted"
    CHECK(str.c_str()[str.size()] == '\0');
    CHECK(std::strlen(str.c_str()) == str.size());
}

TEST_CASE("null termination with resize operations") {
    small::small_string str("start");
    
    // Test null termination after resize larger
    str.resize(10, 'x');
    CHECK(str.c_str()[str.size()] == '\0');
    CHECK(str.c_str()[10] == '\0');
    CHECK(std::strlen(str.c_str()) == 10);
    
    // Test null termination after resize smaller
    str.resize(3);
    CHECK(str.c_str()[str.size()] == '\0');
    CHECK(str.c_str()[3] == '\0');
    CHECK(std::strlen(str.c_str()) == 3);
}

TEST_CASE("null termination with replace operations") {
    small::small_string str("replace_test_string");
    
    // Test null termination after replace
    str.replace(8, 4, "NEW");
    CHECK(str.c_str()[str.size()] == '\0');
    CHECK(std::strlen(str.c_str()) == str.size());
    CHECK(std::strcmp(str.c_str(), "replace_NEW_string") == 0);
    
    // Test null termination after replace with longer string
    str.replace(8, 3, "REPLACEMENT");
    CHECK(str.c_str()[str.size()] == '\0');
    CHECK(std::strlen(str.c_str()) == str.size());
}

TEST_CASE("null termination with large string operations") {
    // Test null termination with strings that trigger external allocation
    small::small_string large_str(1000, 'L');
    
    CHECK(large_str.c_str()[large_str.size()] == '\0');
    CHECK(large_str.c_str()[1000] == '\0');
    CHECK(std::strlen(large_str.c_str()) == 1000);
    
    // Test after modification
    large_str.append(500, 'A');
    CHECK(large_str.c_str()[large_str.size()] == '\0');
    CHECK(large_str.c_str()[1500] == '\0');
    CHECK(std::strlen(large_str.c_str()) == 1500);
}

TEST_CASE("null termination with PMR strings") {
    small::pmr::small_string pmr_str("pmr string test");
    
    // Verify PMR strings also maintain null termination
    CHECK(pmr_str.c_str()[pmr_str.size()] == '\0');
    CHECK(std::strlen(pmr_str.c_str()) == pmr_str.size());
    CHECK(std::strcmp(pmr_str.c_str(), "pmr string test") == 0);
    
    // Test after modification
    pmr_str += " appended";
    CHECK(pmr_str.c_str()[pmr_str.size()] == '\0');
    CHECK(std::strlen(pmr_str.c_str()) == pmr_str.size());
}

TEST_CASE("null termination with move and copy operations") {
    small::small_string original("original string");
    
    // Test null termination after copy construction
    small::small_string copied(original);
    CHECK(copied.c_str()[copied.size()] == '\0');
    CHECK(std::strcmp(copied.c_str(), "original string") == 0);
    
    // Test null termination after move construction
    small::small_string moved(std::move(copied));
    CHECK(moved.c_str()[moved.size()] == '\0');
    CHECK(std::strcmp(moved.c_str(), "original string") == 0);
    
    // Test null termination after assignment
    small::small_string assigned;
    assigned = original;
    CHECK(assigned.c_str()[assigned.size()] == '\0');
    CHECK(std::strcmp(assigned.c_str(), "original string") == 0);
}

TEST_CASE("null termination with empty string") {
    small::small_string empty_str;
    
    // Test null termination for empty string
    CHECK(empty_str.c_str()[0] == '\0');
    CHECK(std::strlen(empty_str.c_str()) == 0);
    CHECK(empty_str.size() == 0);
    
    // Test after making non-empty and then empty again
    empty_str = "temporary";
    empty_str.clear();
    CHECK(empty_str.c_str()[0] == '\0');
    CHECK(std::strlen(empty_str.c_str()) == 0);
    CHECK(empty_str.size() == 0);
}

TEST_CASE("null termination with iterator operations") {
    small::small_string str("iterator test");
    
    // Test null termination after iterator-based insert
    std::string to_insert("INSERTED");
    str.insert(str.begin() + 8, to_insert.begin(), to_insert.end());
    CHECK(str.c_str()[str.size()] == '\0');
    CHECK(std::strlen(str.c_str()) == str.size());
    
    // Test null termination after iterator-based erase
    str.erase(str.begin() + 8, str.begin() + 16);  // Remove "INSERTED"
    CHECK(str.c_str()[str.size()] == '\0');
    CHECK(std::strlen(str.c_str()) == str.size());
    CHECK(std::strcmp(str.c_str(), "iterator test") == 0);
}

TEST_CASE("null termination with string containing null characters") {
    // Test string that contains internal null characters
    small::small_string str("hello\0world", 11);  // Explicit length to include \0
    
    // The string should still be null-terminated at the end
    CHECK(str.size() == 11);
    CHECK(str.c_str()[11] == '\0');
    CHECK(str[5] == '\0');  // Internal null character
    
    // strlen will stop at first null, but our string continues beyond it
    CHECK(std::strlen(str.c_str()) == 5);  // stops at first \0
    
    // Test modification preserves null termination
    str.append("!");
    CHECK(str.size() == 12);
    CHECK(str.c_str()[12] == '\0');
}

TEST_CASE("null termination verification across buffer types") {
    // Test null termination for different internal buffer sizes
    
    // Very small string (likely internal buffer)
    small::small_string tiny("a");
    CHECK(tiny.c_str()[1] == '\0');
    CHECK(std::strlen(tiny.c_str()) == 1);
    
    // Medium string (likely short external buffer)
    small::small_string medium(50, 'm');
    CHECK(medium.c_str()[50] == '\0');
    CHECK(std::strlen(medium.c_str()) == 50);
    
    // Large string (likely long external buffer)
    small::small_string large(2000, 'L');
    CHECK(large.c_str()[2000] == '\0');
    CHECK(std::strlen(large.c_str()) == 2000);
}

TEST_CASE("null termination with substring operations") {
    small::small_string original("substring_test_string");
    
    // Test null termination of substring
    auto sub = original.substr(10, 4);  // "test"
    CHECK(sub.c_str()[sub.size()] == '\0');
    CHECK(sub.c_str()[4] == '\0');
    CHECK(std::strlen(sub.c_str()) == 4);
    CHECK(std::strcmp(sub.c_str(), "test") == 0);
}

// ============================================================================
// Tests for NullTerminated=false behavior (small_byte_string)
// ============================================================================

TEST_CASE("non_null_terminated verification for data() method") {
    small::small_byte_string str("hello world");
    
    // Verify data() points to string data (no null termination guaranteed)
    const char* data_ptr = str.data();
    CHECK(data_ptr != nullptr);
    CHECK(str.size() == 11);
    // Note: We cannot check data_ptr[str.size()] == '\0' because it's not guaranteed
    // The string contains the data but may not be null-terminated
}

TEST_CASE("non_null_terminated after string modifications") {
    small::small_byte_string str("initial");
    
    // Test behavior after append
    str.append(" text");
    CHECK(str.size() == 12);
    CHECK(std::string_view(str.data(), str.size()) == "initial text");
    
    // Test behavior after insert
    str.insert(7, " inserted");
    CHECK(str.size() == 21);
    CHECK(std::string_view(str.data(), str.size()) == "initial inserted text");
    
    // Test behavior after erase
    str.erase(7, 9);  // Remove " inserted"
    CHECK(str.size() == 12);
    CHECK(std::string_view(str.data(), str.size()) == "initial text");
}

TEST_CASE("non_null_terminated with resize operations") {
    small::small_byte_string str("start");
    
    // Test behavior after resize larger
    str.resize(10, 'x');
    CHECK(str.size() == 10);
    CHECK(std::string_view(str.data(), str.size()) == "startxxxxx");
    
    // Test behavior after resize smaller
    str.resize(3);
    CHECK(str.size() == 3);
    CHECK(std::string_view(str.data(), str.size()) == "sta");
}

TEST_CASE("non_null_terminated with replace operations") {
    small::small_byte_string str("replace_test_string");
    
    // Test behavior after replace
    str.replace(8, 4, "NEW");
    CHECK(str.size() == 18);
    CHECK(std::string_view(str.data(), str.size()) == "replace_NEW_string");
    
    // Test behavior after replace with longer string
    str.replace(8, 3, "REPLACEMENT");
    CHECK(str.size() == 26);
    CHECK(std::string_view(str.data(), str.size()) == "replace_REPLACEMENT_string");
}

TEST_CASE("non_null_terminated with large string operations") {
    // Test behavior with strings that trigger external allocation
    small::small_byte_string large_str(1000, 'L');
    
    CHECK(large_str.size() == 1000);
    CHECK(large_str.data() != nullptr);
    
    // Test after modification
    large_str.append(500, 'A');
    CHECK(large_str.size() == 1500);
    // Verify the content is correct by checking first and last characters
    CHECK(large_str[0] == 'L');
    CHECK(large_str[999] == 'L');
    CHECK(large_str[1000] == 'A');
    CHECK(large_str[1499] == 'A');
}

TEST_CASE("non_null_terminated with PMR strings") {
    small::pmr::small_byte_string pmr_str("pmr string test");
    
    // Verify PMR byte strings behavior
    CHECK(pmr_str.size() == 15);
    CHECK(std::string_view(pmr_str.data(), pmr_str.size()) == "pmr string test");
    
    // Test after modification
    pmr_str += " appended";
    CHECK(pmr_str.size() == 24);
    CHECK(std::string_view(pmr_str.data(), pmr_str.size()) == "pmr string test appended");
}

TEST_CASE("non_null_terminated with move and copy operations") {
    small::small_byte_string original("original string");
    
    // Test behavior after copy construction
    small::small_byte_string copied(original);
    CHECK(copied.size() == 15);
    CHECK(std::string_view(copied.data(), copied.size()) == "original string");
    
    // Test behavior after move construction
    small::small_byte_string moved(std::move(copied));
    CHECK(moved.size() == 15);
    CHECK(std::string_view(moved.data(), moved.size()) == "original string");
    
    // Test behavior after assignment
    small::small_byte_string assigned;
    assigned = original;
    CHECK(assigned.size() == 15);
    CHECK(std::string_view(assigned.data(), assigned.size()) == "original string");
}

TEST_CASE("non_null_terminated with empty string") {
    small::small_byte_string empty_str;
    
    // Test behavior for empty string
    CHECK(empty_str.size() == 0);
    CHECK(empty_str.data() != nullptr);  // data() should still return valid pointer
    
    // Test after making non-empty and then empty again
    empty_str = "temporary";
    empty_str.clear();
    CHECK(empty_str.size() == 0);
}

TEST_CASE("non_null_terminated with iterator operations") {
    small::small_byte_string str("iterator test");
    
    // Test behavior after iterator-based insert
    std::string to_insert("INSERTED");
    str.insert(str.begin() + 8, to_insert.begin(), to_insert.end());
    CHECK(str.size() == 21);
    CHECK(std::string_view(str.data(), str.size()) == "iteratorINSERTED test");
    
    // Test behavior after iterator-based erase
    str.erase(str.begin() + 8, str.begin() + 16);  // Remove "INSERTED"
    CHECK(str.size() == 13);
    CHECK(std::string_view(str.data(), str.size()) == "iterator test");
}

TEST_CASE("non_null_terminated with string containing null characters") {
    // Test string that contains internal null characters
    small::small_byte_string str("hello\0world", 11);  // Explicit length to include \0
    
    // The string should contain the null character internally
    CHECK(str.size() == 11);
    CHECK(str[5] == '\0');  // Internal null character
    
    // data() should return pointer to the full data including internal nulls
    CHECK(std::string_view(str.data(), str.size()).size() == 11);
    
    // Test modification preserves internal structure
    str.append("!");
    CHECK(str.size() == 12);
    CHECK(str[5] == '\0');  // Internal null still there
}

TEST_CASE("non_null_terminated verification across buffer types") {
    // Test behavior for different internal buffer sizes
    
    // Very small string (likely internal buffer)
    small::small_byte_string tiny("a");
    CHECK(tiny.size() == 1);
    CHECK(std::string_view(tiny.data(), tiny.size()) == "a");
    
    // Medium string (likely short external buffer)
    small::small_byte_string medium(50, 'm');
    CHECK(medium.size() == 50);
    CHECK(medium[0] == 'm');
    CHECK(medium[49] == 'm');
    
    // Large string (likely long external buffer)
    small::small_byte_string large(2000, 'L');
    CHECK(large.size() == 2000);
    CHECK(large[0] == 'L');
    CHECK(large[1999] == 'L');
}

TEST_CASE("non_null_terminated with substring operations") {
    small::small_byte_string original("substring_test_string");
    
    // Test behavior of substring
    auto sub = original.substr(10, 4);  // "test"
    CHECK(sub.size() == 4);
    CHECK(std::string_view(sub.data(), sub.size()) == "test");
}

TEST_CASE("comparison between null_terminated and non_null_terminated") {
    small::small_string null_term_str("test string");
    small::small_byte_string non_null_str("test string");
    
    // Both should have same content and size
    CHECK(null_term_str.size() == non_null_str.size());
    
    // Content should be identical when viewed as string_view
    CHECK(std::string_view(null_term_str.data(), null_term_str.size()) == 
          std::string_view(non_null_str.data(), non_null_str.size()));
    
    // null_term_str.c_str() should be null-terminated
    CHECK(null_term_str.c_str()[null_term_str.size()] == '\0');
    CHECK(std::strlen(null_term_str.c_str()) == null_term_str.size());
    
    // non_null_str should work with data() but no guarantee about null termination
    CHECK(std::string_view(non_null_str.data(), non_null_str.size()) == "test string");
}

TEST_CASE("capacity differences between null_terminated and non_null_terminated") {
    // For same buffer size, non-null-terminated should have 1 more character capacity
    small::small_string null_term_str;
    small::small_byte_string non_null_str;
    
    // Reserve same amount and check capacity
    null_term_str.reserve(100);
    non_null_str.reserve(100);
    
    // Both should have adequate capacity, but exact values depend on implementation
    CHECK(null_term_str.capacity() >= 100);
    CHECK(non_null_str.capacity() >= 100);
    
    // For small strings, non-null version might have +1 capacity
    null_term_str.clear();
    non_null_str.clear();
    
    // Internal buffer capacity should differ by 1
    CHECK(non_null_str.capacity() == null_term_str.capacity() + 1);
}