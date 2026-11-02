# Project #08 — Custom Memory Pool
## Detailed Technical Documentation

**Language:** C++17  
**Build System:** CMake  
**Compiler:** GCC / MinGW  
**Project Type:** Dynamic Memory Management / Systems Programming

---

# 1. Project Overview

## 1.1 What Is a Memory Pool?

A Memory Pool is a custom memory management technique in which a large memory region is allocated once and divided into smaller, fixed-size blocks.

Instead of requesting memory from the operating system or general-purpose heap allocator every time an object is needed, the program reuses blocks from its own preallocated memory region.

In this implementation:

- Memory is allocated once during MemoryPool construction.
- The allocated region is divided into fixed-size blocks.
- A Free List tracks available blocks.
- `allocate()` retrieves a free block.
- `deallocate()` returns a block to the Free List.
- The entire memory region is released when the MemoryPool is destroyed.

## 1.2 Why Do We Need a Memory Pool?

Consider an application that frequently creates and destroys thousands of objects.

Using ordinary dynamic allocation:

```cpp
Object* object = new Object();

// Use object

delete object;
```

Each allocation and deallocation is handled by the general-purpose memory allocator.

Repeated allocations may introduce allocator bookkeeping overhead and fragmentation.

A Memory Pool provides an alternative:

```cpp
void* memory = pool.allocate();

Object* object = new (memory) Object();

// Use object

object->~Object();

pool.deallocate(memory);
```

Here, memory blocks are reused instead of individually allocating and freeing heap memory for every object.

## 1.3 When Is a Memory Pool Useful?

Memory pools are useful when:

- Many objects have the same or similar sizes.
- Objects are frequently created and destroyed.
- Predictable allocation behavior is important.
- Allocation overhead needs to be reduced.
- Memory reuse is beneficial.

Examples:

- Game engines
- Network servers
- Real-time systems
- Embedded applications
- High-performance applications
- Particle systems
- Packet processing systems

**Important:** A Memory Pool is not automatically faster than `new/delete`. Performance depends on the workload, implementation, compiler, and underlying allocator.

---

# 2. Project Architecture

## 2.1 Project Directory Structure

```text
08-memory-pool/
│
├── CMakeLists.txt
├── README.md
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
│
└── build/
```

## 2.2 Responsibilities of Each Directory

| Directory/File | Responsibility |
|---|---|
| `include/` | Public class declarations |
| `src/` | MemoryPool implementation |
| `tests/` | Unit tests and validation |
| `benchmarks/` | Performance comparison |
| `docs/` | Technical documentation |
| `build/` | Generated build files and executables |
| `CMakeLists.txt` | Project build configuration |

---

# 3. Core Memory Management Concepts

## 3.1 Stack Memory vs Heap Memory

### Stack Memory

Stack memory is generally used for local variables and function call management.

Example:

```cpp
int value = 100;
```

The variable is automatically managed according to its scope and lifetime.

### Heap Memory

Dynamic memory allocation allows a program to request memory at runtime.

Example:

```cpp
int* value = new int(100);

delete value;
```

The programmer is responsible for managing the dynamically allocated object's lifetime.

### Memory Pool

A Memory Pool uses dynamically allocated memory as its backing storage.

Conceptually:

```text
General-purpose Heap
        |
        | Allocate one large region
        v
+-----------------------------------+
|           Memory Pool             |
+-----------------------------------+
| Block 0 | Block 1 | Block 2 | ... |
+-----------------------------------+
```

The pool manages these blocks internally.

---

# 4. Raw Memory vs Objects

This is one of the most important concepts in this project.

## 4.1 Allocating Raw Memory

Consider:

```cpp
void* memory = ::operator new(100);
```

This requests 100 bytes of raw storage.

It does not automatically construct an object of a particular class.

The returned pointer is `void*`, which means it does not carry a specific object type.

## 4.2 Constructing an Object

An object can be constructed inside previously allocated storage using placement new.

```cpp
void* memory = ::operator new(sizeof(int));

int* value = new (memory) int(100);
```

Here:

1. `::operator new` obtains raw memory.
2. Placement new constructs an integer in that memory.
3. `value` points to the constructed integer.

## 4.3 Destroying an Object

When an object is created using placement new, its destructor must be called explicitly when its lifetime ends.

```cpp
value->~int();
```

For a class type:

```cpp
object->~TestObject();
```

After destruction, the storage can be reused.

## 4.4 Important Difference

| Operation | Responsibility |
|---|---|
| `::operator new(size)` | Allocates raw storage |
| Placement new | Constructs an object in existing storage |
| Explicit destructor | Ends the object's lifetime |
| `pool.deallocate()` | Returns the storage block to the pool |
| `::operator delete()` | Releases the original raw allocation |

**Important:** Never use `delete` on an object created using placement new in a MemoryPool block. Destroy the object explicitly and return its block to the pool.

---

# 5. MemoryPool Design

## 5.1 Fixed-Size Block Allocation

Our initial implementation uses fixed-size blocks.

The constructor accepts:

```cpp
MemoryPool(std::size_t blockSize,
           std::size_t blockCount);
```

Example:

```cpp
MemoryPool pool(32, 100);
```

This creates a pool with:

- Requested block size: 32 bytes
- Number of blocks: 100
- Total requested storage: 3200 bytes, assuming no size adjustment

The actual internal block size may be larger because of alignment and FreeBlock requirements.

## 5.2 Memory Layout

Conceptually:

```text
Memory Pool
+----------+----------+----------+----------+
| Block 0  | Block 1  | Block 2  | Block 3  |
+----------+----------+----------+----------+
     |          |          |          |
     v          v          v          v
  FreeBlock  FreeBlock  FreeBlock  FreeBlock
```

Initially, all blocks are available.

After allocation, a block is removed from the Free List and becomes available to the caller.

---

# 6. Free List Data Structure

## 6.1 What Is a Free List?

A Free List is a linked list that maintains the addresses of currently available memory blocks.

Our implementation uses an intrusive singly linked list.

The word intrusive means that the linked-list metadata is stored inside the free memory blocks themselves.

## 6.2 FreeBlock Structure

```cpp
struct FreeBlock
{
    FreeBlock* next;
};
```

Each free block stores the address of the next available block.

Conceptual representation:

```text
m_freeList
    |
    v
+----------+      +----------+      +----------+
| Block A  | ---> | Block B  | ---> | Block C  |
| next     |      | next     |      | next     |
+----------+      +----------+      +----------+
                                      |
                                     null
```

## 6.3 Why Store the Pointer Inside the Block?

A separate linked-list node allocation would require additional memory.

Instead, the unused block itself stores the `next` pointer.

When a block is allocated, the FreeBlock metadata is no longer needed, and the same storage can be used by the caller.

## 6.4 Important Requirement

Each block must be large enough to hold the FreeBlock structure.

Therefore:

```cpp
std::size_t requiredSize =
    (blockSize > sizeof(FreeBlock))
        ? blockSize
        : sizeof(FreeBlock);
```

This ensures:

```text
Actual block size >= sizeof(FreeBlock)
```

---

# 7. Memory Alignment

## 7.1 What Is Alignment?

Alignment describes the memory-address boundaries at which a data type should be stored.

Different types can have different alignment requirements.

Example:

```cpp
alignof(int)
alignof(double)
alignof(std::max_align_t)
```

The `alignof` operator returns the alignment requirement of a type.

## 7.2 Why Is Alignment Important?

A memory address that does not satisfy a type's alignment requirement may result in undefined behavior when an object of that type is created there.

Therefore, our MemoryPool must ensure that its blocks have suitable alignment.

## 7.3 alignUp()

Our implementation uses:

```cpp
static std::size_t alignUp(
    std::size_t size,
    std::size_t alignment
);
```

Its purpose is to round a size up to the next multiple of an alignment value.

Example:

```text
Requested size = 17
Alignment      = 16
Aligned size   = 32
```

The formula is:

```text
remainder = size % alignment

if remainder == 0:
    aligned size = size
else:
    padding = alignment - remainder
    aligned size = size + padding
```

## 7.4 Why Check for Overflow?

Before adding padding, the implementation checks:

```cpp
if (size >
    std::numeric_limits<std::size_t>::max() - padding)
{
    throw std::bad_array_new_length();
}
```

This prevents unsigned integer overflow.

Without this check, a large size could wrap around and produce an incorrect smaller allocation size.

## 7.5 Block Stride Alignment

The starting address of the memory region must be suitably aligned.

Additionally, every block must start at an aligned address.

Our implementation rounds the block size to:

```cpp
alignof(std::max_align_t)
```

This supports fundamental alignment requirements, but does not provide a general guarantee for over-aligned types.

---

# 8. C++ Types Used in Memory Management

## 8.1 std::size_t

`std::size_t` is an unsigned integer type used to represent object sizes and array indices.

Examples:

```cpp
std::size_t blockSize = 32;
std::size_t blockCount = 100;
```

It is suitable for memory sizes because it can represent the size of objects supported by the implementation.

## 8.2 void*

A `void*` is a generic object pointer.

It can hold the address of different object types.

Example:

```cpp
void* memory = pool.allocate();
```

It does not provide direct typed access to an object.

A conversion is required before accessing a typed object.

## 8.3 std::byte*

`std::byte` represents a byte of storage.

Example:

```cpp
auto* start =
    static_cast<std::byte*>(m_memory);
```

Pointer arithmetic on `std::byte*` operates in byte-sized steps.

```cpp
std::byte* secondBlock =
    start + m_blockSize;
```

This moves the pointer forward by `m_blockSize` bytes.

## 8.4 static_cast

Example:

```cpp
static_cast<std::byte*>(m_memory);
```

This converts a `void*` into a `std::byte*`.

## 8.5 reinterpret_cast

Example:

```cpp
reinterpret_cast<std::uintptr_t>(m_memory);
```

This converts the pointer into an integer representation capable of holding a pointer value when `std::uintptr_t` is available.

Our implementation uses it to calculate a block's offset from the pool's base address.

This approach is practical on our target platforms, but integer-based address checks are not a universal guarantee of pointer-provenance portability.

---

# 9. MemoryPool Class Design

## 9.1 Header File

File:

```text
include/memory_pool/MemoryPool.hpp
```

The class exposes:

```cpp
class MemoryPool
{
public:
    explicit MemoryPool(
        std::size_t blockSize,
        std::size_t blockCount
    );

    ~MemoryPool();

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;

    void* allocate();
    void deallocate(void* ptr);

private:
    struct FreeBlock
    {
        FreeBlock* next;
    };

    static std::size_t alignUp(
        std::size_t size,
        std::size_t alignment
    );

    void initializeFreeList();

    std::size_t getBlockIndex(void* ptr) const;

    void* m_memory;
    std::size_t m_blockSize;
    std::size_t m_blockCount;
    FreeBlock* m_freeList;

    std::vector<unsigned char> m_allocated;
};
```

## 9.2 Member Variables

| Member | Purpose |
|---|---|
| `m_memory` | Base address of the allocated memory region |
| `m_blockSize` | Actual aligned size of each block |
| `m_blockCount` | Total number of blocks |
| `m_freeList` | Head of the Free List |
| `m_allocated` | Tracks whether each block is allocated |

## 9.3 Why Is `m_allocated` Required?

The allocation tracking array helps detect:

- Double deallocation
- Deallocation of a block that was never allocated
- Returning an already free block

Each array element represents one block.

```text
m_allocated:

Index:     0  1  2  3  4
State:     1  0  1  0  1
```

Here:

- `1` means allocated.
- `0` means free.

The tracking array uses one byte per block in this implementation.

## 9.4 Why Disable Copy and Move?

A MemoryPool owns a raw memory allocation.

If two MemoryPool objects shared the same `m_memory` pointer, both destructors could attempt to release the same allocation.

This could cause double-free and memory corruption.

Therefore:

```cpp
MemoryPool(const MemoryPool&) = delete;
MemoryPool& operator=(const MemoryPool&) = delete;
```

Copy operations are disabled.

Move operations are also disabled in this initial implementation to keep ownership rules simple.

---

# 10. Constructor Implementation

The constructor is responsible for preparing the complete memory pool.

Its major responsibilities are:

1. Validate constructor arguments.
2. Calculate the minimum required block size.
3. Align the block size.
4. Check multiplication overflow.
5. Initialize allocation tracking.
6. Allocate the backing memory.
7. Initialize the Free List.

## 10.1 Validate Arguments

```cpp
if (blockSize == 0 || blockCount == 0)
{
    throw std::invalid_argument(
        "Block size and block count must be greater than zero"
    );
}
```

A pool with zero-sized blocks or zero blocks is not useful in this implementation.

## 10.2 Calculate Required Block Size

```cpp
std::size_t requiredSize =
    (blockSize > sizeof(FreeBlock))
        ? blockSize
        : sizeof(FreeBlock);
```

This ensures each free block can store its linked-list pointer.

## 10.3 Calculate Aligned Block Size

```cpp
m_blockSize = alignUp(
    requiredSize,
    alignof(std::max_align_t)
);
```

This creates a block stride suitable for fundamental alignment.

## 10.4 Check Total Size Overflow

```cpp
if (m_blockCount >
    std::numeric_limits<std::size_t>::max() / m_blockSize)
{
    throw std::bad_array_new_length();
}
```

This protects the multiplication:

```cpp
m_blockSize * m_blockCount
```

## 10.5 Allocate Backing Memory

```cpp
std::size_t totalSize =
    m_blockSize * m_blockCount;

m_memory = ::operator new(totalSize);
```

This allocates one large raw memory region.

The global allocation function is used explicitly.

---

# 11. Initializing the Free List

Function:

```cpp
void MemoryPool::initializeFreeList()
```

Implementation:

```cpp
auto* start =
    static_cast<std::byte*>(m_memory);

m_freeList = nullptr;

for (std::size_t i = 0; i < m_blockCount; ++i)
{
    std::byte* address =
        start + (i * m_blockSize);

    FreeBlock* block =
        ::new (static_cast<void*>(address))
            FreeBlock{nullptr};

    block->next = m_freeList;
    m_freeList = block;
}
```

## 11.1 Step-by-Step Execution

Suppose the pool has four blocks:

```text
Block 0
Block 1
Block 2
Block 3
```

Initially:

```text
m_freeList = nullptr
```

After initializing Block 0:

```text
Block 0 -> nullptr
```

After initializing Block 1:

```text
Block 1 -> Block 0 -> nullptr
```

After initializing Block 2:

```text
Block 2 -> Block 1 -> Block 0 -> nullptr
```

After initializing Block 3:

```text
Block 3 -> Block 2 -> Block 1 -> Block 0 -> nullptr
```

The final Free List contains all blocks.

## 11.2 Why Placement New Is Used Here

```cpp
::new (static_cast<void*>(address))
    FreeBlock{nullptr};
```

This constructs a `FreeBlock` object inside the existing memory region.

The `FreeBlock` objects are needed because the Free List stores pointers to them.

---

# 12. Block Allocation

Function:

```cpp
void* MemoryPool::allocate()
```

Implementation:

```cpp
if (m_freeList == nullptr)
{
    throw std::bad_alloc();
}

FreeBlock* block = m_freeList;

m_freeList = m_freeList->next;

std::size_t index = getBlockIndex(
    static_cast<void*>(block)
);

m_allocated[index] = 1;

return static_cast<void*>(block);
```

## 12.1 Allocation Flow

Suppose:

```text
Free List:

Block 3 -> Block 2 -> Block 1 -> Block 0
```

When `allocate()` is called:

1. Save the current head.
2. Move the head to the next block.
3. Find the allocated block's index.
4. Mark the block as allocated.
5. Return the block's address.

After allocation:

```text
Returned block: Block 3

Remaining Free List:

Block 2 -> Block 1 -> Block 0
```

## 12.2 Why Check for nullptr?

```cpp
if (m_freeList == nullptr)
```

If the Free List is empty, there are no available blocks.

The function throws:

```cpp
std::bad_alloc
```

This prevents returning invalid memory.

## 12.3 Allocation Complexity

Free List removal happens at the head.

Time complexity:

```text
O(1)
```

---

# 13. Finding a Block Index

Function:

```cpp
std::size_t MemoryPool::getBlockIndex(void* ptr) const
```

This function validates the supplied pointer and identifies which pool block it represents.

## 13.1 Validation Steps

The function checks:

1. Pointer is not null.
2. Pointer is not before the pool's base address.
3. Pointer is inside the pool's memory region.
4. Pointer is located exactly at a block boundary.

## 13.2 Calculate the Offset

Conceptually:

```text
offset = pointer address - base address
```

Then:

```text
block index = offset / block size
```

The implementation also checks:

```cpp
offset % m_blockSize == 0
```

This ensures the pointer refers to the beginning of a valid block rather than an interior address.

## 13.3 Why Is This Important?

Without pointer validation, a caller might accidentally return:

- An unrelated pointer.
- A pointer inside another block.
- An already freed block.

These errors can corrupt the Free List.

---

# 14. Block Deallocation

Function:

```cpp
void MemoryPool::deallocate(void* ptr)
```

The function returns an allocated block to the Free List.

## 14.1 Handle nullptr

```cpp
if (ptr == nullptr)
{
    return;
}
```

Deallocating `nullptr` is treated as a no-op.

## 14.2 Validate the Pointer

```cpp
std::size_t index = getBlockIndex(ptr);
```

This checks whether the pointer belongs to the pool and points to a valid block boundary.

## 14.3 Detect Double-Free

```cpp
if (m_allocated[index] == 0)
{
    throw std::invalid_argument(
        "Block is already free or was never allocated"
    );
}
```

The allocation tracking array helps detect invalid deallocation.

## 14.4 Return Block to Free List

```cpp
FreeBlock* block =
    ::new (ptr) FreeBlock{m_freeList};

m_freeList = block;

m_allocated[index] = 0;
```

The returned block becomes the new head of the Free List.

Conceptually:

Before deallocation:

```text
Free List:

Block 2 -> Block 1 -> Block 0

Allocated:

Block 3
```

After deallocating Block 3:

```text
Free List:

Block 3 -> Block 2 -> Block 1 -> Block 0
```

## 14.5 Why Placement New Is Used Again

The block might previously have contained an object of another type.

After the caller destroys that object, placement new constructs a new `FreeBlock` object in the same storage.

This allows the pool to reuse the block as Free List metadata.

**Important:** The caller must destroy any constructed object before calling `deallocate()`.

---

# 15. MemoryPool Destructor

Implementation:

```cpp
MemoryPool::~MemoryPool()
{
    ::operator delete(m_memory);
}
```

The pool owns one large memory allocation.

Therefore, it releases the entire region once when the MemoryPool object is destroyed.

It does not individually delete every block.

The allocation tracking vector manages its own memory through its destructor.

**Important:** The pool does not automatically invoke the destructors of user-created objects. All live objects must be destroyed before the pool itself is destroyed.

---

# 16. Object Lifecycle Using MemoryPool

Example:

```cpp
struct TestObject
{
    int value;

    explicit TestObject(int v)
        : value(v)
    {
    }

    ~TestObject()
    {
    }
};

int main()
{
    MemoryPool pool(sizeof(TestObject), 10);

    // Request a free memory block.
    void* memory = pool.allocate();

    // Construct an object inside that block.
    TestObject* object =
        new (memory) TestObject(100);

    // Use the object.
    std::cout << object->value << '\n';

    // Explicitly destroy the object.
    object->~TestObject();

    // Return the storage block to the pool.
    pool.deallocate(memory);
}
```

## 16.1 Complete Lifecycle

```text
Free Block
    |
    v
pool.allocate()
    |
    v
Raw Storage
    |
    v
Placement New
    |
    v
Constructed Object
    |
    v
Use Object
    |
    v
Explicit Destructor
    |
    v
Raw Storage Again
    |
    v
pool.deallocate()
    |
    v
Free Block
```

This distinction between storage management and object lifetime management is fundamental to understanding custom allocators.

---

# 17. Error Handling and Safety Checks

The implementation handles several invalid conditions.

| Condition | Behavior |
|---|---|
| Zero block size | Throws `std::invalid_argument` |
| Zero block count | Throws `std::invalid_argument` |
| Pool exhausted | Throws `std::bad_alloc` |
| Null deallocation | Returns without error |
| Foreign pointer | Throws `std::invalid_argument` |
| Pointer outside pool | Throws `std::invalid_argument` |
| Interior pointer | Throws `std::invalid_argument` |
| Double-free | Throws `std::invalid_argument` |
| Size multiplication overflow | Throws `std::bad_array_new_length` |
| Alignment calculation overflow | Throws `std::bad_array_new_length` |

These checks make the initial implementation safer than a simple unchecked Free List.

---

# 18. Unit Testing

## 18.1 Why Unit Tests?

A successful compilation only confirms that the program can be compiled and linked.

It does not prove that allocation and deallocation behave correctly.

Unit tests verify the expected behavior of individual operations and error conditions.

## 18.2 Test Cases Implemented

The test suite currently contains 12 individual test cases.

| No. | Test | Expected Behavior |
|---:|---|---|
| 1 | Valid constructor | Pool constructs successfully |
| 2 | Zero block size | Rejects invalid input |
| 3 | Zero block count | Rejects invalid input |
| 4 | Unique allocations | Returns distinct blocks |
| 5 | Pool exhaustion | Throws `std::bad_alloc` |
| 6 | Block reuse | Reuses a returned block |
| 7 | nullptr deallocation | Does not throw |
| 8 | Foreign pointer | Rejects external pointer |
| 9 | Interior pointer | Rejects non-boundary pointer |
| 10 | Double-free | Detects repeated deallocation |
| 11 | Placement new lifecycle | Constructor and destructor counts are correct |
| 12 | Copy/move disabled | Confirms ownership restrictions |

## 18.3 Placement New Lifecycle Test

The test object tracks constructor and destructor calls.

```cpp
struct TestObject
{
    static int constructorCount;
    static int destructorCount;

    int value;

    explicit TestObject(int v)
        : value(v)
    {
        ++constructorCount;
    }

    ~TestObject()
    {
        ++destructorCount;
    }
};
```

The test verifies:

- Object construction occurs.
- Object data is correct.
- Explicit destruction occurs.
- The block can be returned to the pool.

## 18.4 CTest Configuration

Root `CMakeLists.txt`:

```cmake
enable_testing()
add_subdirectory(src)
add_subdirectory(tests)
```

The actual project conditionally enables tests through:

```cmake
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

Test CMake configuration:

```cmake
add_executable(test_memory_pool
    test_memory_pool.cpp
)

target_link_libraries(test_memory_pool
    PRIVATE memory_pool
)

add_test(
    NAME MemoryPoolTests
    COMMAND test_memory_pool
)
```

## 18.5 Build and Run Tests

Configure:

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=C:/MinGW/bin/mingw32-make.exe
```

Build:

```bash
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Run the executable directly on Windows CMD:

```cmd
.\build\tests\test_memory_pool.exe
```

## 18.6 Actual Test Verification

The project was successfully configured and built.

Observed CTest output:

```text
Test project C:/Users/omkar/Cpp-Engineering-Projects/08-memory-pool/build

Start 1: MemoryPoolTests
1/1 Test #1: MemoryPoolTests .................. Passed

100% tests passed, 0 tests failed out of 1
```

**Interpretation:** The registered MemoryPool test suite passed.

The test suite contains 12 individual test cases, but the CTest summary reports one registered test executable.

---

# 19. Benchmarking

## 19.1 Why Benchmark?

A MemoryPool is designed to reduce repeated allocation overhead.

However, a custom allocator should not be assumed to be faster without measurement.

Benchmarking helps us compare the implementation against standard dynamic allocation.

## 19.2 Implemented Comparison

The benchmark compares:

1. Standard `new/delete`.
2. Custom `MemoryPool`.

Both use the same `BenchmarkObject` type.

## 19.3 Benchmark Object

```cpp
struct BenchmarkObject
{
    std::uint64_t value;
    std::uint64_t data[3];

    explicit BenchmarkObject(std::uint64_t v)
        : value(v), data{v, v + 1, v + 2}
    {
    }
};
```

The object contains:

- One 64-bit integer.
- Three additional 64-bit integers.

The same object is used in both benchmark methods.

## 19.4 Standard Allocation

```cpp
BenchmarkObject* object =
    new BenchmarkObject(i);
```

This performs dynamic allocation and object construction.

After use:

```cpp
delete object;
```

This destroys the object and releases its individual allocation.

## 19.5 MemoryPool Allocation

```cpp
void* memory = pool.allocate();

BenchmarkObject* object =
    new (memory) BenchmarkObject(i);
```

After use:

```cpp
object->~BenchmarkObject();

pool.deallocate(object);
```

The object is explicitly destroyed, and its storage is returned to the pool.

## 19.6 Benchmark Timing

The implementation uses:

```cpp
std::chrono::steady_clock
```

This provides a monotonic clock suitable for measuring elapsed time.

The measured duration is converted into milliseconds:

```cpp
std::chrono::duration<double, std::milli>
```

## 19.7 Benchmark Configuration

Current benchmark settings:

| Parameter | Value |
|---|---:|
| Objects per round | 200,000 |
| Benchmark rounds | 5 |
| Object type | BenchmarkObject |
| Clock | `steady_clock` |
| Reported result | Median duration |

A warm-up run is also performed before collecting the five measured rounds.

## 19.8 Why Use Multiple Rounds?

Execution time can vary due to:

- CPU scheduling
- Background processes
- Cache behavior
- Memory allocator state
- Operating system activity

Multiple rounds provide a more useful comparison than a single measurement.

The benchmark sorts the five recorded durations and reports the median.

## 19.9 Speedup Calculation

If MemoryPool is faster:

```text
Speedup = new/delete time / MemoryPool time
```

Example:

```text
new/delete = 20 ms
MemoryPool = 10 ms

Speedup = 20 / 10
        = 2x
```

This hypothetical result means the MemoryPool took half the time in that measurement.

If MemoryPool takes longer, the benchmark reports that `new/delete` was faster.

**Important:** Pool construction is outside the measured interval. The current benchmark measures repeated object allocation, construction, destruction, and deallocation—not the full cost of creating and destroying the pool.

The actual benchmark output must be recorded separately after running the benchmark on the target machine.

## 19.10 Run Benchmark

Build:

```cmd
cmake --build build
```

Run:

```cmd
.\build\benchmarks\benchmark_memory_pool.exe
```

---

# 20. CMake Build System

## 20.1 Root CMakeLists.txt

The project uses CMake 3.20 or newer.

The root configuration includes:

```cmake
cmake_minimum_required(VERSION 3.20)

project(MemoryPool
    VERSION 1.0.0
    DESCRIPTION "Custom Memory Pool Implementation in C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)

add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
```

## 20.2 C++17

The project requires C++17.

This is important because the implementation uses:

- `std::byte`
- Modern C++ allocation and type utilities

## 20.3 Build Options

Tests and benchmarks can be enabled or disabled independently.

Build without tests:

```bash
cmake -S . -B build -DBUILD_TESTS=OFF
```

Build without benchmarks:

```bash
cmake -S . -B build -DBUILD_BENCHMARKS=OFF
```

Build both:

```bash
cmake -S . -B build -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=ON
```

---

# 21. Complexity Analysis

| Operation | Time Complexity |
|---|---|
| Constructor / Free List initialization | O(N) |
| `allocate()` | O(1) |
| `deallocate()` | O(N) in this implementation |
| Destructor | O(1) for backing allocation release |

Here, N represents the number of blocks.

**Why is `deallocate()` O(N)?**

The current implementation calls `getBlockIndex()` to validate the pointer. It calculates the block index using address arithmetic, so the validation itself is O(1).

Therefore, the actual `deallocate()` operation is also O(1), assuming the normal constant-time operations of the tracking vector.

Corrected complexity:

| Operation | Time Complexity |
|---|---|
| Constructor / Free List initialization | O(N) |
| `allocate()` | O(1) |
| `deallocate()` | O(1) |
| Destructor | O(1) for backing allocation release |

The tracking vector uses O(N) additional memory.

The Free List uses O(1) additional metadata per free block, stored inside the blocks themselves.

---

# 22. Current Implementation Limitations

The current version is intentionally a fixed-capacity, single-threaded MemoryPool.

Limitations:

1. **Fixed capacity:** The pool cannot grow after construction.
2. **Fixed block size:** All blocks have the same internal size.
3. **No thread safety:** Concurrent access requires synchronization.
4. **No automatic object destruction:** Callers must destroy placement-new objects.
5. **Fundamental alignment only:** Over-aligned types are not generally supported.
6. **No automatic type safety:** The pool returns `void*`; the caller is responsible for constructing the correct type.
7. **Pointer validation limitation:** Integer-based address validation is practical for the current targets but is not a universal pointer-provenance guarantee.
8. **No automatic leak detection for live objects:** The pool does not destroy objects that remain alive when the pool is destroyed.

---

# 23. Important Engineering Lessons

Through this project, the following C++ systems concepts were implemented and explored:

- Dynamic memory allocation
- Raw storage management
- Object lifetime
- Placement new
- Explicit destructor calls
- Intrusive singly linked lists
- Free List allocation strategy
- Pointer arithmetic
- `void*` and `std::byte*`
- `static_cast` and `reinterpret_cast`
- Memory alignment
- Integer overflow checks
- Exception handling
- RAII and resource ownership
- Deleted copy and move operations
- Allocation tracking
- Unit testing with CTest
- Build configuration with CMake
- Performance benchmarking with `std::chrono`

---

# 24. Future Improvements

The following improvements are not part of the current implementation yet.

| No. | Improvement | Purpose |
|---:|---|---|
| 1 | Alignment tests | Verify block alignment |
| 2 | Stress tests | Validate repeated allocation/deallocation |
| 3 | Thread-safe allocation | Support concurrent access |
| 4 | Dynamic pool growth | Allocate additional regions when exhausted |
| 5 | Typed allocation API | Simplify object construction |
| 6 | Over-aligned allocation | Support types with extended alignment |
| 7 | Custom allocator integration | Integrate with STL containers |
| 8 | Sanitizer testing | Detect memory errors |
| 9 | Better benchmark methodology | Test different object sizes and workloads |
| 10 | Move support | Transfer pool ownership safely |

These are possible future extensions, not claims about existing functionality.

---

# 25. Project Completion Checklist

## Core Implementation

- [x] MemoryPool class design
- [x] Fixed-size block allocation
- [x] Backing memory allocation
- [x] Alignment calculation
- [x] Free List initialization
- [x] O(1) block allocation
- [x] O(1) block deallocation
- [x] Allocation tracking
- [x] Double-free protection
- [x] Foreign pointer validation
- [x] Copy and move disabled
- [x] Memory cleanup

## Testing

- [x] Constructor validation
- [x] Allocation tests
- [x] Pool exhaustion
- [x] Block reuse
- [x] Invalid pointer tests
- [x] Double-free test
- [x] Placement new lifecycle test
- [x] Copy/move restriction test
- [x] CTest execution successful

## Benchmarking

- [x] Standard `new/delete` benchmark
- [x] MemoryPool benchmark
- [x] Multiple benchmark rounds
- [x] Median calculation
- [x] Speedup calculation
- [ ] Actual benchmark result recorded

---

# 26. Final Project Summary

The Custom Memory Pool project demonstrates how a program can manage a preallocated memory region using a Free List.

Instead of performing an individual heap allocation for every object, the implementation divides one large allocation into fixed-size blocks and reuses those blocks.

The project covers important low-level C++ concepts, especially raw memory allocation, placement new, object lifetime, alignment, pointer arithmetic, and resource ownership.

The current implementation has been successfully compiled and linked using CMake, and the registered CTest suite has passed.

The benchmark implementation has also been prepared to compare standard `new/delete` with the custom MemoryPool. Actual performance results should be recorded after executing the benchmark.

**Project Status:** Core implementation and initial unit testing completed. Benchmark results and additional robustness testing remain to be verified.