add_executable(benchmark_memory_pool
    benchmark.cpp
)

target_link_libraries(benchmark_memory_pool
    PRIVATE memory_pool
)