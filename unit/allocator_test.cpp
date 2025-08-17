#include <memory>
#include <memory_resource>
#include <vector>
#include <algorithm>
#include "include/smallstring.hpp"
#include "doctest/doctest/doctest.h"

TEST_CASE("smallstring allocator support") {
    SUBCASE("get_allocator functionality") {
        // Standard allocator
        small::small_string str("test");
        auto alloc = str.get_allocator();
        
        // Should be able to use allocator
        using alloc_type = decltype(alloc);
        static_assert(std::is_same_v<alloc_type, std::allocator<char>>);
        
        // PMR allocator
        auto pmr_resource = std::pmr::new_delete_resource();
        std::pmr::polymorphic_allocator<char> pmr_alloc(pmr_resource);
        small::pmr::small_string pmr_str("pmr test", pmr_alloc);
        
        auto pmr_retrieved = pmr_str.get_allocator();
        CHECK(pmr_retrieved.resource() == pmr_resource);
        
        // Allocator propagation in copy construction
        small::pmr::small_string pmr_copy(pmr_str);
        auto pmr_copy_alloc = pmr_copy.get_allocator();
        CHECK(pmr_copy_alloc.resource() == pmr_resource);
    }
    
    SUBCASE("PMR allocator construction and assignment") {
        auto resource1 = std::pmr::new_delete_resource();
        auto resource2 = std::pmr::new_delete_resource();
        
        std::pmr::polymorphic_allocator<char> alloc1(resource1);
        std::pmr::polymorphic_allocator<char> alloc2(resource2);
        
        // Construction with specific allocator
        small::pmr::small_string str1("hello", alloc1);
        CHECK(str1.get_allocator().resource() == resource1);
        CHECK(str1 == "hello");
        
        // Copy construction with different allocator
        small::pmr::small_string str2(str1, alloc2);
        CHECK(str2.get_allocator().resource() == resource2);
        CHECK(str2 == "hello");
        CHECK(str2 == str1); // Content should be the same
        
        // Move construction (allocator should move)
        small::pmr::small_string str3(std::move(str1), alloc2);
        CHECK(str3.get_allocator().resource() == resource2);
        CHECK(str3 == "hello");
        
        // Assignment doesn't change allocator
        str2 = "world";
        CHECK(str2.get_allocator().resource() == resource2);
        CHECK(str2 == "world");
        
        str3 = str2;
        CHECK(str3.get_allocator().resource() == resource2); // Allocator unchanged
        CHECK(str3 == "world");
    }
    
    SUBCASE("allocator-extended constructors") {
        auto resource = std::pmr::new_delete_resource();
        std::pmr::polymorphic_allocator<char> alloc(resource);
        
        // Default construction with allocator
        small::pmr::small_string str1(alloc);
        CHECK(str1.empty());
        CHECK(str1.get_allocator().resource() == resource);
        
        // Count + char construction with allocator
        small::pmr::small_string str2(5, 'a', alloc);
        CHECK(str2 == "aaaaa");
        CHECK(str2.get_allocator().resource() == resource);
        
        // C-string construction with allocator
        small::pmr::small_string str3("hello", alloc);
        CHECK(str3 == "hello");
        CHECK(str3.get_allocator().resource() == resource);
        
        // C-string with count construction with allocator
        small::pmr::small_string str4("hello world", 5, alloc);
        CHECK(str4 == "hello");
        CHECK(str4.get_allocator().resource() == resource);
        
        // Iterator construction with allocator
        std::string source = "iterator test";
        small::pmr::small_string str5(source.begin(), source.end(), alloc);
        CHECK(str5 == "iterator test");
        CHECK(str5.get_allocator().resource() == resource);
        
        // Initializer list construction with allocator
        small::pmr::small_string str6({'a', 'b', 'c'}, alloc);
        CHECK(str6 == "abc");
        CHECK(str6.get_allocator().resource() == resource);
        
        // String view construction with allocator
        std::string_view sv("string_view");
        small::pmr::small_string str7(sv, alloc);
        CHECK(str7 == "string_view");
        CHECK(str7.get_allocator().resource() == resource);
    }
}

TEST_CASE("smallstring swap operations") {
    SUBCASE("swap with same allocator type") {
        small::small_string str1("first");
        small::small_string str2("second string that is much longer");
        
        // Store original values
        std::string orig1(str1.data(), str1.size());
        std::string orig2(str2.data(), str2.size());
        (void)str1.capacity(); // Suppress unused warning
        (void)str2.capacity(); // Suppress unused warning
        
        // Swap
        str1.swap(str2);
        
        // Verify swap
        CHECK(str1 == orig2);
        CHECK(str2 == orig1);
        
        // Capacities might have been swapped efficiently
        CHECK(str1.capacity() >= orig2.size());
        CHECK(str2.capacity() >= orig1.size());
        
        // Swap back
        str2.swap(str1);
        CHECK(str1 == orig1);
        CHECK(str2 == orig2);
        
        // Free function swap
        using std::swap;
        swap(str1, str2);
        CHECK(str1 == orig2);
        CHECK(str2 == orig1);
    }
    
    SUBCASE("swap across storage boundaries") {
        // Internal <-> External swap
        small::small_string small_str("abc");      // Internal storage
        small::small_string large_str(100, 'x');   // External storage
        
        std::string orig_small(small_str.data(), small_str.size());
        std::string orig_large(large_str.data(), large_str.size());
        
        small_str.swap(large_str);
        
        CHECK(small_str == orig_large);
        CHECK(large_str == orig_small);
        CHECK(small_str.size() == 100);
        CHECK(large_str.size() == 3);
        
        // Verify they still work correctly after swap
        small_str.push_back('y');
        CHECK(small_str.size() == 101);
        CHECK(small_str.back() == 'y');
        
        large_str += "def";
        CHECK(large_str == "abcdef");
    }
    
    SUBCASE("PMR swap operations") {
        auto resource1 = std::pmr::new_delete_resource();
        auto resource2 = std::pmr::new_delete_resource();
        
        std::pmr::polymorphic_allocator<char> alloc1(resource1);
        std::pmr::polymorphic_allocator<char> alloc2(resource2);
        
        small::pmr::small_string str1("first", alloc1);
        small::pmr::small_string str2("second", alloc1); // Same allocator
        
        std::string orig1(str1.data(), str1.size());
        std::string orig2(str2.data(), str2.size());
        
        // Swap with same allocator
        str1.swap(str2);
        CHECK(str1 == orig2);
        CHECK(str2 == orig1);
        CHECK(str1.get_allocator().resource() == resource1);
        CHECK(str2.get_allocator().resource() == resource1);
        
        // Create string with different allocator
        small::pmr::small_string str3("third", alloc2);
        
        // Swap should work even with different allocators
        // (allocators stay with their original strings)
        str1.swap(str3);
        CHECK(str1 == "third");
        CHECK(str3 == orig2);
        CHECK(str1.get_allocator().resource() == resource1); // Unchanged
        CHECK(str3.get_allocator().resource() == resource2); // Unchanged
    }
    
    SUBCASE("swap edge cases") {
        // Empty string swaps
        small::small_string empty1;
        small::small_string empty2;
        small::small_string non_empty("content");
        
        // Empty <-> Empty
        empty1.swap(empty2);
        CHECK(empty1.empty());
        CHECK(empty2.empty());
        
        // Empty <-> Non-empty
        std::string orig_content(non_empty.data(), non_empty.size());
        empty1.swap(non_empty);
        CHECK(empty1 == orig_content);
        CHECK(non_empty.empty());
        
        // Self swap (should be no-op)
        std::string before_self_swap(empty1.data(), empty1.size());
        empty1.swap(empty1);
        CHECK(empty1 == before_self_swap);
        
        // Swap very large strings
        small::small_string huge1(10000, 'A');
        small::small_string huge2(5000, 'B');
        
        huge1.swap(huge2);
        CHECK(huge1.size() == 5000);
        CHECK(huge2.size() == 10000);
        CHECK(huge1[0] == 'B');
        CHECK(huge2[0] == 'A');
        CHECK(huge1[4999] == 'B');
        CHECK(huge2[9999] == 'A');
    }
}

TEST_CASE("smallstring move semantics and performance") {
    SUBCASE("move constructor efficiency") {
        // Create original string with external storage
        small::small_string original(100, 'M');
        original += "marker";
        std::string original_content(original.data(), original.size());
        (void)original.capacity(); // Suppress unused warning
        (void)original.data(); // Suppress unused warning
        
        // Move construct
        small::small_string moved(std::move(original));
        
        // Moved-to string should have the content
        CHECK(moved == original_content);
        CHECK(moved.size() == original_content.size());
        
        // For external storage, data pointer might be moved
        // (implementation detail - can't guarantee this)
        CHECK(moved.capacity() >= original_content.size());
        
        // Original should be in valid but unspecified state
        CHECK(original.size() <= original.capacity()); // Basic invariant
        
        // Should be able to assign to moved-from object
        original = "new content";
        CHECK(original == "new content");
        CHECK(moved == original_content); // Unchanged
    }
    
    SUBCASE("move assignment efficiency") {
        small::small_string target("target");
        small::small_string source(200, 'S');
        source += "source";
        
        std::string source_content(source.data(), source.size());
        
        // Move assign
        target = std::move(source);
        
        CHECK(target == source_content);
        CHECK(target.size() == source_content.size());
        
        // Source should be valid but unspecified
        CHECK(source.size() <= source.capacity());
        
        // Should be able to use source again
        source.clear();
        source = "reused";
        CHECK(source == "reused");
    }
    
    SUBCASE("move semantics with different storage sizes") {
        // Small -> Small move
        small::small_string small1("abc");
        small::small_string small2("xyz");
        small2 = std::move(small1);
        CHECK(small2 == "abc");
        
        // Large -> Small move
        small::small_string large(150, 'L');
        small::small_string small("s");
        small = std::move(large);
        CHECK(small.size() == 150);
        CHECK(small[0] == 'L');
        CHECK(small[149] == 'L');
        
        // Small -> Large move
        small::small_string small_src("small");
        small::small_string large_target(200, 'T');
        large_target = std::move(small_src);
        CHECK(large_target == "small");
        CHECK(large_target.size() == 5);
        
        // Large -> Large move
        small::small_string large1(300, 'A');
        small::small_string large2(400, 'B');
        std::string large1_content(large1.data(), large1.size());
        large2 = std::move(large1);
        CHECK(large2 == large1_content);
        CHECK(large2.size() == 300);
    }
    
    SUBCASE("PMR move semantics") {
        auto resource1 = std::pmr::new_delete_resource();
        auto resource2 = std::pmr::new_delete_resource();
        
        std::pmr::polymorphic_allocator<char> alloc1(resource1);
        std::pmr::polymorphic_allocator<char> alloc2(resource2);
        
        // Move with same allocator
        small::pmr::small_string source("source", alloc1);
        small::pmr::small_string target(alloc1);
        
        target = std::move(source);
        CHECK(target == "source");
        CHECK(target.get_allocator().resource() == resource1);
        
        // Move construction with different allocator
        small::pmr::small_string source2("source2", alloc1);
        small::pmr::small_string target2(std::move(source2), alloc2);
        CHECK(target2 == "source2");
        CHECK(target2.get_allocator().resource() == resource2);
        
        // Move assignment with different allocators
        small::pmr::small_string source3("source3", alloc1);
        small::pmr::small_string target3("target3", alloc2);
        
        target3 = std::move(source3);
        CHECK(target3 == "source3");
        CHECK(target3.get_allocator().resource() == resource2); // Allocator unchanged
    }
}

TEST_CASE("smallstring resource management") {
    SUBCASE("capacity management across operations") {
        small::small_string str;
        std::vector<size_t> capacities;
        
        // Track capacity changes during various operations
        capacities.push_back(str.capacity());
        
        // Initial growth
        str = "initial";
        capacities.push_back(str.capacity());
        
        // Reserve
        str.reserve(100);
        capacities.push_back(str.capacity());
        
        // Append to reserved space
        str.append(50, 'x');
        capacities.push_back(str.capacity());
        
        // Shrink content but not capacity
        str.resize(10);
        capacities.push_back(str.capacity());

        CHECK(capacities[0] == 6); // empty internal string's cap
        CHECK(capacities[1] == 7); // "initial" will alignUp to 8, then reduce a '\0'
        CHECK(capacities[2] == 103); // 100 will alignUp to 104, then reduce a '\0'
        CHECK(capacities[3] == 103); // resize will not change capacity

        // shrink_to_fit might reduce capacity
        auto cap_before_shrink = str.capacity();
        str.shrink_to_fit();
        CHECK(str.capacity() >= str.size());
        CHECK(str.capacity() <= cap_before_shrink); // Should not increase
        // Can't guarantee it actually shrunk (implementation-defined)
    }
    
    SUBCASE("memory efficiency verification") {
        // Small strings should not allocate
        small::small_string tiny("abc");
        small::small_string tiny2("xyz");
        
        // These should be efficient (no allocation for small strings)
        tiny = tiny2;
        tiny.swap(tiny2);
        
        // Large strings should manage memory efficiently
        small::small_string large1(1000, 'A');
        size_t large1_capacity = large1.capacity();
        CHECK(large1_capacity >= 1000);
        CHECK(large1_capacity <= 2000); // Should not over-allocate too much
        
        // Copy should not waste space
        small::small_string large2 = large1;
        CHECK(large2.capacity() >= 1000);
        CHECK(large2.capacity() <= large1_capacity * 1.5); // Reasonable overhead
        
        // Move should be efficient
        (void)large1.data(); // Suppress unused warning
        small::small_string large3 = std::move(large1);
        
        // For large strings, move should ideally transfer ownership
        // (implementation detail, can't guarantee exact behavior)
        CHECK(large3.size() == 1000);
        CHECK(large3.capacity() >= 1000);
    }
}