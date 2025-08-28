# String Performance Benchmarks

This directory contains comprehensive benchmarks comparing performance between:
- `std::string` (standard library)
- `small::small_string` (null-terminated small string)
- `small::small_byte_string` (non-null-terminated small string)

## Building and Running

```bash
# From the build directory
cmake --build . --target string_benchmark
./benchmark/string_benchmark
```

Or using the convenience target:
```bash
make run_benchmark
```

## Benchmark Categories

### 1. Construction Benchmarks
- **Empty Construction**: Creating empty strings
- **Literal Construction**: Small string literals (4 chars)
- **Long String Construction**: Longer strings (55 chars)
- **Fill Construction**: Strings filled with repeated characters (100 chars)

### 2. Append Operations
- **Character Append**: Adding single characters repeatedly
- **String Append**: Concatenating string segments

### 3. Copy Operations
- **Short String Copy**: Copying small strings (≤7 chars, SSO range)
- **Long String Copy**: Copying larger strings requiring heap allocation

### 4. Move Operations (SSO Advantage)
- **Move Constructor (SSO)**: Moving small strings (4 chars, internal storage)
- **Move Constructor (Short)**: Moving medium strings (30 chars, short buffer)
- **Move Constructor (External)**: Moving large strings (135 chars, heap allocated)
- **Move Assignment**: Move assignment operator performance
- **Vector Push Back Moves**: Bulk move operations into containers
- **String Swap**: Swapping string contents efficiently
- **Function Return Move**: Move semantics in function returns

### 4. Search Operations
- **Substring Find**: Finding substrings within text
- **Character Find**: Finding individual characters

### 5. Insert/Erase Operations
- **Insert String**: Inserting strings at arbitrary positions
- **Erase Substring**: Removing substrings from arbitrary positions

### 6. Memory Efficiency
- **Object Size Comparison**: Memory footprint of different string types
- **Many Small Strings**: Performance when creating many small string objects

## Key Performance Insights

### Memory Footprint
- `std::string`: 32 bytes per object
- `small::small_string`: 8 bytes per object (4x smaller)
- `small::small_byte_string`: 8 bytes per object (4x smaller)

### Performance Characteristics

**Empty/Small String Construction (≤4 characters)**:
- Small strings: **5-7x faster** empty construction, **1.7-1.9x faster** literal construction
- Excellent for configuration keys, enums, and short identifiers

**Move Operations - Small String Advantages**:
- **Move Constructor (SSO)**: **1.2x faster** for small strings due to compact 8-byte structure
- **Function Return Move**: **1.4x faster** - compact objects move more efficiently
- **Move Assignment**: **1.06x faster** for medium strings
- **Vector Operations**: std::string faster for bulk moves (1.8x) due to optimized implementations
- **String Swaps**: std::string much faster (2.5x) - specialized swap optimizations

**Large String Operations (>30 characters)**:
- Performance generally comparable to std::string
- Some operations slightly slower due to buffer type determination overhead
- Long string copies: small strings slightly faster (1.06-1.08x)

**String Manipulation Trade-offs**:
- **Append Operations**: std::string **1.4-1.5x faster** for frequent appends
- **Insert Operations**: std::string **1.7-1.8x faster** for string insertions
- **Search Operations**: Nearly identical performance (within 3-8%)
- **Erase Operations**: Comparable performance

**Memory Efficiency**:
- **Object Size**: Small strings use **4x less memory** (8 vs 32 bytes)
- **Cache Performance**: Better cache locality due to smaller object size
- **Container Storage**: Significant memory savings when storing many string objects

**Trade-offs Summary**:
- **Pros**: 4x memory savings, faster construction/moves for small strings, better cache locality
- **Cons**: Slower bulk operations, string manipulation, and specialized optimizations like swap

## Implementation Details

The benchmarks use:
- 100,000 iterations per operation for statistical significance
- 1,000 warmup iterations to stabilize CPU caches
- High-resolution timing with nanosecond precision
- Compiler optimizations (-O3) with LTO disabled to prevent over-optimization
- Various string sizes to test different code paths

## Use Cases Where Small Strings Excel

1. **Configuration keys/values**: Often short strings
2. **Database field names**: Typically under 20 characters
3. **File paths components**: Individual directory/file names
4. **Network protocol headers**: Short field names and values
5. **JSON object keys**: Usually short identifiers
6. **Log levels and categories**: Short enum-like strings

The 4x reduction in memory usage can lead to significant improvements in cache performance and overall memory consumption for applications dealing with many small string objects.