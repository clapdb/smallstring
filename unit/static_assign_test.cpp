#include <set>
#include "include/smallstring.hpp"
#include "doctest/doctest/doctest.h"

// Note: static_assign is a private function, so we test equivalent public functionality
TEST_CASE("smallstring fill constructor and assignment tests") {
    SUBCASE("fill constructor basic functionality") {
        // Test fill constructor which provides similar functionality to static_assign
        small::small_string str1(1, 'a');
        CHECK(str1.size() == 1);
        CHECK(str1[0] == 'a');
        CHECK(str1 == "a");
        
        small::small_string str3(3, 'b');
        CHECK(str3.size() == 3);
        CHECK(str3[0] == 'b');
        CHECK(str3[1] == 'b');
        CHECK(str3[2] == 'b');
        CHECK(str3 == "bbb");
        
        small::small_string str5(5, 'x');
        CHECK(str5.size() == 5);
        CHECK(str5 == "xxxxx");
        for (size_t i = 0; i < 5; ++i) {
            CHECK(str5[i] == 'x');
        }
    }
    
    SUBCASE("fill assignment with assign method") {
        small::small_string str;
        
        // Test assign method with count and character
        str.assign(4, 'A');
        CHECK(str == "AAAA");
        
        str.assign(2, '0');
        CHECK(str == "00");
        
        str.assign(6, ' ');
        CHECK(str == "      ");
        CHECK(str.size() == 6);
        
        str.assign(1, '\n');
        CHECK(str.size() == 1);
        CHECK(str[0] == '\n');
    }
    
    SUBCASE("fill operations at internal buffer boundary") {
        // Test at what should be near the internal buffer limit
        small::small_string str(6, 'z');
        CHECK(str.size() == 6);
        CHECK(str == "zzzzzz");
        
        // Verify all characters are correct
        for (size_t i = 0; i < str.size(); ++i) {
            CHECK(str[i] == 'z');
        }
        
        // Test that it's null terminated
        CHECK(str.c_str()[6] == '\0');
    }
    
    SUBCASE("assign overwrites previous content") {
        small::small_string str("initial content that is longer");
        CHECK(str.size() > 10);
        
        // Assign should completely replace content
        str.assign(3, 'X');
        CHECK(str.size() == 3);
        CHECK(str == "XXX");
        
        // Verify no remnants of previous content
        CHECK(str.c_str()[3] == '\0');
    }
    
    SUBCASE("single character fill") {
        small::small_string str("previous");
        
        str.assign(1, '!');
        CHECK(str.size() == 1);
        CHECK(str[0] == '!');
        CHECK(str == "!");
        CHECK(str.front() == '!');
        CHECK(str.back() == '!');
    }
    
    SUBCASE("PMR version fill operations") {
        small::pmr::small_string pmr_str;
        
        pmr_str.assign(4, '#');
        CHECK(pmr_str.size() == 4);
        CHECK(pmr_str == "####");
        
        // Verify it works with different characters
        pmr_str.assign(2, '$');
        CHECK(pmr_str.size() == 2);
        CHECK(pmr_str == "$$");
    }
    
    SUBCASE("fill with special characters") {
        small::small_string str;
        
        // Test with null character (though this might be unusual usage)
        str.assign(3, '\0');
        CHECK(str.size() == 3);
        // Note: string content comparison might be tricky with embedded nulls
        CHECK(str[0] == '\0');
        CHECK(str[1] == '\0');
        CHECK(str[2] == '\0');
        
        // Test with high ASCII values
        str.assign(2, char(255));
        CHECK(str.size() == 2);
        CHECK(str[0] == char(255));
        CHECK(str[1] == char(255));
    }
}

TEST_CASE("smallstring CoreType constants tests") {
    SUBCASE("CoreType constants have correct values") {
        // Test that the constants match the enum values
        CHECK(small::kIsInternal == static_cast<uint8_t>(small::CoreType::Internal));
        CHECK(small::kIsShort == static_cast<uint8_t>(small::CoreType::Short));
        CHECK(small::kIsMedian == static_cast<uint8_t>(small::CoreType::Median));
        CHECK(small::kIsLong == static_cast<uint8_t>(small::CoreType::Long));
    }
    
    SUBCASE("CoreType constants are distinct") {
        // Verify all constants have different values
        std::set<uint8_t> values = {
            small::kIsInternal,
            small::kIsShort,
            small::kIsMedian,
            small::kIsLong
        };
        CHECK(values.size() == 4); // All values should be unique
    }
    
    SUBCASE("CoreType constants can be used in comparisons") {
        // Test that we can use these constants for type checking
        // (This is somewhat artificial since the constants are mainly for internal use)
        uint8_t test_value = small::kIsInternal;
        CHECK(test_value == small::kIsInternal);
        CHECK(test_value != small::kIsShort);
        CHECK(test_value != small::kIsMedian);
        CHECK(test_value != small::kIsLong);
    }
}