#pragma once

#include <cstddef>

namespace fluxmatch::bench
{

struct AllocationSnapshot
{
    unsigned long long count = 0;
    unsigned long long bytes = 0;
};

AllocationSnapshot allocationSnapshot();
void resetAllocationCounters();

} // namespace fluxmatch::bench
