#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

TEST_CASE("smallstring stream operations") {
    SUBCASE("output stream operator") {
        small::small_string str("hello world");
        std::ostringstream oss;

        // Basic output
        oss << str;
        CHECK(oss.str() == "hello world");

        // Multiple outputs
        oss.str("");
        small::small_string str1("first");
        small::small_string str2("second");
        oss << str1 << " " << str2;
        CHECK(oss.str() == "first second");

        // Empty string output
        oss.str("");
        small::small_string empty;
        oss << empty;
        CHECK(oss.str() == "");

        // Large string output
        oss.str("");
        small::small_string large(100, 'X');
        oss << large;
        CHECK(oss.str() == std::string(100, 'X'));

        // Special characters
        oss.str("");
        small::small_string special("line1\nline2\ttab");
        oss << special;
        CHECK(oss.str() == "line1\nline2\ttab");

        // PMR string output
        oss.str("");
        small::pmr::small_string pmr_str("pmr string");
        oss << pmr_str;
        CHECK(oss.str() == "pmr string");

        // Output with formatting
        oss.str("");
        oss << std::setw(15) << std::left << str;
        CHECK(oss.str().size() >= str.size());
        CHECK(oss.str().substr(0, str.size()) == str);

        oss.str("");
        oss << std::setw(10) << std::right << small::small_string("right");
        std::string formatted = oss.str();
        CHECK(formatted.size() == 10);
        CHECK(formatted.substr(5) == "right");
    }

    SUBCASE("input stream operator") {
        std::istringstream iss;
        small::small_string str;

        // Basic input
        iss.str("hello");
        iss >> str;
        CHECK(str == "hello");
        CHECK(!iss.fail());  // Operation should succeed even if EOF reached

        // Input stops at whitespace
        iss.clear();
        iss.str("hello world");
        str.clear();
        iss >> str;
        CHECK(str == "hello");

        // Continue reading
        small::small_string str2;
        iss >> str2;
        CHECK(str2 == "world");

        // Input with empty stream
        iss.clear();
        iss.str("");
        str.clear();
        iss >> str;
        CHECK(str.empty());
        CHECK(iss.fail());

        // Input very long word
        iss.clear();
        std::string long_word(1000, 'L');
        iss.str(long_word + " next");
        str.clear();
        iss >> str;
        CHECK(str == long_word);
        CHECK(str.size() == 1000);

        // Input with leading whitespace
        iss.clear();
        iss.str("   \t  word");
        str.clear();
        iss >> str;
        CHECK(str == "word");

        // Multiple consecutive reads
        iss.clear();
        iss.str("one two three four");
        small::small_string words[4];
        for (int i = 0; i < 4; ++i) {
            iss >> words[i];
            CHECK(!iss.fail());
        }
        CHECK(words[0] == "one");
        CHECK(words[1] == "two");
        CHECK(words[2] == "three");
        CHECK(words[3] == "four");

        // Test EOF handling
        small::small_string extra;
        iss >> extra;
        CHECK(iss.eof());
        CHECK(extra.empty());
    }

    SUBCASE("getline function") {
        std::istringstream iss;
        small::small_string line;
        std::string std_line;

        // Basic getline
        iss.str("first line\nsecond line");
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line == "first line");
        CHECK(iss.good());

        // Continue with second line
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line == "second line");
        CHECK(iss.eof());  // Should be at end now

        // getline with empty line
        iss.clear();
        iss.str("line1\n\nline3");
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line == "line1");
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line.empty());  // Empty line
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line == "line3");

        // getline with custom delimiter
        iss.clear();
        iss.str("field1,field2,field3");
        std::getline(iss, std_line, ',');
        line = std_line;
        CHECK(line == "field1");
        std::getline(iss, std_line, ',');
        line = std_line;
        CHECK(line == "field2");
        std::getline(iss, std_line, ',');
        line = std_line;
        CHECK(line == "field3");

        // getline very long line
        iss.clear();
        std::string long_line(2000, 'L');
        iss.str(long_line);
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line.size() == 2000);
        CHECK(line == long_line);

        // getline with no newline at end
        iss.clear();
        iss.str("no newline");
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line == "no newline");
        CHECK(iss.eof());

        // getline with mixed line endings
        iss.clear();
        iss.str("unix\nwindows\r\nmac\r");
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line == "unix");
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line == "windows\r");  // \r remains
        std::getline(iss, std_line);
        line = std_line;
        CHECK(line == "mac\r");
    }
}

TEST_CASE("smallstring conversion utilities") {
    SUBCASE("to_small_string functions") {
        using small::to_small_string;

        // Integer conversions
        CHECK(to_small_string<small::small_string>(42) == "42");
        CHECK(to_small_string<small::small_string>(-123) == "-123");
        CHECK(to_small_string<small::small_string>(0) == "0");

        // Large integer
        CHECK(to_small_string<small::small_string>(1234567890LL) == "1234567890");
        CHECK(to_small_string<small::small_string>(-9876543210LL) == "-9876543210");

        // Unsigned integers
        CHECK(to_small_string<small::small_string>(42u) == "42");
        CHECK(to_small_string<small::small_string>(0u) == "0");
        CHECK(to_small_string<small::small_string>(4294967295u) == "4294967295");

        // Floating point
        auto float_str = to_small_string<small::small_string>(3.14f);
        CHECK(float_str.find("3.14") == 0);  // Should start with 3.14

        auto double_str = to_small_string<small::small_string>(2.71828);
        CHECK(double_str.find("2.71828") == 0);

        // Special float values
        auto inf_str = to_small_string<small::small_string>(std::numeric_limits<double>::infinity());
        CHECK((inf_str == "inf" || inf_str == "infinity"));

        // String conversions
        CHECK(to_small_string<small::small_string>("hello") == "hello");
        CHECK(to_small_string<small::small_string>(std::string("world")) == "world");
        CHECK(to_small_string<small::small_string>(std::string_view("view")) == "view");

        // Empty string
        CHECK(to_small_string<small::small_string>("") == "");
        CHECK(to_small_string<small::small_string>(std::string()) == "");

        // PMR versions
        auto pmr_resource = std::pmr::new_delete_resource();
        std::pmr::polymorphic_allocator<char> pmr_alloc(pmr_resource);

        auto pmr_int = small::pmr::to_small_string<small::pmr::small_string>(123, pmr_alloc);
        CHECK(pmr_int == "123");
        CHECK(pmr_int.get_allocator().resource() == pmr_resource);

        auto pmr_str = small::pmr::to_small_string<small::pmr::small_string>("pmr test", pmr_alloc);
        CHECK(pmr_str == "pmr test");
        CHECK(pmr_str.get_allocator().resource() == pmr_resource);

        auto pmr_view = small::pmr::to_small_string<small::pmr::small_string>(std::string_view("pmr view"), pmr_alloc);
        CHECK(pmr_view == "pmr view");
        CHECK(pmr_view.get_allocator().resource() == pmr_resource);
    }
}

TEST_CASE("smallstring hash support") {
    SUBCASE("std::hash specialization") {
        small::small_string str1("test");
        small::small_string str2("test");
        small::small_string str3("different");

        std::hash<small::small_string> hasher;

        // Same strings should hash the same
        auto hash1 = hasher(str1);
        auto hash2 = hasher(str2);
        CHECK(hash1 == hash2);

        // Different strings should likely hash differently
        auto hash3 = hasher(str3);
        CHECK(hash1 != hash3);  // Very likely but not guaranteed

        // Empty string hash
        small::small_string empty;
        auto empty_hash = hasher(empty);
        (void)empty_hash;  // Just ensure it doesn't crash

        // Large string hash
        small::small_string large(1000, 'H');
        auto large_hash = hasher(large);
        (void)large_hash;

        // Same content, different storage might hash the same
        small::small_string internal("short");
        small::small_string external = internal;
        external.reserve(100);  // Force external storage
        external.shrink_to_fit();

        auto internal_hash = hasher(internal);
        auto external_hash = hasher(external);
        CHECK(internal_hash == external_hash);  // Same content

        // PMR string hash compatibility
        small::pmr::small_string pmr_str("pmr hash test");
        std::hash<small::pmr::small_string> pmr_hasher;
        auto pmr_hash = pmr_hasher(pmr_str);
        (void)pmr_hash;

        // Hash should work with standard containers
        std::unordered_set<small::small_string> string_set;
        string_set.insert(str1);
        string_set.insert(str2);  // Should not insert duplicate
        string_set.insert(str3);

        CHECK(string_set.size() == 2);  // Only str1/str2 and str3
        CHECK(string_set.count(str1) == 1);
        CHECK(string_set.count(str2) == 1);  // Same as str1
        CHECK(string_set.count(str3) == 1);

        // Hash map usage
        std::unordered_map<small::small_string, int> string_map;
        string_map[str1] = 1;
        string_map[str2] = 2;  // Should overwrite str1's value
        string_map[str3] = 3;

        CHECK(string_map.size() == 2);
        CHECK(string_map[str1] == 2);  // Overwritten by str2
        CHECK(string_map[str3] == 3);
    }

    SUBCASE("hash consistency and distribution") {
        std::hash<small::small_string> hasher;
        std::vector<size_t> hashes;

        // Generate hashes for various strings
        for (int i = 0; i < 100; ++i) {
            small::small_string test_str = "test_string_" + std::to_string(i);
            hashes.push_back(hasher(test_str));
        }

        // Check for uniqueness (most should be unique)
        std::sort(hashes.begin(), hashes.end());
        auto unique_end = std::unique(hashes.begin(), hashes.end());
        size_t unique_count = static_cast<size_t>(std::distance(hashes.begin(), unique_end));

        // Should have good distribution (at least 90% unique)
        CHECK(unique_count >= 90);

        // Test hash stability
        small::small_string stable_str("stability test");
        auto hash_first = hasher(stable_str);

        // Modify and restore
        stable_str += " modified";
        stable_str.resize(14);  // Back to "stability test"
        auto hash_second = hasher(stable_str);

        CHECK(hash_first == hash_second);  // Should be same

        // Test with different content, same length
        small::small_string same_len1("abcdefgh");
        small::small_string same_len2("ijklmnop");
        CHECK(hasher(same_len1) != hasher(same_len2));  // Very likely different
    }
}