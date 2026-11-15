#include <string>
#include <string_view>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

namespace {
template <class String, bool NullTerminated>
void check_aliased_insert() {
    const size_t lengths[] = {1, 3, 6, 7, 100, 255, 256, 16384};
    for (bool spare_capacity : {false, true}) {
        for (size_t length : lengths) {
            for (int position = 0; position < 3; ++position) {
                for (int operation = 0; operation < 10; ++operation) {
                    if (!NullTerminated && operation >= 7) continue;
                    CAPTURE(spare_capacity);
                    CAPTURE(length);
                    CAPTURE(position);
                    CAPTURE(operation);
                    std::string input(length, 'a');
                    for (size_t i = 0; i < length; ++i) input[i] = static_cast<char>('a' + i % 23);
                    String str(input.data(), input.size());
                    if (spare_capacity) str.reserve(length * 3 + 8);
                    else str.resize(str.capacity(), '#');
                    const auto* old_data = str.data();
                    const auto old_capacity = str.capacity();
                    const std::string before(str.data(), str.size());
                    const size_t index = position == 0 ? 0 : position == 1 ? before.size() / 2 : before.size();
                    const size_t offset = before.size() / 3;
                    std::string suffix = before;
                    switch (operation) {
                        case 0: str.insert(index, str.data(), str.size()); break;
                        case 1: str.insert(index, str); break;
                        case 2:
                            suffix = before.substr(offset);
                            str.insert(index, str.data() + offset, str.size() - offset);
                            break;
                        case 3:
                            suffix = before.substr(offset);
                            str.insert(index, str, offset, str.size() - offset);
                            break;
                        case 4: {
                            auto result = str.insert(str.begin() + index, std::string_view(str.data(), str.size()));
                            CHECK(result == str.begin() + index);
                            break;
                        }
                        case 5: {
                            suffix = before.substr(offset);
                            auto result = str.insert(str.begin() + index, str.begin() + offset, str.end());
                            CHECK(result == str.begin() + index);
                            break;
                        }
                        case 6: {
                            suffix.assign(before.rbegin(), before.rend());
                            auto result = str.insert(str.begin() + index, str.rbegin(), str.rend());
                            CHECK(result == str.begin() + index);
                            break;
                        }
                        case 7: str.insert(index, str.data()); break;
                        case 8:
                            suffix.push_back('\0');
                            str.insert(index, str.data(), str.size() + 1);
                            break;
                        case 9:
                            suffix.assign(1, '\0');
                            str.insert(index, str.data() + str.size(), 1);
                            break;
                    }
                    std::string expected = before;
                    expected.insert(index, suffix);
                    CHECK(std::string(str.data(), str.size()) == expected);
                    if (spare_capacity) CHECK(str.data() == old_data);
                    else CHECK(str.size() > old_capacity);
                    if constexpr (NullTerminated) CHECK(str.data()[str.size()] == '\0');
                }
            }
        }
    }
    String empty;
    empty.insert(size_t{0}, empty.data(), size_t{0});
    CHECK(empty.empty());
    CHECK(empty.insert(empty.begin(), empty.begin(), empty.end()) == empty.begin());
}
}  // namespace

TEST_CASE("self-referential insert preserves its source and result iterator") {
    check_aliased_insert<small::small_string, true>();
    check_aliased_insert<small::small_byte_string, false>();
    check_aliased_insert<small::pmr::small_string, true>();
    check_aliased_insert<small::pmr::small_byte_string, false>();
}
