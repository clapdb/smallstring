#include <iterator>
#include <algorithm>
#include <string>
#include <string_view>
#include "include/smallstring.hpp"
#include "doctest/doctest/doctest.h"

TEST_CASE("smallstring spaceship operator tests") {
    SUBCASE("small_string <=> small_string") {
        small::small_string str1("abc");
        small::small_string str2("abc");
        small::small_string str3("def");
        small::small_string str4("ab");
        small::small_string str5("abcd");
        
        // Equality
        CHECK((str1 <=> str2) == std::strong_ordering::equal);
        CHECK((str2 <=> str1) == std::strong_ordering::equal);
        
        // Lexicographic ordering
        CHECK((str1 <=> str3) == std::strong_ordering::less);
        CHECK((str3 <=> str1) == std::strong_ordering::greater);
        
        // Length differences
        CHECK((str1 <=> str4) == std::strong_ordering::greater); // "abc" > "ab"
        CHECK((str4 <=> str1) == std::strong_ordering::less);    // "ab" < "abc"
        CHECK((str1 <=> str5) == std::strong_ordering::less);    // "abc" < "abcd"
        CHECK((str5 <=> str1) == std::strong_ordering::greater); // "abcd" > "abc"
    }
    
    SUBCASE("small_string <=> std::string") {
        small::small_string small_str("hello");
        std::string std_str("hello");
        std::string std_str2("world");
        std::string std_str3("hell");
        
        // Equality
        CHECK((small_str <=> std_str) == std::strong_ordering::equal);
        CHECK((std_str <=> small_str) == std::strong_ordering::equal);
        
        // Ordering
        CHECK((small_str <=> std_str2) == std::strong_ordering::less);
        CHECK((std_str2 <=> small_str) == std::strong_ordering::greater);
        CHECK((small_str <=> std_str3) == std::strong_ordering::greater);
        CHECK((std_str3 <=> small_str) == std::strong_ordering::less);
    }
    
    SUBCASE("small_string <=> const char*") {
        small::small_string str("test");
        
        // Equality
        CHECK((str <=> "test") == std::strong_ordering::equal);
        CHECK(("test" <=> str) == std::strong_ordering::equal);
        
        // Ordering
        CHECK((str <=> "zebra") == std::strong_ordering::less);
        CHECK(("zebra" <=> str) == std::strong_ordering::greater);
        CHECK((str <=> "apple") == std::strong_ordering::greater);
        CHECK(("apple" <=> str) == std::strong_ordering::less);
        
        // Empty string comparisons
        small::small_string empty;
        CHECK((empty <=> "") == std::strong_ordering::equal);
        CHECK(("" <=> empty) == std::strong_ordering::equal);
        CHECK((empty <=> "a") == std::strong_ordering::less);
        CHECK(("a" <=> empty) == std::strong_ordering::greater);
    }
    
    SUBCASE("small_string <=> std::string_view") {
        small::small_string str("example");
        std::string_view sv1("example");
        std::string_view sv2("test");
        std::string_view sv3("exam");
        
        // Equality
        CHECK((str <=> sv1) == std::strong_ordering::equal);
        CHECK((sv1 <=> str) == std::strong_ordering::equal);
        
        // Ordering
        CHECK((str <=> sv2) == std::strong_ordering::less);
        CHECK((sv2 <=> str) == std::strong_ordering::greater);
        CHECK((str <=> sv3) == std::strong_ordering::greater);
        CHECK((sv3 <=> str) == std::strong_ordering::less);
    }
    
    SUBCASE("boundary conditions for spaceship operator") {
        // Test with internal/external storage transitions
        small::small_string internal("zebra");      // Likely internal storage
        small::small_string external("apple_long_string_that_exceeds_internal_buffer"); // External storage
        small::small_string internal2("zebra");
        
        CHECK((internal <=> internal2) == std::strong_ordering::equal);
        CHECK((internal <=> external) == std::strong_ordering::greater); // "zebra" > "apple..."
        CHECK((external <=> internal) == std::strong_ordering::less);
    }
    
    SUBCASE("PMR string spaceship operator") {
        small::pmr::small_string pmr1("hello");
        small::pmr::small_string pmr2("hello");
        small::pmr::small_string pmr3("world");
        
        CHECK((pmr1 <=> pmr2) == std::strong_ordering::equal);
        CHECK((pmr1 <=> pmr3) == std::strong_ordering::less);
        CHECK((pmr3 <=> pmr1) == std::strong_ordering::greater);
    }
}

TEST_CASE("smallstring comparison operators comprehensive") {
    SUBCASE("equality operators") {
        small::small_string str1("test");
        small::small_string str2("test");
        small::small_string str3("TEST");
        std::string std_str("test");
        
        // Equality
        CHECK(str1 == str2);
        CHECK(str2 == str1);
        CHECK(str1 == std_str);
        CHECK(std_str == str1);
        CHECK(str1 == "test");
        CHECK("test" == str1);
        
        // Inequality
        CHECK(str1 != str3);
        CHECK(str3 != str1);
        CHECK(str1 != "TEST");
        CHECK("TEST" != str1);
    }
    
    SUBCASE("relational operators") {
        small::small_string a("apple");
        small::small_string b("banana");
        small::small_string c("cherry");
        
        // Less than
        CHECK(a < b);
        CHECK(b < c);
        CHECK(a < c);
        CHECK(!(b < a));
        CHECK(!(c < b));
        CHECK(!(a < a));
        
        // Greater than
        CHECK(c > b);
        CHECK(b > a);
        CHECK(c > a);
        CHECK(!(a > b));
        CHECK(!(b > c));
        CHECK(!(a > a));
        
        // Less than or equal
        CHECK(a <= b);
        CHECK(a <= a);
        CHECK(!(b <= a));
        
        // Greater than or equal
        CHECK(b >= a);
        CHECK(a >= a);
        CHECK(!(a >= b));
    }
    
    SUBCASE("case sensitivity") {
        small::small_string lower("hello");
        small::small_string upper("HELLO");
        small::small_string mixed("Hello");
        
        CHECK(lower != upper);
        CHECK(lower != mixed);
        CHECK(upper != mixed);
        
        // Verify lexicographic ordering (uppercase comes before lowercase in ASCII)
        CHECK(upper < lower);
        CHECK(upper < mixed);
        CHECK(mixed < lower);
    }
    
    SUBCASE("comparison with different allocators") {
        small::small_string regular("test");
        small::pmr::small_string pmr_string("test");
        
        // These should work despite different allocator types
        CHECK(regular == "test");
        CHECK(pmr_string == "test");
        CHECK("test" == regular);
        CHECK("test" == pmr_string);
    }
    
    SUBCASE("edge cases for comparison") {
        small::small_string empty1;
        small::small_string empty2;
        small::small_string non_empty("a");
        
        // Empty string comparisons
        CHECK(empty1 == empty2);
        CHECK(empty1 <= empty2);
        CHECK(empty1 >= empty2);
        CHECK(!(empty1 < empty2));
        CHECK(!(empty1 > empty2));
        CHECK(!(empty1 != empty2));
        
        // Empty vs non-empty
        CHECK(empty1 < non_empty);
        CHECK(non_empty > empty1);
        CHECK(empty1 <= non_empty);
        CHECK(non_empty >= empty1);
        CHECK(empty1 != non_empty);
        CHECK(!(empty1 > non_empty));
        CHECK(!(non_empty < empty1));
    }
}

TEST_CASE("smallstring compare method tests") {
    SUBCASE("basic compare functionality") {
        small::small_string str("hello");
        
        // Compare with same string
        CHECK(str.compare("hello") == 0);
        CHECK(str.compare(str) == 0);
        
        // Compare with different strings
        CHECK(str.compare("world") < 0);  // "hello" < "world"
        CHECK(str.compare("apple") > 0);  // "hello" > "apple"
        CHECK(str.compare("help") < 0);   // "hello" < "help"
    }
    
    SUBCASE("compare with substrings") {
        small::small_string str("hello world");
        
        // Compare substring
        CHECK(str.compare(0, 5, "hello") == 0);
        CHECK(str.compare(6, 5, "world") == 0);
        CHECK(str.compare(0, 11, "hello world") == 0);
        
        // Partial comparisons
        CHECK(str.compare(0, 3, "hel") == 0);
        CHECK(str.compare(0, 3, "help") < 0);
    }
    
    SUBCASE("compare with string_view") {
        small::small_string str("example");
        std::string_view sv1("example");
        std::string_view sv2("test");
        std::string_view sv3("exam");
        
        CHECK(str.compare(sv1) == 0);
        CHECK(str.compare(sv2) < 0);
        CHECK(str.compare(sv3) > 0);
    }
    
    SUBCASE("compare edge cases") {
        small::small_string empty;
        small::small_string non_empty("test");
        
        // Empty string comparisons
        CHECK(empty.compare("") == 0);
        CHECK(empty.compare(empty) == 0);
        CHECK(empty.compare("test") < 0);
        CHECK(non_empty.compare("") > 0);
    }
}