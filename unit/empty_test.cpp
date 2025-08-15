#include <iterator>
#include <algorithm>
#include <cstring>
#include "include/smallstring.hpp"
#include "doctest/doctest/doctest.h"

TEST_CASE("smallstring empty string edge cases") {
    SUBCASE("default constructor creates empty string") {
        small::small_string str;
        CHECK(str.empty());
        CHECK(str.size() == 0);
        CHECK(str.length() == 0);
        CHECK(str.capacity() >= 0);
        CHECK(str.data() != nullptr);
        CHECK(*str.c_str() == '\0');
        CHECK(str.begin() == str.end());
        CHECK(str.cbegin() == str.cend());
        CHECK(str.rbegin() == str.rend());
        CHECK(str.crbegin() == str.crend());
    }
    
    SUBCASE("empty string from empty literal") {
        small::small_string str("");
        CHECK(str.empty());
        CHECK(str.size() == 0);
        CHECK(str.length() == 0);
        CHECK(strcmp(str.c_str(), "") == 0);
        CHECK(str.data() != nullptr);
    }
    
    SUBCASE("empty string from count constructor") {
        small::small_string str(0, 'a');
        CHECK(str.empty());
        CHECK(str.size() == 0);
        CHECK(str.data() != nullptr);
        CHECK(*str.c_str() == '\0');
    }
    
    SUBCASE("empty string from substring of empty string") {
        small::small_string empty;
        small::small_string sub = empty.substr(0, 0);
        CHECK(sub.empty());
        CHECK(sub.size() == 0);
    }
    
    SUBCASE("empty string operations don't crash") {
        small::small_string empty;
        
        // Iterators on empty string
        auto it = empty.begin();
        CHECK(it == empty.end());
        auto distance = std::distance(empty.begin(), empty.end());
        CHECK(distance == 0);
        
        // String operations
        CHECK(empty.find("test") == small::small_string::npos);
        CHECK(empty.find('a') == small::small_string::npos);
        CHECK(empty.rfind("test") == small::small_string::npos);
        CHECK(empty.rfind('a') == small::small_string::npos);
        CHECK(empty.find_first_of("abc") == small::small_string::npos);
        CHECK(empty.find_last_of("abc") == small::small_string::npos);
        CHECK(empty.find_first_not_of("abc") == small::small_string::npos);
        CHECK(empty.find_last_not_of("abc") == small::small_string::npos);
        
        // Comparison with empty strings
        small::small_string empty2;
        CHECK(empty == empty2);
        CHECK(empty <= empty2);
        CHECK(empty >= empty2);
        CHECK(!(empty < empty2));
        CHECK(!(empty > empty2));
        CHECK(empty.compare(empty2) == 0);
    }
    
    SUBCASE("empty string with append operations") {
        small::small_string empty;
        
        // Append to empty string
        empty += "test";
        CHECK(empty == "test");
        CHECK(empty.size() == 4);
        
        // Reset to empty
        empty.clear();
        CHECK(empty.empty());
        
        // Append character to empty
        empty += 'x';
        CHECK(empty == "x");
        CHECK(empty.size() == 1);
        
        // Clear again
        empty.clear();
        
        // Append empty string to empty string
        small::small_string empty2;
        empty += empty2;
        CHECK(empty.empty());
    }
    
    SUBCASE("empty string resize operations") {
        small::small_string empty;
        
        // Resize empty string to 0 (should be no-op)
        empty.resize(0);
        CHECK(empty.empty());
        
        // Resize empty string to larger size
        empty.resize(5, 'a');
        CHECK(empty == "aaaaa");
        CHECK(empty.size() == 5);
        
        // Resize back to 0
        empty.resize(0);
        CHECK(empty.empty());
    }
    
    SUBCASE("empty string copy and assignment") {
        small::small_string empty;
        
        // Copy constructor
        small::small_string copy(empty);
        CHECK(copy.empty());
        CHECK(copy == empty);
        
        // Assignment operator
        small::small_string assigned;
        assigned = "test";  // Make it non-empty first
        assigned = empty;   // Assign empty
        CHECK(assigned.empty());
        CHECK(assigned == empty);
        
        // Move operations
        small::small_string move_source;
        small::small_string moved(std::move(move_source));
        CHECK(moved.empty());
        
        small::small_string move_assigned;
        move_assigned = "test";
        move_assigned = std::move(empty);
        CHECK(move_assigned.empty());
    }
    
    SUBCASE("PMR empty string") {
        small::pmr::small_string pmr_empty;
        CHECK(pmr_empty.empty());
        CHECK(pmr_empty.size() == 0);
        CHECK(pmr_empty.data() != nullptr);
        CHECK(*pmr_empty.c_str() == '\0');
        
        auto distance = std::distance(pmr_empty.begin(), pmr_empty.end());
        CHECK(distance == 0);
    }
}