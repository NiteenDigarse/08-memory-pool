
#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include <cstddef>
#include <vector>

class MemoryPool
{
public:

    // =========================================================
    // CONSTRUCTOR
    // =========================================================
    //
    // blockSize  = Requested size of each memory block.
    // blockCount = Total number of blocks in the pool.
    //
    // Constructor allocates one large memory region and
    // divides it into fixed-size blocks.
    //
    explicit MemoryPool(std::size_t blockSize,
                        std::size_t blockCount);


    // =========================================================
    // DESTRUCTOR
    // =========================================================
    //
    // Releases the entire memory region allocated by this pool.
    //
    ~MemoryPool();


    // =========================================================
    // COPY AND MOVE OPERATIONS DISABLED
    // =========================================================
    //
    // MemoryPool owns raw memory.
    //
    // Copying would make two objects own the same memory,
    // potentially causing double deletion.
    //
    // Moving is also disabled in this initial implementation.
    //

    MemoryPool(const MemoryPool&) = delete;

    MemoryPool& operator=(const MemoryPool&) = delete;

    MemoryPool(MemoryPool&&) = delete;

    MemoryPool& operator=(MemoryPool&&) = delete;


    // =========================================================
    // MEMORY ALLOCATION
    // =========================================================
    //
    // Returns one available memory block.
    //
    // Throws std::bad_alloc if no blocks are available.
    //
    void* allocate();


    // =========================================================
    // MEMORY DEALLOCATION
    // =========================================================
    //
    // Returns a previously allocated block to the Free List.
    //
    // Throws std::invalid_argument for invalid pointers
    // or blocks that have already been deallocated.
    //
    void deallocate(void* ptr);


private:

    // =========================================================
    // FREE BLOCK STRUCTURE
    // =========================================================
    //
    // Each free memory block stores a pointer to the next
    // available block.
    //
    // This is called an Intrusive Free List because
    // the linked-list metadata is stored inside free memory.
    //

    struct FreeBlock
    {
        FreeBlock* next;
    };


    // =========================================================
    // HELPER FUNCTION: alignUp()
    // =========================================================
    //
    // Adjusts a requested size to the next suitable
    // alignment boundary.
    //
    // Example:
    // alignUp(13, 8) = 16
    //
    static std::size_t alignUp(std::size_t size,
                               std::size_t alignment);


    // =========================================================
    // HELPER FUNCTION: initializeFreeList()
    // =========================================================
    //
    // Divides the allocated memory into blocks.
    //
    // Constructs FreeBlock objects and connects them
    // to create the initial Free List.
    //
    void initializeFreeList();


    // =========================================================
    // HELPER FUNCTION: getBlockIndex()
    // =========================================================
    //
    // Validates a pointer and determines which block
    // it belongs to.
    //
    // Returns the block index.
    //
    std::size_t getBlockIndex(void* ptr) const;


private:

    // =========================================================
    // MEMORY MANAGEMENT DATA MEMBERS
    // =========================================================

    // Starting address of the entire allocated memory region.
    void* m_memory;

    // Actual aligned size of each block.
    std::size_t m_blockSize;

    // Total number of blocks in the pool.
    std::size_t m_blockCount;

    // Head pointer of the Free List.
    FreeBlock* m_freeList;

    // Tracks allocation status of every block.
    //
    // 0 = Free
    // 1 = Allocated
    //
    // Used to detect invalid deallocation and double-free.
    //
    std::vector<unsigned char> m_allocated;

};

#endif
