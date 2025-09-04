#include "include/smallstring.hpp"
#include <doctest/doctest.h>
#include <string>
#include <memory_resource>

TEST_SUITE("Maximum Coverage Tests") {

// Test all different buffer types
TEST_CASE("buffer_type_coverage") {
    SUBCASE("internal_buffer") {
        small::small_string s("test");  // Internal buffer (<=7 chars)
        CHECK(s.size() == 4);
        CHECK(s == "test");
        s.resize(6);
        CHECK(s.size() == 6);
        s.resize(2);
        CHECK(s.size() == 2);
    }
    
    SUBCASE("short_buffer") {
        small::small_string s(100, 'a');  // Short buffer (8-255 chars)
        CHECK(s.size() == 100);
        s.resize(80);
        CHECK(s.size() == 80);
        s.resize(120);
        CHECK(s.size() == 120);
        s.append(50, 'b');
        CHECK(s.size() == 170);
    }
    
    SUBCASE("median_buffer") {
        small::small_string s(1000, 'c');  // Median buffer (256-16383 chars)
        CHECK(s.size() == 1000);
        s.resize(800);
        CHECK(s.size() == 800);
        s.resize(1200);
        CHECK(s.size() == 1200);
        s.append(300, 'd');
        CHECK(s.size() == 1500);
    }
    
    SUBCASE("long_buffer") {
        small::small_string s(20000, 'e');  // Long buffer (>16KB)
        CHECK(s.size() == 20000);
        s.resize(18000);
        CHECK(s.size() == 18000);
        s.resize(22000);
        CHECK(s.size() == 22000);
        s.append(3000, 'f');
        CHECK(s.size() == 25000);
    }
}

// Test PMR allocator
TEST_CASE("pmr_allocator") {
    std::pmr::monotonic_buffer_resource pool{8192};
    
    SUBCASE("basic_pmr") {
        small::pmr::small_string s{&pool};
        s = "Hello PMR";
        CHECK(s == "Hello PMR");
    }
    
    SUBCASE("large_pmr") {
        small::pmr::small_string s{&pool};
        s.assign(2000, 'x');
        CHECK(s.size() == 2000);
        CHECK(s[1999] == 'x');
    }
}

// Test non-null-terminated version
TEST_CASE("byte_string_coverage") {
    SUBCASE("basic_byte_string") {
        small::small_byte_string s("hello");
        CHECK(s.size() == 5);
        CHECK(s == "hello");
    }
    
    SUBCASE("large_byte_string") {
        small::small_byte_string s(1500, 'z');
        CHECK(s.size() == 1500);
        s.resize(1000);
        CHECK(s.size() == 1000);
    }
}

// Test capacity and reserve operations
TEST_CASE("capacity_operations") {
    SUBCASE("reserve_operations") {
        small::small_string s("initial");
        
        // Test reserve on different buffer types
        s.reserve(50);   // Internal -> Short
        CHECK(s.capacity() >= 50);
        
        s.reserve(500);  // Short -> Median
        CHECK(s.capacity() >= 500);
        
        s.reserve(20000); // Median -> Long
        CHECK(s.capacity() >= 20000);
    }
    
    SUBCASE("shrink_to_fit") {
        small::small_string s(1000, 'a');
        s.resize(100);
        s.shrink_to_fit();
        CHECK(s.size() == 100);
    }
}

// Test assignment operations that might be uncovered
TEST_CASE("assignment_operations") {
    SUBCASE("move_assignment") {
        small::small_string s1(1000, 'a');
        small::small_string s2;
        s2 = std::move(s1);
        CHECK(s2.size() == 1000);
    }
    
    SUBCASE("self_assignment") {
        small::small_string s("test");
        #pragma GCC diagnostic push
        #ifdef __clang__
        #pragma clang diagnostic ignored "-Wself-assign-overloaded"
        #endif
        s = s;  // Self assignment
        #pragma GCC diagnostic pop
        CHECK(s == "test");
    }
    
    SUBCASE("initializer_list_assignment") {
        small::small_string s;
        s = {'h', 'e', 'l', 'l', 'o'};
        CHECK(s == "hello");
    }
}

// Test string operations that might be uncovered
TEST_CASE("string_operations") {
    SUBCASE("replace_operations") {
        small::small_string s("hello world");
        
        // Replace with different sizes
        s.replace(6, 5, "universe");
        CHECK(s == "hello universe");
        
        s.replace(6, 8, "C++");
        CHECK(s == "hello C++");
        
        // Replace with iterators
        std::string replacement = "beautiful";
        s.replace(s.begin() + 6, s.end(), replacement.begin(), replacement.end());
        CHECK(s == "hello beautiful");
    }
    
    SUBCASE("erase_operations") {
        small::small_string s("hello world test");
        
        // Erase range
        s.erase(s.begin() + 5, s.begin() + 11);
        CHECK(s == "hello test");
        
        // Erase single position
        s.erase(s.begin() + 5);
        CHECK(s == "hellotest");
    }
    
    SUBCASE("insert_operations") {
        small::small_string s("hello");
        
        // Insert at different positions
        s.insert(5, " world");
        CHECK(s == "hello world");
        
        s.insert(0, "Say ");
        CHECK(s == "Say hello world");
        
        // Insert with iterators
        std::string extra = " beautiful";
        s.insert(s.begin() + 9, extra.begin(), extra.end());
        CHECK(s == "Say hello beautiful world");
    }
}

// Test error conditions and edge cases
TEST_CASE("error_conditions") {
    SUBCASE("invalid_ranges") {
        small::small_string s("hello");
        
        // Test various edge cases that might trigger asserts or error paths
        auto sub1 = s.substr(0, 5);
        CHECK(sub1 == "hello");
        auto sub2 = s.substr(5, 0);
        CHECK(sub2.empty());
        
        // These should not crash
        s.resize(0);
        CHECK(s.empty());
        
        s.resize(1000);
        CHECK(s.size() == 1000);
    }
}

// Test comparison operations comprehensively
TEST_CASE("comparison_operations") {
    small::small_string s1("abc");
    small::small_string s2("abc");
    small::small_string s3("abd");
    
    CHECK(s1 == s2);
    CHECK(s1 != s3);
    CHECK(s1 < s3);
    CHECK(s3 > s1);
    CHECK(s1 <= s2);
    CHECK(s1 <= s3);
    CHECK(s3 >= s1);
    CHECK(s2 >= s1);
    
    // Compare with string literals
    CHECK(s1 == "abc");
    CHECK("abc" == s1);
    CHECK(s1 != "abd");
    CHECK("abd" != s1);
}

// Test append operations
TEST_CASE("append_operations") {
    SUBCASE("append_transitions") {
        small::small_string s("a");  // Internal
        
        s.append(20, 'b');  // Internal -> Short
        CHECK(s.size() == 21);
        
        s.append(500, 'c'); // Short -> Median
        CHECK(s.size() == 521);
        
        s.append(20000, 'd'); // Median -> Long
        CHECK(s.size() == 20521);
    }
}

// Test swap operations
TEST_CASE("swap_operations") {
    small::small_string s1("hello");
    small::small_string s2("world");
    
    s1.swap(s2);
    CHECK(s1 == "world");
    CHECK(s2 == "hello");
    
    std::swap(s1, s2);
    CHECK(s1 == "hello");
    CHECK(s2 == "world");
}

// Test format-like operations if available
TEST_CASE("utility_operations") {
    SUBCASE("clear_and_assign") {
        small::small_string s(1000, 'a');
        s.clear();
        CHECK(s.empty());
        
        s.assign(500, 'b');
        CHECK(s.size() == 500);
        CHECK(s[0] == 'b');
    }
    
    SUBCASE("resize_with_fill") {
        small::small_string s("test");
        s.resize(10, 'x');
        CHECK(s.size() == 10);
        CHECK(s.substr(4) == "xxxxxx");
    }
}

// Test iterator operations
TEST_CASE("iterator_operations") {
    SUBCASE("iterator_arithmetic") {
        small::small_string s("hello world");
        
        auto it = s.begin();
        CHECK(*it == 'h');
        
        it += 6;
        CHECK(*it == 'w');
        
        auto it2 = it + 5;
        CHECK(it2 == s.end());
        
        auto distance = it2 - it;
        CHECK(distance == 5);
    }
    
    SUBCASE("reverse_iterators") {
        small::small_string s("hello");
        std::string reversed;
        
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed += *it;
        }
        CHECK(reversed == "olleh");
    }
}

}