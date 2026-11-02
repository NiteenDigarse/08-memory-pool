
#include "memory_pool/MemoryPool.hpp"

#include <iostream>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <cstddef>

// ============================================================
// SIMPLE TESTING FRAMEWORK
// ============================================================

int totalTests = 0;
int passedTests = 0;
int failedTests = 0;

// Prints the result of each test.
void reportTest(const char* testName, bool passed)
{
    ++totalTests;

    if (passed)
    {
        ++passedTests;
        std::cout << "[PASS] " << testName << '\n';
    }
    else
    {
        ++failedTests;
        std::cout << "[FAIL] " << testName << '\n';
    }
}

// Checks whether an operation throws the expected exception.
template <typename Exception, typename Function>
bool throwsException(Function&& function)
{
    try
    {
        function();
    }
    catch (const Exception&)
    {
        return true;
    }
    catch (...)
    {
        return false;
    }

    return false;
}


// ============================================================
// TEST OBJECT
// ============================================================
//
// Used to verify placement new and object lifetime.
//
// Constructor increments constructorCount.
// Destructor increments destructorCount.
//
// ============================================================

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

int TestObject::constructorCount = 0;
int TestObject::destructorCount = 0;


// ============================================================
// TEST 1: VALID CONSTRUCTOR
// ============================================================

void testValidConstructor()
{
    bool passed = false;

    try
    {
        MemoryPool pool(16, 5);
        passed = true;
    }
    catch (...)
    {
        passed = false;
    }

    reportTest("Valid constructor", passed);
}


// ============================================================
// TEST 2: ZERO BLOCK SIZE
// ============================================================

void testZeroBlockSize()
{
    bool passed = throwsException<std::invalid_argument>(
        []()
        {
            MemoryPool pool(0, 5);
        }
    );

    reportTest("Zero block size rejection", passed);
}


// ============================================================
// TEST 3: ZERO BLOCK COUNT
// ============================================================

void testZeroBlockCount()
{
    bool passed = throwsException<std::invalid_argument>(
        []()
        {
            MemoryPool pool(16, 0);
        }
    );

    reportTest("Zero block count rejection", passed);
}


// ============================================================
// TEST 4: UNIQUE MEMORY ALLOCATIONS
// ============================================================
//
// Every allocation should return a different block address.
//
// ============================================================

void testUniqueAllocations()
{
    MemoryPool pool(16, 5);

    void* block1 = pool.allocate();
    void* block2 = pool.allocate();
    void* block3 = pool.allocate();

    bool passed =
        block1 != nullptr &&
        block2 != nullptr &&
        block3 != nullptr &&
        block1 != block2 &&
        block2 != block3 &&
        block1 != block3;

    reportTest("Unique block allocations", passed);
}


// ============================================================
// TEST 5: POOL EXHAUSTION
// ============================================================
//
// Pool contains only 3 blocks.
//
// After allocating all 3 blocks, the next allocation
// should throw std::bad_alloc.
//
// ============================================================

void testPoolExhaustion()
{
    MemoryPool pool(16, 3);

    void* block1 = pool.allocate();
    void* block2 = pool.allocate();
    void* block3 = pool.allocate();

    bool allAllocated =
        block1 != nullptr &&
        block2 != nullptr &&
        block3 != nullptr;

    bool throwsBadAlloc =
        throwsException<std::bad_alloc>(
            [&pool]()
            {
                pool.allocate();
            }
        );

    reportTest(
        "Pool exhaustion handling",
        allAllocated && throwsBadAlloc
    );
}


// ============================================================
// TEST 6: DEALLOCATION AND REUSE
// ============================================================
//
// Allocate one block.
// Deallocate it.
// Allocate again.
//
// The pool should reuse the returned block.
//
// ============================================================

void testBlockReuse()
{
    MemoryPool pool(16, 3);

    void* original = pool.allocate();

    pool.deallocate(original);

    void* reused = pool.allocate();

    reportTest(
        "Deallocated block reuse",
        original == reused
    );
}


// ============================================================
// TEST 7: NULLPTR DEALLOCATION
// ============================================================
//
// deallocate(nullptr) should safely return without error.
//
// ============================================================

void testNullptrDeallocation()
{
    MemoryPool pool(16, 3);

    bool passed = true;

    try
    {
        pool.deallocate(nullptr);
    }
    catch (...)
    {
        passed = false;
    }

    reportTest("nullptr deallocation", passed);
}


// ============================================================
// TEST 8: FOREIGN POINTER REJECTION
// ============================================================
//
// A pointer belonging to another memory location
// must not be accepted by this pool.
//
// ============================================================

void testForeignPointer()
{
    MemoryPool pool(16, 3);

    int externalValue = 100;

    bool passed =
        throwsException<std::invalid_argument>(
            [&pool, &externalValue]()
            {
                pool.deallocate(&externalValue);
            }
        );

    reportTest("Foreign pointer rejection", passed);
}


// ============================================================
// TEST 9: INTERIOR POINTER REJECTION
// ============================================================
//
// Only the starting address of a block is valid.
//
// A pointer to the middle of a block must be rejected.
//
// ============================================================

void testInteriorPointer()
{
    MemoryPool pool(32, 3);

    void* block = pool.allocate();

    auto* bytes = static_cast<std::byte*>(block);

    void* invalidPointer = bytes + 1;

    bool passed =
        throwsException<std::invalid_argument>(
            [&pool, invalidPointer]()
            {
                pool.deallocate(invalidPointer);
            }
        );

    // Return the actual block correctly.
    pool.deallocate(block);

    reportTest("Interior pointer rejection", passed);
}


// ============================================================
// TEST 10: DOUBLE-FREE PROTECTION
// ============================================================
//
// Returning the same block twice must be rejected.
//
// ============================================================

void testDoubleFree()
{
    MemoryPool pool(16, 3);

    void* block = pool.allocate();

    pool.deallocate(block);

    bool passed =
        throwsException<std::invalid_argument>(
            [&pool, block]()
            {
                pool.deallocate(block);
            }
        );

    reportTest("Double-free protection", passed);
}


// ============================================================
// TEST 11: PLACEMENT NEW AND OBJECT LIFETIME
// ============================================================
//
// MemoryPool allocates raw memory.
// Placement new constructs an object in that memory.
//
// The object destructor must be called explicitly
// before returning the block to the pool.
//
// ============================================================

void testPlacementNewLifecycle()
{
    TestObject::constructorCount = 0;
    TestObject::destructorCount = 0;

    MemoryPool pool(sizeof(TestObject), 2);

    void* memory = pool.allocate();

    TestObject* object =
        ::new (memory) TestObject(100);

    bool constructedCorrectly =
        object->value == 100 &&
        TestObject::constructorCount == 1;

    // Explicitly end the TestObject lifetime.
    object->~TestObject();

    bool destroyedCorrectly =
        TestObject::destructorCount == 1;

    // Return the now-free storage to MemoryPool.
    pool.deallocate(memory);

    reportTest(
        "Placement new object lifecycle",
        constructedCorrectly && destroyedCorrectly
    );
}


// ============================================================
// TEST 12: COPY AND MOVE OPERATIONS DISABLED
// ============================================================
//
// MemoryPool owns raw memory.
// Copying or moving is disabled in this version.
//
// Compile-time checks verify these restrictions.
//
// ============================================================

void testCopyAndMoveDisabled()
{
    constexpr bool copyConstructorDisabled =
        !std::is_copy_constructible<MemoryPool>::value;

    constexpr bool copyAssignmentDisabled =
        !std::is_copy_assignable<MemoryPool>::value;

    constexpr bool moveConstructorDisabled =
        !std::is_move_constructible<MemoryPool>::value;

    constexpr bool moveAssignmentDisabled =
        !std::is_move_assignable<MemoryPool>::value;

    bool passed =
        copyConstructorDisabled &&
        copyAssignmentDisabled &&
        moveConstructorDisabled &&
        moveAssignmentDisabled;

    reportTest("Copy and move operations disabled", passed);
}


// ============================================================
// MAIN TEST RUNNER
// ============================================================

int main()
{
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "       MEMORY POOL UNIT TESTS\n";
    std::cout << "========================================\n\n";

    testValidConstructor();

    testZeroBlockSize();

    testZeroBlockCount();

    testUniqueAllocations();

    testPoolExhaustion();

    testBlockReuse();

    testNullptrDeallocation();

    testForeignPointer();

    testInteriorPointer();

    testDoubleFree();

    testPlacementNewLifecycle();

    testCopyAndMoveDisabled();


    // --------------------------------------------------------
    // FINAL TEST SUMMARY
    // --------------------------------------------------------

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "             TEST SUMMARY\n";
    std::cout << "========================================\n";

    std::cout << "Total Tests : " << totalTests << '\n';
    std::cout << "Passed      : " << passedTests << '\n';
    std::cout << "Failed      : " << failedTests << '\n';

    std::cout << "========================================\n";

    if (failedTests == 0)
    {
        std::cout << "\nALL TESTS PASSED!\n";
        return 0;
    }

    std::cout << "\nSOME TESTS FAILED!\n";
    return 1;
}
