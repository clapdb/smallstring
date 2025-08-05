#include <iterator>
#include <algorithm>
#include "include/smallstring.hpp"
#include "doctest/doctest/doctest.h"

TEST_CASE("smallstring iterator distance") {
    SUBCASE("std::distance with regular iterators") {
        small::small_string str("Hello, World!");
        
        auto distance = std::distance(str.begin(), str.end());
        CHECK(distance == str.size());
        CHECK(distance == 13);
    }
    
    SUBCASE("std::distance with const iterators") {
        const small::small_string const_str("Test");
        auto const_distance = std::distance(const_str.begin(), const_str.end());
        CHECK(const_distance == const_str.size());
        CHECK(const_distance == 4);
    }
    
    SUBCASE("std::distance with partial range") {
        small::small_string str("Hello, World!");
        if (str.size() >= 5) {
            auto partial_distance = std::distance(str.begin(), str.begin() + 5);
            CHECK(partial_distance == 5);
        }
    }
    
    SUBCASE("std::distance with PMR version") {
        small::pmr::small_string pmr_str("PMR test");
        auto pmr_distance = std::distance(pmr_str.begin(), pmr_str.end());
        CHECK(pmr_distance == pmr_str.size());
        CHECK(pmr_distance == 8);
    }
    
    SUBCASE("std::distance with empty string") {
        small::small_string empty;
        auto empty_distance = std::distance(empty.begin(), empty.end());
        CHECK(empty_distance == 0);
    }
    
    SUBCASE("iterator category verification") {
        small::small_string str("test");
        auto it = str.begin();
        
        // Verify that iterators are random access
        static_assert(std::is_same_v<
            std::iterator_traits<decltype(it)>::iterator_category,
            std::random_access_iterator_tag>);
    }
    
    SUBCASE("reverse iterator distance") {
        small::small_string str("Hello");
        auto distance = std::distance(str.rbegin(), str.rend());
        CHECK(distance == str.size());
        CHECK(distance == 5);
    }
}