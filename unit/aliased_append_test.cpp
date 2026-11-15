#include <string>
#include <string_view>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

namespace {
template <class String, bool NullTerminated>
void check_aliased_append() {
    const size_t lengths[] = {1, 3, 6, 7, 100, 255, 256, 512, 16384};
    for (bool spare_capacity : {false, true}) {
        for (size_t length : lengths) {
            for (int operation = 0; operation < 11; ++operation) {
                if (!NullTerminated && operation >= 8) continue;
                CAPTURE(spare_capacity);
                CAPTURE(length);
                CAPTURE(operation);
                std::string input(length, 'a');
                for (size_t i = 0; i < length; ++i) input[i] = static_cast<char>('a' + i % 23);
                String str(input.data(), input.size());
                if (spare_capacity) str.reserve(length * 3 + 8);
                else str.resize(str.capacity(), '#');
                const auto* old_data = str.data();
                const auto old_capacity = str.capacity();
                const std::string before(str.data(), str.size());
                const size_t offset = before.size() / 3;
                std::string suffix = before;
                switch (operation) {
                    case 0: str.append(str.data(), str.size()); break;
                    case 1: str.append(std::string_view(str.data(), str.size())); break;
                    case 2:
                        suffix = before.substr(offset);
                        str.append(str.data() + offset, str.size() - offset);
                        break;
                    case 3:
                        suffix = before.substr(offset);
                        str.append(str, offset, str.size() - offset);
                        break;
                    case 4:
                        suffix = before.substr(offset);
                        str.append(std::string_view(str.data(), str.size()), offset);
                        break;
                    case 5: str.append(str.begin(), str.end()); break;
                    case 6:
                        suffix.assign(before.rbegin(), before.rend());
                        str.append(str.rbegin(), str.rend());
                        break;
                    case 7: str.append(str); break;
                    case 8: str.append(str.data()); break;
                    case 9:
                        suffix.push_back('\0');
                        str.append(str.data(), str.size() + 1);
                        break;
                    case 10:
                        suffix.assign(1, '\0');
                        str.append(str.data() + str.size(), 1);
                        break;
                }
                CHECK(std::string(str.data(), str.size()) == before + suffix);
                if (spare_capacity) CHECK(str.data() == old_data);
                else CHECK(str.size() > old_capacity);
                if constexpr (NullTerminated) CHECK(str.data()[str.size()] == '\0');
            }
        }
    }
    String empty;
    empty.append(empty.data(), 0);
    empty.append(empty.begin(), empty.end());
    CHECK(empty.empty());
}
}  // namespace

TEST_CASE("self-referential append preserves its source") {
    check_aliased_append<small::small_string, true>();
    check_aliased_append<small::small_byte_string, false>();
    check_aliased_append<small::pmr::small_string, true>();
    check_aliased_append<small::pmr::small_byte_string, false>();
}
