/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <sys/types.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <fmt/format.h>

namespace small {
#ifndef Assert
#define Assert(condition, message) assert((condition) && (message))
#endif
namespace {
inline constexpr uint64_t kMinAlignSize = 8;  // 64 bits for modern cpu
/**
 * @brief Aligns a value up to the next multiple of N
 * @tparam N Alignment boundary (must be power of 2, >= 8)
 * @param n Value to align
 * @return Value aligned up to next N boundary
 * @note Uses bit manipulation for efficient alignment calculation
 * @note From "Hacker's Delight 2nd edition", Chapter 3
 */
template <uint64_t N>
[[gnu::always_inline]] constexpr auto AlignUpTo(uint64_t n) noexcept -> uint64_t {
    // Align n to next multiple of N
    // (from <Hacker's Delight 2rd edition>,Chapter 3.)
    static_assert((N & (N - 1)) == 0, "AlignUpToN, N is power of 2");
    static_assert(N >= kMinAlignSize, "AlignUpToN, N should be more than 8");  // align to 2/4 doesnt make sense
    return (n + N - 1) & static_cast<uint64_t>(-N);
}

}  // namespace

/**
 * @brief Storage strategy enumeration for small string optimization
 * @note Defines four storage modes based on string length and capacity requirements
 * @note Stored as 2-bit flag in the least significant bits of storage metadata
 * @note Ordered by capacity: Internal < Short < Median < Long
 */
enum class CoreType : uint8_t
{
    /**
     * @brief Internal storage for very small strings (up to 6-7 characters)
     * @note Data stored directly in the string object, no heap allocation
     * @note Most efficient for short strings, zero allocation overhead
     */
    Internal = 0,

    /**
     * @brief Short external storage for small strings (up to 255 characters)
     * @note Uses external heap allocation with compact 8-byte metadata
     * @note Capacity limited to 32 * 8 bytes, size up to 256 characters
     */
    Short = 1,

    /**
     * @brief Median external storage with idle capacity tracking (up to 16383 characters)
     * @note Uses buffer header to store capacity and size information
     * @note Tracks idle capacity for efficient append operations without reallocation
     */
    Median = 2,

    /**
     * @brief Long external storage for large strings (unlimited size)
     * @note Uses buffer header with full capacity and size tracking
     * @note Supports strings larger than 16KB with optimal memory management
     */
    Long = 3
};

/// @brief Constant representing the Internal core type as uint8_t
/// @note Used for efficient type checking and storage classification
constexpr inline auto kIsInternal = static_cast<uint8_t>(CoreType::Internal);

/// @brief Constant representing the Short core type as uint8_t
/// @note Used for efficient type checking and storage classification
constexpr inline auto kIsShort = static_cast<uint8_t>(CoreType::Short);

/// @brief Constant representing the Median core type as uint8_t
/// @note Used for efficient type checking and storage classification
constexpr inline auto kIsMedian = static_cast<uint8_t>(CoreType::Median);

/// @brief Constant representing the Long core type as uint8_t
/// @note Used for efficient type checking and storage classification
constexpr inline auto kIsLong = static_cast<uint8_t>(CoreType::Long);

/**
 * @brief Combines buffer size information with core type flag
 * @tparam S Size type (typically uint32_t)
 * @note Used for efficient storage of buffer metadata in 8-byte aligned structure
 */
template <typename S>
struct buffer_type_and_size
{
    S buffer_size;       ///< Total size of allocated buffer in bytes
    CoreType core_type;  ///< Storage strategy: Internal/Short/Median/Long
};

static_assert(sizeof(buffer_type_and_size<uint32_t>) == 8);

/**
 * @brief Stores string capacity and current size for external buffers
 * @tparam S Size type (typically uint32_t)
 * @note Used in buffer headers for Median and Long storage types
 * @note Capacity excludes null termination and header overhead
 */
template <typename S>
struct capacity_and_size
{
    S capacity;  ///< Maximum character capacity (excluding null terminator)
    S size;      ///< Current number of characters in string
};

static_assert(sizeof(capacity_and_size<uint32_t>) == 8);

/**
 * the struct was wrapped all of status and data / ptr.
 */
template <typename Char, bool NullTerminated>
struct malloc_core
{
    using size_type = std::uint32_t;
    using use_std_allocator = std::true_type;

    /**
     * the internal_core will hold the data and the size, for sizeof(Char) == 1, and NullTerminated is true, the
     * capacity = 6, or if NullTerminated is false, the capacity = 7 will be stored in the core, the buf_ptr ==
     * c_str_ptr == data
     *
     * TODO: if want to support sizeof(Char) != 1, internal_core maybe to be deprecated.
     */
    struct internal_core
    {
        Char data[7];               ///< Embedded character storage (6 chars + null term or 7 chars)
        uint8_t internal_size : 6;  ///< Current string length (0-63, but limited by data array)
        uint8_t flag : 2;           ///< Storage type flag: must be 00 for Internal storage
    };  // struct internal_core
    static_assert(sizeof(internal_core) == 8);

    /**
     * the external_core will hold a pointer to the buffer, if the buffer size is less than 4k, the capacity and size
     * will be stored in the core, the buf_ptr == c_str_ptr
     * or store the capacity and size in the buffer head. [capacity:4B, size:4B]
     * the c_str_ptr point to the 8 bytes after the head, the c_str_ptr - 2 is the capacity, the c_str_ptr - 1 is the
     * size (char*)buf_ptr + 8 == c_str_ptr
     */
    struct external_core
    {
        /// Pointer to string data (48-bit addresses on modern 64-bit systems)
        int64_t c_str_ptr : 48;  ///< Address of character data (assumes little-endian)

        /**
         * @brief Compact storage for Short buffer metadata
         * @note Used when total capacity ≤ 256 characters
         * @note Fits capacity and size info in remaining 16 bits of external_core
         */
        struct cap_and_size
        {
            uint8_t cap : 5;    ///< Encoded capacity: real_capacity = (cap + 1) * 8 (range: 8-256)
            uint16_t size : 9;  ///< Current string size (max 256 characters)
            uint8_t flag : 2;   ///< Storage type flag: must be 01 for Short storage
        };

        /**
         * @brief Tracks available space in Median buffers
         * @note Used when buffer capacity is between 256-16383 characters
         * @note Capacity and size are stored in buffer header for these sizes
         */
        struct idle_cap
        {
            uint16_t idle_or_ignore : 14;  ///< Available unused capacity (max 16383 chars)
            uint8_t flag : 2;              ///< Storage type flag: 10=Median, 11=Long
        };

        static_assert(sizeof(idle_cap) == 2);

        /**
         * @brief Union providing different views of the 16-bit metadata field
         * @note Interpretation depends on storage type flag:
         *       - flag=01: cap_size for Short buffers
         *       - flag=10/11: idle for Median/Long buffers
         *       - mask: raw 16-bit value for direct manipulation
         */
        union
        {
            idle_cap idle;          ///< Idle capacity info for Median/Long storage
            cap_and_size cap_size;  ///< Capacity/size info for Short storage
            uint16_t mask;          ///< Raw metadata bits for atomic operations
        };

        /**
         * @brief Returns pointer to start of allocated buffer
         * @return Pointer to buffer beginning (before any header data)
         * @note For Short buffers: returns c_str_ptr directly
         * @note For Median/Long: returns c_str_ptr minus header size
         */
        [[nodiscard, gnu::always_inline]] auto get_buffer_ptr() const noexcept -> Char* {
            Assert(idle.flag > 0, "the flag should be 01 / 10 / 11");
            if (cap_size.flag == 1) [[likely]] {
                return reinterpret_cast<Char*>(c_str_ptr);
            } else {
                return reinterpret_cast<Char*>(c_str_ptr) - sizeof(struct capacity_and_size<size_type>);
            }
        }
    };  // struct external_core

    static_assert(sizeof(external_core) == 8);
    /**
     * @brief Core storage union - exactly 8 bytes for efficient operations
     * @note All views represent the same memory location
     * @note Storage type determined by flag bits in lower 2 positions
     */
    union
    {
        int64_t body;            ///< Raw 64-bit value for atomic operations and fast copying
        Char init_slice[8];      ///< Initialization helper: init[7]=0 makes empty string
        internal_core internal;  ///< Small string storage (embedded data + metadata)
        external_core external;  ///< Large string storage (pointer + metadata)
    };

    /**
     * @brief Calculates maximum capacity for internal (embedded) storage
     * @return Maximum characters that can be stored internally
     * @note Accounts for null termination and metadata overhead
     */
    consteval static inline auto internal_buffer_size() noexcept -> size_type {
        if constexpr (NullTerminated) {
            return sizeof(internal_core) - 2;
        } else {
            return sizeof(internal_core) - 1;
        }
    }

    /**
     * @brief Calculates header size for median and long external buffers
     * @return Size of metadata header in bytes
     * @note Header stores capacity and size information
     */
    consteval static inline auto median_long_buffer_header_size() noexcept -> size_type {
        if constexpr (NullTerminated) {
            return sizeof(struct capacity_and_size<size_type>) + 1;
        } else {
            return sizeof(struct capacity_and_size<size_type>);
        }
    }

    /**
     * @brief Returns maximum size for short buffer optimization
     * @return Maximum capacity for short buffer type
     * @note Short buffers use compact header encoding
     */
    consteval static inline auto max_short_buffer_size() noexcept -> size_type {
        if constexpr (NullTerminated) {
            return 255;
        } else {
            return 256;
        }
    }

    /**
     * @brief Returns maximum size for median buffer optimization
     * @return Maximum capacity for median buffer type
     * @note Limited by 14-bit idle capacity field (16383 characters)
     */
    consteval static inline auto max_median_buffer_size() noexcept -> size_type {
        // the idle is a 14bits value, so the max idle is 2**14 - 1, the max size is 2**14 - 1 + 8
        return (1UL << 14UL) - 1;
    }

    /**
     * @brief Returns maximum size for long buffer type
     * @return Maximum capacity for long buffer type
     * @note Limited by size_type range minus header overhead
     */
    consteval static inline auto max_long_buffer_size() noexcept -> size_type {
        return std::numeric_limits<size_type>::max() - median_long_buffer_header_size();
    }

    constexpr static uint8_t kInterCore = static_cast<uint8_t>(CoreType::Internal);
    constexpr static uint8_t kShortCore = static_cast<uint8_t>(CoreType::Short);
    constexpr static uint8_t kMedianCore = static_cast<uint8_t>(CoreType::Median);
    constexpr static uint8_t kLongCore = static_cast<uint8_t>(CoreType::Long);

    /**
     * @brief Returns current storage type
     * @return Core type flag (Internal=0, Short=1, Median=2, Long=3)
     * @note Used for runtime type dispatch
     */
    [[nodiscard]] auto get_core_type() const -> uint8_t { return external.idle.flag; }

    /**
     * @brief Checks if string uses external (heap) storage
     * @return true if using external buffer, false if internal
     * @note Internal storage has flag=0, all external types have flag!=0
     */
    [[nodiscard, gnu::always_inline]] inline auto is_external() const noexcept -> bool { return internal.flag != 0; }

    /**
     * @brief Retrieves total buffer capacity from external buffer header
     * @return Total allocated buffer size in bytes
     * @note Only valid for Median and Long buffer types
     * @note Capacity includes space for data, header, and null termination
     */
    [[nodiscard, gnu::always_inline]] constexpr auto capacity_from_buffer_header() const noexcept -> size_type {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        // the capacity is stored in the buffer header, the capacity is 4 bytes, and it will handle NullTerminated in
        // allocate_new_external_buffer's logic
        return *(reinterpret_cast<size_type*>(external.c_str_ptr) - 2);
    }

    /**
     * @brief Calculates maximum usable capacity from external buffer header
     * @return Maximum number of characters that can be stored in the buffer
     * @note Only valid for Median and Long buffer types
     * @note Subtracts header size and null termination (if enabled) from total capacity
     * @note This is the actual usable space for string data
     */
    [[nodiscard, gnu::always_inline]] constexpr auto max_real_cap_from_buffer_header() const noexcept -> size_type {
        if constexpr (NullTerminated) {
            return capacity_from_buffer_header() - 1 - sizeof(struct capacity_and_size<size_type>);
        } else {
            return capacity_from_buffer_header() - sizeof(struct capacity_and_size<size_type>);
        }
    }

    /**
     * @brief Retrieves current string size from external buffer header
     * @return Current number of characters in string
     * @note Only valid for Median and Long buffer types
     * @note Size excludes null termination character
     */
    [[nodiscard, gnu::always_inline]] constexpr auto size_from_buffer_header() const noexcept -> size_type {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        return *(reinterpret_cast<size_type*>(external.c_str_ptr) - 1);
    }

    /**
     * @brief Updates string size in external buffer header
     * @param new_size New string size to set
     * @note Only valid for Median and Long buffer types
     * @note Performs bounds checking in debug builds
     */
    [[gnu::always_inline]] constexpr auto set_size_to_buffer_header(size_type new_size) const noexcept -> void {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        // check the new_size is less than the capacity
        Assert(capacity_from_buffer_header() > 256, "the capacity should be more than 256");
        Assert(new_size <= max_real_cap_from_buffer_header(), "the new size should be less than the capacity");
        *(reinterpret_cast<size_type*>(external.c_str_ptr) - 1) = new_size;
    }

    /**
     * @brief Atomically increases string size in external buffer header
     * @param size_to_increase Number of characters to add to current size
     * @return New size after increase
     * @note Only valid for Median and Long buffer types
     * @note Verifies sufficient idle capacity exists
     */
    [[nodiscard, gnu::always_inline]] constexpr auto increase_size_to_buffer_header(size_type size_to_increase) noexcept
      -> size_type {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        Assert(capacity_from_buffer_header() > 256, "the capacity should be no more than 32");
        // Capacity check is done by caller in increase_size_and_idle_and_set_term
        // check the new_size is less than the capacity
        return *(reinterpret_cast<size_type*>(external.c_str_ptr) - 1) += size_to_increase;
    }

    /**
     * @brief Atomically decreases string size in external buffer header
     * @param size_to_decrease Number of characters to remove from current size
     * @return New size after decrease
     * @note Only valid for Median and Long buffer types
     * @note Verifies decrease amount doesn't exceed current size
     */
    [[nodiscard, gnu::always_inline]] constexpr auto decrease_size_to_buffer_header(
      size_type size_to_decrease) const noexcept -> size_type {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        // check the new_size is less than the capacity
        Assert(capacity_from_buffer_header() > 256, "the capacity should be more than 256");
        Assert(size_to_decrease <= size_from_buffer_header(),
               "the size to decrease should be less than the current size");
        return *(reinterpret_cast<size_type*>(external.c_str_ptr) - 1) -= size_to_decrease;
    }

    /**
     * Retrieves the capacity and size values stored in the buffer header for medium/long strings.
     * The capacity_and_size struct is stored before the string data in the allocated buffer.
     * @return capacity_and_size<size_type> containing both capacity and size values
     */
    [[nodiscard, gnu::always_inline]] constexpr auto get_capacity_and_size_from_buffer_header() const noexcept
      -> capacity_and_size<size_type> {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        Assert(capacity_from_buffer_header() > 256, "the capacity should be more than 256");
        Assert(size_from_buffer_header() <= max_real_cap_from_buffer_header(),
               "the size should be less than the max real capacity");
        return *(reinterpret_cast<capacity_and_size<size_type>*>(external.c_str_ptr) - 1);
    }

    /**
     * @brief Calculates remaining capacity in external buffer for Median/Long types
     * @return Idle capacity as uint16_t (unused space in buffer)
     * @note Only valid for Median and Long buffer types (flag > 1)
     * @note Requires buffer capacity > 256 bytes
     * @note Subtracts current size, header size, and null termination from total capacity
     */
    [[nodiscard, gnu::always_inline]] constexpr auto get_idle_capacity_from_buffer_header() const noexcept
      -> size_type {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        Assert(capacity_from_buffer_header() > 256, "the capacity should be more than 256");
        auto [cap, size] = get_capacity_and_size_from_buffer_header();
        if constexpr (NullTerminated) {
            return cap - size - 1 - sizeof(struct capacity_and_size<size_type>);
        } else {
            return cap - size - sizeof(struct capacity_and_size<size_type>);
        }
    }

    /**
     * @brief Calculates remaining capacity for all string storage types
     * @return Number of additional characters that can be stored
     * @note Handles all storage types: Internal (0), Short (1), Median (2), Long (3)
     * @note For internal storage: buffer size minus current size
     * @note For short storage: allocated capacity minus current size
     * @note For median/long storage: delegates to get_idle_capacity_from_buffer_header
     */
    [[nodiscard, gnu::always_inline]] constexpr auto idle_capacity() const noexcept -> size_type {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:  // internal
                return internal_buffer_size() - internal.internal_size;
            case 1: {  // short
                Assert(external.cap_size.cap <= 32, "the cap should be no more than 32");
                Assert(external.cap_size.size <= 256, "the size should be no more than 256");
                if constexpr (NullTerminated) {
                    return (external.cap_size.cap + 1UL) * 8UL - external.cap_size.size - 1UL;
                } else {
                    return (external.cap_size.cap + 1UL) * 8UL - external.cap_size.size;
                }
            }
            case 2:  // median
                return external.idle.idle_or_ignore;
            case 3:  // long
                return get_idle_capacity_from_buffer_header();
            default:
                Assert(false, "the flag should be just 01-03");
                __builtin_unreachable();
        }
    }

    /**
     * @brief Gets the total storage capacity of the string
     * @return Maximum number of characters that can be stored without reallocation
     * @note Handles all storage types: Internal (0), Short (1), Median/Long (2+)
     * @note For internal storage: returns fixed internal buffer size
     * @note For short storage: calculates from encoded capacity field
     * @note For median/long storage: reads from buffer header minus overhead
     * @note Capacity excludes null termination character if NullTerminated is true
     */
    [[nodiscard, gnu::always_inline]] constexpr auto capacity() const noexcept -> size_type {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                return internal_buffer_size();
            case 1:
                if constexpr (NullTerminated) {
                    return (external.cap_size.cap + 1UL) * 8UL - 1UL;
                } else {
                    return (external.cap_size.cap + 1UL) * 8UL;
                }
            default:
                if constexpr (NullTerminated) {
                    return capacity_from_buffer_header() - 1 - sizeof(struct capacity_and_size<size_type>);
                } else {
                    return capacity_from_buffer_header() - sizeof(struct capacity_and_size<size_type>);
                }
        }
    }

    /**
     * @brief Gets the current size (length) of the string
     * @return Number of characters currently stored in the string
     * @note Handles all storage types: Internal (0), Short (1), Median/Long (2+)
     * @note For internal storage: reads from internal_size field
     * @note For short storage: reads from cap_size.size field
     * @note For median/long storage: reads from buffer header
     * @note Size excludes null termination character
     */
    [[nodiscard, gnu::always_inline]] constexpr auto size() const noexcept -> size_type {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                return internal.internal_size;
            case 1:
                return external.cap_size.size;
            default:
                return size_from_buffer_header();
        }
    }

    /**
     * set_size, whithout cap changing
     */
    /**
     * @brief Sets string size and updates idle capacity, with null termination
     * @param new_size New string size to set
     * @note Updates size in appropriate storage location based on storage type
     * @note Automatically adds null termination when NullTerminated=true
     * @note Updates idle capacity tracking for Median storage
     */
    constexpr void set_size_and_idle_and_set_term(size_type new_size) noexcept {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0: {
                // Internal buffer, the size is stored in the internal_core
                Assert(new_size <= internal_buffer_size(), "size exceeds internal buffer capacity");
                internal.internal_size = static_cast<uint8_t>(new_size);
                // set the terminator
                if constexpr (NullTerminated) {
                    Assert(internal.internal_size < 7, "internal size exceeds limit");
                    internal.data[internal.internal_size] = '\0';
                }
                break;
            }
            case 1: {
                // Short buffer, the size is stored in the external_core
                Assert(new_size <= (external.cap_size.cap + 1UL) * 8UL - (NullTerminated ? 1UL : 0UL),
                       "the new size should be less than the max real capacity");
                external.cap_size.size = static_cast<uint16_t>(new_size);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_size] = '\0';
                }
                break;
            }
            case 2: {
                // Median buffer, the size is stored in the buffer header, the idle_or_ignore is the idle size
                set_size_to_buffer_header(new_size);
                external.idle.idle_or_ignore = get_idle_capacity_from_buffer_header();
                if constexpr (NullTerminated) {
                    // set the terminator
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_size] = '\0';
                }
                break;
            }
            case 3: {
                set_size_to_buffer_header(new_size);
                if constexpr (NullTerminated) {
                    // set the terminator
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_size] = '\0';
                }
            }
        }
    }

    /**
     * @brief Increases string size by specified amount with idle capacity update
     * @param size_to_increase Number of characters to add to current size
     * @note Validates sufficient idle capacity exists before increasing
     * @note Updates idle capacity tracking for efficient space management
     * @note Adds null termination at new end position
     */
    constexpr void increase_size_and_idle_and_set_term(size_type size_to_increase) noexcept {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                Assert(internal.internal_size + size_to_increase <= internal_buffer_size(),
                       "the new size should be less than the capacity");
                internal.internal_size += size_to_increase;
                if constexpr (NullTerminated) {
                    internal.data[internal.internal_size] = '\0';
                }
                break;
            case 1:
                Assert(size_to_increase <= idle_capacity(), "the size to increase should be less than the idle size");
                external.cap_size.size += size_to_increase;
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[external.cap_size.size] = '\0';
                }
                break;
            case 2: {
                Assert(size_to_increase <= idle_capacity(), "the size to increase should be less than the idle size");
                external.idle.idle_or_ignore -= size_to_increase;
                [[maybe_unused]] auto new_str_size = increase_size_to_buffer_header(size_to_increase);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_str_size] = '\0';
                }
                break;
            }
            case 3: {
                Assert(size_to_increase <= idle_capacity(), "the size to increase should be less than the idle size");
                [[maybe_unused]] auto new_str_size = increase_size_to_buffer_header(size_to_increase);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_str_size] = '\0';
                }
                break;
            }
        }
    }

    /**
     * @brief Decreases string size by specified amount with idle capacity update
     * @param size_to_decrease Number of characters to remove from current size
     * @note Validates decrease amount doesn't exceed current size
     * @note Updates idle capacity tracking to reflect increased available space
     * @note Adds null termination at new end position
     */
    constexpr void decrease_size_and_idle_and_set_term(size_type size_to_decrease) noexcept {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                Assert(size_to_decrease <= internal.internal_size, "the size to decrease should be less than the size");
                internal.internal_size -= size_to_decrease;
                if constexpr (NullTerminated) {
                    internal.data[internal.internal_size] = '\0';
                }
                break;
            case 1:
                Assert(size_to_decrease <= external.cap_size.size,
                       "the size to decrease should be less than the current size");
                external.cap_size.size -= size_to_decrease;
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[external.cap_size.size] = '\0';
                }
                break;
            case 2: {
                Assert(size_to_decrease <= size_from_buffer_header(),
                       "the size to decrease should be less than the current size");
                external.idle.idle_or_ignore += size_to_decrease;
                [[maybe_unused]] auto new_str_size = decrease_size_to_buffer_header(size_to_decrease);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_str_size] = '\0';
                }
                break;
            }
            case 3: {
                Assert(size_to_decrease <= size_from_buffer_header(),
                       "the size to decrease should be less than the idle size");
                [[maybe_unused]] auto new_str_size = decrease_size_to_buffer_header(size_to_decrease);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_str_size] = '\0';
                }
                break;
            }
        }
    }

    [[nodiscard]] constexpr auto get_capacity_and_size() const noexcept -> capacity_and_size<size_type> {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                return {.capacity = internal_buffer_size(), .size = internal.internal_size};
            case 1: {
                if constexpr (NullTerminated) {
                    return {.capacity = static_cast<uint32_t>((external.cap_size.cap + 1) * 8UL) - 1,
                            .size = external.cap_size.size};
                } else {
                    return {.capacity = static_cast<uint32_t>((external.cap_size.cap + 1) * 8UL),
                            .size = external.cap_size.size};
                }
            }
            default: {
                auto [cap, size] = get_capacity_and_size_from_buffer_header();
                size_type real_cap;
                if constexpr (NullTerminated) {
                    real_cap = cap - 1 - sizeof(struct capacity_and_size<size_type>);
                } else {
                    real_cap = cap - sizeof(struct capacity_and_size<size_type>);
                }
                return {.capacity = real_cap, .size = size};
            }
        }
    }

    /**
     * @brief Returns pointer to beginning of string data
     * @return Mutable pointer to first character of string
     * @note For internal storage: points to embedded data array
     * @note For external storage: points to heap-allocated buffer
     * @note Always points to actual character data, not buffer header
     */
    [[nodiscard, gnu::always_inline]] inline auto begin_ptr() noexcept -> Char* {
        return is_external() ? reinterpret_cast<Char*>(external.c_str_ptr) : internal.data;
    }

    [[nodiscard, gnu::always_inline]] inline auto get_string_view() const noexcept -> std::string_view {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                return std::string_view{internal.data, internal.internal_size};
            case 1:
                return std::string_view{reinterpret_cast<Char*>(external.c_str_ptr), external.cap_size.size};
            default:
                return std::string_view{reinterpret_cast<Char*>(external.c_str_ptr), size_from_buffer_header()};
        }
    }

    /**
     * @brief Returns pointer to one-past-end of string data
     * @return Mutable pointer to position after last character
     * @note Efficient one-pass calculation avoiding separate size() calls
     * @note Points to null terminator position for null-terminated strings
     * @note Used for iterator end() and range-based operations
     */
    [[nodiscard, gnu::always_inline]] constexpr auto end_ptr() noexcept -> Char* {
        auto flag = internal.flag;
        switch (flag) {
            case 0:
                return &internal.data[internal.internal_size];
            case 1:
                return reinterpret_cast<Char*>(external.c_str_ptr) + external.cap_size.size;
            default:
                return reinterpret_cast<Char*>(external.c_str_ptr) + size_from_buffer_header();
        }
    }

    /**
     * @brief Efficiently swaps two malloc_core instances
     * @param other The core to swap with
     * @note Uses simple 64-bit value swap for maximum performance
     * @note Swaps all storage state atomically
     */
    auto swap(malloc_core& other) noexcept -> void {
        auto temp_body = other.body;
        other.body = body;
        body = temp_body;
    }

    // Constructors and lifecycle management

    /**
     * @brief Default constructor creating empty string state
     * @param unused Allocator parameter (ignored for malloc_core)
     * @note Initializes to empty string with internal storage
     */
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    constexpr malloc_core([[maybe_unused]] const std::allocator<Char>& unused = std::allocator<Char>{}) noexcept
        : body{0} {}

    /// Copy constructor - copies entire storage state
    constexpr malloc_core(const malloc_core& other) noexcept : body(other.body) {}

    /// Allocator-extended copy constructor (allocator ignored)
    constexpr malloc_core(const malloc_core& other, [[maybe_unused]] const std::allocator<Char>& unused) noexcept
        : body(other.body) {}

    /// Move constructor - transfers ownership and resets source to empty
    constexpr malloc_core(malloc_core&& gone) noexcept : body{std::exchange(gone.body, 0)} {}
    ~malloc_core() = default;
    /// Copy assignment deleted - cores should not be reassigned after construction
    auto operator=(const malloc_core& other) -> malloc_core& = delete;
    /// Move assignment deleted - cores should not be reassigned after construction
    auto operator=(malloc_core&& other) noexcept -> malloc_core& = delete;
};  // struct malloc_core

static_assert(sizeof(malloc_core<char, true>) == 8, "malloc_core should be same as a pointer");

/**
 * @brief PMR-enabled core extending malloc_core with polymorphic allocation
 * @tparam Char Character type (char, wchar_t, etc.)
 * @tparam NullTerminated Whether strings are null-terminated
 * @note Inherits all storage logic from malloc_core but uses PMR allocator
 * @note Allows custom memory resource management for specialized use cases
 */
template <typename Char, bool NullTerminated>
struct pmr_core : public malloc_core<Char, NullTerminated>
{
    /// PMR allocator for custom memory resource management
    std::pmr::polymorphic_allocator<Char> pmr_allocator = {};

    using use_std_allocator = std::false_type;  ///< Type trait: uses PMR instead of std::allocator
    /**
     * @brief Swaps two pmr_core instances with allocator validation
     * @param other The core to swap with
     * @note Requires both cores to use the same memory resource
     * @note Validates allocator compatibility before swapping
     * @throws Assertion failure if allocators are incompatible
     */
    auto swap(pmr_core& other) noexcept -> void {
        Assert(pmr_allocator.resource() == other.pmr_allocator.resource(), "the swap's 2 allocator is not the same");
        malloc_core<Char, NullTerminated>::swap(other);
        // the buffer and the allocator_ptr belongs to same Arena or Object, so the swap both is OK
    }
    // PMR constructors and lifecycle management

    /**
     * @brief Constructor with explicit PMR allocator
     * @param allocator The polymorphic allocator to use
     * @note Required for PMR usage - no default constructor
     */
    constexpr pmr_core(const std::pmr::polymorphic_allocator<Char>& allocator) noexcept
        : malloc_core<Char, NullTerminated>(), pmr_allocator(allocator) {}

    /// Default constructor deleted - PMR requires explicit allocator
    constexpr pmr_core() noexcept = delete;

    /// Copy constructor - copies state and allocator
    constexpr pmr_core(const pmr_core& other) noexcept
        : malloc_core<Char, NullTerminated>(other), pmr_allocator(other.pmr_allocator) {}

    /// Allocator-extended copy constructor - copies state with new allocator
    constexpr pmr_core(const pmr_core& other, const std::pmr::polymorphic_allocator<Char>& allocator) noexcept
        : malloc_core<Char, NullTerminated>(other), pmr_allocator(allocator) {}

    /// Move constructor - transfers state and allocator
    constexpr pmr_core(pmr_core&& gone) noexcept
        : malloc_core<Char, NullTerminated>(std::move(gone)), pmr_allocator(gone.pmr_allocator) {}

    ~pmr_core() = default;
    /// Copy assignment deleted - PMR cores should not be reassigned after construction
    auto operator=(const pmr_core& other) -> pmr_core& = delete;
    /// Move assignment deleted - PMR cores should not be reassigned after construction
    auto operator=(pmr_core&& other) noexcept -> pmr_core& = delete;

};  // struct malloc_core_and_pmr_allocator

/**
 * @brief Buffer management class handling memory allocation for small string optimization
 * @tparam Char Character type (char, wchar_t, etc.)
 * @tparam Core Core storage type (malloc_core or pmr_core)
 * @tparam Traits Character traits (std::char_traits)
 * @tparam Allocator Allocator type for memory management
 * @tparam NullTerminated Whether strings maintain null termination
 * @tparam Growth Growth factor for buffer reallocation (default 1.5)
 * @note Manages all memory operations and storage strategy transitions
 * @note Provides interface between string operations and core storage
 */
template <typename Char, template <typename, bool> typename Core, class Traits, class Allocator, bool NullTerminated,
          float Growth = 1.5F>
class small_string_buffer
{
   protected:
    /// Standard STL container type definitions for compatibility
    using value_type = typename Traits::char_type;  ///< Character type from traits
    using traits_type = Traits;                     ///< Character traits for operations
    using allocator_type = Allocator;               ///< Memory allocator type
    using size_type = std::uint32_t;                ///< Size/index type (32-bit for efficiency)
    using difference_type = typename std::allocator_traits<Allocator>::difference_type;  ///< Iterator difference type

    /// Reference types for element access
    using reference = typename std::allocator_traits<Allocator>::value_type&;              ///< Mutable reference
    using const_reference = const typename std::allocator_traits<Allocator>::value_type&;  ///< Immutable reference
    using pointer = typename std::allocator_traits<Allocator>::pointer;                    ///< Mutable pointer
    using const_pointer = const typename std::allocator_traits<Allocator>::const_pointer;  ///< Immutable pointer

    /// Iterator types for STL compatibility
    using iterator = Char*;                                                ///< Mutable iterator (raw pointer)
    using const_iterator = const Char*;                                    ///< Immutable iterator (raw pointer)
    using reverse_iterator = std::reverse_iterator<iterator>;              ///< Reverse mutable iterator
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;  ///< Reverse immutable iterator

    /// Standard "not found" position marker (maximum size_type value)
    constexpr static size_t npos = std::numeric_limits<size_type>::max();

    using core_type = Core<Char, NullTerminated>;  ///< Storage core (malloc_core or pmr_core)

    /**
     * @brief Enum indicating whether null termination is required
     * @note Used to optimize operations when null termination isn't needed
     */
    enum class Need0 : bool
    {
        Yes = true,  ///< Null termination required
        No = false   ///< Null termination not needed
    };

   private:
    core_type _core;  ///< The actual storage core containing all string data and metadata

    /**
     * @brief Validates that the allocator is properly initialized
     * @return true if allocator is valid, false otherwise
     * @note For standard allocators always returns true, for PMR checks initialization
     */
    [[nodiscard]] auto check_the_allocator() const -> bool {
        if constexpr (core_type::use_std_allocator::value) {
            return true;
        } else {
            return _core.pmr_allocator != std::pmr::polymorphic_allocator<Char>{};
        }
    }

    /**
     * @brief Calculates remaining capacity in median/long buffers after accounting for headers
     * @param buffer_size Total allocated buffer size
     * @param old_size Current string size
     * @return Available idle capacity for growth
     * @note Accounts for buffer headers and null termination if enabled
     */
    [[nodiscard, gnu::always_inline]] constexpr static inline auto calculate_median_long_real_idle_capacity(
      size_type buffer_size, size_type old_size) noexcept -> size_type {
        if constexpr (NullTerminated) {
            Assert(buffer_size >= sizeof(struct capacity_and_size<size_type>) + 1 + old_size,
                   "the buffer_size should be no less than the size of the buffer header and the old size");
            return buffer_size - sizeof(struct capacity_and_size<size_type>) - 1 - old_size;
        } else {
            Assert(buffer_size >= sizeof(struct capacity_and_size<size_type>) + old_size,
                   "the buffer_size should be no less than the size of the buffer header and the old size");
            return buffer_size - sizeof(struct capacity_and_size<size_type>) - old_size;
        }
    }

    /**
     * @brief Allocates a new external buffer and sets up header metadata
     * @param type_and_size Buffer configuration (type and size)
     * @param old_str_size Size of existing string data to preserve
     * @param allocator_ptr Optional PMR allocator pointer
     * @return Configured external core structure with allocated buffer
     * @note Handles Short, Median, and Long buffer types with appropriate headers
     */
    inline static auto allocate_new_external_buffer(
      struct buffer_type_and_size<size_type> type_and_size, size_type old_str_size,
      std::pmr::polymorphic_allocator<Char>* allocator_ptr = nullptr) noexcept -> typename core_type::external_core {
        // make sure the old_str_size <= new_buffer_size
        auto type = type_and_size.core_type;
        auto new_buffer_size = type_and_size.buffer_size;

        switch (type) {
            case CoreType::Internal: {
                Assert(false, "the internal buffer should not be allocated");
                __builtin_unreachable();
            }
            case CoreType::Short: {
                Assert(type_and_size.buffer_size % 8 == 0, "the buffer_size should be aligned to 8");
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = allocator_ptr->allocate(new_buffer_size);
                }
                return {.c_str_ptr = reinterpret_cast<int64_t>(buf),
                        .cap_size = {.cap = static_cast<uint8_t>(type_and_size.buffer_size / 8 - 1),
                                     .size = static_cast<uint16_t>(old_str_size),
                                     .flag = kIsShort}};
            }
            case CoreType::Median: {
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = allocator_ptr->allocate(new_buffer_size);
                }
                auto* head = reinterpret_cast<capacity_and_size<size_type>*>(buf);
                head->capacity = new_buffer_size;
                head->size = old_str_size;
                return {.c_str_ptr = reinterpret_cast<int64_t>(head + 1),
                        .idle = {.idle_or_ignore = static_cast<uint16_t>(
                                   calculate_median_long_real_idle_capacity(new_buffer_size, old_str_size)),
                                 .flag = kIsMedian}};
            }
            case CoreType::Long: {
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = allocator_ptr->allocate(new_buffer_size);
                }
                auto* head = reinterpret_cast<capacity_and_size<size_type>*>(buf);
                head->capacity = new_buffer_size;
                head->size = old_str_size;
                return {.c_str_ptr = reinterpret_cast<int64_t>(head + 1),
                        .idle = {.idle_or_ignore = 0, .flag = kIsLong}};
            }
            default:
                __builtin_unreachable();
        }
    }

   protected:
    /**
     * @brief Determines optimal buffer size and type for a given string size
     * @param size Required string capacity
     * @return Buffer configuration with type (Internal/Short/Median/Long) and aligned size
     * @note Uses size thresholds to select most efficient storage strategy
     */
    [[nodiscard, gnu::always_inline]] constexpr static inline auto calculate_new_buffer_size(size_t size) noexcept
      -> buffer_type_and_size<size_type> {
        if (size <= core_type::internal_buffer_size()) [[unlikely]] {
            return {.buffer_size = core_type::internal_buffer_size(), .core_type = CoreType::Internal};
        }
        if (size <= core_type::max_short_buffer_size()) [[likely]] {
            if constexpr (NullTerminated) {
                return {.buffer_size = static_cast<size_type>(AlignUpTo<8>(size + 1)), .core_type = CoreType::Short};
            } else {
                return {.buffer_size = static_cast<size_type>(AlignUpTo<8>(size)), .core_type = CoreType::Short};
            }
        }
        if (size <= core_type::max_median_buffer_size()) [[likely]] {  // faster than 3-way compare
            return {
              .buffer_size = static_cast<size_type>(AlignUpTo<8>(size + core_type::median_long_buffer_header_size())),
              .core_type = CoreType::Median};
        }
        Assert(size <= core_type::max_long_buffer_size(),
               "the buffer size should be less than the max value of size_type");
        return {.buffer_size = static_cast<size_type>(AlignUpTo<8>(size + core_type::median_long_buffer_header_size())),
                .core_type = CoreType::Long};
    }

    /**
     * @brief Calculates buffer size with growth factor applied
     * @param size Base string size
     * @param growth Growth multiplier (e.g., 1.5 for 50% growth)
     * @return Buffer configuration for grown capacity
     * @note Used when reallocating for capacity expansion
     */
    [[nodiscard, gnu::always_inline]] constexpr static inline auto calculate_new_buffer_size(size_t size,
                                                                                             float growth) noexcept
      -> buffer_type_and_size<size_type> {
        return calculate_new_buffer_size(size * growth);
    }

    /**
     * @brief Returns the current storage type of the string buffer
     * @return Core type (Internal/Short/Median/Long) as uint8_t
     * @note Used for debugging and optimization decisions
     */
    [[nodiscard]] auto get_core_type() const -> uint8_t { return _core.get_core_type(); }

    /**
     * @brief Fast initial allocation with predetermined buffer configuration
     * @param cap_and_type Pre-calculated buffer type and capacity
     * @param size String size to initialize
     * @note Optimized path - caller must ensure cap_and_type is valid for size
     * @note Handles Internal, Short, Median, and Long buffer allocation strategies
     */
    constexpr void initial_allocate(buffer_type_and_size<size_type> cap_and_type, size_type size) noexcept {
        auto type = cap_and_type.core_type;
        auto new_buffer_size = cap_and_type.buffer_size;
        switch (type) {
            case CoreType::Internal:
                _core.internal.flag = kIsInternal;
                _core.internal.internal_size = static_cast<uint8_t>(size);
                if constexpr (NullTerminated) {
                    _core.internal.data[size] = '\0';
                }
                break;
            case CoreType::Short: {
                Assert(new_buffer_size % 8 == 0, "the buffer_size should be aligned to 8");
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = _core.pmr_allocator.allocate(new_buffer_size);
                }
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(buf)[size] = '\0';
                }
                _core.external = {.c_str_ptr = reinterpret_cast<int64_t>(buf),
                                  .cap_size = {.cap = static_cast<uint8_t>(new_buffer_size / 8 - 1),
                                               .size = static_cast<uint16_t>(size),
                                               .flag = kIsShort}};
                break;
            }
            case CoreType::Median: {
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = _core.pmr_allocator.allocate(new_buffer_size);
                }
                auto* head = reinterpret_cast<capacity_and_size<size_type>*>(buf);
                head->capacity = new_buffer_size;
                head->size = size;
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(buf)[size + sizeof(struct capacity_and_size<size_type>)] = '\0';
                }
                _core.external = {.c_str_ptr = reinterpret_cast<int64_t>(head + 1),
                                  .idle = {.idle_or_ignore = static_cast<uint16_t>(
                                             calculate_median_long_real_idle_capacity(new_buffer_size, size)),
                                           .flag = kIsMedian}};
                break;
            }
            case CoreType::Long: {
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = _core.pmr_allocator.allocate(new_buffer_size);
                }
                auto* head = reinterpret_cast<capacity_and_size<size_type>*>(buf);
                head->capacity = new_buffer_size;
                head->size = size;
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(buf)[size + sizeof(struct capacity_and_size<size_type>)] = '\0';
                }
                _core.external = {.c_str_ptr = reinterpret_cast<int64_t>(head + 1),
                                  .idle = {.idle_or_ignore = 0, .flag = kIsLong}};
                break;
            }
        }
        return;
    }

    /**
     * @brief Convenient initial allocation that calculates optimal buffer configuration
     * @param new_string_size Size of string to allocate space for
     * @note Automatically determines best buffer type and size for given capacity
     */
    [[gnu::always_inline]] constexpr inline void initial_allocate(size_t new_string_size) noexcept {
        // Assert(new_string_size <= std::numeric_limits<size_type>::max(),
        Assert(new_string_size < core_type::max_long_buffer_size(),
               "the new_string_size should be less than the max value of size_type");
        auto cap_and_type = calculate_new_buffer_size(new_string_size);
        initial_allocate(cap_and_type, static_cast<size_type>(new_string_size));
    }

    /**
     * @brief Expands buffer capacity when current capacity is insufficient
     * @tparam Term Whether to add null termination
     * @param new_append_size Additional capacity needed
     * @note Reallocates to external buffer if needed, preserving existing data
     * @note Uses growth factor to reduce future reallocations
     */
    template <Need0 Term = Need0::Yes>
    void allocate_more(size_type new_append_size) noexcept {
        size_type old_delta = _core.idle_capacity();

        // if no need, do nothing, just update the size or delta
        if (old_delta >= new_append_size) [[likely]] {
            // just return and do nothing
            return;
        }
        auto old_size = size();
        // if need allocate a new buffer, always a external_buffer
        // do the allocation
        typename core_type::external_core new_external;
        if constexpr (core_type::use_std_allocator::value) {
            new_external =
              allocate_new_external_buffer(calculate_new_buffer_size(old_size + new_append_size, Growth), old_size);

        } else {
            new_external = allocate_new_external_buffer(calculate_new_buffer_size(old_size + new_append_size, Growth),
                                                        old_size, &_core.pmr_allocator);
        }

        // copy the old data to the new buffer
        std::memcpy(reinterpret_cast<Char*>(new_external.c_str_ptr), get_buffer(), old_size);
        // set the '\0' at the end of the buffer if needed;
        if constexpr (NullTerminated and Term == Need0::Yes) {
            reinterpret_cast<Char*>(new_external.c_str_ptr)[old_size] = '\0';
        }
        // deallocate the old buffer
        if (_core.is_external()) [[likely]] {
            if constexpr (core_type::use_std_allocator::value) {
                std::free(_core.external.get_buffer_ptr());
            } else {
                // the size is not used, so just set it to 0
                _core.pmr_allocator.deallocate(_core.external.get_buffer_ptr(), 0);
            }
        }
        // replace the old external with the new one
        _core.external = new_external;
    }

   public:
    /**
     * @brief Reserves buffer capacity, optionally preserving existing data
     * @tparam Term Whether to maintain null termination
     * @tparam NeedCopy Whether to copy existing data to new buffer
     * @param new_cap Minimum capacity to reserve
     * @note If NeedCopy=false, existing data may be lost during reallocation
     */
    template <Need0 Term, bool NeedCopy>
    constexpr auto buffer_reserve(size_type new_cap) -> void {
#ifndef NDEBUG
        auto origin_core_type = _core.get_core_type();
#endif
        // check the new_cap is larger than the internal capacity, and larger than current cap
        auto [old_cap, old_size] = get_capacity_and_size();
        if (new_cap > old_cap) [[likely]] {
            // no growth was needed
            typename core_type::external_core new_external;

            auto new_buffer_type_and_size = calculate_new_buffer_size(new_cap);
            // allocate a new buffer
            if constexpr (core_type::use_std_allocator::value) {
                new_external = allocate_new_external_buffer(new_buffer_type_and_size, old_size);
            } else {
                new_external = allocate_new_external_buffer(new_buffer_type_and_size, old_size, &_core.pmr_allocator);
            }
            if constexpr (NeedCopy) {
                // copy the old data to the new buffer
                std::memcpy(reinterpret_cast<Char*>(new_external.c_str_ptr), get_buffer(), old_size);
            }
            if constexpr (NullTerminated and Term == Need0::Yes and NeedCopy) {
                reinterpret_cast<Char*>(new_external.c_str_ptr)[old_size] = '\0';
            }
            // deallocate the old buffer
            if (_core.is_external()) [[likely]] {
                if constexpr (core_type::use_std_allocator::value) {
                    std::free(_core.external.get_buffer_ptr());
                } else {
                    _core.pmr_allocator.deallocate(_core.external.get_buffer_ptr(), 0);
                }
            }
            // replace the old external with the new one
            _core.external = new_external;
        }
#ifndef NDEBUG
        auto new_core_type = _core.get_core_type();
        Assert(origin_core_type <= new_core_type, "the core type should stay or grow");
#endif
    }

    [[nodiscard, gnu::always_inline]] auto idle_capacity() const noexcept -> size_type { return _core.idle_capacity(); }

    [[nodiscard]] auto get_capacity_and_size() const noexcept -> capacity_and_size<size_type> {
        return _core.get_capacity_and_size();
    }

    // increase the size, and won't change the capacity, so the internal/exteral'type or ptr will not change
    // you should always call the allocate_new_external_buffer first, then call this function
    [[gnu::always_inline]] inline void increase_size(size_type delta) noexcept {
#ifndef NDEBUG
        auto origin_core_type = _core.get_core_type();
#endif
        _core.increase_size_and_idle_and_set_term(delta);
#ifndef NDEBUG
        auto new_core_type = _core.get_core_type();
        Assert(origin_core_type == new_core_type, "the core type should not change");
#endif
    }

    [[gnu::always_inline]] inline void set_size(size_type new_size) noexcept {
#ifndef NDEBUG
        auto origin_core_type = _core.get_core_type();
#endif
        _core.set_size_and_idle_and_set_term(new_size);
#ifndef NDEBUG
        auto new_core_type = _core.get_core_type();
        Assert(origin_core_type == new_core_type, "the core type should not change");
#endif
    }

    [[gnu::always_inline]] inline void decrease_size(size_type delta) noexcept {
#ifndef NDEBUG
        auto origin_core_type = _core.get_core_type();
#endif
        _core.decrease_size_and_idle_and_set_term(delta);
#ifndef NDEBUG
        auto new_core_type = _core.get_core_type();
        Assert(origin_core_type == new_core_type, "the core type should not change");
#endif
    }

   public:
    constexpr small_string_buffer([[maybe_unused]] const Allocator& allocator) noexcept : _core{allocator} {
#ifdef SMALL_STD_PMR_CHECK
        if constexpr (not core_type::use_std_allocator::value) {
            Assert(check_the_allocator(), "the pmr default allocator is not allowed to be used in small_string");
        }
#endif
    }

    constexpr small_string_buffer(const small_string_buffer& other) = delete;
    constexpr small_string_buffer(const small_string_buffer& other,
                                  [[maybe_unused]] const Allocator& allocator) = delete;

    constexpr small_string_buffer(small_string_buffer&& other) noexcept = delete;

    // move constructor
    constexpr small_string_buffer(small_string_buffer&& other, [[maybe_unused]] const Allocator& /*unused*/) noexcept
        : _core(std::move(other._core)) {
#ifdef SMALL_STD_PMR_CHECK
        Assert(
          check_the_allocator(),
          "the pmr default allocator is not allowed to be used in small_string");  // very important, check the
                                                                                   // allocator incorrect injected into.
#endif
    }

    ~small_string_buffer() noexcept {
        if constexpr (core_type::use_std_allocator::value) {
            if (_core.is_external()) [[likely]] {
                std::free(reinterpret_cast<void*>(_core.external.get_buffer_ptr()));
            }
        } else {
            if (_core.is_external()) [[likely]] {
                // the size is not used, so just set it to 0
                _core.pmr_allocator.deallocate(_core.external.get_buffer_ptr(), 0);
            }
        }
    }

    /**
     * @brief Swaps buffer contents with another buffer
     * @param other Buffer to swap with
     * @note Delegates to core swap for efficient exchange
     * @note Maintains all invariants during swap operation
     */
    constexpr auto swap(small_string_buffer& other) noexcept -> void { _core.swap(other._core); }

    constexpr auto operator=(const small_string_buffer& other) noexcept = delete;
    constexpr auto operator=(small_string_buffer&& other) noexcept = delete;

    [[nodiscard]] constexpr auto get_buffer() noexcept -> Char* { return _core.begin_ptr(); }

    [[nodiscard]] constexpr auto get_buffer() const noexcept -> const Char* {
        return const_cast<small_string_buffer*>(this)->_core.begin_ptr();
    }

    // will be fast than call get_buffer() + size(), it will waste many times for if checking
    [[nodiscard]] constexpr auto end() noexcept -> Char* { return _core.end_ptr(); }

    [[nodiscard]] constexpr auto size() const noexcept -> size_type { return _core.size(); }

    [[nodiscard]] constexpr auto capacity() const noexcept -> size_type { return _core.capacity(); }

    /**
     * @brief Returns the allocator used by the buffer
     * @return Copy of the allocator instance
     * @note For std::allocator: returns default-constructed instance
     * @note For PMR: returns the polymorphic allocator from core
     */
    [[nodiscard]] constexpr auto get_allocator() const -> Allocator {
        if constexpr (core_type::use_std_allocator::value) {
            return Allocator();
        } else {
            return _core.pmr_allocator;
        }
    }

    /**
     * @brief Assigns a character to fill the string with a compile-time specified size
     * @tparam Size The number of characters to assign (must be > 0 and <= internal buffer size)
     * @param ch Character to fill the string with
     * @note Uses memset for efficient bulk character assignment
     * @note Size is validated at compile-time via static_assert
     */
    template <size_type Size>
    constexpr void static_assign(Char ch) {
        static_assert(Size > 0 and Size <= core_type::internal_buffer_size(), "the size should be greater than 0");
        // TODO(leo): combine the memset and set_size in same switch case
        memset(get_buffer(), ch, Size);
        set_size(Size);
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto get_string_view() const noexcept -> std::string_view {
        return _core.get_string_view();
    }

};  // class small_string_buffer

// if NullTerminated is true, the string will be null terminated, and the size will be the length of the string
// if NullTerminated is false, the string will not be null terminated, and the size will still be the length of the
/**
 * @brief High-performance string class with small string optimization
 * @tparam Char Character type (char, wchar_t, etc.)
 * @tparam Buffer Buffer management policy (default: small_string_buffer)
 * @tparam Core Storage core type (default: malloc_core, alternative: pmr_core)
 * @tparam Traits Character traits for string operations (default: std::char_traits<Char>)
 * @tparam Allocator Memory allocator type (default: std::allocator<Char>)
 * @tparam NullTerminated Whether strings maintain null termination (default: true)
 * @tparam Growth Growth factor for buffer reallocation (default: 1.5)
 *
 * @note Uses small string optimization with 4 storage strategies:
 *       - Internal: up to 6-7 chars embedded directly in object
 *       - Short: up to 255 chars with compact metadata
 *       - Median: up to 16383 chars with idle capacity tracking
 *       - Long: unlimited size with full external allocation
 * @note Maintains 16-byte object size regardless of string length
 * @note Thread-safe for read operations, requires external synchronization for writes
 */
template <typename Char,
          template <typename, template <typename, bool> class, class, class, bool, float> class Buffer =
            small_string_buffer,
          template <typename, bool> class Core = malloc_core, class Traits = std::char_traits<Char>,
          class Allocator = std::allocator<Char>, bool NullTerminated = true, float Growth = 1.5F>
class basic_small_string : private Buffer<Char, Core, Traits, Allocator, NullTerminated, Growth>
{
   public:
    /// STL-compatible type definitions for template specialization and trait access
    using buffer_type =
      Buffer<Char, Core, Traits, Allocator, NullTerminated, Growth>;  ///< Underlying buffer management type
    using value_type = typename Traits::char_type;                    ///< Character type (same as Char)
    using traits_type = Traits;                                       ///< Character traits class
    using allocator_type = Allocator;                                 ///< Memory allocator type
    using size_type = std::uint32_t;                                  ///< Size/index type (32-bit for space efficiency)
    using difference_type = typename std::allocator_traits<Allocator>::difference_type;  ///< Signed difference type

    /// Reference and pointer types for STL compatibility
    using reference = typename std::allocator_traits<Allocator>::value_type&;  ///< Mutable element reference
    using const_reference =
      const typename std::allocator_traits<Allocator>::value_type&;      ///< Immutable element reference
    using pointer = typename std::allocator_traits<Allocator>::pointer;  ///< Mutable element pointer
    using const_pointer =
      const typename std::allocator_traits<Allocator>::const_pointer;  ///< Immutable element pointer

    /// Iterator types for STL algorithms and range-based for loops
    using iterator = Char*;                                                ///< Mutable random access iterator
    using const_iterator = const Char*;                                    ///< Immutable random access iterator
    using reverse_iterator = std::reverse_iterator<iterator>;              ///< Mutable reverse iterator
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;  ///< Immutable reverse iterator

    /// Standard "not found" sentinel value (maximum representable index)
    constexpr static size_type npos = std::numeric_limits<size_type>::max();

    /**
     * @brief Tag type for constructors that defer initialization
     * @note Used to create string objects that will be initialized later
     * @note Useful for performance-critical code that needs custom initialization
     */
    struct initialized_later
    {};

   public:
    /**
     * @brief Creates an uninitialized string with reserved capacity
     * @param new_string_size Size to reserve for the string
     * @param allocator Allocator instance to use for memory management
     * @note String data is uninitialized and must be filled manually
     */
    constexpr basic_small_string(initialized_later /*unused*/, size_t new_string_size,
                                 const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        Assert((static_cast<size_type>(new_string_size) +
                Core<Char, NullTerminated>::median_long_buffer_header_size()) <= std::numeric_limits<size_type>::max(),
               "the new_string_size should be less than the max value of size_type");
        buffer_type::initial_allocate(static_cast<size_type>(new_string_size));
    }

    /**
     * @brief Creates an uninitialized string with specific buffer type and size
     * @param type_and_size Buffer type and size configuration
     * @param new_string_size Size to allocate for the string
     * @param allocator Allocator instance to use for memory management
     * @note Advanced constructor for explicit buffer type control
     */
    constexpr basic_small_string(initialized_later /*unused*/, buffer_type_and_size<size_type> type_and_size,
                                 size_t new_string_size, const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        buffer_type::initial_allocate(type_and_size, new_string_size);
    }

    /**
     * @brief Static factory method to create an uninitialized string
     * @param new_string_size Size to reserve for the string
     * @param allocator Allocator instance to use for memory management
     * @return Uninitialized string with reserved capacity
     * @note Preferred over direct constructor for clarity
     */
    static constexpr auto create_uninitialized_string(size_t new_string_size, const Allocator& allocator = Allocator())
      -> basic_small_string {
        return basic_small_string(initialized_later{}, new_string_size, allocator);
    }

    /**
     * @brief Default constructor creates an empty string
     * @param allocator Allocator instance to use for memory management
     * @note Creates empty string with no allocation
     */
    constexpr explicit basic_small_string([[maybe_unused]] const Allocator& allocator = Allocator()) noexcept
        : buffer_type(allocator) {}

    /**
     * @brief Fill constructor creates string with repeated character
     * @param count Number of characters to create
     * @param ch Character to repeat
     * @param allocator Allocator instance to use for memory management
     * @note Creates string like "aaaaa" if ch='a' and count=5
     */
    constexpr basic_small_string(size_t count, Char ch, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, count, allocator) {
        memset(data(), ch, count);
    }

    /**
     * @brief Copy constructor creates deep copy of another string
     * @param other String to copy from
     * @note Allocates new memory and copies all data
     */
    constexpr basic_small_string(const basic_small_string& other)
        : basic_small_string(initialized_later{}, other.size(), other.get_allocator()) {
        std::memcpy(data(), other.data(), other.size());
    }

    /**
     * @brief Copy constructor with different allocator
     * @param other String to copy from
     * @param allocator Different allocator to use for the copy
     * @note Creates copy using specified allocator instead of other's allocator
     */
    constexpr basic_small_string(const basic_small_string& other, [[maybe_unused]] const Allocator& allocator)
        : basic_small_string(initialized_later{}, other.size(), other.get_allocator()) {
        std::memcpy(data(), other.data(), other.size());
    }

    /**
     * @brief Copy constructor from substring starting at position
     * @param other String to copy from
     * @param pos Starting position in other string
     * @param allocator Allocator instance to use
     * @note Creates copy of other[pos:] (from pos to end)
     */
    constexpr basic_small_string(const basic_small_string& other, size_t pos,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(other.substr(pos)) {}

    /**
     * @brief Move constructor transfers ownership from another string
     * @param other String to move from (will be left in valid but unspecified state)
     * @param allocator Allocator instance to use
     * @note Efficient transfer of resources without copying data
     */
    constexpr basic_small_string(basic_small_string&& other,
                                 [[maybe_unused]] const Allocator& allocator = Allocator()) noexcept
        : buffer_type(std::move(other), allocator) {}

    /**
     * @brief Move constructor from substring starting at position
     * @param other String to move from
     * @param pos Starting position in other string
     * @param allocator Allocator instance to use
     * @note Creates new string from other[pos:] using move semantics
     */
    constexpr basic_small_string(basic_small_string&& other, size_t pos,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string{other.substr(pos)} {}

    /**
     * @brief Copy constructor from substring with position and count
     * @param other String to copy from
     * @param pos Starting position in other string
     * @param count Number of characters to copy
     * @param allocator Allocator instance to use
     * @note Creates copy of other[pos:pos+count]
     */
    constexpr basic_small_string(const basic_small_string& other, size_t pos, size_t count,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string{other.substr(pos, count)} {}

    /**
     * @brief Move constructor from substring with position and count
     * @param other String to move from
     * @param pos Starting position in other string
     * @param count Number of characters to copy
     * @param allocator Allocator instance to use
     * @note Creates new string from other[pos:pos+count] using move semantics
     */
    constexpr basic_small_string(basic_small_string&& other, size_t pos, size_t count,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string{other.substr(pos, count), allocator} {}

    /**
     * @brief Constructor from C-string with explicit length
     * @param s Pointer to character array (may contain null characters)
     * @param count Number of characters to copy from s
     * @param allocator Allocator instance to use
     * @note Does not require null termination, copies exactly count characters
     */
    constexpr basic_small_string(const Char* s, size_t count,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, count, allocator) {
        std::memcpy(data(), s, count);
    }

    /**
     * @brief Constructor from null-terminated C-string
     * @param s Null-terminated character array
     * @param allocator Allocator instance to use
     * @note Automatically calculates length using Traits::length
     */
    constexpr basic_small_string(const Char* s, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(s, Traits::length(s), allocator) {}

    /**
     * @brief Constructor from iterator range
     * @tparam InputIt Input iterator type
     * @param first Iterator to beginning of range
     * @param last Iterator to end of range
     * @param allocator Allocator instance to use
     * @note Creates string by copying elements from [first, last)
     */
    template <class InputIt>
    constexpr basic_small_string(InputIt first, InputIt last, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, static_cast<size_type>(std::distance(first, last)), allocator) {
        std::copy(first, last, begin());
    }

    /**
     * @brief Constructor from initializer list
     * @param ilist Initializer list of characters
     * @param allocator Allocator instance to use
     * @note Enables syntax like small_string{'h', 'e', 'l', 'l', 'o'}
     */
    constexpr basic_small_string(std::initializer_list<Char> ilist,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, ilist.size(), allocator) {
        std::copy(ilist.begin(), ilist.end(), begin());
    }

    /**
     * @brief Constructor from string_view-like objects
     * @tparam StringViewLike Type convertible to string_view but not to Char*
     * @param s String view-like object to copy from
     * @param allocator Allocator instance to use
     * @note Accepts std::string_view and similar types
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr basic_small_string(const StringViewLike& s, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, s.size(), allocator) {
        std::copy(s.begin(), s.end(), begin());
    }

    /**
     * @brief Constructor from string-like object with position and length
     * @tparam StringViewLike Type convertible to Char* but not to string_view
     * @param s String-like object to copy from
     * @param pos Starting position in s
     * @param n Number of characters to copy
     * @param allocator Allocator instance to use
     * @note Specialized for types that behave like character arrays
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, const Char*> and
                 not std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>>)
    constexpr basic_small_string(const StringViewLike& s, size_type pos, size_type n,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, n, allocator) {
        std::copy(s.begin() + pos, s.begin() + pos + n, begin());
    }

    /**
     * @brief Deleted constructor for nullptr - prevents accidental null initialization
     * @param unused nullptr parameter (explicitly unused)
     * @throws std::logic_error Always throws to prevent nullptr initialization
     * @note This constructor exists to provide clear error message for nullptr usage
     */
    basic_small_string(std::nullptr_t /*unused*/) : buffer_type(Allocator{}) {
        throw std::logic_error("basic_small_string cannot be initialized with nullptr");
    }

    /**
     * @brief Destructor automatically manages memory cleanup
     * @note Default destructor is sufficient due to RAII design
     */
    ~basic_small_string() noexcept = default;

    /**
     * @brief Returns the allocator used by the string
     * @return Copy of the allocator instance
     * @note Delegates to buffer_type::get_allocator()
     */
    [[nodiscard]] constexpr auto get_allocator() const -> Allocator { return buffer_type::get_allocator(); }

    /**
     * @brief Returns the current storage type of the string
     * @return Core type as size_type (0=Internal, 1=Short, 2=Median, 3=Long)
     * @note Used for debugging and performance analysis
     */
    [[nodiscard]] constexpr auto get_core_type() const -> size_type { return buffer_type::get_core_type(); }

    /**
     * @brief Copy assignment operator
     * @param other String to copy from
     * @return Reference to this string after assignment
     * @note Self-assignment safe, delegates to assign() for efficiency
     */
    auto operator=(const basic_small_string& other) -> basic_small_string& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        // assign the other to this
        return assign(other.data(), other.size());
    }

    /**
     * @brief Move assignment operator
     * @param other String to move from (will be left in valid but unspecified state)
     * @return Reference to this string after move
     * @note Self-assignment safe, efficient transfer of resources
     * @note Strong exception safety guarantee
     */
    auto operator=(basic_small_string&& other) noexcept -> basic_small_string& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        this->~basic_small_string();
        // call the move constructor
        new (this) basic_small_string(std::move(other));
        return *this;
    }

    /**
     * @brief Assignment from C-string
     * @param s Null-terminated string to assign from
     * @return Reference to this string after assignment
     * @note Length calculated automatically using Traits::length()
     */
    auto operator=(const Char* s) -> basic_small_string& { return assign(s, Traits::length(s)); }

    /**
     * @brief Assignment from single character
     * @param ch Character to assign (string becomes single-character string)
     * @return Reference to this string after assignment
     * @note Replaces entire string content with single character
     */
    auto operator=(Char ch) -> basic_small_string& {
        this->template static_assign<1>(ch);
        return *this;
    }

    /**
     * @brief Assignment from initializer list
     * @param ilist Initializer list of characters to assign
     * @return Reference to this string after assignment
     * @note Replaces entire string content with initializer list contents
     */
    auto operator=(std::initializer_list<Char> ilist) -> basic_small_string& {
        return assign(ilist.begin(), ilist.size());
    }

    /**
     * @brief Assignment from string view-like object
     * @tparam StringViewLike Type convertible to string_view but not to char*
     * @param s String view-like object to assign from
     * @return Reference to this string after assignment
     * @note Uses .data() and .size() methods from string view interface
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    auto operator=(const StringViewLike& s) -> basic_small_string& {
        return assign(s.data(), s.size());
    }

    /// Nullptr assignment deleted - use clear() or assign() instead
    auto operator=(std::nullptr_t) -> basic_small_string& = delete;

    /**
     * @brief Assigns content by filling with character repetition
     * @tparam Safe Whether to perform safe buffer reservation (default: true)
     * @param count Number of characters to assign
     * @param ch Character to fill with
     * @return Reference to this string after assignment
     * @note Replaces entire string content with count copies of ch
     */
    template <bool Safe = true>
    auto assign(size_type count, Char ch) -> basic_small_string& {
        if constexpr (Safe) {
            this->template buffer_reserve<buffer_type::Need0::No, false>(count);
        }
        auto* buffer = buffer_type::get_buffer();
        memset(buffer, ch, count);
        // do not use resize, to avoid the if checking
        buffer_type::set_size(count);

        return *this;
    }

    /**
     * @brief Assigns content from another string
     * @tparam Safe Whether to perform safe buffer operations (default: true)
     * @param str String to copy from
     * @return Reference to this string after assignment
     * @note Self-assignment safe, delegates to character array assignment
     */
    template <bool Safe = true>
    [[gnu::always_inline]] auto assign(const basic_small_string& str) -> basic_small_string& {
        if (this == &str) [[unlikely]] {
            return *this;
        }
        return assign<Safe>(str.data(), str.size());
    }

    /**
     * @brief Assigns substring from another string
     * @tparam Safe Whether to perform safe buffer operations (default: true)
     * @param str String to copy from
     * @param pos Starting position in source string
     * @param count Number of characters to copy (default: npos for all remaining)
     * @return Reference to this string after assignment
     * @throws std::out_of_range if pos > str.size()
     * @note Self-assignment safe with special handling for overlap
     */
    template <bool Safe = true>
    auto assign(const basic_small_string& str, size_type pos, size_type count = npos) -> basic_small_string& {
        auto other_size = str.size();
        if (pos > other_size) [[unlikely]] {
            throw std::out_of_range("assign: input pos is out of range");
        }
        if (this == &str) [[unlikely]] {
            // erase the other part
            count = std::min<size_type>(count, other_size - pos);
            auto* buffer = buffer_type::get_buffer();
            std::memmove(buffer, buffer + pos, count);
            buffer_type::set_size(count);
            return *this;
        }
        return assign<Safe>(str.data() + pos, std::min<size_type>(count, other_size - pos));
    }

    /**
     * @brief Move assigns from another string
     * @param gone String to move from (will be left in valid but unspecified state)
     * @return Reference to this string after assignment
     * @note Self-assignment safe, efficiently transfers resources using move constructor
     */
    auto assign(basic_small_string&& gone) noexcept -> basic_small_string& {
        if (this == &gone) [[unlikely]] {
            return *this;
        }
        this->~basic_small_string();
        // call the move constructor
        new (this) basic_small_string(std::move(gone));
        return *this;
    }

    /**
     * @brief Assigns content from character array
     * @tparam Safe Whether to perform safe buffer reservation (default: true)
     * @param s Pointer to character array (must not be nullptr)
     * @param count Number of characters to copy
     * @return Reference to this string after assignment
     * @note Uses memmove to handle potential overlap safely
     */
    template <bool Safe = true>
    auto assign(const Char* s, size_type count) -> basic_small_string& {
        Assert(s != nullptr, "assign: s should not be nullptr");
        if constexpr (Safe) {
            this->template buffer_reserve<buffer_type::Need0::No, false>(count);
        }
        auto* buffer = buffer_type::get_buffer();
        // use memmove to handle the overlap
        std::memmove(buffer, s, count);
        buffer_type::set_size(count);
        return *this;
    }

    /**
     * @brief Assigns content from character array with size_t count
     * @tparam Safe Whether to perform safe buffer operations (default: true)
     * @param s Pointer to character array
     * @param count Number of characters to copy (must fit in size_type)
     * @return Reference to this string after assignment
     * @note Converts size_t to size_type with bounds checking
     */
    template <bool Safe = true>
    auto assign(const Char* s, size_t count) -> basic_small_string& {
        Assert((count <= Core<Char, NullTerminated>::max_long_buffer_size()),
               "assign: count should be less than the max value of size_type");
        return assign(s, static_cast<size_type>(count));
    }

    /**
     * @brief Assigns content from null-terminated string
     * @tparam Safe Whether to perform safe buffer operations (default: true)
     * @param s Null-terminated string to copy from
     * @return Reference to this string after assignment
     * @note Length determined automatically using Traits::length()
     */
    template <bool Safe = true>
    auto assign(const Char* s) -> basic_small_string& {
        return assign<Safe>(s, Traits::length(s));
    }

    /**
     * @brief Assigns content from iterator range
     * @tparam InputIt Input iterator type
     * @tparam Safe Whether to perform safe buffer operations (default: true)
     * @param first Iterator to beginning of range
     * @param last Iterator to end of range
     * @return Reference to this string after assignment
     * @note Distance calculated using std::distance, content copied using std::copy
     */
    template <class InputIt, bool Safe = true>
    auto assign(InputIt first, InputIt last) -> basic_small_string& {
        auto count = std::distance(first, last);
        if constexpr (Safe) {
            this->template buffer_reserve<buffer_type::Need0::No, false>(count);
        }
        std::copy(first, last, begin());
        buffer_type::set_size(count);
        return *this;
    }

    /**
     * @brief Assigns content from initializer list
     * @tparam Safe Whether to perform safe buffer operations (default: true)
     * @param ilist Initializer list of characters
     * @return Reference to this string after assignment
     * @note Delegates to iterator range assignment
     */
    template <bool Safe = true>
    auto assign(std::initializer_list<Char> ilist) -> basic_small_string& {
        return assign<decltype(ilist.begin()), Safe>(ilist.begin(), ilist.end());
    }

    /**
     * @brief Assigns content from string view-like object
     * @tparam StringViewLike Type convertible to string_view but not to char*
     * @tparam Safe Whether to perform safe buffer operations (default: true)
     * @param s String view-like object to copy from
     * @return Reference to this string after assignment
     * @note Uses .data() and .size() methods from string view interface
     */
    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    auto assign(const StringViewLike& s) -> basic_small_string& {
        return assign<Safe>(s.data(), s.size());
    }

    /**
     * @brief Assigns substring from string-like object convertible to char*
     * @tparam StringViewLike Type convertible to char* but not to string_view
     * @tparam Safe Whether to perform safe buffer operations (default: true)
     * @param s String-like object to copy from
     * @param pos Starting position in source
     * @param n Number of characters to copy (default: npos for all remaining)
     * @return Reference to this string after assignment
     * @note Handles types that provide char* conversion but not string_view interface
     */
    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, const Char*> and
                 not std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>>)
    auto assign(const StringViewLike& s, size_type pos, size_type n = npos) -> basic_small_string& {
        return assign<Safe>(s + pos, std::min<size_type>(n, s.size() - pos));
    }

    /**
     * @brief Accesses character at position with bounds checking
     * @tparam Safe Whether to perform bounds checking (default: true)
     * @param pos Position of character to access
     * @return Reference to character at position
     * @throws std::out_of_range if pos >= size() and Safe=true
     * @note Safe=false provides unchecked access for performance
     */
    template <bool Safe = true>
    [[nodiscard]] constexpr auto at(size_type pos) noexcept(not Safe) -> reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("at: pos is out of range");
            }
        }
        return *(buffer_type::get_buffer() + pos);
    }

    /**
     * @brief Accesses character at position with bounds checking (const version)
     * @tparam Safe Whether to perform bounds checking (default: true)
     * @param pos Position of character to access
     * @return Const reference to character at position
     * @throws std::out_of_range if pos >= size() and Safe=true
     * @note Safe=false provides unchecked access for performance
     */
    template <bool Safe = true>
    [[nodiscard]] constexpr auto at(size_type pos) const noexcept(not Safe) -> const_reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("at: pos is out of range");
            }
        }
        return *(data() + pos);
    }

    /**
     * @brief Array-style character access operator
     * @tparam Safe Whether to perform bounds checking (default: true)
     * @param pos Position of character to access
     * @return Reference to character at position
     * @throws std::out_of_range if pos >= size() and Safe=true
     * @note Provides familiar array syntax: string[index]
     */
    template <bool Safe = true>
    constexpr auto operator[](size_type pos) noexcept(not Safe) -> reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("operator []: pos is out of range");
            }
        }
        return *(buffer_type::get_buffer() + pos);
    }

    /**
     * @brief Array-style character access operator (const version)
     * @tparam Safe Whether to perform bounds checking (default: true)
     * @param pos Position of character to access
     * @return Const reference to character at position
     * @throws std::out_of_range if pos >= size() and Safe=true
     * @note Provides familiar array syntax: string[index]
     */
    template <bool Safe = true>
    constexpr auto operator[](size_type pos) const noexcept(not Safe) -> const_reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("operator []: pos is out of range");
            }
        }
        return *(data() + pos);
    }

    /**
     * @brief Accesses the first character in the string
     * @return Reference to first character
     * @note Undefined behavior if string is empty
     */
    [[nodiscard]] constexpr auto front() noexcept -> reference { return *reinterpret_cast<Char*>(data()); }

    /**
     * @brief Accesses the first character in the string (const version)
     * @return Const reference to first character
     * @note Undefined behavior if string is empty
     */
    [[nodiscard]] constexpr auto front() const noexcept -> const_reference { return *data(); }

    /**
     * @brief Accesses the last character in the string
     * @return Reference to last character
     * @note Undefined behavior if string is empty
     */
    [[nodiscard]] constexpr auto back() noexcept -> reference {
        auto size = buffer_type::size();
        auto buffer = buffer_type::get_buffer();
        return buffer[size - 1];
    }

    /**
     * @brief Accesses the last character in the string (const version)
     * @return Const reference to last character
     * @note Undefined behavior if string is empty
     */
    [[nodiscard]] constexpr auto back() const noexcept -> const_reference {
        auto size = buffer_type::size();
        auto buffer = buffer_type::get_buffer();
        return buffer[size - 1];
    }

    /**
     * @brief Returns pointer to null-terminated C-string
     * @return Const pointer to null-terminated character array
     * @note Only available when NullTerminated template parameter is true
     * @note Guaranteed to be null-terminated for C interoperability
     */
    template <bool U = NullTerminated, typename = std::enable_if_t<U, std::true_type>>
    [[nodiscard, gnu::always_inline]] auto c_str() const noexcept -> const Char* {
        return buffer_type::get_buffer();
    }

    /**
     * @brief Returns pointer to character data (const version)
     * @return Const pointer to character array
     * @note May or may not be null-terminated depending on NullTerminated template parameter
     */
    [[nodiscard, gnu::always_inline]] auto data() const noexcept -> const Char* { return buffer_type::get_buffer(); }

    /**
     * @brief Returns pointer to character data (mutable version)
     * @return Mutable pointer to character array
     * @note May or may not be null-terminated depending on NullTerminated template parameter
     */
    [[nodiscard, gnu::always_inline]] auto data() noexcept -> Char* { return buffer_type::get_buffer(); }

    /**
     * @brief Returns iterator to beginning of string
     * @return Mutable iterator pointing to first character
     * @note Enables range-based for loops and STL algorithms
     */
    [[nodiscard]] constexpr auto begin() noexcept -> iterator { return data(); }

    /**
     * @brief Returns const iterator to beginning of string
     * @return Const iterator pointing to first character
     * @note Read-only access for const strings
     */
    [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator { return data(); }

    /**
     * @brief Returns const iterator to beginning of string
     * @return Const iterator pointing to first character
     * @note Explicitly const version, same as const begin()
     */
    [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator { return data(); }

    /**
     * @brief Returns iterator to one past last character
     * @return Mutable iterator pointing past the end
     * @note Standard STL end iterator for range operations
     */
    [[nodiscard]] constexpr auto end() noexcept -> iterator { return buffer_type::end(); }

    /**
     * @brief Returns const iterator to one past last character
     * @return Const iterator pointing past the end
     * @note Read-only end iterator for const strings
     */
    [[nodiscard]] constexpr auto end() const noexcept -> const_iterator {
        return const_cast<basic_small_string*>(this)->end();
    }

    /**
     * @brief Returns const iterator to one past last character
     * @return Const iterator pointing past the end
     * @note Explicitly const version, same as const end()
     */
    [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator {
        return const_cast<basic_small_string*>(this)->end();
    }

    /**
     * @brief Returns reverse iterator to last character
     * @return Mutable reverse iterator for backward iteration
     * @note Enables reverse iteration from end to beginning
     */
    [[nodiscard]] constexpr auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }

    /**
     * @brief Returns const reverse iterator to last character
     * @return Const reverse iterator for backward iteration
     * @note Read-only reverse iteration for const strings
     */
    [[nodiscard]] constexpr auto rbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    /**
     * @brief Returns const reverse iterator to last character
     * @return Const reverse iterator for backward iteration
     * @note Explicitly const version, same as const rbegin()
     */
    [[nodiscard]] constexpr auto crbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    /**
     * @brief Returns reverse iterator to one before first character
     * @return Mutable reverse iterator marking reverse end
     * @note End marker for reverse iteration
     */
    [[nodiscard]] constexpr auto rend() noexcept -> reverse_iterator { return reverse_iterator{begin()}; }

    /**
     * @brief Returns const reverse iterator to one before first character
     * @return Const reverse iterator marking reverse end
     * @note Read-only end marker for reverse iteration
     */
    [[nodiscard]] constexpr auto rend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    /**
     * @brief Returns const reverse iterator to one before first character
     * @return Const reverse iterator marking reverse end
     * @note Explicitly const version, same as const rend()
     */
    [[nodiscard]] constexpr auto crend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    // capacity
    /**
     * @brief Checks if the string is empty
     * @return true if the string has no characters, false otherwise
     * @note Equivalent to size() == 0
     * @note Constexpr and noexcept for compile-time and exception-safe usage
     */
    [[nodiscard, gnu::always_inline]] constexpr auto empty() const noexcept -> bool { return size() == 0; }

    /**
     * @brief Returns the number of characters in the string
     * @return Current string length (number of characters)
     * @note Does not include null terminator in the count
     * @note Constant time operation for all storage types
     * @note Same as length() - provided for STL compatibility
     */
    [[nodiscard, gnu::always_inline]] constexpr auto size() const noexcept -> size_t { return buffer_type::size(); }

    /**
     * @brief Returns the number of characters in the string
     * @return Current string length (number of characters)
     * @note Identical to size() - provided for STL string compatibility
     * @note Does not include null terminator in the count
     * @note Constant time operation for all storage types
     */
    [[nodiscard, gnu::always_inline]] constexpr auto length() const noexcept -> size_type {
        return buffer_type::size();
    }

    /**
     * @brief The maximum number of elements that can be stored in the string.
     * the buffer largest size is 1 << 15, the cap occupy 2 bytes, so the max size is 1 << 15 - 2, is 65534
     */
    [[nodiscard, gnu::always_inline]] constexpr auto max_size() const noexcept -> size_type {
        if constexpr (NullTerminated) {
            return std::numeric_limits<size_type>::max() - sizeof(struct buffer_type_and_size<size_type>) - 1;
        } else {
            return std::numeric_limits<size_type>::max() - sizeof(struct buffer_type_and_size<size_type>);
        }
    }

    /**
     * @brief Reserves storage capacity for at least new_cap characters
     * @param new_cap Minimum capacity to reserve
     * @note If new_cap > capacity(), reallocates to accommodate the new capacity
     * @note If new_cap <= capacity(), does nothing (no shrinking)
     * @note Preserves existing string content during reallocation
     * @note Enables null termination during buffer operations
     */
    constexpr auto reserve(size_type new_cap) -> void {
        this->template buffer_reserve<buffer_type::Need0::Yes, true>(new_cap);
    }

    /**
     * @brief Returns the current storage capacity
     * @return Number of characters that can be stored without reallocation
     * @note Does not include space for null terminator in the count
     * @note For internal storage: returns embedded buffer capacity
     * @note For external storage: returns allocated buffer capacity minus headers
     * @note Avoid frequent calls in hot paths as it may be slow in some cases
     */
    [[nodiscard, gnu::always_inline]] constexpr auto capacity() const noexcept -> size_type {
        return buffer_type::capacity();
    }

    /**
     * @brief Reduces capacity to fit the current size
     * @note Requests removal of unused capacity to minimize memory usage
     * @note May reallocate to a smaller buffer if significant space can be saved
     * @note Implementation may choose not to shrink if overhead isn't justified
     * @note All iterators and references may be invalidated
     * @note Strong exception safety guarantee
     */
    constexpr auto shrink_to_fit() -> void {
#ifndef NDEBUG
        auto origin_core_type = buffer_type::get_core_type();
#endif

        auto [cap, size] = buffer_type::get_capacity_and_size();
        Assert(cap >= size, "cap should always be greater or equal to size");
        auto cap_and_type = buffer_type::calculate_new_buffer_size(size);
        if (cap > cap_and_type.buffer_size) {  // the cap is larger than the best cap, so need to shrink
            basic_small_string new_str{initialized_later{}, cap_and_type, size, buffer_type::get_allocator()};
            std::memcpy(new_str.data(), data(), size);
            swap(new_str);
        }

#ifndef NDEBUG
        auto new_core_type = buffer_type::get_core_type();
        Assert(origin_core_type >= new_core_type, "the core type should stay or shrink");
#endif
    }

    // modifiers
    /**
     * @brief Clears string content without deallocating memory
     * @note Sets size to 0 and null terminates if NullTerminated=true
     * @note Capacity remains unchanged for performance
     */
    constexpr auto clear() noexcept -> void { buffer_type::set_size(0); }

    /**
     * @brief Inserts count copies of character at specified position
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param index Position to insert at
     * @param count Number of characters to insert
     * @param ch Character to insert
     * @return Reference to this string
     * @throws std::out_of_range if index > size() and Safe=true
     */
    template <bool Safe = true>
    constexpr auto insert(size_type index, size_type count, Char ch) -> basic_small_string& {
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        if (index > size()) [[unlikely]] {
            throw std::out_of_range("index is out of range");
        }

        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        auto old_size = size();
        auto* buffer = buffer_type::get_buffer();
        if (index < old_size) [[likely]] {
            // if index == old_size, do not need memmove
            std::memmove(buffer + index + count, buffer + index, old_size - index);
        }
        std::memset(buffer + index, ch, count);
        buffer_type::increase_size(count);
        return *this;
    }

    /**
     * @brief Inserts null-terminated C-string at specified position
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param index Position to insert at
     * @param str Null-terminated string to insert
     * @return Reference to this string
     * @note For non-null-terminated strings, use insert(index, str, count)
     */
    template <bool Safe = true>
    constexpr auto insert(size_type index, const Char* str) -> basic_small_string& {
        return insert<Safe>(index, str, std::strlen(str));
    }

    /**
     * @brief Inserts character array of specified length at position
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param index Position to insert at
     * @param str Character array to insert (may contain null characters)
     * @param count Number of characters to insert from str
     * @return Reference to this string
     * @throws std::out_of_range if index > size() and Safe=true
     */
    template <bool Safe = true>
    constexpr auto insert(size_type index, const Char* str, size_type count) -> basic_small_string& {
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        auto old_size = size();
        if (index > old_size) [[unlikely]] {
            throw std::out_of_range("index is out of range");
        }
        // check if the capacity is enough
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        // by now, the capacity is enough
        // memmove the data to the new position
        auto* buffer = buffer_type::get_buffer();
        if (index < old_size) [[likely]] {
            std::memmove(buffer + index + count, buffer + index, old_size - index);
        }
        // set the new data
        std::memcpy(buffer + index, str, count);
        buffer_type::increase_size(count);
        return *this;
    }

    /**
     * @brief Inserts another small_string at specified position
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param index Position to insert at
     * @param other String to insert
     * @return Reference to this string
     */
    template <bool Safe = true>
    constexpr auto insert(size_type index, const basic_small_string& other) -> basic_small_string& {
        return insert<Safe>(index, other.data(), other.size());
    }

    /**
     * @brief Inserts substring of another string at specified position
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param index Position to insert at
     * @param str Source string
     * @param other_index Starting position in source string
     * @param count Number of characters to insert (npos = to end)
     * @return Reference to this string
     */
    template <bool Safe = true>
    constexpr auto insert(size_type index, const basic_small_string& str, size_type other_index, size_type count = npos)
      -> basic_small_string& {
        return insert<Safe>(index, str.substr(other_index, count));
    }

    /**
     * @brief Inserts single character at iterator position
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param pos Iterator position to insert at
     * @param ch Character to insert
     * @return Iterator to the inserted character
     */
    template <bool Safe = true>
    constexpr auto insert(const_iterator pos, Char ch) -> iterator {
        return insert<Safe>(pos, 1, ch);
    }

    /**
     * @brief Inserts count copies of character at iterator position
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param pos Iterator position to insert at
     * @param count Number of characters to insert
     * @param ch Character to insert
     * @return Iterator to the first inserted character
     */
    template <bool Safe = true>
    constexpr auto insert(const_iterator pos, size_type count, Char ch) -> iterator {
        size_type index = pos - begin();
        insert(index, count, ch);
        return begin() + index;
    }

    /**
     * @brief Inserts range of characters at iterator position
     * @tparam InputIt Input iterator type
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param pos Iterator position to insert at
     * @param first Iterator to start of range
     * @param last Iterator to end of range
     * @return Iterator to first inserted character
     */
    template <class InputIt, bool Safe = true>
    constexpr auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator {
        // static check the type of the input iterator
        static_assert(std::is_same_v<typename std::iterator_traits<InputIt>::value_type, Char>,
                      "the value type of the input iterator is not the same as the char type");
        // calculate the count
        size_type count = std::distance(first, last);
        if (count == 0) [[unlikely]] {
            // do nothing
            return const_cast<iterator>(pos);
        }
        size_type index = pos - begin();
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        // by now, the capacity is enough
        if (index < size()) [[likely]] {
            // move the data to the new position
            std::memmove(data() + index + count, data() + index, static_cast<size_type>(size() - index));
        }
        // copy the new data
        std::copy(first, last, data() + index);
        buffer_type::increase_size(count);
        return const_cast<iterator>(pos);
    }

    /**
     * @brief Inserts characters from initializer list at iterator position
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param pos Iterator position to insert at
     * @param ilist Initializer list of characters to insert
     * @return Iterator to the first inserted character
     */
    template <bool Safe = true>
    constexpr auto insert(const_iterator pos, std::initializer_list<Char> ilist) -> iterator {
        return insert<decltype(ilist.begin()), Safe>(pos, ilist.begin(), ilist.end());
    }

    /**
     * @brief Inserts string view-like object at iterator position
     * @tparam StringViewLike Type convertible to string_view but not to char*
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param pos Iterator position to insert at
     * @param t String view-like object to insert
     * @return Iterator to the first inserted character
     */
    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto insert(const_iterator pos, const StringViewLike& t) -> iterator {
        size_type index = pos - begin();
        insert<Safe>(index, t.data(), static_cast<size_type>(t.size()));
        return begin() + index;
    }

    /**
     * @brief Inserts substring from string view-like object at iterator position
     * @tparam StringViewLike Type convertible to string_view but not to char*
     * @tparam Safe Whether to perform bounds checking and automatic reallocation
     * @param pos Iterator position to insert at
     * @param t String view-like object to insert from
     * @param pos2 Starting position in source string
     * @param count Number of characters to insert (npos = to end)
     * @return Iterator to the first inserted character
     */
    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto insert(const_iterator pos, const StringViewLike& t, size_type pos2, size_type count = npos)
      -> iterator {
        auto sub = t.substr(pos2, count);
        size_type index = pos - begin();
        insert<Safe>(index, sub.data(), static_cast<size_type>(sub.size()));
        return begin() + index;
    }

    /**
     * @brief Removes characters from string starting at index
     * @param index Starting position to erase from (default: 0)
     * @param count Number of characters to erase (default: npos = to end)
     * @return Reference to this string
     * @throws std::out_of_range if index > size()
     * @note Capacity remains unchanged for performance
     */
    auto erase(size_type index = 0, size_type count = npos) -> basic_small_string& {
        auto old_size = this->size();
        if (index > old_size) [[unlikely]] {
            throw std::out_of_range("erase: index is out of range");
        }
        // calc the real count
        size_type real_count = std::min(count, static_cast<size_type>(old_size - index));
        if (real_count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        auto new_size = old_size - real_count;
        Assert(old_size >= index + real_count, "old size should be greater or equal than the index + real count");
        auto right_size = old_size - index - real_count;
        // if the count is greater than 0, then move right part of  the data to the new position
        char* buffer_ptr = buffer_type::get_buffer();
        if (right_size > 0) {
            // memmove the data to the new position
            std::memmove(buffer_ptr + index, buffer_ptr + index + real_count, static_cast<size_type>(right_size));
        }
        // set the new size
        buffer_type::set_size(new_size);
        return *this;
    }

    /**
     * @brief Erases single character at iterator position
     * @param first Iterator to character to erase
     * @return Iterator to character following the erased one
     */
    constexpr auto erase(const_iterator first) -> iterator {
        // the first must be valid, and of the string.
        auto index = first - begin();
        return erase(index, 1).begin() + index;
    }

    /**
     * @brief Erases range of characters between iterators
     * @param first Iterator to first character to erase
     * @param last Iterator to one past last character to erase
     * @return Iterator to character following the erased range
     */
    constexpr auto erase(const_iterator first, const_iterator last) -> iterator {
        auto index = first - begin();
        return erase(index, static_cast<size_t>(last - first)).begin() + index;
    }

    /**
     * @brief Appends single character to end of string
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param c Character to append
     * @note Optimized for frequent single-character additions
     */
    template <bool Safe = true>
    [[gnu::always_inline]] inline void push_back(Char c) {
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(1UL);
        }
        data()[size()] = c;
        buffer_type::increase_size(1);
    }

    /**
     * @brief Removes last character from string
     * @note Undefined behavior if string is empty
     */
    void pop_back() { buffer_type::decrease_size(1); }

    /**
     * @brief Appends count copies of character to end of string
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param count Number of characters to append
     * @param c Character to append
     * @return Reference to this string
     */
    template <bool Safe = true>
    constexpr auto append(size_type count, Char c) -> basic_small_string& {
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        // by now, the capacity is enough
        std::memset(end(), c, count);
        buffer_type::increase_size(count);
        return *this;
    }

    /**
     * @brief Appends another string to end of this string
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param other String to append
     * @return Reference to this string
     */
    template <bool Safe = true>
    constexpr auto append(const basic_small_string& other) -> basic_small_string& {
        if (other.empty()) [[unlikely]] {
            // do nothing
            return *this;
        }
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(other.size());
        }
        // by now, the capacity is enough
        // size() function maybe slower than while the size is larger than 4k, so store it.
        auto other_size = other.size();
        std::memcpy(end(), other.data(), other_size);
        buffer_type::increase_size(other_size);
        return *this;
    }

    /**
     * @brief Appends substring from another string
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param other Source string to append from
     * @param pos Starting position in source string
     * @param count Number of characters to append (npos = to end)
     * @return Reference to this string
     * @throws std::out_of_range if pos > other.size()
     */
    template <bool Safe = true>
    constexpr auto append(const basic_small_string& other, size_type pos, size_type count = npos)
      -> basic_small_string& {
        if (pos > other.size()) [[unlikely]] {
            throw std::out_of_range("append: pos is out of range");
        }
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        count = std::min(count, static_cast<size_type>(other.size() - pos));
        return append<Safe>(other.data() + pos, count);
    }

    /**
     * @brief Appends character array of specified length
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param s Pointer to character array
     * @param count Number of characters to append
     * @return Reference to this string
     * @note Can handle arrays containing null characters
     */
    template <bool Safe = true>
    constexpr auto append(const Char* s, size_type count) -> basic_small_string& {
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }

        std::memcpy(end(), s, count);
        buffer_type::increase_size(count);
        return *this;
    }

    /**
     * @brief Appends null-terminated string
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param s Null-terminated string to append
     * @return Reference to this string
     * @note Length determined automatically using std::strlen
     */
    template <bool Safe = true>
    constexpr auto append(const Char* s) -> basic_small_string& {
        return append<Safe>(s, std::strlen(s));
    }

    /**
     * @brief Appends range of characters from iterators
     * @tparam InputIt Input iterator type
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param first Iterator to beginning of range
     * @param last Iterator to end of range
     * @return Reference to this string
     * @note Iterator value_type must match Char type
     */
    template <class InputIt, bool Safe = true>
    constexpr auto append(InputIt first, InputIt last) -> basic_small_string& {
        static_assert(std::is_same_v<typename std::iterator_traits<InputIt>::value_type, Char>,
                      "the value type of the input iterator is not the same as the char type");
        size_type count = std::distance(first, last);
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        std::copy(first, last, end());
        buffer_type::increase_size(count);
        return *this;
    }

    /**
     * @brief Appends characters from initializer list
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param ilist Initializer list of characters to append
     * @return Reference to this string
     */
    template <bool Safe = true>
    constexpr auto append(std::initializer_list<Char> ilist) -> basic_small_string& {
        return append<Safe>(ilist.begin(), ilist.size());
    }

    /**
     * @brief Appends string view-like object
     * @tparam StringViewLike Type convertible to string_view but not to char*
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param t String view-like object to append
     * @return Reference to this string
     */
    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto append(const StringViewLike& t) -> basic_small_string& {
        return append<Safe>(t.data(), t.size());
    }

    /**
     * @brief Appends substring from string view-like object
     * @tparam StringViewLike Type convertible to string_view but not to char*
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param s String view-like object to append from
     * @param pos Starting position in source
     * @param count Number of characters to append (npos = to end)
     * @return Reference to this string
     */
    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto append(const StringViewLike& s, size_type pos, size_type count = npos) -> basic_small_string& {
        if (count == npos or count + pos > s.size()) {
            count = s.size() - pos;
        }
        return append<Safe>(s.data() + pos, count);
    }

    /**
     * @brief Compound assignment (append) from another string
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param other String to append
     * @return Reference to this string after appending
     * @note Equivalent to append(other)
     */
    template <bool Safe = true>
    auto operator+=(const basic_small_string& other) -> basic_small_string& {
        return append<Safe>(other);
    }

    /**
     * @brief Compound assignment (append) from single character
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param ch Character to append
     * @return Reference to this string after appending
     * @note Equivalent to push_back(ch)
     */
    template <bool Safe = true>
    auto operator+=(Char ch) -> basic_small_string& {
        push_back<Safe>(ch);
        return *this;
    }

    /**
     * @brief Compound assignment (append) from C-string
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param s Null-terminated string to append
     * @return Reference to this string after appending
     * @note Equivalent to append(s)
     */
    template <bool Safe = true>
    auto operator+=(const Char* s) -> basic_small_string& {
        return append<Safe>(s);
    }

    /**
     * @brief Compound assignment (append) from initializer list
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param ilist Initializer list of characters to append
     * @return Reference to this string after appending
     * @note Equivalent to append(ilist)
     */
    template <bool Safe = true>
    auto operator+=(std::initializer_list<Char> ilist) -> basic_small_string& {
        return append<Safe>(ilist);
    }

    /**
     * @brief Compound assignment (append) from string view-like object
     * @tparam StringViewLike Type convertible to string_view but not to char*
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param t String view-like object to append
     * @return Reference to this string after appending
     * @note Equivalent to append(t)
     */
    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    auto operator+=(const StringViewLike& t) -> basic_small_string& {
        return append<Safe>(t);
    }

    /**
     * @brief Replaces substring with repeated character
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param pos Starting position of substring to replace
     * @param count Number of characters to replace
     * @param count2 Number of replacement characters
     * @param ch Character to repeat for replacement
     * @return Reference to this string after replacement
     * @throws std::out_of_range if pos > size()
     * @note If count2 is 0, equivalent to erase(pos, count)
     */
    template <bool Safe = true>
    auto replace(size_type pos, size_type count, size_type count2, Char ch) -> basic_small_string& {
        auto [cap, old_size] = buffer_type::get_capacity_and_size();
        // check the pos is not out of range
        if (pos > old_size) [[unlikely]] {
            throw std::out_of_range("replace: pos is out of range");
        }
        if (count2 == 0) [[unlikely]] {
            // just erase the data
            return erase(pos, count);
        }
        count = std::min(count, static_cast<size_type>(old_size - pos));

        if ((count >= old_size - pos) and count2 <= (cap - pos)) {
            // copy the data to pos, and no need to move the right part
            std::memset(buffer_type::get_buffer() + pos, ch, count2);
            auto new_size = pos + count2;
            buffer_type::set_size(new_size);
            return *this;
        }

        if (count == count2) {
            // just replace
            std::memset(buffer_type::get_buffer() + pos, ch, count2);
            return *this;
        }

        // else, the count != count2, need to move the right part
        // init a new string
        basic_small_string ret{initialized_later{}, static_cast<size_type>(old_size - count + count2),
                               buffer_type::get_allocator()};
        Char* p = ret.data();
        // copy the left party
        std::memcpy(p, data(), pos);
        p += pos;
        std::memset(p, ch, count2);
        p += count2;
        std::copy(begin() + pos + count, end(), p);
        *this = std::move(ret);
        return *this;
    }

    /**
     * @brief Replaces substring with character array
     * @tparam Safe Whether to perform automatic reallocation if needed
     * @param pos Starting position of substring to replace
     * @param count Number of characters to replace
     * @param str Pointer to character array for replacement
     * @param count2 Number of characters from str to use
     * @return Reference to this string after replacement
     * @throws std::out_of_range if pos > size()
     * @note If count2 is 0, equivalent to erase(pos, count)
     */
    template <bool Safe = true>
    auto replace(size_type pos, size_type count, const Char* str, size_type count2) -> basic_small_string& {
        auto [cap, old_size] = buffer_type::get_capacity_and_size();
        if (pos > old_size) [[unlikely]] {
            throw std::out_of_range("replace: pos is out of range");
        }
        if (count2 == 0) [[unlikely]] {
            return erase(pos, count);
        }
        count = std::min(count, static_cast<size_type>(old_size - pos));

        if ((count >= old_size - pos) and count2 <= (cap - pos)) {  // count == npos still >= old_size - pos.
            // copy the data to pos, and no need to move the right part
            std::memcpy(buffer_type::get_buffer() + pos, str, count2);
            auto new_size = pos + count2;
            buffer_type::set_size(new_size);
            return *this;
        }
        if (count == count2) {
            // just replace
            std::memcpy(buffer_type::get_buffer() + pos, str, count2);
            return *this;
        }
        // else, the count != count2, need to move the right part
        // init a new string
        basic_small_string ret{initialized_later{}, static_cast<size_type>(old_size - count + count2),
                               buffer_type::get_allocator()};
        Char* p = ret.data();
        // copy the left party
        std::memcpy(p, data(), pos);
        p += pos;
        // copy the new data
        std::memcpy(p, str, count2);
        p += count2;
        std::copy(begin() + pos + count, end(), p);
        *this = std::move(ret);
        return *this;
    }

    /**
     * @brief Replaces substring with another string
     * @param pos Starting position of substring to replace
     * @param count Number of characters to replace
     * @param other String to use as replacement
     * @return Reference to this string after replacement
     * @note Delegates to replace(pos, count, other.data(), other.size())
     */
    auto replace(size_type pos, size_type count, const basic_small_string& other) -> basic_small_string& {
        return replace(pos, count, other.data(), other.size());
    }

    /**
     * @brief Replaces iterator range with another string
     * @param first Iterator to beginning of range to replace
     * @param last Iterator to end of range to replace
     * @param other String to use as replacement
     * @return Reference to this string after replacement
     * @note Converts iterators to position and count, then delegates
     */
    auto replace(const_iterator first, const_iterator last, const basic_small_string& other) -> basic_small_string& {
        return replace(static_cast<size_t>(first - begin()), static_cast<size_t>(last - first), other.data(),
                       other.size());
    }

    /**
     * @brief Replaces substring with substring from another string
     * @param pos Starting position of substring to replace
     * @param count Number of characters to replace
     * @param other Source string for replacement
     * @param pos2 Starting position in source string
     * @param count2 Number of characters from source (npos = to end)
     * @return Reference to this string after replacement
     * @note Uses substr to extract replacement substring
     */
    auto replace(size_type pos, size_type count, const basic_small_string& other, size_type pos2,
                 size_type count2 = npos) -> basic_small_string& {
        if (count2 == npos) {
            count2 = other.size() - pos2;
        }
        return replace(pos, count, other.substr(pos2, count2));
    }

    /**
     * @brief Replaces iterator range with character array
     * @param first Iterator to beginning of range to replace
     * @param last Iterator to end of range to replace
     * @param cstr Pointer to character array for replacement
     * @param count2 Number of characters from cstr to use
     * @return Reference to this string after replacement
     * @note Converts iterators to position and count, then delegates
     */
    auto replace(const_iterator first, const_iterator last, const Char* cstr, size_t count2) -> basic_small_string& {
        return replace(static_cast<size_t>(first - begin()), static_cast<size_t>(last - first), cstr, count2);
    }

    /**
     * @brief Replaces substring with null-terminated string
     * @param pos Starting position of substring to replace
     * @param count Number of characters to replace
     * @param cstr Null-terminated string for replacement
     * @return Reference to this string after replacement
     * @note Length calculated automatically using traits_type::length()
     */
    auto replace(size_type pos, size_type count, const Char* cstr) -> basic_small_string& {
        return replace(pos, count, cstr, traits_type::length(cstr));
    }

    /**
     * @brief Replaces iterator range with null-terminated string
     * @param first Iterator to beginning of range to replace
     * @param last Iterator to end of range to replace
     * @param cstr Null-terminated string for replacement
     * @return Reference to this string after replacement
     * @note Length calculated automatically using traits_type::length()
     */
    auto replace(const_iterator first, const_iterator last, const Char* cstr) -> basic_small_string& {
        return replace(static_cast<size_t>(first - begin()), static_cast<size_t>(last - first), cstr,
                       traits_type::length(cstr));
    }

    /**
     * @brief Replaces iterator range with repeated character
     * @param first Iterator to beginning of range to replace
     * @param last Iterator to end of range to replace
     * @param count Number of replacement characters
     * @param ch Character to repeat for replacement
     * @return Reference to this string after replacement
     * @note Converts iterators to position and count, then delegates
     */
    auto replace(const_iterator first, const_iterator last, size_type count, Char ch) -> basic_small_string& {
        return replace(static_cast<size_t>(first - begin()), static_cast<size_t>(last - first), count, ch);
    }

    /**
     * @brief Replaces iterator range with iterator range
     * @tparam InputIt Input iterator type
     * @param first Iterator to beginning of range to replace
     * @param last Iterator to end of range to replace
     * @param first2 Iterator to beginning of replacement range
     * @param last2 Iterator to end of replacement range
     * @return Reference to this string after replacement
     * @throws std::invalid_argument if replacement range is invalid
     * @note Iterator value_type must match Char type
     */
    template <class InputIt>
    auto replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2) -> basic_small_string& {
        static_assert(std::is_same_v<typename std::iterator_traits<InputIt>::value_type, Char>,
                      "the value type of the input iterator is not the same as the char type");
        Assert(first2 <= last2, "the range is valid");
        auto pos = std::distance(cbegin(), first);
        auto count = std::distance(first, last);
        if (count < 0) [[unlikely]] {
            throw std::invalid_argument("the range is invalid");
        }
        auto count2 = std::distance(first2, last2);
        if (count2 == 0) [[unlikely]] {
            // just delete the right part
            erase(first, last);
            return *this;
        }

        auto [cap, old_size] = buffer_type::get_capacity_and_size();

        if (last == end() and count2 <= (cap - pos)) {
            // copy the data to pos, and no need to move the right part
            std::copy(first2, last2, data() + pos);
            auto new_size = pos + count2;
            buffer_type::set_size(new_size);
            return *this;
        }

        if (count == count2) {
            // just replace
            std::copy(first2, last2, data() + pos);
            return *this;
        }

        // else, the count != count2, need to move the right part
        // init a new string
        basic_small_string ret{initialized_later{}, static_cast<size_type>(old_size - count + count2),
                               buffer_type::get_allocator()};
        Char* p = ret.data();
        // copy the left party
        std::copy(cbegin(), first, ret.begin());
        p += pos;
        // copy the new data
        std::copy(first2, last2, p);
        p += count2;
        std::copy(last, cend(), p);
        *this = std::move(ret);
        return *this;
    }

    /**
     * @brief Replaces iterator range with initializer list
     * @param first Iterator to beginning of range to replace
     * @param last Iterator to end of range to replace
     * @param ilist Initializer list of characters for replacement
     * @return Reference to this string after replacement
     * @note Delegates to iterator range replacement
     */
    auto replace(const_iterator first, const_iterator last, std::initializer_list<Char> ilist) -> basic_small_string& {
        return replace(first, last, ilist.begin(), ilist.end());
    }

    /**
     * @brief Replaces iterator range with string view-like object
     * @tparam StringViewLike Type convertible to string_view but not to char*
     * @param first Iterator to beginning of range to replace
     * @param last Iterator to end of range to replace
     * @param view String view-like object for replacement
     * @return Reference to this string after replacement
     * @note Uses .data() and .size() methods from string view interface
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    auto replace(const_iterator first, const_iterator last, const StringViewLike& view) -> basic_small_string& {
        return replace(first, last, view.data(), view.size());
    }

    /**
     * @brief Replaces substring with substring from string view-like object
     * @tparam StringViewLike Type convertible to string_view but not to char*
     * @param pos Starting position of substring to replace
     * @param count Number of characters to replace
     * @param view Source string view-like object for replacement
     * @param pos2 Starting position in source view
     * @param count2 Number of characters from source (npos = to end)
     * @return Reference to this string after replacement
     * @note Extracts substring from view using data() + pos2
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    auto replace(size_type pos, size_type count, const StringViewLike& view, size_type pos2, size_type count2 = npos)
      -> basic_small_string& {
        if (count2 == npos) {
            count2 = view.size() - pos2;
        }
        return replace(pos, count, view.data() + pos, count2);
    }

    /**
     * @brief Copies substring to destination buffer
     * @param dest Pointer to destination character array
     * @param count Maximum number of characters to copy (npos = all remaining)
     * @param pos Starting position in source string (default: 0)
     * @return Number of characters actually copied
     * @throws std::out_of_range if pos > size()
     * @note Does not null-terminate the destination buffer
     * @note If count exceeds available characters, copies only what's available
     * @note Destination buffer must have sufficient capacity for copied characters
     */
    auto copy(Char* dest, size_type count = npos, size_type pos = 0) const -> size_t {
        auto current_size = size();
        if (pos > current_size) [[unlikely]] {
            throw std::out_of_range("copy's pos > size()");
        }
        if ((count == npos) or (pos + count > current_size)) {
            count = current_size - pos;
        }
        std::memcpy(dest, data() + pos, count);
        return count;
    }

    /**
     * @brief Resizes string to specified length
     * @param count New size for the string
     * @note If count < size(): truncates string, removing characters from end
     * @note If count > size(): extends string, filling new characters with null ('\0')
     * @note If count == size(): no operation performed
     * @note May trigger reallocation if count > capacity()
     * @note All iterators and references may be invalidated if reallocation occurs
     */
    auto resize(size_t count) -> void {
        Assert(count < std::numeric_limits<size_type>::max(), "count is too large");
        auto [cap, old_size] = buffer_type::get_capacity_and_size();

        if (count <= old_size) [[likely]] {
            // if count == old_size, just set the size
            buffer_type::set_size(count);
            return;
        }
        if (count > cap) {
            this->template buffer_reserve<buffer_type::Need0::No, true>(count);
        }
        std::memset(data() + old_size, '\0', count - old_size);
        buffer_type::set_size(count);
        return;
    }

    /**
     * @brief Resizes string to specified length with fill character
     * @param count New size for the string
     * @param ch Character to fill new positions if expanding
     * @note If count < size(): truncates string, removing characters from end
     * @note If count > size(): extends string, filling new characters with ch
     * @note If count == size(): no operation performed
     * @note May trigger reallocation if count > capacity()
     * @note All iterators and references may be invalidated if reallocation occurs
     */
    auto resize(size_type count, Char ch) -> void {
        Assert(count < std::numeric_limits<size_type>::max(), "count is too large");
        auto [cap, old_size] = buffer_type::get_capacity_and_size();

        if (count <= old_size) [[likely]] {
            // if count == old_size, just set the size
            buffer_type::set_size(count);
            return;
        }
        if (count > cap) {
            // if the count is greater than the size, need to reserve
            this->template buffer_reserve<buffer_type::Need0::No, true>(count);
        }
        // by now, the capacity is enough
        std::memset(data() + old_size, ch, count - old_size);
        buffer_type::set_size(count);
        return;
    }

    /**
     * @brief Swaps string contents with another string
     * @param other String to swap with
     * @note Constant-time operation regardless of string sizes
     * @note Compatible with std::swap and ADL
     * @note Self-assignment safe
     */
    void swap(basic_small_string& other) noexcept {
        // just a temp variable to avoid self-assignment, std::swap will do the same thing, maybe faster?
        buffer_type::swap(other);
    }

    /**
     * @brief Finds first occurrence of substring in string
     * @param str Character array to search for
     * @param pos Starting position for search (default: 0)
     * @param count Length of substring to find
     * @return Position of first match, or npos if not found
     * @note Uses optimized Boyer-Moore-like algorithm for performance
     */
    constexpr auto find(const Char* str, size_type pos, size_type count) const -> size_t {
        auto current_size = buffer_type::size();
        if (count == 0) [[unlikely]] {
            return pos <= current_size ? pos : npos;
        }
        if (pos >= current_size) [[unlikely]] {
            return npos;
        }

        const auto elem0 = str[0];
        const auto* data_ptr = buffer_type::get_buffer();
        const auto* first_ptr = data_ptr + pos;
        const auto* const last_ptr = data_ptr + current_size;
        auto len = current_size - pos;

        while (len >= count) {
            first_ptr = traits_type::find(first_ptr, len - count + 1, elem0);
            if (first_ptr == nullptr) {
                return npos;
            }
            if (traits_type::compare(first_ptr, str, count) == 0) {
                return size_t(first_ptr - data_ptr);
            }
            len = static_cast<size_type>(last_ptr - ++first_ptr);
        }
        return npos;
    }

    /**
     * @brief Finds first occurrence of null-terminated substring
     * @param needle Null-terminated string to search for
     * @param pos Starting position for search (default: 0)
     * @return Position of first match, or npos if not found
     */
    [[nodiscard]] constexpr auto find(const Char* needle, size_type pos = 0) const -> size_t {
        return find(needle, pos, traits_type::length(needle));
    }

    /**
     * @brief Finds first occurrence of another string
     * @param other String to search for
     * @param pos Starting position for search (default: 0)
     * @return Position of first match, or npos if not found
     */
    [[nodiscard]] constexpr auto find(const basic_small_string& other, size_type pos = 0) const -> size_t {
        return find(other.data(), pos, other.size());
    }

    /**
     * @brief Finds first occurrence of string_view-like object
     * @tparam StringViewLike Type convertible to string_view
     * @param view String view-like object to search for
     * @param pos Starting position for search (default: 0)
     * @return Position of first match, or npos if not found
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find(const StringViewLike& view, size_type pos = 0) const -> size_t {
        return find(view.data(), pos, view.size());
    }

    /**
     * @brief Finds first occurrence of character
     * @param ch Character to search for
     * @param pos Starting position for search (default: 0)
     * @return Position of first match, or npos if not found
     * @note Optimized single-character search
     */
    [[nodiscard]] constexpr auto find(Char ch, size_type pos = 0) const -> size_t {
        auto* found = traits_type::find(data() + pos, size() - pos, ch);
        return found == nullptr ? npos : size_t(found - data());
    }

    /**
     * @brief Finds last occurrence of substring in string
     * @param str Character array to search for
     * @param pos Starting position for reverse search
     * @param str_length Length of substring to find
     * @return Position of last match, or npos if not found
     * @note Searches backwards from pos
     */
    [[nodiscard]] constexpr auto rfind(const Char* str, size_type pos, size_type str_length) const -> size_t {
        auto current_size = buffer_type::size();
        if (str_length <= current_size) [[likely]] {
            pos = std::min(pos, current_size - str_length);
            const auto* buffer_ptr = buffer_type::get_buffer();
            do {
                if (traits_type::compare(buffer_ptr + pos, str, str_length) == 0) {
                    return pos;
                }
            } while (pos-- > 0);
        }
        return npos;
    }

    /**
     * @brief Finds last occurrence of null-terminated substring
     * @param needle Null-terminated string to search for
     * @param pos Starting position for reverse search (default: npos)
     * @return Position of last match, or npos if not found
     */
    [[nodiscard]] constexpr auto rfind(const Char* needle, size_type pos = npos) const -> size_t {
        return rfind(needle, pos, traits_type::length(needle));
    }

    /**
     * @brief Finds last occurrence of another small_string
     * @param other String to search for
     * @param pos Starting position for reverse search (default: npos)
     * @return Position of last match, or npos if not found
     */
    [[nodiscard]] constexpr auto rfind(const basic_small_string& other, size_type pos = npos) const -> size_t {
        return rfind(other.data(), pos, other.size());
    }

    /**
     * @brief Finds last occurrence of string-view-like object
     * @tparam StringViewLike Type convertible to string_view but not to const Char*
     * @param view String view object to search for
     * @param pos Starting position for reverse search (default: npos)
     * @return Position of last match, or npos if not found
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto rfind(const StringViewLike& view, size_type pos = npos) const -> size_t {
        return rfind(view.data(), pos, view.size());
    }

    /**
     * @brief Finds last occurrence of character
     * @param ch Character to search for
     * @param pos Starting position for reverse search (default: npos = from end)
     * @return Position of last match, or npos if not found
     * @note Optimized single-character reverse search
     */
    [[nodiscard]] constexpr auto rfind(Char ch, size_type pos = npos) const -> size_t {
        auto current_size = size();
        const auto* buffer_ptr = buffer_type::get_buffer();
        if (current_size > 0) [[likely]] {
            if (--current_size > pos) {
                current_size = pos;
            }
            for (++current_size; current_size-- > 0;) {
                if (traits_type::eq(buffer_ptr[current_size], ch)) {
                    return current_size;
                }
            }
        }
        return npos;
    }

    /**
     * @brief Finds first character that matches any character in given set
     * @param str Character set to match against
     * @param pos Starting position for search
     * @param count Number of characters in set
     * @return Position of first matching character, or npos if not found
     * @note Useful for finding characters from a specific set
     */
    [[nodiscard]] constexpr auto find_first_of(const Char* str, size_type pos, size_type count) const -> size_t {
        auto current_size = this->size();
        auto buffer_ptr = buffer_type::get_buffer();
        for (; count > 0 && pos < current_size; ++pos) {
            if (traits_type::find(str, count, buffer_ptr[pos]) != nullptr) {
                return pos;
            }
        }
        return npos;
    }

    /**
     * @brief Finds first character matching any in null-terminated set
     * @param str Null-terminated character set to match against
     * @param pos Starting position for search (default: 0)
     * @return Position of first matching character, or npos if not found
     */
    [[nodiscard]] constexpr auto find_first_of(const Char* str, size_type pos = 0) const -> size_t {
        return find_first_of(str, pos, traits_type::length(str));
    }

    /**
     * @brief Finds first character matching any in another small_string
     * @param other String containing character set to match against
     * @param pos Starting position for search (default: 0)
     * @return Position of first matching character, or npos if not found
     */
    [[nodiscard]] constexpr auto find_first_of(const basic_small_string& other, size_type pos = 0) const -> size_t {
        return find_first_of(other.data(), pos, other.size());
    }

    /**
     * @brief Finds first character matching any in string-view-like object
     * @tparam StringViewLike Type convertible to string_view but not to const Char*
     * @param view String view containing character set to match against
     * @param pos Starting position for search (default: 0)
     * @return Position of first matching character, or npos if not found
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_first_of(const StringViewLike& view, size_type pos = 0) const -> size_t {
        return find_first_of(view.data(), pos, view.size());
    }

    /**
     * @brief Finds first occurrence of specific character
     * @param ch Character to search for
     * @param pos Starting position for search (default: 0)
     * @return Position of character, or npos if not found
     * @note Convenience wrapper for single-character find_first_of
     */
    [[nodiscard]] constexpr auto find_first_of(Char ch, size_type pos = 0) const -> size_t {
        return find_first_of(&ch, pos, 1);
    }

    /**
     * @brief Finds first character that doesn't match any character in given set
     * @param str Character set to exclude
     * @param pos Starting position for search
     * @param count Number of characters in set
     * @return Position of first non-matching character, or npos if not found
     * @note Inverse of find_first_of - finds characters NOT in the set
     */
    [[nodiscard]] constexpr auto find_first_not_of(const Char* str, size_type pos, size_type count) const -> size_t {
        auto current_size = this->size();
        const auto* buffer_ptr = buffer_type::get_buffer();
        for (; pos < current_size; ++pos) {
            if (traits_type::find(str, count, buffer_ptr[pos]) == nullptr) {
                return pos;
            }
        }
        return npos;
    }

    /**
     * @brief Finds first character not in null-terminated exclusion set
     * @param str Null-terminated character set to exclude
     * @param pos Starting position for search (default: 0)
     * @return Position of first non-matching character, or npos if not found
     */
    [[nodiscard]] constexpr auto find_first_not_of(const Char* str, size_type pos = 0) const -> size_t {
        return find_first_not_of(str, pos, traits_type::length(str));
    }

    /**
     * @brief Finds first character not matching any in another small_string
     * @param other String containing character set to exclude
     * @param pos Starting position for search (default: 0)
     * @return Position of first non-matching character, or npos if not found
     */
    [[nodiscard]] constexpr auto find_first_not_of(const basic_small_string& other, size_type pos = 0) const -> size_t {
        return find_first_not_of(other.data(), pos, other.size());
    }

    /**
     * @brief Finds first character not matching any in string-view-like object
     * @tparam StringViewLike Type convertible to string_view but not to const Char*
     * @param view String view containing character set to exclude
     * @param pos Starting position for search (default: 0)
     * @return Position of first non-matching character, or npos if not found
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_first_not_of(const StringViewLike& view, size_type pos = 0) const -> size_t {
        return find_first_not_of(view.data(), pos, view.size());
    }

    /**
     * @brief Finds first character that doesn't match specified character
     * @param ch Character to exclude
     * @param pos Starting position for search (default: 0)
     * @return Position of first non-matching character, or npos if not found
     * @note Convenience wrapper for single-character find_first_not_of
     */
    [[nodiscard]] constexpr auto find_first_not_of(Char ch, size_type pos = 0) const -> size_t {
        return find_first_not_of(&ch, pos, 1);
    }

    /**
     * @brief Finds the last occurrence of any character from the specified range.
     * @param str Pointer to the character array to search for
     * @param pos Position to start the search from (searches backwards)
     * @param count Number of characters from str to consider
     * @return Position of the last occurrence of any character from str, or npos if not found
     */
    [[nodiscard]] constexpr auto find_last_of(const Char* str, size_type pos, size_type count) const -> size_t {
        auto current_size = this->size();
        const auto* buffer_ptr = buffer_type::get_buffer();
        if (current_size && count) [[likely]] {
            if (--current_size > pos) {
                current_size = pos;
            }
            do {
                if (traits_type::find(str, count, buffer_ptr[current_size]) != nullptr) {
                    return current_size;
                }
            } while (current_size-- != 0);
        }
        return npos;
    }

    /**
     * @brief Finds the last occurrence of any character from the null-terminated string.
     * @param str Null-terminated string to search for
     * @param pos Position to start the search from (searches backwards), defaults to end of string
     * @return Position of the last occurrence of any character from str, or npos if not found
     */
    [[nodiscard]] constexpr auto find_last_of(const Char* str, size_type pos = npos) const -> size_t {
        return find_last_of(str, pos, traits_type::length(str));
    }

    /**
     * @brief Finds the last occurrence of any character from another string.
     * @param other String containing characters to search for
     * @param pos Position to start the search from (searches backwards), defaults to end of string
     * @return Position of the last occurrence of any character from other, or npos if not found
     */
    [[nodiscard]] constexpr auto find_last_of(const basic_small_string& other, size_type pos = npos) const -> size_t {
        return find_last_of(other.data(), pos, other.size());
    }

    /**
     * @brief Finds the last occurrence of any character from a string-like object.
     * @tparam StringViewLike Type convertible to string_view but not to const Char*
     * @param view String-like object containing characters to search for
     * @param pos Position to start the search from (searches backwards), defaults to end of string
     * @return Position of the last occurrence of any character from view, or npos if not found
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_last_of(const StringViewLike& view, size_type pos = npos) const -> size_t {
        return find_last_of(view.data(), pos, view.size());
    }

    /**
     * @brief Finds the last occurrence of a specific character.
     * @param ch Character to search for
     * @param pos Position to start the search from (searches backwards), defaults to end of string
     * @return Position of the last occurrence of ch, or npos if not found
     */
    [[nodiscard]] constexpr auto find_last_of(Char ch, size_type pos = npos) const -> size_t {
        return find_last_of(&ch, pos, 1);
    }

    /**
     * @brief Finds the last character that is not in the specified character set.
     * @param str Pointer to the character array to avoid
     * @param pos Position to start the search from (searches backwards)
     * @param count Number of characters from str to consider
     * @return Position of the last character not in str, or npos if not found
     */
    [[nodiscard]] constexpr auto find_last_not_of(const Char* str, size_type pos, size_type count) const -> size_t {
        auto current_size = buffer_type::size();
        if (current_size > 0) {
            if (--current_size > pos) {
                current_size = pos;
            }
            do {
                if (traits_type::find(str, count, at(current_size)) == nullptr) {
                    return current_size;
                }
            } while (current_size-- != 0);
        }
        return npos;
    }

    /**
     * @brief Finds the last character that is not in the null-terminated character set.
     * @param str Null-terminated string containing characters to avoid
     * @param pos Position to start the search from (searches backwards), defaults to end of string
     * @return Position of the last character not in str, or npos if not found
     */
    [[nodiscard]] constexpr auto find_last_not_of(const Char* str, size_type pos = npos) const -> size_t {
        return find_last_not_of(str, pos, traits_type::length(str));
    }

    /**
     * @brief Finds the last character that is not in another string.
     * @param other String containing characters to avoid
     * @param pos Position to start the search from (searches backwards), defaults to end of string
     * @return Position of the last character not in other, or npos if not found
     */
    [[nodiscard]] constexpr auto find_last_not_of(const basic_small_string& other, size_type pos = npos) const
      -> size_t {
        return find_last_not_of(other.data(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    /**
     * @brief Finds the last character that is not in a string-like object.
     * @tparam StringViewLike Type convertible to string_view but not to const Char*
     * @param view String-like object containing characters to avoid
     * @param pos Position to start the search from (searches backwards), defaults to end of string
     * @return Position of the last character not in view, or npos if not found
     */
    [[nodiscard]] constexpr auto find_last_not_of(const StringViewLike& view, size_type pos = npos) const -> size_t {
        return find_last_not_of(view.data(), pos, view.size());
    }

    /**
     * @brief Finds the last character that is not equal to the specified character.
     * @param ch Character to avoid
     * @param pos Position to start the search from (searches backwards), defaults to end of string
     * @return Position of the last character not equal to ch, or npos if not found
     */
    [[nodiscard]] constexpr auto find_last_not_of(Char ch, size_type pos = npos) const -> size_t {
        return find_last_not_of(&ch, pos, 1);
    }

    /**
     * @brief Lexicographically compares this string with another
     * @param other String to compare with
     * @return Negative value if this < other, 0 if equal, positive if this > other
     * @note Standard three-way comparison semantics
     */
    [[nodiscard]] constexpr auto compare(const basic_small_string& other) const noexcept -> int {
        auto this_size = size();
        auto other_size = other.size();
        auto r = traits_type::compare(data(), other.data(), std::min(this_size, other_size));
        return r != 0 ? r : this_size > other_size ? 1 : this_size < other_size ? -1 : 0;
    }
    /**
     * @brief Compares substring of this string with another string
     * @param pos Starting position in this string
     * @param count Length of substring to compare
     * @param other String to compare with
     * @return Negative value if substring < other, 0 if equal, positive if substring > other
     */
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const basic_small_string& other) const -> int {
        return compare(pos, count, other.data(), other.size());
    }
    /**
     * @brief Compares substring of this string with substring of another
     * @param pos1 Starting position in this string
     * @param count1 Length of substring in this string
     * @param other Other string to compare with
     * @param pos2 Starting position in other string
     * @param count2 Length of substring in other string (default: npos = to end)
     * @return Negative value if substring1 < substring2, 0 if equal, positive if substring1 > substring2
     * @throws std::out_of_range if pos2 > other.size()
     */
    [[nodiscard]] constexpr auto compare(size_type pos1, size_type count1, const basic_small_string& other,
                                         size_type pos2, size_type count2 = npos) const -> int {
        if (pos2 > other.size()) [[unlikely]] {
            throw std::out_of_range("compare: pos2 is out of range");
        }
        count2 = std::min(count2, static_cast<size_type>(other.size() - pos2));
        return compare(pos1, count1, other.data() + pos2, count2);
    }
    /**
     * @brief Compares this string with null-terminated C-string
     * @param str Null-terminated string to compare with
     * @return Negative value if this < str, 0 if equal, positive if this > str
     */
    [[nodiscard]] constexpr auto compare(const Char* str) const noexcept -> int {
        return compare(0, size(), str, traits_type::length(str));
    }
    /**
     * @brief Compares substring with null-terminated C-string
     * @param pos Starting position in this string
     * @param count Length of substring to compare
     * @param str Null-terminated string to compare with
     * @return Negative value if substring < str, 0 if equal, positive if substring > str
     */
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const Char* str) const -> int {
        return compare(pos, count, str, traits_type::length(str));
    }
    /**
     * @brief Compares substring with character array of specified length
     * @param pos1 Starting position in this string
     * @param count1 Length of substring to compare
     * @param str Character array to compare with
     * @param count2 Length of character array
     * @return Negative value if substring < str, 0 if equal, positive if substring > str
     * @throws std::out_of_range if pos1 > size()
     * @note Handles overlapping ranges safely
     */
    [[nodiscard]] constexpr auto compare(size_type pos1, size_type count1, const Char* str, size_type count2) const
      -> int {
        // make sure the pos1 is valid
        if (pos1 > size()) [[unlikely]] {
            throw std::out_of_range("compare: pos1 is out of range");
        }
        // make sure the count1 is valid, if count1 > size() - pos1, set count1 = size() - pos1
        count1 = std::min(count1, static_cast<size_type>(size() - pos1));
        auto r = traits_type::compare(data() + pos1, str, std::min<size_t>(count1, count2));
        return r != 0 ? r : count1 > count2 ? 1 : count1 < count2 ? -1 : 0;
    }

    /**
     * @brief Compares this string with string_view-like object
     * @tparam StringViewLike Type convertible to string_view
     * @param view String view-like object to compare with
     * @return Negative value if this < view, 0 if equal, positive if this > view
     */
    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto compare(const StringViewLike& view) const noexcept -> int {
        auto this_size = size();
        auto view_size = view.size();
        auto r = traits_type::compare(data(), view.data(), std::min<size_t>(this_size, view_size));
        return r != 0 ? r : this_size > view_size ? 1 : this_size < view_size ? -1 : 0;
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const StringViewLike& view) const -> int {
        return compare(pos, count, view.data(), view.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto compare(size_type pos1, size_type count1, const StringViewLike& view, size_type pos2,
                                         size_type count2 = npos) const -> int {
        if (pos2 > view.size()) [[unlikely]] {
            throw std::out_of_range("compare: pos2 is out of range");
        }
        count2 = std::min<size_type>(count2, view.size() - pos2);
        return compare(pos1, count1, view.data() + pos2, count2);
    }

    /**
     * @brief Checks if string starts with given string view
     * @param view String view to check for at beginning
     * @return true if string starts with view, false otherwise
     * @note C++20-compatible starts_with functionality
     */
    [[nodiscard]] constexpr auto starts_with(std::basic_string_view<Char> view) const noexcept -> bool {
        return size() >= view.size() && compare(0, view.size(), view.data(), view.size()) == 0;
    }

    /**
     * @brief Checks if string starts with given character
     * @param ch Character to check for at beginning
     * @return true if string starts with ch, false otherwise
     */
    [[nodiscard]] constexpr auto starts_with(Char ch) const noexcept -> bool {
        return not empty() && traits_type::eq(front(), ch);
    }

    /**
     * @brief Checks if string starts with given C-string
     * @param str Null-terminated string to check for at beginning
     * @return true if string starts with str, false otherwise
     */
    [[nodiscard]] constexpr auto starts_with(const Char* str) const noexcept -> bool {
        auto len = traits_type::length(str);
        return size() >= len && compare(0, len, str) == 0;
    }

    /**
     * @brief Checks if the string ends with the specified string view.
     * @param view String view to check for at the end
     * @return true if the string ends with view, false otherwise
     */
    [[nodiscard]] constexpr auto ends_with(std::basic_string_view<Char> view) const noexcept -> bool {
        return size() >= view.size() && compare(size() - view.size(), view.size(), view.data(), view.size()) == 0;
    }

    /**
     * @brief Checks if the string ends with the specified character.
     * @param ch Character to check for at the end
     * @return true if the string ends with ch, false otherwise
     */
    [[nodiscard]] constexpr auto ends_with(Char ch) const noexcept -> bool {
        return not empty() && traits_type::eq(back(), ch);
    }

    /**
     * @brief Checks if the string ends with the specified null-terminated string.
     * @param str Null-terminated string to check for at the end
     * @return true if the string ends with str, false otherwise
     */
    [[nodiscard]] constexpr auto ends_with(const Char* str) const noexcept -> bool {
        auto len = traits_type::length(str);
        return size() >= len && compare(size() - len, len, str) == 0;
    }

    /**
     * @brief Checks if the string contains the specified string view.
     * @param view String view to search for
     * @return true if the string contains view, false otherwise
     */
    [[nodiscard]] constexpr auto contains(std::basic_string_view<Char> view) const noexcept -> bool {
        return find(view) != npos;
    }

    /**
     * @brief Checks if the string contains the specified character.
     * @param ch Character to search for
     * @return true if the string contains ch, false otherwise
     */
    [[nodiscard]] constexpr auto contains(Char ch) const noexcept -> bool { return find(ch) != npos; }

    /**
     * @brief Checks if the string contains the specified null-terminated string.
     * @param str Null-terminated string to search for
     * @return true if the string contains str, false otherwise
     */
    [[nodiscard]] constexpr auto contains(const Char* str) const noexcept -> bool { return find(str) != npos; }

    /**
     * @brief Returns a substring of this string (const lvalue reference version).
     * @param pos Starting position of the substring (default: 0)
     * @param count Maximum length of the substring (default: npos = to end of string)
     * @return A new string containing the substring
     * @throws std::out_of_range if pos > size()
     */
    [[nodiscard]] constexpr auto substr(size_type pos = 0, size_type count = npos) const& -> basic_small_string {
        auto current_size = this->size();
        if (pos > current_size) [[unlikely]] {
            throw std::out_of_range("substr: pos is out of range");
        }

        return basic_small_string{data() + pos, static_cast<size_type>(std::min<size_t>(count, current_size - pos)),
                                  get_allocator()};
    }

    /**
     * @brief Returns a substring of this string (rvalue reference version).
     * @param pos Starting position of the substring (default: 0)
     * @param count Maximum length of the substring (default: npos = to end of string)
     * @return A new string containing the substring (optimized for move semantics)
     * @throws std::out_of_range if pos > size()
     * @note This version modifies the current string and returns it by move
     */
    [[nodiscard]] constexpr auto substr(size_type pos = 0, size_type count = npos) && -> basic_small_string {
        auto current_size = this->size();
        if (pos > current_size) [[unlikely]] {
            throw std::out_of_range("substr: pos is out of range");
        }
        erase(0, pos);
        if (count < current_size - pos) {
            resize(count);
        }
        // or just return *this
        return std::move(*this);
    }

    // convert to std::basic_string_view, to support C++11 compatibility. and it's noexcept.
    // and small_string can be converted to std::basic_string_view implicity, so third party String can be converted
    // from small_string.
    [[nodiscard, gnu::always_inline]] inline operator std::basic_string_view<Char, Traits>() const noexcept {
        return buffer_type::get_string_view();
    }
};  // class basic_small_string

// input/output
/**
 * @brief Stream insertion operator for basic_small_string.
 * @tparam Char Character type
 * @tparam Buffer Buffer template used for memory management
 * @tparam Core Core template used for storage strategy
 * @tparam Traits Character traits type
 * @tparam Allocator Allocator type
 * @tparam NullTerminated Whether the string maintains null termination
 * @tparam Growth Growth factor for buffer expansion
 * @param os Output stream to write to
 * @param str String to write to the stream
 * @return Reference to the output stream for chaining operations
 */
/**
 * @brief Stream insertion operator for basic_small_string.
 *
 * Inserts the contents of a basic_small_string into an output stream.
 * This operator allows basic_small_string objects to be used with standard
 * C++ output streams like std::cout.
 *
 * @param os Output stream to write to
 * @param str String to be inserted into the stream
 * @return Reference to the output stream for chaining operations
 *
 * @complexity Linear in the length of the string
 * @note This function directly writes the string data using stream.write(),
 *       which is more efficient than character-by-character insertion
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<<(std::basic_ostream<Char, Traits>& os,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& str)
  -> std::basic_ostream<Char, Traits>& {
    return os << std::basic_string_view<Char, Traits>(str.data(), str.size());
}

/**
 * @brief Stream extraction operator for basic_small_string.
 * @tparam Char Character type
 * @tparam Buffer Buffer template used for memory management
 * @tparam Core Core template used for storage strategy
 * @tparam Traits Character traits type
 * @tparam Allocator Allocator type
 * @tparam NullTerminated Whether the string maintains null termination
 * @tparam Growth Growth factor for buffer expansion
 * @param is Input stream to read from
 * @param str String to store the extracted data
 * @return Reference to the input stream for chaining operations
 * @note Reads characters until whitespace is encountered or stream width limit is reached
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>>(std::basic_istream<Char, Traits>& is,
                       basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& str)
  -> std::basic_istream<Char, Traits>& {
    using _istream_type = std::basic_istream<
      typename basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>::value_type,
      typename basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>::traits_type>;
    typename _istream_type::sentry sentry(is);
    size_t extracted = 0;
    typename _istream_type::iostate err = _istream_type::goodbit;
    if (sentry) {
        int64_t n = is.width();
        if (n <= 0) {
            n = int64_t(str.max_size());
        }
        str.erase();
        for (auto got = is.rdbuf()->sgetc(); extracted != static_cast<size_t>(n); ++extracted) {
            if (got == Traits::eof()) {
                err |= _istream_type::eofbit;
                is.width(0);
                break;
            }
            if (std::isspace(static_cast<char>(got))) {
                break;
            }
            str.push_back(got);
            got = is.rdbuf()->snextc();
        }
    }
    if (!extracted) {
        err |= _istream_type::failbit;
    }
    if (err) {
        is.setstate(err);
    }
    is.width(0);  // Reset width after extraction
    return is;
}

/**
 * @name String concatenation operators
 * @{
 */

/**
 * @brief Concatenates two basic_small_string objects.
 *
 * Creates a new string by concatenating the left-hand string with the right-hand string.
 *
 * @param lhs Left-hand side string
 * @param rhs Right-hand side string
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the sum of the lengths of both strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = lhs;
    result.append(rhs);
    return result;
}

/**
 * @brief Concatenates a basic_small_string with a C-style string.
 *
 * Creates a new string by appending the null-terminated string rhs to lhs.
 *
 * @param lhs Left-hand side string
 * @param rhs Right-hand side null-terminated string
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the sum of the lengths of both strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const Char* rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = lhs;
    result.append(rhs);
    return result;
}

/**
 * @brief Concatenates a basic_small_string with a single character.
 *
 * Creates a new string by appending the character rhs to lhs.
 *
 * @param lhs Left-hand side string
 * @param rhs Right-hand side character
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the length of lhs, constant for appending the character
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      Char rhs) -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = lhs;
    result.push_back(rhs);
    return result;
}

/**
 * @brief Concatenates a C-style string with a basic_small_string.
 *
 * Creates a new string by prepending the null-terminated string lhs to rhs.
 *
 * @param lhs Left-hand side null-terminated string
 * @param rhs Right-hand side string
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the sum of the lengths of both strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const Char* lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(lhs, rhs.get_allocator()) + rhs;
}

/**
 * @brief Concatenates a single character with a basic_small_string.
 *
 * Creates a new string by prepending the character lhs to rhs.
 *
 * @param lhs Left-hand side character
 * @param rhs Right-hand side string
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the length of rhs, constant for prepending the character
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(Char lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(1, lhs, rhs.get_allocator()) + rhs;
}

/**
 * @brief Concatenates two basic_small_string rvalue references.
 *
 * Efficiently concatenates two temporary strings by moving from both operands.
 *
 * @param lhs Left-hand side string (moved from)
 * @param rhs Right-hand side string (moved from)
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the sum of the lengths of both strings, but more efficient
 *             due to move semantics avoiding unnecessary copies
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& lhs,
                      basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> result(std::move(lhs));
    result.append(std::move(rhs));
    return result;
}

/**
 * @brief Concatenates an rvalue basic_small_string with a const basic_small_string.
 *
 * Efficiently concatenates by moving from the left operand and copying the right.
 *
 * @param lhs Left-hand side string (moved from)
 * @param rhs Right-hand side string (copied)
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the sum of the lengths of both strings, optimized by
 *             moving from lhs to avoid one copy operation
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = std::move(lhs);
    result.append(rhs);
    return result;
}

/**
 * @brief Concatenates an rvalue basic_small_string with a C-style string.
 *
 * Efficiently concatenates by moving from the left operand and appending the string.
 *
 * @param lhs Left-hand side string (moved from)
 * @param rhs Right-hand side null-terminated string
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the sum of the lengths of both strings, optimized by
 *             moving from lhs to avoid copying
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& lhs,
                      const Char* rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = std::move(lhs);
    result.append(rhs);
    return result;
}

/**
 * @brief Concatenates an rvalue basic_small_string with a single character.
 *
 * Efficiently concatenates by moving from the left operand and appending the character.
 *
 * @param lhs Left-hand side string (moved from)
 * @param rhs Right-hand side character
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the length of lhs for the move, constant for appending the character
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& lhs, Char rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = std::move(lhs);
    result.push_back(rhs);
    return result;
}

/**
 * @brief Concatenates a const basic_small_string with an rvalue basic_small_string.
 *
 * Concatenates by copying the left operand and moving from the right.
 *
 * @param lhs Left-hand side string (copied)
 * @param rhs Right-hand side string (moved from)
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the sum of the lengths of both strings, with move optimization
 *             for rhs reducing one copy operation
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = lhs;
    result.append(std::move(rhs));
    return result;
}

/**
 * @brief Concatenates a C-style string with an rvalue basic_small_string.
 *
 * Creates a new string from the C-style string and concatenates with the moved string.
 *
 * @param lhs Left-hand side null-terminated string
 * @param rhs Right-hand side string (moved from)
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the sum of the lengths of both strings, with move optimization
 *             for rhs reducing one copy operation
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const Char* lhs,
                      basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(lhs, rhs.get_allocator()) +
           std::move(rhs);
}

/**
 * @brief Concatenates a single character with an rvalue basic_small_string.
 *
 * Creates a new string from the character and concatenates with the moved string.
 *
 * @param lhs Left-hand side character
 * @param rhs Right-hand side string (moved from)
 * @return A new string containing lhs followed by rhs
 *
 * @complexity Linear in the length of rhs, with move optimization reducing one copy operation
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(Char lhs, basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(1, lhs, rhs.get_allocator()) +
           std::move(rhs);
}

// comparison operators
/**
 * @brief Three-way comparison operator between two basic_small_string objects
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side basic_small_string
 * @return std::strong_ordering result of lexicographical comparison
 * @note Uses compare() method for efficient string comparison
 * @complexity Linear in the length of the shorter string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept
  -> std::strong_ordering {
    return lhs.compare(rhs) <=> 0;
}

/**
 * @name Equality comparison operators
 * @{
 */

/**
 * @brief Tests two basic_small_string objects for equality.
 *
 * Compares two strings for equality by first checking their sizes and then
 * comparing their contents character by character.
 *
 * @param lhs Left-hand side string
 * @param rhs Right-hand side string
 * @return true if both strings have the same size and content, false otherwise
 *
 * @complexity Linear in the length of the strings in the worst case, but optimized
 *             with early size check that runs in constant time
 * @note This function is noexcept and will not throw any exceptions
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief Tests two basic_small_string objects for inequality.
 *
 * Compares two strings for inequality. This is the logical negation of operator==.
 *
 * @param lhs Left-hand side string
 * @param rhs Right-hand side string
 * @return true if the strings differ in size or content, false if they are equal
 *
 * @complexity Same as operator==: Linear in the length of the strings in the worst case,
 *             with constant time size check optimization
 * @note This function is noexcept and will not throw any exceptions
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return not(lhs == rhs);
}

/**
 * @name Relational comparison operators
 * @{
 */

/**
 * @brief Tests if left string is lexicographically greater than right string.
 *
 * Performs lexicographical comparison using the compare() method.
 *
 * @param lhs Left-hand side string
 * @param rhs Right-hand side string
 * @return true if lhs is lexicographically greater than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and uses lexicographical ordering based on character traits
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.compare(rhs) > 0;
}

/**
 * @brief Tests if left string is lexicographically less than right string.
 *
 * Performs lexicographical comparison using the compare() method.
 *
 * @param lhs Left-hand side string
 * @param rhs Right-hand side string
 * @return true if lhs is lexicographically less than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and uses lexicographical ordering based on character traits
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

/**
 * @brief Tests if left string is lexicographically greater than or equal to right string.
 *
 * Performs lexicographical comparison by negating the less-than operator.
 *
 * @param lhs Left-hand side string
 * @param rhs Right-hand side string
 * @return true if lhs is lexicographically greater than or equal to rhs, false otherwise
 *
 * @complexity Same as operator<: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and is implemented as the logical negation of operator<
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return not(lhs < rhs);
}

/**
 * @brief Tests if left string is lexicographically less than or equal to right string.
 *
 * Performs lexicographical comparison by negating the greater-than operator.
 *
 * @param lhs Left-hand side string
 * @param rhs Right-hand side string
 * @return true if lhs is lexicographically less than or equal to rhs, false otherwise
 *
 * @complexity Same as operator>: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and is implemented as the logical negation of operator>
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return not(lhs > rhs);
}

/**
 * @}
 */

// basic_string compatibility routines
/**
 * @brief Three-way comparison operator between basic_small_string and std::basic_string
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::basic_string
 * @return std::strong_ordering result of lexicographical comparison
 * @note Uses compare() method for efficient string comparison
 * @complexity Linear in the length of the shorter string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<=>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                        const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) <=> 0;
}

/**
 * @brief Three-way comparison operator between std::basic_string and basic_small_string
 * @param lhs Left-hand side std::basic_string
 * @param rhs Right-hand side basic_small_string
 * @return std::strong_ordering result of lexicographical comparison
 * @note Uses compare() method for efficient string comparison
 * @complexity Linear in the length of the shorter string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<=>(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept
  -> std::strong_ordering {
    return 0 <=> rhs.compare(0, rhs.size(), lhs.data(), lhs.size());
}

/**
 * @brief Three-way comparison operator between basic_small_string and C-style string
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side C-style string (null-terminated)
 * @return std::strong_ordering result of lexicographical comparison
 * @note Uses compare() method and computes C-string length
 * @complexity Linear in the length of the shorter string (includes C-string length computation)
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                        const Char* rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs, Traits::length(rhs)) <=> 0;
}

/**
 * @brief Three-way comparison operator between C-style string and basic_small_string
 * @param lhs Left-hand side C-style string (null-terminated)
 * @param rhs Right-hand side basic_small_string
 * @return std::strong_ordering result of lexicographical comparison
 * @note Uses Traits::compare() and computes C-string length
 * @complexity Linear in the length of the shorter string (includes C-string length computation)
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept
  -> std::strong_ordering {
    auto rhs_size = rhs.size();
    auto lhs_size = Traits::length(lhs);
    auto r = Traits::compare(lhs, rhs.data(), std::min<decltype(rhs_size)>(lhs_size, rhs_size));
    return r != 0 ? r <=> 0 : lhs_size <=> rhs_size;
}

/**
 * @brief Three-way comparison operator between basic_small_string and std::string_view
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::string_view
 * @return std::strong_ordering result of lexicographical comparison
 * @note Uses compare() method for efficient string comparison
 * @complexity Linear in the length of the shorter string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                        std::string_view rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) <=> 0;
}

/**
 * @brief Three-way comparison operator between std::string_view and basic_small_string
 * @param lhs Left-hand side std::string_view
 * @param rhs Right-hand side basic_small_string
 * @return std::strong_ordering result of lexicographical comparison
 * @note Uses compare() method for efficient string comparison
 * @complexity Linear in the length of the shorter string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept
  -> std::strong_ordering {
    return 0 <=> rhs.compare(0, rhs.size(), lhs.data(), lhs.size());
}

/**
 * @brief Tests a basic_small_string and std::basic_string for equality.
 *
 * Compares a basic_small_string with a std::basic_string for equality.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::basic_string
 * @return true if both strings have the same size and content, false otherwise
 *
 * @complexity Linear in the length of the strings in the worst case, with constant time size check
 * @note This function is noexcept and provides interoperability with std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator==(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief Tests a std::basic_string and basic_small_string for equality.
 *
 * Compares a std::basic_string with a basic_small_string for equality.
 *
 * @param lhs Left-hand side std::basic_string
 * @param rhs Right-hand side basic_small_string
 * @return true if both strings have the same size and content, false otherwise
 *
 * @complexity Linear in the length of the strings in the worst case, with constant time size check
 * @note This function is noexcept and provides interoperability with std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator==(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief Tests a C-style string and basic_small_string for equality.
 *
 * Compares a null-terminated C-style string with a basic_small_string for equality.
 *
 * @param lhs Left-hand side null-terminated string
 * @param rhs Right-hand side basic_small_string
 * @return true if the strings have the same length and content, false otherwise
 *
 * @complexity Linear in the length of the strings (requires computing C-string length)
 * @note This function is noexcept and provides interoperability with C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.size() == Traits::length(lhs) and std::equal(rhs.begin(), rhs.end(), lhs);
}

/**
 * @brief Tests a basic_small_string and C-style string for equality.
 *
 * Compares a basic_small_string with a null-terminated C-style string for equality.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side null-terminated string
 * @return true if the strings have the same length and content, false otherwise
 *
 * @complexity Linear in the length of the strings (requires computing C-string length)
 * @note This function is noexcept and provides interoperability with C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const Char* rhs) noexcept -> bool {
    return lhs.size() == Traits::length(rhs) and std::equal(lhs.begin(), lhs.end(), rhs);
}

/**
 * @brief Tests a basic_small_string and std::string_view for equality.
 *
 * Compares a basic_small_string with a std::string_view for equality.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::string_view
 * @return true if both strings have the same size and content, false otherwise
 *
 * @complexity Linear in the length of the strings in the worst case, with constant time size check
 * @note This function is noexcept and provides interoperability with std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       std::string_view rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief Tests a std::string_view and basic_small_string for equality.
 *
 * Compares a std::string_view with a basic_small_string for equality.
 *
 * @param lhs Left-hand side std::string_view
 * @param rhs Right-hand side basic_small_string
 * @return true if both strings have the same size and content, false otherwise
 *
 * @complexity Linear in the length of the strings in the worst case, with constant time size check
 * @note This function is noexcept and provides interoperability with std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief Tests a basic_small_string and std::basic_string for inequality.
 *
 * Compares a basic_small_string with a std::basic_string for inequality.
 * This is the logical negation of the corresponding operator==.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::basic_string
 * @return true if the strings differ in size or content, false if they are equal
 *
 * @complexity Same as operator==: Linear in the length of strings, with size check optimization
 * @note This function is noexcept and provides interoperability with std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator!=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

/**
 * @brief Tests a std::basic_string and basic_small_string for inequality.
 *
 * Compares a std::basic_string with a basic_small_string for inequality.
 * This is the logical negation of the corresponding operator==.
 *
 * @param lhs Left-hand side std::basic_string
 * @param rhs Right-hand side basic_small_string
 * @return true if the strings differ in size or content, false if they are equal
 *
 * @complexity Same as operator==: Linear in the length of strings, with size check optimization
 * @note This function is noexcept and provides interoperability with std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator!=(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

/**
 * @brief Tests a basic_small_string and C-style string for inequality.
 *
 * Compares a basic_small_string with a null-terminated C-style string for inequality.
 * This is the logical negation of the corresponding operator==.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side null-terminated string
 * @return true if the strings differ in length or content, false if they are equal
 *
 * @complexity Same as operator==: Linear in the length of strings (requires C-string length computation)
 * @note This function is noexcept and provides interoperability with C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const Char* rhs) noexcept -> bool {
    return !(lhs == rhs);
}

/**
 * @brief Tests a C-style string and basic_small_string for inequality.
 *
 * Compares a null-terminated C-style string with a basic_small_string for inequality.
 * This is the logical negation of the corresponding operator==.
 *
 * @param lhs Left-hand side null-terminated string
 * @param rhs Right-hand side basic_small_string
 * @return true if the strings differ in length or content, false if they are equal
 *
 * @complexity Same as operator==: Linear in the length of strings (requires C-string length computation)
 * @note This function is noexcept and provides interoperability with C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

/**
 * @brief Tests a basic_small_string and std::string_view for inequality.
 *
 * Compares a basic_small_string with a std::string_view for inequality.
 * This is the logical negation of the corresponding operator==.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::string_view
 * @return true if the strings differ in size or content, false if they are equal
 *
 * @complexity Same as operator==: Linear in the length of strings, with size check optimization
 * @note This function is noexcept and provides interoperability with std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       std::string_view rhs) noexcept -> bool {
    return !(lhs == rhs);
}

/**
 * @brief Tests a std::string_view and basic_small_string for inequality.
 *
 * Compares a std::string_view with a basic_small_string for inequality.
 * This is the logical negation of the corresponding operator==.
 *
 * @param lhs Left-hand side std::string_view
 * @param rhs Right-hand side basic_small_string
 * @return true if the strings differ in size or content, false if they are equal
 *
 * @complexity Same as operator==: Linear in the length of strings, with size check optimization
 * @note This function is noexcept and provides interoperability with std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

/**
 * @brief Tests if a basic_small_string is lexicographically less than a std::basic_string.
 *
 * Performs lexicographical comparison using the compare() method.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::basic_string
 * @return true if lhs is lexicographically less than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and provides interoperability with std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

/**
 * @brief Tests if a std::basic_string is lexicographically less than a basic_small_string.
 *
 * Performs lexicographical comparison using the basic_small_string's compare() method.
 *
 * @param lhs Left-hand side std::basic_string
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically less than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and provides interoperability with std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) > 0;
}

/**
 * @brief Tests if a basic_small_string is lexicographically less than a C-style string.
 *
 * Performs lexicographical comparison using the compare() method.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side null-terminated string
 * @return true if lhs is lexicographically less than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string (requires computing C-string length)
 * @note This function is noexcept and provides interoperability with C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const Char* rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

/**
 * @brief Tests if a C-style string is lexicographically less than a basic_small_string.
 *
 * Performs lexicographical comparison using the basic_small_string's compare() method.
 *
 * @param lhs Left-hand side null-terminated string
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically less than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string (requires computing C-string length)
 * @note This function is noexcept and provides interoperability with C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) > 0;
}

/**
 * @brief Tests if a basic_small_string is lexicographically less than a std::string_view.
 *
 * Performs lexicographical comparison using the compare() method.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::string_view
 * @return true if lhs is lexicographically less than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and provides interoperability with std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      std::string_view rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

/**
 * @brief Tests if a std::string_view is lexicographically less than a basic_small_string.
 *
 * Performs lexicographical comparison using the basic_small_string's compare() method.
 *
 * @param lhs Left-hand side std::string_view
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically less than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and provides interoperability with std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) > 0;
}

/**
 * @brief Tests if a std::basic_string is lexicographically greater than a basic_small_string.
 *
 * Performs lexicographical comparison using the basic_small_string's compare() method.
 *
 * @param lhs Left-hand side std::basic_string
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically greater than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and provides interoperability with std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator>(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) < 0;
}

/**
 * @brief Tests if a C-style string is lexicographically greater than a basic_small_string.
 *
 * Performs lexicographical comparison using the basic_small_string's compare() method.
 *
 * @param lhs Left-hand side null-terminated string
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically greater than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string (requires computing C-string length)
 * @note This function is noexcept and provides interoperability with C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) < 0;
}

/**
 * @brief Tests if a basic_small_string is lexicographically greater than a C-style string.
 *
 * Performs lexicographical comparison using the compare() method.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side null-terminated string
 * @return true if lhs is lexicographically greater than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string (requires computing C-string length)
 * @note This function is noexcept and provides interoperability with C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const Char* rhs) noexcept -> bool {
    return lhs.compare(rhs) > 0;
}

/**
 * @brief Tests if a basic_small_string is lexicographically greater than a std::string_view.
 *
 * Performs lexicographical comparison using the compare() method.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::string_view
 * @return true if lhs is lexicographically greater than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and provides interoperability with std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      std::string_view rhs) noexcept -> bool {
    return lhs.compare(rhs) > 0;
}

/**
 * @brief Tests if a std::string_view is lexicographically greater than a basic_small_string.
 *
 * Performs lexicographical comparison using the basic_small_string's compare() method.
 *
 * @param lhs Left-hand side std::string_view
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically greater than rhs, false otherwise
 *
 * @complexity Linear in the length of the shorter string in the worst case
 * @note This function is noexcept and provides interoperability with std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) < 0;
}

/**
 * @brief Tests if a basic_small_string is lexicographically less than or equal to a std::basic_string.
 *
 * Performs lexicographical comparison by negating the greater-than operator.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::basic_string
 * @return true if lhs is lexicographically less than or equal to rhs, false otherwise
 *
 * @complexity Same as operator>: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept, implemented as logical negation of operator>, and provides interoperability with
 * std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

/**
 * @brief Tests if a std::basic_string is lexicographically less than or equal to a basic_small_string.
 *
 * Performs lexicographical comparison by negating the greater-than operator.
 *
 * @param lhs Left-hand side std::basic_string
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically less than or equal to rhs, false otherwise
 *
 * @complexity Same as operator>: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept, implemented as logical negation of operator>, and provides interoperability with
 * std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<=(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

/**
 * @brief Tests if a basic_small_string is lexicographically less than or equal to a C-style string.
 *
 * Performs lexicographical comparison by negating the greater-than operator.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side null-terminated string
 * @return true if lhs is lexicographically less than or equal to rhs, false otherwise
 *
 * @complexity Same as operator>: Linear in the length of the shorter string (requires computing C-string length)
 * @note This function is noexcept, implemented as logical negation of operator>, and provides interoperability with
 * C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const Char* rhs) noexcept -> bool {
    return !(lhs > rhs);
}

/**
 * @brief Tests if a C-style string is lexicographically less than or equal to a basic_small_string.
 *
 * Performs lexicographical comparison by negating the greater-than operator.
 *
 * @param lhs Left-hand side null-terminated string
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically less than or equal to rhs, false otherwise
 *
 * @complexity Same as operator>: Linear in the length of the shorter string (requires computing C-string length)
 * @note This function is noexcept, implemented as logical negation of operator>, and provides interoperability with
 * C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

/**
 * @brief Tests if a basic_small_string is lexicographically less than or equal to a std::string_view.
 *
 * Performs lexicographical comparison by negating the greater-than operator.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::string_view
 * @return true if lhs is lexicographically less than or equal to rhs, false otherwise
 *
 * @complexity Same as operator>: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept, implemented as logical negation of operator>, and provides interoperability with
 * std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       std::string_view rhs) noexcept -> bool {
    return !(lhs > rhs);
}

/**
 * @brief Tests if a std::string_view is lexicographically less than or equal to a basic_small_string.
 *
 * Performs lexicographical comparison by negating the greater-than operator.
 *
 * @param lhs Left-hand side std::string_view
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically less than or equal to rhs, false otherwise
 *
 * @complexity Same as operator>: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept, implemented as logical negation of operator>, and provides interoperability with
 * std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

/**
 * @brief Tests if a basic_small_string is lexicographically greater than or equal to a std::basic_string.
 *
 * Performs lexicographical comparison by negating the less-than operator.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::basic_string
 * @return true if lhs is lexicographically greater than or equal to rhs, false otherwise
 *
 * @complexity Same as operator<: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept, implemented as logical negation of operator<, and provides interoperability with
 * std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator>=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

/**
 * @brief Tests if a std::basic_string is lexicographically greater than or equal to a basic_small_string.
 *
 * Performs lexicographical comparison by negating the less-than operator.
 *
 * @param lhs Left-hand side std::basic_string
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically greater than or equal to rhs, false otherwise
 *
 * @complexity Same as operator<: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept, implemented as logical negation of operator<, and provides interoperability with
 * std::basic_string
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator>=(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

/**
 * @brief Tests if a basic_small_string is lexicographically greater than or equal to a C-style string.
 *
 * Performs lexicographical comparison by negating the less-than operator.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side null-terminated string
 * @return true if lhs is lexicographically greater than or equal to rhs, false otherwise
 *
 * @complexity Same as operator<: Linear in the length of the shorter string (requires computing C-string length)
 * @note This function is noexcept, implemented as logical negation of operator<, and provides interoperability with
 * C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const Char* rhs) noexcept -> bool {
    return !(lhs < rhs);
}

/**
 * @brief Tests if a C-style string is lexicographically greater than or equal to a basic_small_string.
 *
 * Performs lexicographical comparison by negating the less-than operator.
 *
 * @param lhs Left-hand side null-terminated string
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically greater than or equal to rhs, false otherwise
 *
 * @complexity Same as operator<: Linear in the length of the shorter string (requires computing C-string length)
 * @note This function is noexcept, implemented as logical negation of operator<, and provides interoperability with
 * C-style strings
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

/**
 * @brief Tests if a basic_small_string is lexicographically greater than or equal to a std::string_view.
 *
 * Performs lexicographical comparison by negating the less-than operator.
 *
 * @param lhs Left-hand side basic_small_string
 * @param rhs Right-hand side std::string_view
 * @return true if lhs is lexicographically greater than or equal to rhs, false otherwise
 *
 * @complexity Same as operator<: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept, implemented as logical negation of operator<, and provides interoperability with
 * std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       std::string_view rhs) noexcept -> bool {
    return !(lhs < rhs);
}

/**
 * @brief Tests if a std::string_view is lexicographically greater than or equal to a basic_small_string.
 *
 * Performs lexicographical comparison by negating the less-than operator.
 *
 * @param lhs Left-hand side std::string_view
 * @param rhs Right-hand side basic_small_string
 * @return true if lhs is lexicographically greater than or equal to rhs, false otherwise
 *
 * @complexity Same as operator<: Linear in the length of the shorter string in the worst case
 * @note This function is noexcept, implemented as logical negation of operator<, and provides interoperability with
 * std::string_view
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

using small_string = basic_small_string<char>;
using small_byte_string =
  basic_small_string<char, small_string_buffer, malloc_core, std::char_traits<char>, std::allocator<char>, false>;

static_assert(sizeof(small_string) == 8, "small_string should be same as a pointer");
static_assert(sizeof(small_byte_string) == 8, "small_byte_string should be same as a pointer");

/**
 * @brief Converts a value to a small string using fmt::format.
 *
 * This function template provides a generic way to convert any formattable value
 * to a small string type using fmt::format functionality.
 *
 * @tparam String The target small string type to create
 * @tparam T The type of value to convert (must be formattable)
 * @param value The value to convert to string
 * @return A String object containing the formatted representation of value
 *
 * @complexity Linear in the formatted size of the value
 * @note Requires fmt::format support
 * @note Uses fmt::formatted_size for efficient memory allocation
 */
template <typename String, typename T>
auto to_small_string(T value) -> String {
    auto size = fmt::formatted_size("{}", value);
    auto formatted = String::create_uninitialized_string(size);
    fmt::format_to(formatted.data(), "{}", value);
    return formatted;
}

/**
 * @brief Converts a C-style string to a small string.
 *
 * Creates a small string from a null-terminated C-style string.
 *
 * @tparam String The target small string type to create
 * @param value Null-terminated C-style string to convert
 * @return A String object containing a copy of the C-style string
 *
 * @complexity Linear in the length of the C-style string
 * @note The input string must be null-terminated
 */
template <typename String>
auto to_small_string(const char* value) -> String {
    return String{value};
}

/**
 * @brief Converts a std::string to a small string.
 *
 * Creates a small string from a std::string, copying its contents.
 *
 * @tparam String The target small string type to create
 * @param value The std::string to convert
 * @return A String object containing a copy of the std::string's contents
 *
 * @complexity Linear in the length of the std::string
 * @note Provides interoperability between std::string and small string types
 */
template <typename String>
auto to_small_string(const std::string& value) -> String {
    return String{value};
}

/**
 * @brief Converts a std::string_view to a small string.
 *
 * Creates a small string from a std::string_view, copying the viewed data.
 *
 * @tparam String The target small string type to create
 * @param view The std::string_view to convert
 * @return A String object containing a copy of the string_view's data
 *
 * @complexity Linear in the length of the string_view
 * @note Provides efficient conversion from string views to small strings
 * @note The resulting string owns its data independently of the original view
 */
template <typename String>
auto to_small_string(std::string_view view) -> String {
    return String{view};
}

}  // namespace small

// decl the formatter of small_string

/**
 * @brief fmt::format specialization for basic_small_string
 * @note Enables use of basic_small_string with fmt::format, etc.
 * @note Inherits formatting behavior from std::string_view formatter
 * @note Provides zero-copy formatting by creating string_view from string data
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
struct fmt::formatter<small::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>>
    : fmt::formatter<std::string_view>
{
    using fmt::formatter<std::string_view>::parse;

    auto format(const small::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& str,
                fmt::format_context& ctx) const noexcept {
        return fmt::formatter<std::string_view>::format({str.data(), str.size()}, ctx);
    }
};

namespace small::pmr {
using small_string = basic_small_string<char, small_string_buffer, pmr_core, std::char_traits<char>,
                                        std::pmr::polymorphic_allocator<char>, true>;
using small_byte_string = basic_small_string<char, small_string_buffer, pmr_core, std::char_traits<char>,
                                             std::pmr::polymorphic_allocator<char>, false>;

static_assert(sizeof(small_string) == 16, "small_string should be same as a pointer");

/**
 * @brief Converts a value to a PMR small string using fmt::format.
 *
 * This function template provides a generic way to convert any formattable value
 * to a PMR-based small string type using fmt::format functionality and
 * a custom polymorphic allocator.
 *
 * @tparam String The target PMR small string type to create
 * @tparam T The type of value to convert (must be formattable)
 * @param value The value to convert to string
 * @param allocator The polymorphic allocator to use for memory allocation
 * @return A String object containing the formatted representation of value
 *
 * @complexity Linear in the formatted size of the value
 * @note Requires fmt::format support and C++17 PMR
 * @note Uses fmt::formatted_size for efficient memory allocation
 * @note Memory is allocated using the provided polymorphic allocator
 */
template <typename String, typename T>
auto to_small_string(T value, std::pmr::polymorphic_allocator<char> allocator) -> String {
    auto size = fmt::formatted_size("{}", value);
    auto formatted = String::create_uninitialized_string(size, allocator);
    fmt::format_to(formatted.data(), "{}", value);
    return formatted;
}

/**
 * @brief Converts a C-style string to a PMR small string.
 *
 * Creates a PMR-based small string from a null-terminated C-style string
 * using the specified polymorphic allocator.
 *
 * @tparam String The target PMR small string type to create
 * @param value Null-terminated C-style string to convert
 * @param allocator The polymorphic allocator to use for memory allocation
 * @return A String object containing a copy of the C-style string
 *
 * @complexity Linear in the length of the C-style string
 * @note The input string must be null-terminated
 * @note Memory is allocated using the provided polymorphic allocator
 */
template <typename String>
auto to_small_string(const char* value, std::pmr::polymorphic_allocator<char> allocator) -> String {
    return String{value, static_cast<typename String::size_type>(std::strlen(value)), allocator};
}

/**
 * @brief Converts a std::string to a PMR small string.
 *
 * Creates a PMR-based small string from a std::string using the specified
 * polymorphic allocator, copying the string's contents.
 *
 * @tparam String The target PMR small string type to create
 * @param value The std::string to convert
 * @param allocator The polymorphic allocator to use for memory allocation
 * @return A String object containing a copy of the std::string's contents
 *
 * @complexity Linear in the length of the std::string
 * @note Provides interoperability between std::string and PMR small string types
 * @note Memory is allocated using the provided polymorphic allocator
 */
template <typename String>
auto to_small_string(const std::string& value, std::pmr::polymorphic_allocator<char> allocator) -> String {
    return String{value, allocator};
}

/**
 * @brief Converts a std::string_view to a PMR small string.
 *
 * Creates a PMR-based small string from a std::string_view using the specified
 * polymorphic allocator, copying the viewed data.
 *
 * @tparam String The target PMR small string type to create
 * @param view The std::string_view to convert
 * @param allocator The polymorphic allocator to use for memory allocation
 * @return A String object containing a copy of the string_view's data
 *
 * @complexity Linear in the length of the string_view
 * @note Provides efficient conversion from string views to PMR small strings
 * @note The resulting string owns its data independently of the original view
 * @note Memory is allocated using the provided polymorphic allocator
 */
template <typename String>
auto to_small_string(std::string_view view, std::pmr::polymorphic_allocator<char> allocator) -> String {
    return String{view, allocator};
}

}  // namespace small::pmr

namespace std {

/**
 * @brief std::hash specialization for basic_small_string
 * @note Enables use of basic_small_string as key in std::unordered_map, std::unordered_set
 * @note Uses same hash algorithm as std::string_view for consistency
 * @note Provides efficient hashing by avoiding string copying
 */
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
struct hash<small::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>>
{
    /// Type aliases for hash functor compatibility
    using argument_type =
      small::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>;  ///< Input string type
    using result_type = std::size_t;                                                             ///< Hash result type

    auto operator()(const argument_type& str) const noexcept -> result_type {
        return std::hash<std::basic_string_view<Char, Traits>>{}(str);
    }
};

}  // namespace std
