#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"
#include <array>
#include <memory_resource>

// Tests targeting uncovered lines and buffer type transitions

TEST_CASE("internal buffer type operations - very small strings") {
    // Test strings 0-7 characters to trigger internal buffer (CoreType::Internal)
    small::small_string empty_str;
    CHECK(empty_str.size() == 0);
    CHECK(empty_str.capacity() >= 0);
    
    small::small_string one_char("a");
    CHECK(one_char.size() == 1);
    CHECK(one_char.capacity() >= 1);
    CHECK(one_char[0] == 'a');
    
    small::small_string seven_chars("1234567");
    CHECK(seven_chars.size() == 7);
    CHECK(seven_chars.capacity() >= 7);
    
    // Test internal buffer size increase
    empty_str.push_back('x');
    CHECK(empty_str.size() == 1);
    CHECK(empty_str[0] == 'x');
    
    // Test internal buffer size decrease  
    seven_chars.pop_back();
    CHECK(seven_chars.size() == 6);
    CHECK(seven_chars == "123456");
}

TEST_CASE("short buffer type operations - medium strings") {
    // Test strings 8-256 characters to trigger short buffer (CoreType::Short)
    std::string base_8_chars = "12345678";
    small::small_string short_str(base_8_chars);
    CHECK(short_str.size() == 8);
    CHECK(short_str == base_8_chars);
    
    // Test 64-character string
    std::string base_64(64, 'A');
    small::small_string medium_str(base_64);
    CHECK(medium_str.size() == 64);
    CHECK(medium_str == base_64);
    
    // Test 255-character string (near short buffer limit)
    std::string base_255(255, 'B');
    small::small_string large_short(base_255);
    CHECK(large_short.size() == 255);
    CHECK(large_short[0] == 'B');
    CHECK(large_short[254] == 'B');
    
    // Test capacity and idle calculations for short buffers
    short_str.reserve(100);
    CHECK(short_str.capacity() >= 100);
    
    // Test size operations on short buffers
    short_str.resize(32, 'Z');
    CHECK(short_str.size() == 32);
    CHECK(short_str[31] == 'Z');
}

TEST_CASE("median and long buffer operations - large strings") {
    // Test strings > 256 characters to trigger median/long buffers
    std::string base_300(300, 'M');
    small::small_string median_str(base_300);
    CHECK(median_str.size() == 300);
    CHECK(median_str[0] == 'M');
    CHECK(median_str[299] == 'M');
    
    // Test very large string (long buffer)
    std::string base_2000(2000, 'L');
    small::small_string long_str(base_2000);
    CHECK(long_str.size() == 2000);
    CHECK(long_str[0] == 'L');
    CHECK(long_str[1999] == 'L');
    
    // Test buffer header operations for median/long
    median_str.reserve(500);
    CHECK(median_str.capacity() >= 500);
    
    // Test size operations that affect buffer headers
    long_str.resize(1500);
    CHECK(long_str.size() == 1500);
    
    // Test append operations on large strings
    median_str.append(100, 'X');
    CHECK(median_str.size() == 400);
    CHECK(median_str[399] == 'X');
}

TEST_CASE("buffer type transitions during growth") {
    // Test progression: internal -> short -> median -> long
    small::small_string transition_str;
    
    // Start with internal buffer (< 8 chars)
    transition_str = "abc";
    CHECK(transition_str.size() == 3);
    
    // Grow to short buffer (8-256 chars)
    transition_str.append(50, 'S');
    CHECK(transition_str.size() == 53);
    CHECK(transition_str.capacity() >= 53);
    
    // Grow to median buffer (> 256 chars)
    transition_str.append(250, 'M');
    CHECK(transition_str.size() == 303);
    CHECK(transition_str.capacity() >= 303);
    
    // Grow to long buffer (> capacity of median)
    transition_str.append(2000, 'L');
    CHECK(transition_str.size() == 2303);
    CHECK(transition_str.capacity() >= 2303);
}

TEST_CASE("PMR allocator with different buffer types") {
    // Test PMR allocator paths for all buffer sizes
    std::array<std::byte, 4096> buffer;
    std::pmr::monotonic_buffer_resource mbr{static_cast<void*>(buffer.data()), buffer.size()};
    std::pmr::polymorphic_allocator<char> alloc{&mbr};
    
    // Internal buffer with PMR
    small::pmr::small_string pmr_internal("short", alloc);
    CHECK(pmr_internal.size() == 5);
    CHECK(pmr_internal == "short");
    
    // Short buffer with PMR  
    std::string medium_content(100, 'P');
    small::pmr::small_string pmr_short(medium_content, alloc);
    CHECK(pmr_short.size() == 100);
    CHECK(pmr_short[0] == 'P');
    
    // Median buffer with PMR
    std::string large_content(400, 'Q');
    small::pmr::small_string pmr_median(large_content, alloc);
    CHECK(pmr_median.size() == 400);
    CHECK(pmr_median[0] == 'Q');
    
    // Test PMR string operations
    pmr_internal.append(" PMR");
    CHECK(pmr_internal == "short PMR");
    
    // Test PMR buffer transitions
    pmr_short.append(200, 'X');
    CHECK(pmr_short.size() == 300);
    CHECK(pmr_short[299] == 'X');
}

TEST_CASE("PMR allocator non-null-terminated strings") {
    // Test PMR byte strings (NullTerminated=false)
    std::array<std::byte, 2048> buffer;
    std::pmr::monotonic_buffer_resource mbr{static_cast<void*>(buffer.data()), buffer.size()};
    std::pmr::polymorphic_allocator<char> alloc{&mbr};
    
    small::pmr::small_byte_string pmr_byte("PMR byte", alloc);
    CHECK(pmr_byte.size() == 8);
    CHECK(std::string_view(pmr_byte.data(), pmr_byte.size()) == "PMR byte");
    
    // Test operations on PMR byte strings
    pmr_byte.append(" string");
    CHECK(pmr_byte.size() == 15);
    CHECK(std::string_view(pmr_byte.data(), pmr_byte.size()) == "PMR byte string");
    
    // Test large PMR byte string
    std::string large_byte_content(500, 'B');
    small::pmr::small_byte_string large_pmr_byte(large_byte_content, alloc);
    CHECK(large_pmr_byte.size() == 500);
    CHECK(large_pmr_byte[0] == 'B');
}

TEST_CASE("move assignment and self-assignment edge cases") {
    small::small_string source("move source");
    small::small_string target("move target");
    
    // Test regular move assignment
    target = std::move(source);
    CHECK(target == "move source");
    
    // Test self-assignment (should be handled safely)
    small::small_string self_assign("self");
    small::small_string& self_ref = self_assign;
    self_assign = std::move(self_ref);
    CHECK(self_assign.size() >= 0); // Should not crash
    
    // Test move constructor
    small::small_string moved_construct(std::move(target));
    CHECK(moved_construct == "move source");
}

TEST_CASE("end iterator operations for different buffer types") {
    // Test end() method for internal buffer
    small::small_string internal_str("abc");
    auto internal_end = internal_str.end();
    auto internal_begin = internal_str.begin();
    CHECK(internal_end - internal_begin == 3);
    
    // Test end() for short buffer
    small::small_string short_str(50, 'S');
    auto short_end = short_str.end();
    auto short_begin = short_str.begin();
    CHECK(short_end - short_begin == 50);
    
    // Test end() for median buffer
    small::small_string median_str(300, 'M');
    auto median_end = median_str.end();
    auto median_begin = median_str.begin();
    CHECK(median_end - median_begin == 300);
    
    // Test const end() iterator
    const auto& const_str = internal_str;
    auto const_end = const_str.end();
    auto const_begin = const_str.begin();
    CHECK(const_end - const_begin == 3);
}

TEST_CASE("capacity and idle capacity calculations") {
    // Test capacity calculations for different buffer types
    small::small_string test_str;
    
    // Test initial capacity (internal buffer)
    auto initial_capacity = test_str.capacity();
    CHECK(initial_capacity >= 0);
    
    // Force growth to short buffer
    test_str.reserve(50);
    auto short_capacity = test_str.capacity();
    CHECK(short_capacity >= 50);
    
    // Force growth to median buffer
    test_str.reserve(300);
    auto median_capacity = test_str.capacity();
    CHECK(median_capacity >= 300);
    
    // Test idle capacity by checking available space
    test_str = "test";
    auto used_size = test_str.size();
    auto total_capacity = test_str.capacity();
    CHECK(total_capacity >= used_size);
}

TEST_CASE("string boundary operations") {
    // Test exact boundary conditions
    
    // Test 7-character string (internal buffer boundary)
    small::small_string boundary_7("1234567");
    CHECK(boundary_7.size() == 7);
    boundary_7.push_back('8'); // Should transition to external
    CHECK(boundary_7.size() == 8);
    CHECK(boundary_7 == "12345678");
    
    // Test 256-character string (short buffer boundary)  
    small::small_string boundary_256(256, 'X');
    CHECK(boundary_256.size() == 256);
    boundary_256.push_back('Y'); // Should transition to median
    CHECK(boundary_256.size() == 257);
    CHECK(boundary_256[256] == 'Y');
}

TEST_CASE("allocator propagation and swap operations") {
    // Test allocator propagation in different scenarios
    small::small_string str1("string1");
    small::small_string str2("string2");
    
    // Test swap operation
    str1.swap(str2);
    CHECK(str1 == "string2");
    CHECK(str2 == "string1");
    
    // Test copy with allocator propagation
    auto allocator = str1.get_allocator();
    small::small_string copied(str1, allocator);
    CHECK(copied == str1);
    
    // Test PMR swap
    std::pmr::polymorphic_allocator<char> alloc{};
    small::pmr::small_string pmr1("pmr1", alloc);
    small::pmr::small_string pmr2("pmr2", alloc);
    pmr1.swap(pmr2);
    CHECK(pmr1 == "pmr2");
    CHECK(pmr2 == "pmr1");
}

TEST_CASE("operator[] bounds checking and edge cases") {
    small::small_string test_str("test");
    
    // Test valid access
    CHECK(test_str[0] == 't');
    CHECK(test_str[3] == 't');
    
    // Test const version
    const auto& const_str = test_str;
    CHECK(const_str[0] == 't');
    CHECK(const_str[3] == 't');
    
    // Test modification via operator[]
    test_str[1] = 'X';
    CHECK(test_str == "tXst");
    
    // Test with different buffer types
    small::small_string large_str(500, 'L');
    CHECK(large_str[0] == 'L');
    CHECK(large_str[499] == 'L');
    large_str[250] = 'M';
    CHECK(large_str[250] == 'M');
}

TEST_CASE("string comparison edge cases") {
    small::small_string str1("compare");
    small::small_string str2("compare");
    small::small_string str3("different");
    
    // Test all comparison operators with various string sizes
    small::small_string short_str("a");
    small::small_string long_str(1000, 'z');
    
    CHECK(short_str < long_str);
    CHECK_FALSE(long_str < short_str);
    CHECK(short_str <= long_str);
    CHECK_FALSE(long_str <= short_str);
    CHECK_FALSE(short_str > long_str);
    CHECK(long_str > short_str);
    CHECK_FALSE(short_str >= long_str);
    CHECK(long_str >= short_str);
    
    // Test equality with different buffer types
    small::small_string internal_equal("same");
    small::small_string external_equal("same");
    external_equal.reserve(100); // Force external buffer
    CHECK(internal_equal == external_equal);
}

TEST_CASE("buffer type specific get_buffer operations") {
    // Test get_buffer() method for different buffer types
    
    // Internal buffer
    small::small_string internal_str("short");
    auto* internal_buf = internal_str.data();
    CHECK(internal_buf != nullptr);
    
    // External buffer (short)
    small::small_string external_str(100, 'E');
    auto* external_buf = external_str.data();
    CHECK(external_buf != nullptr);
    CHECK(external_buf[0] == 'E');
    CHECK(external_buf[99] == 'E');
    
    // Large external buffer (median/long)
    small::small_string large_str(2000, 'L');
    auto* large_buf = large_str.data();
    CHECK(large_buf != nullptr);
    CHECK(large_buf[0] == 'L');
    CHECK(large_buf[1999] == 'L');
}