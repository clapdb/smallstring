# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a C++ header-only library implementing an optimized small string class (`basic_small_string`) in the `small` namespace. The library provides memory-efficient string storage with small string optimization (SSO).

## Architecture

### Core Components

- **`basic_small_string`** (line 995): The main template class that provides string functionality with customizable memory allocation strategies
- **Core Storage Types**:
  - `malloc_core`: Uses standard malloc/free for memory allocation
  - `pmr_core`: Uses polymorphic memory resources (PMR) 
- **Buffer Management**: `small_string_buffer` class handles memory allocation and buffer management
- **String Types**:
  - `small::small_string`: Standard version using malloc
  - `small::pmr::small_string`: PMR version using polymorphic allocators

### Memory Layout

The implementation uses different storage strategies based on string size:
- **Internal** (CoreType::Internal): Small strings stored directly in the object (up to 7 chars)
- **Short** (CoreType::Short): Short strings with capacity/size stored in the core
- **Median** (CoreType::Median): Medium strings with metadata in buffer head
- **Long** (CoreType::Long): Large strings with external allocation

## Development

### File Structure
```
include/
└── smallstring.hpp    # Main header-only implementation (3000+ lines)
LICENSE.txt           # Apache 2.0 license
```

### Building and Testing
mkdir build.debug
cd build.debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
ninja reg_test


### Memory Management

The library supports two allocation strategies:
- **Standard allocator**: Uses `std::allocator` with malloc/free
- **PMR allocator**: Uses `std::pmr::polymorphic_allocator` for custom memory management

Both versions maintain a 16-byte object size for efficient memory usage.