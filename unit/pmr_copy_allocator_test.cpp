#include <cstring>
#include <memory_resource>
#include <vector>

#include "doctest/doctest/doctest.h"
#include "include/smallstring.hpp"

// A tracking memory resource that records allocations and deallocations,
// used to verify that the correct allocator is used during copy/move.
class tracking_resource : public std::pmr::memory_resource
{
   public:
    int alloc_count = 0;
    int dealloc_count = 0;
    std::pmr::memory_resource* upstream = std::pmr::new_delete_resource();

   protected:
    auto do_allocate(size_t bytes, size_t alignment) -> void* override {
        ++alloc_count;
        return upstream->allocate(bytes, alignment);
    }
    void do_deallocate(void* p, size_t bytes, size_t alignment) override {
        ++dealloc_count;
        upstream->deallocate(p, bytes, alignment);
    }
    auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override {
        return this == &other;
    }
};

// Long string that exceeds the internal SSO buffer (>7 bytes),
// forcing allocation through the polymorphic allocator.
static const char* long_str = "this string is definitely longer than the internal buffer";

TEST_CASE("pmr copy constructor with allocator uses the provided allocator") {
    tracking_resource res_src;
    tracking_resource res_dst;
    std::pmr::polymorphic_allocator<char> alloc_src(&res_src);
    std::pmr::polymorphic_allocator<char> alloc_dst(&res_dst);

    small::pmr::small_byte_string src(long_str, std::strlen(long_str), alloc_src);
    CHECK(src.get_allocator().resource() == &res_src);
    CHECK(res_src.alloc_count > 0);

    int dst_allocs_before = res_dst.alloc_count;
    small::pmr::small_byte_string dst(src, alloc_dst);

    // The copy must use alloc_dst, not alloc_src
    CHECK(dst.get_allocator().resource() == &res_dst);
    CHECK(res_dst.alloc_count > dst_allocs_before);
    CHECK(std::string_view(dst.data(), dst.size()) == long_str);
}

TEST_CASE("pmr copy constructor with allocator — data survives source allocator destruction") {
    tracking_resource res_dst;
    std::pmr::polymorphic_allocator<char> alloc_dst(&res_dst);

    small::pmr::small_byte_string copy_outside;
    {
        tracking_resource res_src;
        std::pmr::polymorphic_allocator<char> alloc_src(&res_src);

        small::pmr::small_byte_string src(long_str, std::strlen(long_str), alloc_src);
        copy_outside = small::pmr::small_byte_string(src, alloc_dst);
    }
    // res_src is destroyed here. If the copy incorrectly held res_src's allocator,
    // accessing or destroying copy_outside would use-after-free.

    CHECK(copy_outside.get_allocator().resource() == &res_dst);
    CHECK(std::string_view(copy_outside.data(), copy_outside.size()) == long_str);
    // copy_outside destructor runs safely because it uses res_dst
}

TEST_CASE("pmr vector of small_byte_string copies use container allocator") {
    tracking_resource res_vec;
    tracking_resource res_elem;
    std::pmr::polymorphic_allocator<char> alloc_vec(&res_vec);
    std::pmr::polymorphic_allocator<char> alloc_elem(&res_elem);

    // Build a vector with alloc_vec
    std::pmr::vector<small::pmr::small_byte_string> vec(alloc_vec);
    // Construct an element with a *different* allocator
    small::pmr::small_byte_string elem(long_str, std::strlen(long_str), alloc_elem);

    vec.push_back(elem);

    // The element inside the vector should use the *vector's* allocator,
    // not the original element's allocator.
    CHECK(vec[0].get_allocator().resource() == &res_vec);
    CHECK(std::string_view(vec[0].data(), vec[0].size()) == long_str);
}
