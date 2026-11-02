# Custom Memory Pool in C++

A fixed-capacity memory pool implementation in C++17 that manages preallocated memory using an intrusive Free List.

The project demonstrates low-level memory management, placement new, object lifetime management, memory alignment, pointer validation, and performance benchmarking.

## Project Overview

A Memory Pool allocates a large memory region once and divides it into multiple fixed-size blocks.

Instead of performing individual heap allocations for every object, the pool manages and reuses its own memory blocks through a Free List.

This implementation provides:

- Fixed-size memory blocks
- Intrusive singly linked Free List
- O(1) allocation and deallocation
- Memory alignment for fundamental types
- Allocation tracking
- Double-free protection
- Invalid pointer validation
- Explicit object lifetime management
- Unit testing with CTest
- Performance benchmarking against standard `new/delete`

## Features

- **Preallocated Memory:** Allocates one large memory region during construction.
- **Free List:** Maintains available blocks using an intrusive singly linked list.
- **Fast Allocation:** Retrieves an available block from the Free List head.
- **Block Reuse:** Returns deallocated blocks to the Free List.
- **Alignment Handling:** Adjusts block size to satisfy fundamental alignment requirements.
- **Pointer Validation:** Rejects foreign pointers and pointers that do not point to valid block boundaries.
- **Double-Free Protection:** Tracks allocation state for every block.
- **Exception Handling:** Handles invalid constructor arguments and pool exhaustion.
- **Ownership Safety:** Disables copy and move operations to prevent accidental shared ownership.
- **Benchmarking:** Compares custom allocation with standard dynamic allocation.

## Project Structure

```text
08-memory-pool/
│
├── include/
│   └── memory_pool/
│       └── MemoryPool.hpp
│
├── src/
│   ├── CMakeLists.txt
│   └── MemoryPool.cpp
│
├── tests/
│   ├── CMakeLists.txt
│   └── test_memory_pool.cpp
│
├── benchmarks/
│   ├── CMakeLists.txt
│   └── benchmark.cpp
│
├── docs/
│   └── 01-MemoryPool-Deep-Dive.md
│
├── CMakeLists.txt
├── README.md
└── .gitignore
```

## Memory Pool Architecture

The pool manages one large memory allocation divided into fixed-size blocks.

```text
             Memory Pool
    +-----------------------------+
    |                             |
    |  Block 0  Block 1  Block 2   |
    |  Block 3  Block 4  Block 5   |
    |                             |
    +-----------------------------+
                  |
                  v
              Free List
                  |
                  v
    Block 5 -> Block 4 -> Block 3
                  |
                  v
                 ...
```

Each free block contains a pointer to the next available block.

The Free List uses the memory inside free blocks to store its linked-list metadata.

## Memory Management Workflow

### 1. Pool Construction

```cpp
MemoryPool pool(32, 100);
```

Creates a pool with 100 blocks using a requested block size of 32 bytes.

The actual internal block size may be adjusted to satisfy alignment and FreeBlock storage requirements.

### 2. Allocate Memory

```cpp
void* memory = pool.allocate();
```

Retrieves a free block from the head of the Free List.

### 3. Construct an Object

```cpp
struct Object
{
    int value;

    explicit Object(int v) : value(v) {}
};

Object* object = new (memory) Object(100);
```

Placement new constructs the object in the memory block returned by the pool.

### 4. Destroy the Object

```cpp
object->~Object();
```

The object must be explicitly destroyed before its storage is returned to the pool.

### 5. Deallocate Memory

```cpp
pool.deallocate(memory);
```

Returns the block to the Free List so it can be reused.

**Important:** Do not use `delete` on an object constructed using placement new in a MemoryPool block.

## Usage Example

```cpp
#include "memory_pool/MemoryPool.hpp"

#include <iostream>
#include <new>

struct Particle
{
    int id;

    explicit Particle(int value)
        : id(value)
    {
    }

    ~Particle() = default;
};

int main()
{
    MemoryPool pool(sizeof(Particle), 10);

    // Allocate a memory block.
    void* memory = pool.allocate();

    // Construct an object in the allocated block.
    Particle* particle =
        new (memory) Particle(100);

    std::cout << "Particle ID: "
              << particle->id << '\n';

    // Destroy the object explicitly.
    particle->~Particle();

    // Return the block to the pool.
    pool.deallocate(memory);

    return 0;
}
```

## Requirements

- C++17 compatible compiler
- CMake 3.20 or newer
- GCC / MinGW or another compatible C++ compiler

## Build Instructions

### 1. Clone the Repository

```bash
git clone <your-repository-url>
cd 08-memory-pool
```

### 2. Configure the Project

For MinGW Makefiles on Windows:

```bash
cmake -S . -B build -G "MinGW Makefiles"
```

If your MinGW installation requires an explicit make executable, specify `CMAKE_MAKE_PROGRAM` according to your environment.

### 3. Build

```bash
cmake --build build
```

## Running Unit Tests

Run the complete registered test suite:

```bash
ctest --test-dir build --output-on-failure
```

On Windows CMD, the test executable can also be run directly:

```cmd
.\build\tests\test_memory_pool.exe
```

### Implemented Test Cases

| No. | Test Case |
|---:|---|
| 1 | Valid constructor |
| 2 | Zero block size rejection |
| 3 | Zero block count rejection |
| 4 | Unique block allocations |
| 5 | Pool exhaustion handling |
| 6 | Deallocated block reuse |
| 7 | nullptr deallocation |
| 8 | Foreign pointer rejection |
| 9 | Interior pointer rejection |
| 10 | Double-free protection |
| 11 | Placement new object lifecycle |
| 12 | Copy and move operations disabled |

### Test Result

The registered CTest suite completed successfully.

```text
Test project: 08-memory-pool

1/1 Test #1: MemoryPoolTests .......... Passed

100% tests passed, 0 tests failed
```

## Benchmarking

The project includes a benchmark comparing:

1. Standard C++ `new/delete`
2. Custom MemoryPool allocation and deallocation

### Benchmark Configuration

| Parameter | Value |
|---|---:|
| Objects per round | 200,000 |
| Benchmark rounds | 5 |
| Object size | 32 bytes |
| Timing mechanism | `std::chrono::steady_clock` |
| Reported metric | Median execution time |

### Run Benchmark

```bash
cmake --build build
```

On Windows CMD:

```cmd
.\build\benchmarks\benchmark_memory_pool.exe
```

### Actual Benchmark Results

Results measured on the development machine:

| Round | Standard `new/delete` | MemoryPool |
|---:|---:|---:|
| 1 | 17.3402 ms | 8.9753 ms |
| 2 | 17.6568 ms | 8.9856 ms |
| 3 | 17.1423 ms | 9.0216 ms |
| 4 | 17.1081 ms | 8.9999 ms |
| 5 | 17.0080 ms | 8.9356 ms |
| **Median** | **17.1423 ms** | **8.9856 ms** |

### Performance Comparison

| Metric | Result |
|---|---:|
| Standard allocation median | 17.1423 ms |
| MemoryPool median | 8.9856 ms |
| Measured speedup | 1.91x |
| Measured time reduction | Approximately 47.6% |

In this particular benchmark, the custom MemoryPool completed the measured workload approximately 1.91 times faster than standard `new/delete`.

**Note:** Pool initialization is excluded from the timed interval. Results depend on hardware, compiler settings, allocator implementation, and workload. This benchmark does not establish that a MemoryPool is universally faster.

## Technical Concepts Demonstrated

This project explores several important C++ systems programming concepts:

- Dynamic memory allocation
- Raw storage management
- Object lifetime and destruction
- Placement new
- Intrusive singly linked lists
- Free List allocation strategy
- Memory alignment
- Pointer arithmetic
- `void*` and `std::byte*`
- `static_cast` and `reinterpret_cast`
- Integer overflow checks
- Exception handling
- RAII and resource ownership
- Deleted copy and move operations
- Unit testing with CTest
- Build configuration using CMake
- Performance benchmarking with `std::chrono`

## Complexity Analysis

| Operation | Time Complexity |
|---|---|
| Pool construction and Free List initialization | O(N) |
| Allocation | O(1) |
| Deallocation | O(1) |
| Pool destruction | O(1) for backing memory release |

Here, N represents the total number of blocks.

The allocation tracking vector requires O(N) additional memory.

## Current Limitations

- Fixed pool capacity.
- Fixed-size blocks.
- Not thread-safe.
- No automatic destruction of user-created objects.
- No general support for over-aligned types.
- No dynamic pool expansion.
- Caller is responsible for object lifetime management.

## Future Improvements

- Alignment and stress testing
- Thread-safe allocation
- Dynamic pool expansion
- Typed allocation interface
- Over-aligned type support
- STL allocator integration
- Sanitizer-based memory validation
- Benchmarks with multiple object sizes and workloads

## Documentation

Detailed technical documentation is available in:

`docs/01-MemoryPool-Deep-Dive.md`

It covers the architecture, memory layout, Free List, alignment, placement new, object lifetime, implementation details, testing, and benchmarking.

## Author

**Niteen Digarse**

C++ | Data Structures & Algorithms | Linux | Systems Programming

---

*This project is part of my C++ Engineering Projects series, focused on understanding low-level programming, memory management, and systems engineering through practical implementations.*