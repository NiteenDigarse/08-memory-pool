 add_library(memory_pool STATIC
 MemoryPool.cpp)

 target_include_directories (memory_pool 
 PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    )

    target_compile_features(memory_pool PUBLIC cxx_std_17)