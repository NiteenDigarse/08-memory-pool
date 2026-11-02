
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
