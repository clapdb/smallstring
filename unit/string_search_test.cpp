#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

TEST_CASE("smallstring advanced find operations") {
    SUBCASE("find with edge cases and boundary conditions") {
        small::small_string str("hello world hello");

        // Empty pattern find
        CHECK(str.find("") == 0);
        CHECK(str.find("", 5) == 5);
        CHECK(str.find("", str.size()) == str.size());

        // Pattern at exact boundaries
        CHECK(str.find("hello", 0) == 0);
        CHECK(str.find("hello", 12) == 12);
        CHECK(str.find("world", 6) == 6);

        // Pattern longer than remaining string
        CHECK(str.find("hello world hello!", 0) == small::small_string::npos);
        CHECK(str.find("hello", 14) == small::small_string::npos);

        // Pattern at end
        CHECK(str.find("llo", 14) == 14);

        // Single character at boundaries
        CHECK(str.find('h', 0) == 0);
        CHECK(str.find('o', 16) == 16);

        // With internal vs external string
        small::small_string small_str("abc");
        small::small_string large_str(100, 'x');
        large_str += "target";
        CHECK(small_str.find('b') == 1);
        CHECK(large_str.find("target") == 100);
    }

    SUBCASE("rfind comprehensive edge cases") {
        small::small_string str("hello world hello world");

        // Basic rfind functionality
        CHECK(str.rfind("world") == 18);
        CHECK(str.rfind("hello") == 12);
        CHECK(str.rfind('d') == 22);

        // rfind with position constraints
        CHECK(str.rfind("hello", 10) == 0);
        CHECK(str.rfind("world", 16) == 6);
        CHECK(str.rfind('o', 4) == 4);

        // Empty pattern
        CHECK(str.rfind("") == str.size());
        CHECK(str.rfind("", 10) == 10);
        CHECK(str.rfind("", 0) == 0);

        // Pattern not found
        CHECK(str.rfind("xyz") == small::small_string::npos);
        CHECK(str.rfind('z') == small::small_string::npos);

        // Position beyond string size
        CHECK(str.rfind("hello", 1000) == 12);
        CHECK(str.rfind('o', 1000) == 19);

        // Overlapping patterns
        small::small_string overlap("aaaa");
        CHECK(overlap.rfind("aa") == 2);
        CHECK(overlap.rfind("aaa") == 1);

        // Single character string
        small::small_string single("x");
        CHECK(single.rfind('x') == 0);
        CHECK(single.rfind("x") == 0);
        CHECK(single.rfind('y') == small::small_string::npos);
    }

    SUBCASE("find_first_of comprehensive testing") {
        small::small_string str("hello, beautiful world!");

        // Basic find_first_of
        CHECK(str.find_first_of("aeiou") == 1);  // 'e'
        CHECK(str.find_first_of("xyz") == small::small_string::npos);

        // Single character search
        CHECK(str.find_first_of('l') == 2);
        CHECK(str.find_first_of('z') == small::small_string::npos);

        // With position
        CHECK(str.find_first_of("aeiou", 2) == 4);  // 'o'
        CHECK(str.find_first_of("l", 3) == 3);

        // Multiple occurrences
        CHECK(str.find_first_of("lw", 0) == 2);  // First 'l'
        CHECK(str.find_first_of("lw", 3) == 3);  // Second 'l'

        // Special characters
        CHECK(str.find_first_of(",!") == 5);  // ','
        CHECK(str.find_first_of(" ") == 6);   // space

        // Empty search string
        CHECK(str.find_first_of("") == small::small_string::npos);

        // All characters match
        small::small_string abc("abc");
        CHECK(abc.find_first_of("abc") == 0);
        CHECK(abc.find_first_of("cba") == 0);

        // String_view compatibility
        std::string_view vowels("aeiou");
        CHECK(str.find_first_of(vowels) == 1);
    }

    SUBCASE("find_last_of comprehensive testing") {
        small::small_string str("hello, beautiful world!");

        // Basic find_last_of
        CHECK(str.find_last_of("aeiou") == 18);  // 'o'
        CHECK(str.find_last_of("xyz") == small::small_string::npos);

        // Single character
        CHECK(str.find_last_of('l') == 20);
        CHECK(str.find_last_of('z') == small::small_string::npos);

        // With position constraint
        CHECK(str.find_last_of("aeiou", 10) == 10);  // 'u'
        CHECK(str.find_last_of("l", 15) == 15);

        // Multiple character set
        CHECK(str.find_last_of("!,") == 22);  // '!'
        CHECK(str.find_last_of("ld") == 21);  // 'd'

        // Position at exact match
        CHECK(str.find_last_of("l", 20) == 20);
        CHECK(str.find_last_of("l", 19) == 15);

        // Empty search string
        CHECK(str.find_last_of("") == small::small_string::npos);

        // String_view compatibility
        std::string_view consonants("bcdfg");
        CHECK(str.find_last_of(consonants) == 21);  // 'r'

        // Edge case: position 0
        CHECK(str.find_last_of("h", 0) == 0);
        CHECK(str.find_last_of("x", 0) == small::small_string::npos);
    }

    SUBCASE("find_first_not_of comprehensive testing") {
        small::small_string str("aaabbbccc");

        // Basic find_first_not_of
        CHECK(str.find_first_not_of("a") == 3);  // First 'b'
        CHECK(str.find_first_not_of("abc") == small::small_string::npos);

        // Single character
        CHECK(str.find_first_not_of('a') == 3);

        // With position
        CHECK(str.find_first_not_of("ab", 0) == 6);  // First 'c'
        CHECK(str.find_first_not_of("a", 4) == 4);   // 'b' at position 4

        // Complex patterns
        small::small_string complex("   hello world   ");
        CHECK(complex.find_first_not_of(" ") == 3);   // 'h'
        CHECK(complex.find_first_not_of(" h") == 4);  // 'e'

        // All characters excluded
        small::small_string digits("123");
        CHECK(digits.find_first_not_of("0123456789") == small::small_string::npos);

        // Empty exclusion string (should find first character)
        CHECK(str.find_first_not_of("") == 0);

        // String_view compatibility
        std::string_view whitespace(" \t\n\r");
        CHECK(complex.find_first_not_of(whitespace) == 3);

        // Position beyond string
        CHECK(str.find_first_not_of("a", 100) == small::small_string::npos);
    }

    SUBCASE("find_last_not_of comprehensive testing") {
        small::small_string str("hello world   ");

        // Basic functionality
        CHECK(str.find_last_not_of(" ") == 10);  // 'd'
        CHECK(str.find_last_not_of("d ") == 9);  // 'l'

        // Single character
        CHECK(str.find_last_not_of('d') == 13);  // last space

        // With position
        CHECK(str.find_last_not_of(" ", 8) == 8);       // 'o'
        CHECK(str.find_last_not_of("world ", 5) == 1);  // 'e'

        // Complex patterns
        small::small_string path("/path/to/file.txt");
        CHECK(path.find_last_not_of("txt.") == 12);  // 'e'
        CHECK(path.find_last_not_of("/") == 16);     // 't'

        // All characters are in exclusion set
        small::small_string spaces("   ");
        CHECK(spaces.find_last_not_of(" ") == small::small_string::npos);

        // Empty exclusion string
        CHECK(str.find_last_not_of("") == str.size() - 1);

        // String_view compatibility
        std::string_view punctuation(".,!?");
        small::small_string sentence("Hello, world!");
        CHECK(sentence.find_last_not_of(punctuation) == 11);  // 'd'

        // Position 0 cases
        CHECK(str.find_last_not_of("h", 0) == small::small_string::npos);
        CHECK(str.find_last_not_of("x", 0) == 0);
    }
}

TEST_CASE("smallstring search with string_view and custom types") {
    SUBCASE("find operations with string_view") {
        small::small_string str("The quick brown fox jumps");

        // string_view find
        std::string_view needle("quick");
        CHECK(str.find(needle) == 4);

        std::string_view not_found("slow");
        CHECK(str.find(not_found) == small::small_string::npos);

        // string_view with position
        std::string_view fox("fox");
        CHECK(str.find(fox, 10) == 16);
        CHECK(str.find(fox, 20) == small::small_string::npos);

        // Empty string_view
        std::string_view empty("");
        CHECK(str.find(empty) == 0);
        CHECK(str.find(empty, 5) == 5);

        // rfind with string_view
        std::string_view space(" ");
        CHECK(str.rfind(space) == 19);     // Last space
        CHECK(str.rfind(space, 10) == 9);  // Space after "quick"
    }

    SUBCASE("find operations with basic_small_string") {
        small::small_string haystack("needle in haystack");
        small::small_string needle("needle");
        small::small_string hay("hay");
        small::small_string missing("thread");

        // Find other small_string
        CHECK(haystack.find(needle) == 0);
        CHECK(haystack.find(hay) == 10);
        CHECK(haystack.find(missing) == small::small_string::npos);

        // rfind with small_string
        CHECK(haystack.rfind(needle) == 0);
        CHECK(haystack.rfind(hay) == 10);

        // Cross-allocator compatibility (PMR)
        small::pmr::small_string pmr_needle("needle");
        CHECK(haystack.find(pmr_needle) == 0);
    }

    SUBCASE("search pattern edge cases") {
        // Overlapping patterns
        small::small_string overlap("abcabcabc");
        CHECK(overlap.find("abca") == 0);
        CHECK(overlap.find("abca", 1) == 3);
        CHECK(overlap.find("bcab") == 1);

        // Partial matches
        small::small_string partial("abcdef");
        CHECK(partial.find("abc") == 0);
        CHECK(partial.find("def") == 3);
        CHECK(partial.find("cde") == 2);
        CHECK(partial.find("abcdefg") == small::small_string::npos);

        // Repeated characters
        small::small_string repeated("aaaaaa");
        CHECK(repeated.find("aa") == 0);
        CHECK(repeated.find("aa", 1) == 1);
        CHECK(repeated.find("aaa") == 0);
        CHECK(repeated.find("aaa", 2) == 2);

        // Special character patterns
        small::small_string special("a\nb\tc\rd");
        CHECK(special.find('\n') == 1);
        CHECK(special.find('\t') == 3);
        CHECK(special.find('\r') == 5);
        CHECK(special.find("b\tc") == 2);
    }

    SUBCASE("performance characteristics") {
        // Large string search
        small::small_string large(1000, 'x');
        large += "target";
        large += std::string(1000, 'y');

        CHECK(large.find("target") == 1000);
        CHECK(large.rfind("target") == 1000);
        CHECK(large.find('t') == 1000);
        CHECK(large.rfind('t') == 1005);

        // Multiple target occurrences
        small::small_string multi("abcabcabcabc");
        CHECK(multi.find("abc") == 0);
        CHECK(multi.find("abc", 1) == 3);
        CHECK(multi.find("abc", 4) == 6);
        CHECK(multi.find("abc", 7) == 9);
        CHECK(multi.find("abc", 10) == small::small_string::npos);

        CHECK(multi.rfind("abc") == 9);
        CHECK(multi.rfind("abc", 8) == 6);
        CHECK(multi.rfind("abc", 5) == 3);
        CHECK(multi.rfind("abc", 2) == 0);
    }
}