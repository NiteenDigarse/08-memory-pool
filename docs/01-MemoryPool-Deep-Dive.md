## 19.11 Actual Benchmark Results

The benchmark was executed on the development machine using:

- 200,000 objects per round
- 5 measured rounds
- 32-byte `BenchmarkObject`
- `std::chrono::steady_clock`
- Median execution time for comparison

### Measured Results

| Round | Standard new/delete | Custom MemoryPool |
|---:|---:|---:|
| 1 | 17.3402 ms | 8.9753 ms |
| 2 | 17.6568 ms | 8.9856 ms |
| 3 | 17.1423 ms | 9.0216 ms |
| 4 | 17.1081 ms | 8.9999 ms |
| 5 | 17.0080 ms | 8.9356 ms |
| **Median** | **17.1423 ms** | **8.9856 ms** |

### Speedup Calculation

```text
Speedup = Standard new/delete time / MemoryPool time

        = 17.1423 / 8.9856

        ≈ 1.90775x
```

### Performance Improvement

```text
Improvement (%) =
    (1 - MemoryPool time / new/delete time) * 100

    ≈ 47.57%
```

### Observation

In this benchmark run, the custom MemoryPool completed the measured workload approximately 1.91 times faster than standard `new/delete`.

The MemoryPool measurements were also relatively consistent across all five rounds.

### Important Benchmark Limitations

- MemoryPool construction and initialization were excluded from the measured interval.
- The benchmark measures repeated allocation, object construction, object destruction, and deallocation.
- Results depend on hardware, compiler optimization, operating system, allocator implementation, and workload.
- This result does not establish that MemoryPool is universally faster than standard dynamic allocation.

**Conclusion:** The initial MemoryPool implementation demonstrated a performance improvement for this particular fixed-size, repeated-allocation workload.