#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include "include/smallstring.hpp"
#include "doctest/doctest/doctest.h"

TEST_CASE("smallstring edge cases and error conditions") {
    SUBCASE("out of bounds access with exceptions") {
        small::small_string str("test");
        
        // Valid access should work
        CHECK(str.at(0) == 't');
        CHECK(str.at(3) == 't');
        
        // Out of bounds access should throw
        CHECK_THROWS_AS((void)str.at(4), std::out_of_range);
        CHECK_THROWS_AS((void)str.at(100), std::out_of_range);
        
        // Const version
        const auto& const_str = str;
        CHECK(const_str.at(0) == 't');
        CHECK_THROWS_AS((void)const_str.at(4), std::out_of_range);
    }
    
    SUBCASE("operator[] bounds checking in safe mode") {
        // Note: This test assumes Safe mode can be enabled
        // The actual behavior depends on the Safe template parameter
        small::small_string str("abc");
        
        // Valid access
        CHECK(str[0] == 'a');
        CHECK(str[2] == 'c');
        
        // In safe mode, out of bounds should throw
        // In non-safe mode, this is undefined behavior
        // We'll test the valid cases only to avoid UB
        const auto& const_str = str;
        CHECK(const_str[0] == 'a');
        CHECK(const_str[2] == 'c');
    }
    
    SUBCASE("large string operations") {
        // Test with strings that definitely exceed internal storage
        std::string large_content(1000, 'A');
        small::small_string large_str(large_content);
        
        CHECK(large_str.size() == 1000);
        CHECK(large_str[0] == 'A');
        CHECK(large_str[999] == 'A');
        CHECK(large_str.back() == 'A');
        CHECK(large_str.front() == 'A');
        
        // Copy operations with large strings
        small::small_string copy(large_str);
        CHECK(copy.size() == 1000);
        CHECK(copy == large_str);
        
        // Assignment with large strings
        small::small_string assigned;
        assigned = large_str;
        CHECK(assigned.size() == 1000);
        CHECK(assigned == large_str);
    }
    
    SUBCASE("maximum capacity handling") {
        small::small_string str;
        
        // Try to reserve a very large amount
        // This should either succeed or handle gracefully
        try {
            str.reserve(1000000);
            CHECK(str.capacity() >= 1000000);
            CHECK(str.empty()); // Should still be empty after reserve
        } catch (const std::exception&) {
            // If it throws due to memory constraints, that's acceptable
            // Just verify the string is still in a valid state
            CHECK(str.empty());
        }
    }
    
    SUBCASE("string operations with embedded nulls") {
        // Create string with embedded null characters
        small::small_string str;
        str.push_back('a');
        str.push_back('\0');
        str.push_back('b');
        str.push_back('\0');
        str.push_back('c');
        
        CHECK(str.size() == 5);
        CHECK(str[0] == 'a');
        CHECK(str[1] == '\0');
        CHECK(str[2] == 'b');
        CHECK(str[3] == '\0');
        CHECK(str[4] == 'c');
        
        // c_str() should still work (though interpretation might differ)
        CHECK(strlen(str.c_str()) <= str.size()); // strlen stops at first null
        
        // Operations should handle embedded nulls correctly
        auto found = str.find('\0');
        CHECK(found == 1); // Should find first null at position 1
    }
    
    SUBCASE("iterator invalidation scenarios") {
        small::small_string str("initial");
        auto it = str.begin();
        auto end_it = str.end();
        
        // Store iterator positions for later validation
        auto initial_distance = std::distance(it, end_it);
        CHECK(initial_distance == 7);
        
        // Operations that might reallocate
        str.reserve(100); // Might invalidate iterators
        
        // Get new iterators after potential reallocation
        auto new_it = str.begin();
        auto new_end = str.end();
        CHECK(std::distance(new_it, new_end) == 7);
        
        // String content should be unchanged
        CHECK(str == "initial");
    }
    
    SUBCASE("move operations with empty strings") {
        small::small_string empty1;
        small::small_string empty2;
        
        // Move empty string
        small::small_string moved(std::move(empty1));
        CHECK(moved.empty());
        
        // Move assign empty
        empty2 = std::move(moved);
        CHECK(empty2.empty());
        
        // Chain moves
        small::small_string final = std::move(empty2);
        CHECK(final.empty());
    }
    
    SUBCASE("self-assignment scenarios") {
        small::small_string str("self");
        
        // Self copy assignment
        #pragma GCC diagnostic push
        #ifdef __clang__
        #pragma clang diagnostic ignored "-Wself-assign-overloaded"
        #endif
        str = str;
        #pragma GCC diagnostic pop
        CHECK(str == "self");
        CHECK(str.size() == 4);
        
        // Self move assignment (should handle gracefully)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wself-move"
        str = std::move(str);
        #pragma GCC diagnostic pop
        // After self-move, state is implementation-defined but should be valid
        CHECK(str.size() <= 1000); // Just check it's reasonable
    }
    
    SUBCASE("operations on moved-from objects") {
        small::small_string source("content");
        small::small_string dest(std::move(source));
        
        CHECK(dest == "content");
        
        // Operations on moved-from object should be safe
        // (though the content is unspecified)
        CHECK(source.size() <= 1000); // Should be reasonable
        
        // Should be able to assign to moved-from object
        source = "new content";
        CHECK(source == "new content");
        
        // Should be able to use other operations
        source.clear();
        CHECK(source.empty());
    }
    
    SUBCASE("PMR allocator edge cases") {
        // Create PMR string and test edge cases specific to it
        small::pmr::small_string pmr_str("pmr test");
        
        // Large operations with PMR
        pmr_str.resize(500, 'X');
        CHECK(pmr_str.size() == 500);
        CHECK(pmr_str[0] == 'p'); // Original start
        CHECK(pmr_str[499] == 'X'); // Filled part
        
        // Copy between PMR and regular strings
        small::small_string regular = pmr_str; // Should work despite different allocators
        CHECK(regular.size() == 500);
        CHECK(regular[0] == 'p');
        
        pmr_str = regular; // Reverse assignment
        CHECK(pmr_str.size() == 500);
        CHECK(pmr_str[0] == 'p');
    }
    
    SUBCASE("substring operations edge cases") {
        small::small_string str("hello world");
        
        // Valid substrings
        CHECK(str.substr(0, 5) == "hello");
        CHECK(str.substr(6) == "world");
        CHECK(str.substr(0) == "hello world");
        
        // Edge case: substring with pos == size (should return empty)
        CHECK(str.substr(str.size()).empty());
        
        // Edge case: substring with len > remaining (should truncate)
        CHECK(str.substr(6, 1000) == "world");
        
        // Out of bounds pos should throw
        CHECK_THROWS_AS((void)str.substr(100), std::out_of_range);
    }
}

TEST_CASE("smallstring capacity and memory management") {
    SUBCASE("capacity never decreases during growth") {
        small::small_string str;
        auto prev_cap = str.capacity();
        
        for (int i = 0; i < 100; ++i) {
            str += char('a' + (i % 26));
            auto curr_cap = str.capacity();
            CHECK(curr_cap >= prev_cap);
            CHECK(curr_cap >= str.size());
            prev_cap = curr_cap;
        }
    }
    
    SUBCASE("shrink_to_fit behavior") {
        small::small_string str(1000, 'x'); // Large string
        (void)str.capacity(); // Suppress unused variable warning
        
        str.resize(10); // Shrink content
        CHECK(str.size() == 10);
        // Capacity might not change immediately
        
        str.shrink_to_fit();
        auto new_capacity = str.capacity();
        CHECK(new_capacity >= str.size());
        // shrink_to_fit is a non-binding request, so we can't guarantee it shrinks
    }
    
    SUBCASE("reserve behavior") {
        small::small_string str;
        
        // Reserve should increase capacity
        str.reserve(50);
        CHECK(str.capacity() >= 50);
        CHECK(str.empty()); // Content should be unchanged
        
        // Reserving less than current capacity might be no-op
        (void)str.capacity(); // Suppress unused variable warning
        str.reserve(10);
        CHECK(str.capacity() >= 10);
        
        // Fill to verify reserve worked
        str.resize(30, 'a');
        CHECK(str.size() == 30);
        CHECK(str.capacity() >= 30);
    }
    
    SUBCASE("growth pattern verification") {
        small::small_string str;
        std::vector<size_t> capacities;
        
        // Track capacity changes during growth
        for (int i = 0; i < 50; ++i) {
            if (str.capacity() != (capacities.empty() ? 0 : capacities.back())) {
                capacities.push_back(str.capacity());
            }
            str += 'x';
        }
        
        // Verify growth is reasonable (capacity should grow exponentially or linearly)
        for (size_t i = 1; i < capacities.size(); ++i) {
            CHECK(capacities[i] > capacities[i-1]); // Should always increase
            CHECK(capacities[i] <= capacities[i-1] * 3); // Growth shouldn't be too aggressive
        }
    }
}