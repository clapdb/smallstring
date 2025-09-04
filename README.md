# SmallString: Memory-Efficient C++20 String Library

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Header Only](https://img.shields.io/badge/Header-Only-green.svg)](https://github.com/nothings/stb)

A memory-efficient C++20 header-only string library optimized for minimal memory footprint and fast move operations. SmallString trades some runtime performance for significant memory savings, making it ideal for memory-constrained applications.

## 🎯 Key Advantages

### 💾 Minimal Memory Footprint
- **8-byte object size** (vs ~32 bytes for `std::string`)
- **16 bytes with PMR support** (still smaller than standard strings)
- **4x-8x memory reduction** for collections of string objects
- **Cache-friendly** due to compact size

### ⚡ Optimized Move Semantics
- **Fast ownership transfer** - essential for modern C++ practices
- **Move-optimized for each storage type** (Internal/Short/Median/Long)
- **Efficient container operations** when transferring string collections
- **Zero-cost moves** for internal storage strings
- **Perfect for modern C++ idioms** (RAII, move-only types, etc.)

### 🏗️ Smart Storage Strategy
SmallString automatically chooses optimal storage based on string size, providing seamless performance across different string lengths without requiring developer intervention.

## ⚖️ Performance Trade-offs

**SmallString is slower than `std::string` for most operations**, but provides:

✅ **Advantages:**
- Significantly smaller memory footprint
- **Faster move operations** - critical in modern C++ (ownership transfer)
- **Faster empty initialization** - optimized for containers
- Better cache locality in collections
- Lower memory allocation overhead
- **Optimized for move semantics** - aligns with modern C++ best practices

❌ **Trade-offs:**
- Slower string operations (concatenation, search, etc.)
- More complex internal logic
- Additional indirection for some operations
- **Maximum string size limited to ~4.3GB** (uses `uint32_t` for size)

## 📦 When to Use SmallString

### ✅ Good Use Cases
```cpp
// Hash maps - critical memory savings due to empty slot overhead
std::unordered_map<small::small_string, ValueType> lookup_table;
// Hash maps waste ~20% slots as empty - SmallString reduces this waste significantly
// std::string: ~32 bytes per slot (including empty ones)  
// small_string: ~8 bytes per slot = 4x memory reduction

// Large string collections in data processing
std::vector<small::small_string> tokens(1000000);  // Saves ~24MB vs std::string

// String interning/caching systems
std::unordered_set<small::small_string> string_pool;  // Memory-efficient string deduplication

// Database-like applications with many string keys
std::map<small::small_string, RecordType> database_index;  // Tree nodes use less memory

// Memory-constrained environments
// Embedded systems, memory pools, real-time systems
```

### ❌ Avoid When
```cpp
// Performance-critical string processing
// Heavy concatenation, searching, parsing
// When string operation speed is more important than memory
```

## 🚀 Quick Start

```cpp
#include "smallstring.hpp"
#include <iostream>

int main() {
    // Same API as std::string
    small::small_string greeting = "Hello";
    small::small_string name = "World";
    
    // Full compatibility
    greeting += " " + name + "!";
    std::cout << greeting << std::endl;  // "Hello World!"
    
    // Memory efficient collections
    std::vector<small::small_string> words = {"small", "memory", "footprint"};
    // Uses ~4x less memory than std::vector<std::string>
    
    return 0;
}
```

## 💡 Usage Examples

### Memory-Efficient Collections

```cpp
// Before: ~32 bytes per std::string object
std::vector<std::string> large_collection(100000);

// After: ~8 bytes per small_string object  
std::vector<small::small_string> compact_collection(100000);
// Saves ~2.4MB of memory!
```

### Modern C++ Move Semantics

```cpp
// Modern C++ emphasizes move over copy for ownership transfer
std::vector<small::small_string> source_data = load_strings();

// Move entire collection - SmallString optimized for this
auto moved_data = std::move(source_data);  // Fast ownership transfer

// Move assignment - where SmallString's optimization shines
small::small_string source = get_large_string();
small::small_string dest;
dest = std::move(source);  // Fast move - SmallString optimized per storage type

// Move in containers - frequent in modern C++
std::vector<small::small_string> strings;
small::small_string temp = build_string();
strings.push_back(std::move(temp));  // Move into container

// Perfect forwarding with moves
template<typename T>
void store_string(T&& str) {
    strings.emplace_back(std::forward<T>(str));  // Move when possible
}
```

### PMR for Custom Memory Management

```cpp
#include <memory_resource>

std::pmr::monotonic_buffer_resource pool(4096);
small::pmr::small_string managed_string(&pool);
managed_string = "Uses custom memory pool";
```

## 🏗️ Building

Header-only library - just include:

```cpp
#include "smallstring.hpp"
```

### Build Tests
```bash
mkdir build.debug
cd build.debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
ninja unit_tests  # Unit testing
ninja reg_test    # Regression testing (borrowed from Folly)
```

## 📊 Memory Comparison

| String Type | Object Size | 1M Objects | Use Case |
|-------------|-------------|------------|----------|
| `std::string` | ~32 bytes | ~32MB | General purpose |
| `small::small_string` | 8 bytes | ~8MB | Memory constrained |
| `small::pmr::small_string` | 16 bytes | ~16MB | Custom allocation |

For detailed performance benchmarks, see [bench/README.md](bench/README.md).

## 🛠️ API Reference

### Core Types

```cpp
// Standard version (8 bytes)
using small::small_string = basic_small_string<char>;

// PMR version (16 bytes) 
using small::pmr::small_string = basic_small_string<char, ..., std::pmr::polymorphic_allocator<char>>;

// Binary data version (no null termination)
using small::small_byte_string = basic_small_string<char, ..., false>;
```

### Additional Methods

```cpp
// Check storage type (for debugging/optimization)
uint8_t storage_type = str.get_core_type();

// Full std::string compatibility
std::string_view view = str;  // Implicit conversion
```

## 💼 Real-World Applications

### Configuration Management
```cpp
// Config files with many small keys/values
std::unordered_map<small::small_string, small::small_string> settings;
// Significant memory savings over std::string version
```

### Token Processing
```cpp
// Large collections of tokens/identifiers
std::vector<small::small_string> identifiers = parse_tokens(source_code);
// Much smaller memory footprint
```

### Embedded Systems
```cpp
// Memory-constrained environments where every byte counts
// SmallString provides std::string compatibility with minimal overhead
```

## 📋 Requirements

- **C++20** compatible compiler (GCC 10+, Clang 12+, MSVC 2022+)
- **Standard Library**: Standard headers only
- **Platform**: Cross-platform (Linux, Windows, macOS)

## 📝 License

Licensed under the Apache License, Version 2.0. See [LICENSE.txt](LICENSE.txt) for details.

## 🤝 Contributing

Contributions welcome! Please ensure:
- Code follows existing conventions
- Tests pass: `ninja unit_tests reg_test`
- New features include tests
- Documentation updated

---

**SmallString**: When memory efficiency matters more than speed. 🏗️