// Global allocation tracker for the allocation benchmark.
//
// This file is linked only into fluxmatch_alloc_bench, never into the engine or
// the normal benchmark. Replacing global operator new is deliberately kept out
// of production code so the hot path is unchanged.

#include "allocation_tracker.hpp"

#include <cstdlib>
#include <new>

namespace
{

unsigned long long g_allocations = 0;
unsigned long long g_bytes = 0;

void record(std::size_t size) noexcept
{
    ++g_allocations;
    g_bytes += size;
}

void *allocate(std::size_t size)
{
    void *memory = std::malloc(size == 0 ? 1 : size);
    if (!memory)
        throw std::bad_alloc();
    record(size);
    return memory;
}

} // namespace

void *operator new(std::size_t size)
{
    return allocate(size);
}

void *operator new[](std::size_t size)
{
    return allocate(size);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    void *memory = std::malloc(size == 0 ? 1 : size);
    if (memory)
        record(size);
    return memory;
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    void *memory = std::malloc(size == 0 ? 1 : size);
    if (memory)
        record(size);
    return memory;
}

void operator delete(void *memory) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory) noexcept
{
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete(void *memory, const std::nothrow_t &) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory, const std::nothrow_t &) noexcept
{
    std::free(memory);
}

namespace fluxmatch::bench
{

AllocationSnapshot allocationSnapshot()
{
    return {g_allocations, g_bytes};
}

void resetAllocationCounters()
{
    g_allocations = 0;
    g_bytes = 0;
}

} // namespace fluxmatch::bench
