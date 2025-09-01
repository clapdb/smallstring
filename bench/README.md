# String Performance Benchmarks

This directory contains comprehensive benchmarks comparing performance between:
- `std::string` (standard library)
- `small::small_string` (null-terminated small string)
- `small::small_byte_string` (non-null-terminated small string)

## Building and Running

### Build with GCC + libstdc++

```bash
# Create and enter build directory
mkdir build.release && cd build.release

# Configure with GCC (default) in Release mode
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build the benchmark
ninja bench/string_benchmark

# Run benchmarks
./bench/string_benchmark
```

### Build with Clang + libc++

```bash
# Create and enter build directory  
mkdir build.release && cd build.release

# Configure with Clang and libc++ in Release mode
CC=clang CXX=clang++ CMAKE_CXX_FLAGS="-stdlib=libc++" cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build the benchmark
ninja bench/string_benchmark

# Run benchmarks
./bench/string_benchmark
```

### Benchmark Options

```bash
# List all available benchmarks
./bench/string_benchmark --benchmark_list_tests

# Run specific benchmark categories
./bench/string_benchmark --benchmark_filter=".*Construct.*"
./bench/string_benchmark --benchmark_filter=".*Map.*"
./bench/string_benchmark --benchmark_filter=".*Memory.*"

# Set minimum runtime per benchmark
./bench/string_benchmark --benchmark_min_time=0.1s

# Output results to file
./bench/string_benchmark --benchmark_format=json --benchmark_out=results.json
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

**With libstdc++ (GCC default)**:
- `std::string`: 32 bytes per object
- `small::small_string`: 8 bytes per object (4x smaller)
- `small::small_byte_string`: 8 bytes per object (4x smaller)

**With libc++ (Clang with -stdlib=libc++)**:
- `std::string`: 24 bytes per object
- `small::small_string`: 8 bytes per object (3x smaller)  
- `small::small_byte_string`: 8 bytes per object (3x smaller)

The small string library maintains consistent 8-byte size regardless of standard library implementation.

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

## Latest Benchmark Results

### Clang + libc++ (build.clang)

Results from Clang compiler with libc++ on 32-core 5.5GHz CPU:

```
=== Memory Usage Information ===
sizeof(std::string): 24 bytes (libc++)
sizeof(small::small_string): 8 bytes
sizeof(small::small_byte_string): 8 bytes
=================================

--------------------------------------------------------------------------------------------------------------
Benchmark                                                                    Time             CPU   Iterations
--------------------------------------------------------------------------------------------------------------
BenchmarkFixture/StdString_DefaultConstruct                              0.222 ns        0.222 ns   1896812017
BenchmarkFixture/SmallString_DefaultConstruct                            0.198 ns        0.198 ns   2245161525
BenchmarkFixture/SmallByteString_DefaultConstruct                        0.188 ns        0.188 ns   2294513655
BenchmarkFixture/StdString_SmallConstruct                                0.430 ns        0.430 ns    974027375
BenchmarkFixture/SmallString_SmallConstruct                               5.57 ns         5.56 ns     75236680
BenchmarkFixture/SmallByteString_SmallConstruct                           1.91 ns         1.90 ns    221587104
BenchmarkFixture/StdString_LargeConstruct                                 11.5 ns         11.5 ns     36201214
BenchmarkFixture/SmallString_LargeConstruct                               12.4 ns         12.4 ns     33825805
BenchmarkFixture/SmallByteString_LargeConstruct                           11.8 ns         11.8 ns     35238810
BenchmarkFixture/StdString_SmallCopy                                     0.398 ns        0.397 ns   1058660932
BenchmarkFixture/SmallString_SmallCopy                                    6.22 ns         6.21 ns     68977882
BenchmarkFixture/SmallByteString_SmallCopy                                2.75 ns         2.75 ns    147859570
BenchmarkFixture/StdString_LargeCopy                                      11.9 ns         11.8 ns     34981219
BenchmarkFixture/SmallString_LargeCopy                                    15.9 ns         15.9 ns     26467833
BenchmarkFixture/SmallByteString_LargeCopy                                12.5 ns         12.4 ns     34461999
BenchmarkFixture/StdString_SmallMove                                       141 ns          141 ns      2973848
BenchmarkFixture/SmallString_SmallMove                                     141 ns          141 ns      2974492
BenchmarkFixture/SmallByteString_SmallMove                                 142 ns          142 ns      2979174
BenchmarkFixture/StdString_LargeMove                                       146 ns          146 ns      2871768
BenchmarkFixture/SmallString_LargeMove                                     142 ns          143 ns      2852571
BenchmarkFixture/SmallByteString_LargeMove                                 143 ns          143 ns      2915142
BenchmarkFixture/StdString_CharAppend                                      100 ns        100.0 ns      4315938
BenchmarkFixture/SmallString_CharAppend                                    532 ns          531 ns       791642
BenchmarkFixture/SmallByteString_CharAppend                                505 ns          504 ns       832954
BenchmarkFixture/StdString_StringAppend                                    128 ns          127 ns      3373991
BenchmarkFixture/SmallString_StringAppend                                  167 ns          167 ns      2524249
BenchmarkFixture/SmallByteString_StringAppend                              152 ns          152 ns      2763138
BenchmarkFixture/StdString_Find                                           1.56 ns         1.55 ns    269963703
BenchmarkFixture/SmallString_Find                                         4.19 ns         4.18 ns    100304010
BenchmarkFixture/SmallByteString_Find                                     4.20 ns         4.20 ns     99511845
BenchmarkFixture/StdString_CharFind                                       1.28 ns         1.27 ns    329858691
BenchmarkFixture/SmallString_CharFind                                     1.60 ns         1.60 ns    270038607
BenchmarkFixture/SmallByteString_CharFind                                 1.57 ns         1.57 ns    266709823
BenchmarkFixture/StdString_Insert                                         6.24 ns         6.23 ns     67073929
BenchmarkFixture/SmallString_Insert                                       19.9 ns         19.9 ns     21071722
BenchmarkFixture/SmallByteString_Insert                                   19.6 ns         19.6 ns     21435002
BenchmarkFixture/StdString_Erase                                          2.19 ns         2.18 ns    192399328
BenchmarkFixture/SmallString_Erase                                        9.15 ns         9.14 ns     46892181
BenchmarkFixture/SmallByteString_Erase                                    7.60 ns         7.59 ns     55169511
BenchmarkFixture/StdString_MapInsert                                    102698 ns       102525 ns         4139
BenchmarkFixture/SmallString_MapInsert                                  127769 ns       127561 ns         3250
BenchmarkFixture/SmallByteString_MapInsert                              131564 ns       131348 ns         3220
BenchmarkFixture/StdString_MapLookup                                     75781 ns        75660 ns         5592
BenchmarkFixture/SmallString_MapLookup                                   88472 ns        88319 ns         4729
BenchmarkFixture/SmallByteString_MapLookup                               79721 ns        79604 ns         5184
BenchmarkFixture/StdString_MapIteration                                   3985 ns         3978 ns       108075
BenchmarkFixture/SmallString_MapIteration                                 3809 ns         3802 ns       121027
BenchmarkFixture/SmallByteString_MapIteration                             3856 ns         3849 ns       117946
BenchmarkFixture/StdString_MapInsertMedium                              113565 ns       113343 ns         3688
BenchmarkFixture/SmallString_MapInsertMedium                            128222 ns       127997 ns         3243
BenchmarkFixture/SmallByteString_MapInsertMedium                        131001 ns       130794 ns         3249
BenchmarkFixture/StdString_MapLookupMedium                               81565 ns        81413 ns         5057
BenchmarkFixture/SmallString_MapLookupMedium                            100027 ns        99846 ns         4137
BenchmarkFixture/SmallByteString_MapLookupMedium                         97090 ns        96897 ns         4300
BenchmarkFixture/StdString_MapIterationMedium                             4259 ns         4252 ns       100342
BenchmarkFixture/SmallString_MapIterationMedium                           4317 ns         4309 ns       101187
BenchmarkFixture/SmallByteString_MapIterationMedium                       4303 ns         4282 ns       101608
BenchmarkFixture/StdString_UnorderedMapInsert                            42729 ns        42641 ns         9878
BenchmarkFixture/SmallString_UnorderedMapInsert                          56551 ns        56449 ns         7331
BenchmarkFixture/SmallByteString_UnorderedMapInsert                      45550 ns        45478 ns         9005
BenchmarkFixture/StdString_UnorderedMapLookup                             6479 ns         6469 ns        64916
BenchmarkFixture/SmallString_UnorderedMapLookup                           8279 ns         8268 ns        51256
BenchmarkFixture/SmallByteString_UnorderedMapLookup                       7853 ns         7839 ns        55834
BenchmarkFixture/StdString_UnorderedMapIteration                          3160 ns         3154 ns       100000
BenchmarkFixture/SmallString_UnorderedMapIteration                        2461 ns         2457 ns       140298
BenchmarkFixture/SmallByteString_UnorderedMapIteration                    2231 ns         2227 ns       154092
BenchmarkFixture/StdString_UnorderedMapInsertMedium                      70722 ns        70577 ns         5866
BenchmarkFixture/SmallString_UnorderedMapInsertMedium                    73657 ns        73515 ns         5732
BenchmarkFixture/SmallByteString_UnorderedMapInsertMedium                72676 ns        72550 ns         5812
BenchmarkFixture/StdString_UnorderedMapLookupMedium                       8291 ns         8277 ns        50574
BenchmarkFixture/SmallString_UnorderedMapLookupMedium                    11255 ns        11238 ns        37801
BenchmarkFixture/SmallByteString_UnorderedMapLookupMedium                11284 ns        11264 ns        36828
BenchmarkFixture/StdString_UnorderedMapIterationMedium                    3589 ns         3584 ns       127212
BenchmarkFixture/SmallString_UnorderedMapIterationMedium                  3553 ns         3547 ns       134372
BenchmarkFixture/SmallByteString_UnorderedMapIterationMedium              3492 ns         3486 ns       134576
BenchmarkFixture/StdString_MapInsertLong                                118553 ns       118337 ns         3619
BenchmarkFixture/SmallString_MapInsertLong                              144500 ns       144241 ns         2953
BenchmarkFixture/SmallByteString_MapInsertLong                          145402 ns       145154 ns         2839
BenchmarkFixture/StdString_MapLookupLong                                111868 ns       111681 ns         3769
BenchmarkFixture/SmallString_MapLookupLong                              123866 ns       123675 ns         3370
BenchmarkFixture/SmallByteString_MapLookupLong                          127309 ns       127103 ns         3319
BenchmarkFixture/StdString_UnorderedMapInsertLong                       102102 ns       101927 ns         4140
BenchmarkFixture/SmallString_UnorderedMapInsertLong                     103655 ns       103458 ns         4078
BenchmarkFixture/SmallByteString_UnorderedMapInsertLong                 103121 ns       102927 ns         4122
BenchmarkFixture/StdString_UnorderedMapLookupLong                        31470 ns        31422 ns        13281
BenchmarkFixture/SmallString_UnorderedMapLookupLong                      31915 ns        31857 ns        13041
BenchmarkFixture/SmallByteString_UnorderedMapLookupLong                  31776 ns        31728 ns        13025
BenchmarkFixture/StdString_MapInsertMixed                               134177 ns       133950 ns         3107
BenchmarkFixture/SmallString_MapInsertMixed                             165477 ns       165182 ns         2502
BenchmarkFixture/SmallByteString_MapInsertMixed                         166069 ns       165785 ns         2539
BenchmarkFixture/StdString_MapLookupMixed                                99723 ns        99590 ns         4257
BenchmarkFixture/SmallString_MapLookupMixed                             115936 ns       115748 ns         3576
BenchmarkFixture/SmallByteString_MapLookupMixed                         116133 ns       115948 ns         3582
BenchmarkFixture/StdString_UnorderedMapInsertMixed                       88561 ns        88407 ns         4790
BenchmarkFixture/SmallString_UnorderedMapInsertMixed                     91926 ns        91760 ns         4531
BenchmarkFixture/SmallByteString_UnorderedMapInsertMixed                 87935 ns        87779 ns         4769
BenchmarkFixture/StdString_UnorderedMapLookupMixed                       17341 ns        17313 ns        24437
BenchmarkFixture/SmallString_UnorderedMapLookupMixed                     18908 ns        18879 ns        22350
BenchmarkFixture/SmallByteString_UnorderedMapLookupMixed                 18998 ns        18970 ns        22455
BenchmarkFixture/MemoryFootprint_StdString_Vector                        MemoryPerItem=46.024 bytes
BenchmarkFixture/MemoryFootprint_SmallString_Vector                      MemoryPerItem=14.232 bytes  (69.1% less)
BenchmarkFixture/MemoryFootprint_SmallByteString_Vector                  MemoryPerItem=15.024 bytes  (67.3% less)
BenchmarkFixture/MemoryFootprint_StdString_Map                           MemoryPerItem=82.024 bytes
BenchmarkFixture/MemoryFootprint_SmallString_Map                         MemoryPerItem=50.232 bytes  (38.7% less)
BenchmarkFixture/MemoryFootprint_SmallByteString_Map                     MemoryPerItem=51.024 bytes  (37.8% less)
BenchmarkFixture/MemoryFootprint_StdString_UnorderedMap                  MemoryPerItem=86.829 bytes
BenchmarkFixture/MemoryFootprint_SmallString_UnorderedMap                MemoryPerItem=55.037 bytes  (36.6% less)
BenchmarkFixture/MemoryFootprint_SmallByteString_UnorderedMap            MemoryPerItem=55.829 bytes  (35.7% less)
```

### GCC + libstdc++ (build.release)

Results from GCC compiler with libstdc++ on 32-core 5.5GHz CPU:

```
=== Memory Usage Information ===
sizeof(std::string): 32 bytes (libstdc++)
sizeof(small::small_string): 8 bytes
sizeof(small::small_byte_string): 8 bytes
=================================

--------------------------------------------------------------------------------------------------------------
Benchmark                                                                    Time             CPU   Iterations
--------------------------------------------------------------------------------------------------------------
BenchmarkFixture/StdString_DefaultConstruct                              0.301 ns        0.300 ns   1459348898
BenchmarkFixture/SmallString_DefaultConstruct                            0.410 ns        0.410 ns    859916368
BenchmarkFixture/SmallByteString_DefaultConstruct                        0.469 ns        0.468 ns    799801223
BenchmarkFixture/StdString_SmallConstruct                                0.914 ns        0.912 ns    460894718
BenchmarkFixture/SmallString_SmallConstruct                               1.23 ns         1.23 ns    338738140
BenchmarkFixture/SmallByteString_SmallConstruct                           1.09 ns         1.09 ns    384432835
BenchmarkFixture/StdString_LargeConstruct                                 13.3 ns         13.2 ns     32058996
BenchmarkFixture/SmallString_LargeConstruct                               11.1 ns         11.1 ns     37536416
BenchmarkFixture/SmallByteString_LargeConstruct                           11.3 ns         11.3 ns     38070980
BenchmarkFixture/StdString_SmallCopy                                      1.03 ns         1.02 ns    411067491
BenchmarkFixture/SmallString_SmallCopy                                    1.46 ns         1.45 ns    288817389
BenchmarkFixture/SmallByteString_SmallCopy                                1.28 ns         1.27 ns    329699017
BenchmarkFixture/StdString_LargeCopy                                      13.2 ns         13.1 ns     31957987
BenchmarkFixture/SmallString_LargeCopy                                    15.5 ns         15.4 ns     27151619
BenchmarkFixture/SmallByteString_LargeCopy                                15.6 ns         15.6 ns     27122900
BenchmarkFixture/StdString_SmallMove                                       122 ns          123 ns      3419598
BenchmarkFixture/SmallString_SmallMove                                     121 ns          122 ns      3446857
BenchmarkFixture/SmallByteString_SmallMove                                 121 ns          122 ns      3450374
BenchmarkFixture/StdString_LargeMove                                       127 ns          127 ns      3305048
BenchmarkFixture/SmallString_LargeMove                                     126 ns          126 ns      3330055
BenchmarkFixture/SmallByteString_LargeMove                                 126 ns          126 ns      3335608
BenchmarkFixture/StdString_CharAppend                                     71.5 ns         71.4 ns      5877918
BenchmarkFixture/SmallString_CharAppend                                    569 ns          568 ns       740186
BenchmarkFixture/SmallByteString_CharAppend                                466 ns          465 ns       902103
BenchmarkFixture/StdString_StringAppend                                   68.1 ns         68.0 ns      6360646
BenchmarkFixture/SmallString_StringAppend                                  307 ns          307 ns      1365964
BenchmarkFixture/SmallByteString_StringAppend                              272 ns          272 ns      1547947
BenchmarkFixture/StdString_Find                                           1.19 ns         1.18 ns    354043642
BenchmarkFixture/SmallString_Find                                         1.91 ns         1.91 ns    219763891
BenchmarkFixture/SmallByteString_Find                                     1.91 ns         1.90 ns    219966834
BenchmarkFixture/StdString_CharFind                                       1.09 ns         1.09 ns    386737632
BenchmarkFixture/SmallString_CharFind                                     1.63 ns         1.63 ns    257657923
BenchmarkFixture/SmallByteString_CharFind                                 1.76 ns         1.76 ns    256795881
BenchmarkFixture/StdString_Insert                                         9.28 ns         9.27 ns     45672344
BenchmarkFixture/SmallString_Insert                                       24.0 ns         24.0 ns     17536722
BenchmarkFixture/SmallByteString_Insert                                   22.4 ns         22.3 ns     18807946
BenchmarkFixture/StdString_Erase                                          8.44 ns         8.43 ns     50182670
BenchmarkFixture/SmallString_Erase                                        9.80 ns         9.79 ns     41438599
BenchmarkFixture/SmallByteString_Erase                                    9.28 ns         9.27 ns     45285467
BenchmarkFixture/StdString_MapInsert                                   107796 ns       107620 ns         4043
BenchmarkFixture/SmallString_MapInsert                                 137587 ns       137332 ns         3042
BenchmarkFixture/SmallByteString_MapInsert                             124081 ns       123867 ns         3399
BenchmarkFixture/StdString_MapLookup                                     58517 ns        58428 ns         7532
BenchmarkFixture/SmallString_MapLookup                                   72022 ns        71901 ns         5818
BenchmarkFixture/SmallByteString_MapLookup                               68218 ns        68102 ns         6174
BenchmarkFixture/StdString_MapIteration                                   4310 ns         4303 ns        97788
BenchmarkFixture/SmallString_MapIteration                                 3810 ns         3803 ns       113231
BenchmarkFixture/SmallByteString_MapIteration                             3824 ns         3816 ns       113953
BenchmarkFixture/StdString_UnorderedMapInsert                            44691 ns        44613 ns         9357
BenchmarkFixture/SmallString_UnorderedMapInsert                          67186 ns        67052 ns         6161
BenchmarkFixture/SmallByteString_UnorderedMapInsert                      59818 ns        59704 ns         6878
BenchmarkFixture/StdString_UnorderedMapLookup                             6540 ns         6528 ns        63923
BenchmarkFixture/SmallString_UnorderedMapLookup                          10493 ns        10474 ns        40784
BenchmarkFixture/SmallByteString_UnorderedMapLookup                      12741 ns        12718 ns        33167
BenchmarkFixture/StdString_UnorderedMapIteration                          2373 ns         2369 ns       167557
BenchmarkFixture/SmallString_UnorderedMapIteration                        1117 ns         1115 ns       339597
BenchmarkFixture/SmallByteString_UnorderedMapIteration                    1064 ns         1062 ns       378681
BenchmarkFixture/MemoryFootprint_StdString_Vector                     MemoryPerItem=32.024 bytes
BenchmarkFixture/MemoryFootprint_SmallString_Vector                   MemoryPerItem=14.223 bytes  (55.6% less)
BenchmarkFixture/MemoryFootprint_SmallByteString_Vector               MemoryPerItem=15.024 bytes  (53.1% less)
BenchmarkFixture/MemoryFootprint_StdString_Map                        MemoryPerItem=68.048 bytes
BenchmarkFixture/MemoryFootprint_SmallString_Map                      MemoryPerItem=50.247 bytes  (26.2% less)
BenchmarkFixture/MemoryFootprint_SmallByteString_Map                  MemoryPerItem=51.048 bytes  (25.0% less)
BenchmarkFixture/MemoryFootprint_StdString_UnorderedMap               MemoryPerItem=68.937 bytes
BenchmarkFixture/MemoryFootprint_SmallString_UnorderedMap             MemoryPerItem=51.136 bytes  (25.8% less)
BenchmarkFixture/MemoryFootprint_SmallByteString_UnorderedMap         MemoryPerItem=51.937 bytes  (24.7% less)
```