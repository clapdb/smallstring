#include <benchmark/benchmark.h>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include "include/smallstring.hpp"

class BenchmarkFixture : public ::benchmark::Fixture {
public:
    void SetUp([[maybe_unused]] const ::benchmark::State& state) override {
        // Generate test data for various string sizes
        short_strings = generate_strings(1000, 3, 7);      // SSO range
        medium_strings = generate_strings(1000, 15, 50);   // Short range
        long_strings = generate_strings(1000, 100, 500);   // Long range
        
        // Specific test strings
        empty_str = "";
        tiny_str = "hi";
        small_str = "hello";
        medium_str = "This is a medium length string for testing";
        large_str = std::string(1000, 'X');
        huge_str = std::string(10000, 'Y');
    }

protected:
    std::vector<std::string> short_strings;
    std::vector<std::string> medium_strings; 
    std::vector<std::string> long_strings;
    
    std::string empty_str;
    std::string tiny_str;
    std::string small_str;
    std::string medium_str;
    std::string large_str;
    std::string huge_str;

private:
    std::vector<std::string> generate_strings(size_t count, size_t min_len, size_t max_len) {
        std::vector<std::string> strings;
        strings.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::uniform_int_distribution<size_t> len_dist(min_len, max_len);
        std::uniform_int_distribution<int> char_dist('a', 'z');
        
        for (size_t i = 0; i < count; ++i) {
            size_t len = len_dist(gen);
            std::string str;
            str.reserve(len);
            for (size_t j = 0; j < len; ++j) {
                str.push_back(static_cast<char>(char_dist(gen)));
            }
            strings.push_back(std::move(str));
        }
        
        return strings;
    }
};

// =============================================================================
// Construction Benchmarks
// =============================================================================

BENCHMARK_F(BenchmarkFixture, StdString_DefaultConstruct)(benchmark::State& state) {
    for (auto _ : state) {
        std::string s;
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_DefaultConstruct)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_string s;
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_DefaultConstruct)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_byte_string s;
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, StdString_SmallConstruct)(benchmark::State& state) {
    for (auto _ : state) {
        std::string s(small_str);
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_SmallConstruct)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_string s(small_str);
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_SmallConstruct)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_byte_string s(small_str);
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, StdString_LargeConstruct)(benchmark::State& state) {
    for (auto _ : state) {
        std::string s(large_str);
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_LargeConstruct)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_string s(large_str);
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_LargeConstruct)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_byte_string s(large_str);
        benchmark::DoNotOptimize(s);
    }
}

// =============================================================================
// Copy Operations
// =============================================================================

BENCHMARK_F(BenchmarkFixture, StdString_SmallCopy)(benchmark::State& state) {
    std::string source(small_str);
    for (auto _ : state) {
        std::string copy = source;
        benchmark::DoNotOptimize(copy);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_SmallCopy)(benchmark::State& state) {
    small::small_string source(small_str);
    for (auto _ : state) {
        small::small_string copy = source;
        benchmark::DoNotOptimize(copy);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_SmallCopy)(benchmark::State& state) {
    small::small_byte_string source(small_str);
    for (auto _ : state) {
        small::small_byte_string copy = source;
        benchmark::DoNotOptimize(copy);
    }
}

BENCHMARK_F(BenchmarkFixture, StdString_LargeCopy)(benchmark::State& state) {
    std::string source(large_str);
    for (auto _ : state) {
        std::string copy = source;
        benchmark::DoNotOptimize(copy);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_LargeCopy)(benchmark::State& state) {
    small::small_string source(large_str);
    for (auto _ : state) {
        small::small_string copy = source;
        benchmark::DoNotOptimize(copy);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_LargeCopy)(benchmark::State& state) {
    small::small_byte_string source(large_str);
    for (auto _ : state) {
        small::small_byte_string copy = source;
        benchmark::DoNotOptimize(copy);
    }
}

// =============================================================================
// Move Operations
// =============================================================================

BENCHMARK_F(BenchmarkFixture, StdString_SmallMove)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        std::string source(small_str);
        state.ResumeTiming();
        std::string moved = std::move(source);
        benchmark::DoNotOptimize(moved);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_SmallMove)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        small::small_string source(small_str);
        state.ResumeTiming();
        small::small_string moved = std::move(source);
        benchmark::DoNotOptimize(moved);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_SmallMove)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        small::small_byte_string source(small_str);
        state.ResumeTiming();
        small::small_byte_string moved = std::move(source);
        benchmark::DoNotOptimize(moved);
    }
}

BENCHMARK_F(BenchmarkFixture, StdString_LargeMove)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        std::string source(large_str);
        state.ResumeTiming();
        std::string moved = std::move(source);
        benchmark::DoNotOptimize(moved);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_LargeMove)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        small::small_string source(large_str);
        state.ResumeTiming();
        small::small_string moved = std::move(source);
        benchmark::DoNotOptimize(moved);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_LargeMove)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        small::small_byte_string source(large_str);
        state.ResumeTiming();
        small::small_byte_string moved = std::move(source);
        benchmark::DoNotOptimize(moved);
    }
}

// =============================================================================
// Append Operations
// =============================================================================

BENCHMARK_F(BenchmarkFixture, StdString_CharAppend)(benchmark::State& state) {
    for (auto _ : state) {
        std::string s;
        for (int i = 0; i < 100; ++i) {
            s += 'a';
        }
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_CharAppend)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_string s;
        for (int i = 0; i < 100; ++i) {
            s += 'a';
        }
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_CharAppend)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_byte_string s;
        for (int i = 0; i < 100; ++i) {
            s += 'a';
        }
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, StdString_StringAppend)(benchmark::State& state) {
    for (auto _ : state) {
        std::string s;
        for (int i = 0; i < 50; ++i) {
            s += "test";
        }
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_StringAppend)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_string s;
        for (int i = 0; i < 50; ++i) {
            s += "test";
        }
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_StringAppend)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_byte_string s;
        for (int i = 0; i < 50; ++i) {
            s += "test";
        }
        benchmark::DoNotOptimize(s);
    }
}

// =============================================================================
// Search Operations
// =============================================================================

BENCHMARK_F(BenchmarkFixture, StdString_Find)(benchmark::State& state) {
    std::string haystack = "Lorem ipsum dolor sit amet, consectetur adipiscing elit";
    for (auto _ : state) {
        auto pos = haystack.find("dolor");
        benchmark::DoNotOptimize(pos);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_Find)(benchmark::State& state) {
    small::small_string haystack = "Lorem ipsum dolor sit amet, consectetur adipiscing elit";
    for (auto _ : state) {
        auto pos = haystack.find("dolor");
        benchmark::DoNotOptimize(pos);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_Find)(benchmark::State& state) {
    small::small_byte_string haystack = "Lorem ipsum dolor sit amet, consectetur adipiscing elit";
    for (auto _ : state) {
        auto pos = haystack.find("dolor");
        benchmark::DoNotOptimize(pos);
    }
}

BENCHMARK_F(BenchmarkFixture, StdString_CharFind)(benchmark::State& state) {
    std::string haystack = "Lorem ipsum dolor sit amet, consectetur adipiscing elit";
    for (auto _ : state) {
        auto pos = haystack.find('e');
        benchmark::DoNotOptimize(pos);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_CharFind)(benchmark::State& state) {
    small::small_string haystack = "Lorem ipsum dolor sit amet, consectetur adipiscing elit";
    for (auto _ : state) {
        auto pos = haystack.find('e');
        benchmark::DoNotOptimize(pos);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_CharFind)(benchmark::State& state) {
    small::small_byte_string haystack = "Lorem ipsum dolor sit amet, consectetur adipiscing elit";
    for (auto _ : state) {
        auto pos = haystack.find('e');
        benchmark::DoNotOptimize(pos);
    }
}

// =============================================================================
// Insert/Erase Operations
// =============================================================================

BENCHMARK_F(BenchmarkFixture, StdString_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        std::string s = "Hello World";
        s.insert(5, " Beautiful");
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_string s = "Hello World";
        s.insert(5, " Beautiful");
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_Insert)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_byte_string s = "Hello World";
        s.insert(5, " Beautiful");
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, StdString_Erase)(benchmark::State& state) {
    for (auto _ : state) {
        std::string s = "Hello Beautiful World";
        s.erase(6, 10);
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallString_Erase)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_string s = "Hello Beautiful World";
        s.erase(6, 10);
        benchmark::DoNotOptimize(s);
    }
}

BENCHMARK_F(BenchmarkFixture, SmallByteString_Erase)(benchmark::State& state) {
    for (auto _ : state) {
        small::small_byte_string s = "Hello Beautiful World";
        s.erase(6, 10);
        benchmark::DoNotOptimize(s);
    }
}



// =============================================================================
// Memory Usage Information
// =============================================================================

static void MemoryInfo(benchmark::State& state) {
    if (state.thread_index() == 0) {
        std::cout << "\n=== Memory Usage Information ===\n";
        std::cout << "sizeof(std::string): " << sizeof(std::string) << " bytes\n";
        std::cout << "sizeof(small::small_string): " << sizeof(small::small_string) << " bytes\n";
        std::cout << "sizeof(small::small_byte_string): " << sizeof(small::small_byte_string) << " bytes\n";
        std::cout << "=================================\n\n";
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(sizeof(std::string));
    }
}

BENCHMARK(MemoryInfo)->Iterations(1);

BENCHMARK_MAIN();