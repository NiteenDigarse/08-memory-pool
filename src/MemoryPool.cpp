
#include "memory_pool/MemoryPool.hpp"

#include <new>
#include <limits>
#include <stdexcept>
#include <cstdint>


// =============================================================
// HELPER FUNCTION: alignUp()
// =============================================================
//
// WHY:
//
// Memory blocks must begin at properly aligned addresses.
//
// Some data types require their memory addresses to satisfy
// specific alignment requirements.
//
// This function calculates the smallest size that is
// greater than or equal to the requested size and is
// divisible by the specified alignment.
//
// MATHEMATICAL FORMULA:
//
// remainder = size % alignment
//
// padding = alignment - remainder
//
// alignedSize = size + padding
//
// EXAMPLE:
//
// size = 13
// alignment = 8
//
// remainder = 13 % 8 = 5
// padding = 8 - 5 = 3
//
// alignedSize = 13 + 3 = 16
//
// IMPORTANT:
//
// We check overflow before adding padding.
//
// This formula also works when size is already aligned.
// In that case, padding is zero.
//
// =============================================================

std::size_t MemoryPool::alignUp(std::size_t size,
                                std::size_t alignment)
{
    // Alignment must never be zero.
    if (alignment == 0)
    {
        throw std::invalid_argument(
            "Alignment cannot be zero"
        );
    }

    // Find the remainder after division.
    std::size_t remainder = size % alignment;

    // Already aligned: no adjustment required.
    if (remainder == 0)
    {
        return size;
    }

    // Calculate additional bytes required.
    std::size_t padding = alignment - remainder;

    // Prevent integer overflow during addition.
    if (size >
        std::numeric_limits<std::size_t>::max() - padding)
    {
        throw std::bad_array_new_length();
    }

    return size + padding;
}



// =============================================================
// CONSTRUCTOR
// =============================================================
//
// RESPONSIBILITIES:
//
// 1. Validate constructor arguments.
// 2. Ensure enough space for FreeBlock metadata.
// 3. Calculate properly aligned block size.
// 4. Prevent integer overflow.
// 5. Prepare allocation tracking.
// 6. Allocate one large memory region.
// 7. Initialize the Free List.
//
// =============================================================

MemoryPool::MemoryPool(std::size_t blockSize,
                       std::size_t blockCount)

    : m_memory(nullptr),
      m_blockSize(0),
      m_blockCount(blockCount),
      m_freeList(nullptr),
      m_allocated()
{
    // ---------------------------------------------------------
    // STEP 1: Validate constructor arguments.
    // ---------------------------------------------------------

    if (blockSize == 0 || blockCount == 0)
    {
        throw std::invalid_argument(
            "Block size and block count must be greater than zero"
        );
    }


    // ---------------------------------------------------------
    // STEP 2: Determine minimum required block size.
    // ---------------------------------------------------------
    //
    // Every free block must be large enough to store
    // its FreeBlock::next pointer.
    //
    // Example:
    //
    // Requested block size = 4 bytes
    // sizeof(FreeBlock)   = 8 bytes (typical 64-bit system)
    //
    // Required size = 8 bytes.
    //
    // ---------------------------------------------------------

    std::size_t requiredSize =
        (blockSize > sizeof(FreeBlock))
            ? blockSize
            : sizeof(FreeBlock);


    // ---------------------------------------------------------
    // STEP 3: Adjust block size according to alignment.
    // ---------------------------------------------------------
    //
    // std::max_align_t represents the fundamental alignment
    // requirement supported by ordinary operator new.
    //
    // Each block's starting address must maintain this
    // alignment.
    //
    // This version supports types with fundamental alignment.
    //
    // ---------------------------------------------------------

    m_blockSize = alignUp(
        requiredSize,
        alignof(std::max_align_t)
    );


    // ---------------------------------------------------------
    // STEP 4: Prevent integer overflow.
    // ---------------------------------------------------------
    //
    // Total memory = block size * block count
    //
    // Before multiplication, verify that the result
    // can fit inside std::size_t.
    //
    // ---------------------------------------------------------

    if (m_blockCount >
        std::numeric_limits<std::size_t>::max() / m_blockSize)
    {
        throw std::bad_array_new_length();
    }


    // ---------------------------------------------------------
    // STEP 5: Prepare allocation tracking.
    // ---------------------------------------------------------
    //
    // Initially, every block is free.
    //
    // 0 = Free
    // 1 = Allocated
    //
    // vector allocation can throw, so prepare it before
    // allocating the raw memory region.
    //
    // ---------------------------------------------------------

    m_allocated.resize(m_blockCount, 0);


    // ---------------------------------------------------------
    // STEP 6: Calculate total memory required.
    // ---------------------------------------------------------

    std::size_t totalSize =
        m_blockSize * m_blockCount;


    // ---------------------------------------------------------
    // STEP 7: Allocate one large raw memory region.
    // ---------------------------------------------------------
    //
    // Global operator new allocates raw storage with
    // default-new alignment.
    //
    // Our block stride is a multiple of max_align_t alignment,
    // so each block maintains fundamental alignment.
    //
    // ---------------------------------------------------------

    m_memory = ::operator new(totalSize);


    // ---------------------------------------------------------
    // STEP 8: Initialize Free List.
    // ---------------------------------------------------------
    //
    // Divide the allocated memory into blocks and connect
    // all blocks using the Free List.
    //
    // initializeFreeList() performs no dynamic allocations.
    //
    // ---------------------------------------------------------

    initializeFreeList();
}



// =============================================================
// HELPER FUNCTION: initializeFreeList()
// =============================================================
//
// PURPOSE:
//
// Divide one large memory region into fixed-size blocks.
//
// Every block will contain a FreeBlock object while free.
//
// Example:
//
// Block 1 -> Block 2 -> Block 3 -> Block 4 -> nullptr
//
// This implementation inserts each new block at the
// beginning of the Free List.
//
// Therefore, final order is reversed.
//
// =============================================================

void MemoryPool::initializeFreeList()
{
    // Convert raw memory into byte-addressable memory.
    //
    // std::byte* allows pointer arithmetic in bytes.
    //
    auto* start =
        static_cast<std::byte*>(m_memory);


    // Initially, the Free List is empty.
    m_freeList = nullptr;


    // Visit every memory block.
    for (std::size_t i = 0; i < m_blockCount; ++i)
    {
        // Calculate the starting address of the current block.
        //
        // address = base address + (index * block size)
        //
        std::byte* address =
            start + (i * m_blockSize);


        // Construct a FreeBlock object in the existing memory.
        //
        // Placement new does not allocate another memory region.
        //
        FreeBlock* block =
            ::new (static_cast<void*>(address))
                FreeBlock{nullptr};


        // Connect the current block to the existing Free List.
        block->next = m_freeList;


        // Make the current block the new Free List head.
        m_freeList = block;
    }
}



// =============================================================
// HELPER FUNCTION: getBlockIndex()
// =============================================================
//
// PURPOSE:
//
// Verify that a pointer belongs to this MemoryPool.
//
// Also verify that it points to the beginning of a block.
//
// This helps prevent:
//
// 1. Foreign pointers.
// 2. Interior pointers.
// 3. Invalid memory addresses.
//
// std::uintptr_t is used to compare numeric address values.
//
// =============================================================

std::size_t MemoryPool::getBlockIndex(void* ptr) const
{
    if (ptr == nullptr)
    {
        throw std::invalid_argument(
            "Cannot use a null pointer"
        );
    }


    // Convert addresses into integer representations.
    std::uintptr_t baseAddress =
        reinterpret_cast<std::uintptr_t>(m_memory);

    std::uintptr_t currentAddress =
        reinterpret_cast<std::uintptr_t>(ptr);


    // Reject addresses before the pool's starting address.
    if (currentAddress < baseAddress)
    {
        throw std::invalid_argument(
            "Pointer does not belong to this MemoryPool"
        );
    }


    // Calculate offset from the beginning of the pool.
    std::uintptr_t offset =
        currentAddress - baseAddress;


    // Calculate total allocated memory.
    //
    // This multiplication is safe because the constructor
    // already checked for overflow.
    //
    std::size_t totalSize =
        m_blockSize * m_blockCount;


    // Reject addresses outside the allocated memory region.
    if (offset >= totalSize)
    {
        throw std::invalid_argument(
            "Pointer is outside MemoryPool"
        );
    }


    // A valid pointer must point to a block boundary.
    //
    // Interior addresses are not valid block pointers.
    //
    if (offset % m_blockSize != 0)
    {
        throw std::invalid_argument(
            "Pointer is not at a valid block boundary"
        );
    }


    // Convert the offset into a block index.
    return static_cast<std::size_t>(
        offset / m_blockSize
    );
}



// =============================================================
// MEMORY ALLOCATION: allocate()
// =============================================================
//
// PURPOSE:
//
// Remove the first available block from the Free List
// and return its address to the caller.
//
// FLOW:
//
// 1. Check whether Free List is empty.
// 2. Get the current head.
// 3. Move head to the next block.
// 4. Mark the block as allocated.
// 5. Return its address.
//
// =============================================================

void* MemoryPool::allocate()
{
    // No available blocks.
    if (m_freeList == nullptr)
    {
        throw std::bad_alloc();
    }


    // Get the first available block.
    FreeBlock* block = m_freeList;


    // Move the Free List head to the next block.
    m_freeList = m_freeList->next;


    // Determine which block was allocated.
    std::size_t index = getBlockIndex(
        static_cast<void*>(block)
    );


    // Mark this block as allocated.
    m_allocated[index] = 1;


    // Return the block address as a generic pointer.
    return static_cast<void*>(block);
}



// =============================================================
// MEMORY DEALLOCATION: deallocate()
// =============================================================
//
// PURPOSE:
//
// Return a previously allocated block to the Free List.
//
// FLOW:
//
// 1. Ignore nullptr.
// 2. Validate the pointer.
// 3. Check allocation status.
// 4. Construct a FreeBlock object again.
// 5. Insert it at the beginning of the Free List.
// 6. Mark the block as free.
//
// IMPORTANT:
//
// If the caller used placement new to construct an object,
// the caller must explicitly call that object's destructor
// before calling deallocate().
//
// =============================================================

void MemoryPool::deallocate(void* ptr)
{
    // Null pointers are safely ignored.
    if (ptr == nullptr)
    {
        return;
    }


    // Verify ownership and block boundary.
    std::size_t index = getBlockIndex(ptr);


    // Check whether this block is already free.
    //
    // This prevents double-free and deallocating
    // a block that was never allocated.
    //
    if (m_allocated[index] == 0)
    {
        throw std::invalid_argument(
            "Block is already free or was never allocated"
        );
    }


    // Construct a new FreeBlock object in the returned storage.
    //
    // The caller must have ended the lifetime of any
    // object previously constructed in this block.
    //
    FreeBlock* block =
        ::new (ptr) FreeBlock{m_freeList};


    // Update the Free List head.
    m_freeList = block;


    // Mark this block as free.
    m_allocated[index] = 0;
}



// =============================================================
// DESTRUCTOR
// =============================================================
//
// PURPOSE:
//
// Release the entire memory region allocated by this pool.
//
// Individual blocks must not be deleted separately.
//
// The whole memory region is released in one operation.
//
// =============================================================

MemoryPool::~MemoryPool()
{
    // Release the allocated raw memory.
    //
    // This matches the global operator new used
    // in the constructor.
    //
    ::operator delete(m_memory);
}
