#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

#include <fluxmatch/MatchingEngine.hpp>

// Temporary portability shim. Phase 1.8 replaces this with a documented
// timer module and measures its overhead.
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#define FLUXMATCH_HAS_RDTSCP 1
#elif (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
#include <x86intrin.h>
#define FLUXMATCH_HAS_RDTSCP 1
#endif

unsigned long long readTSC()
{
#ifdef FLUXMATCH_HAS_RDTSCP
    unsigned int aux;
    return __rdtscp(&aux);
#else
    // Non-x86 fallback: monotonic clock ticks, not CPU cycles.
    return static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

int main()
{
    MatchingEngine engine;

    std::vector<long long> latencies;
    int n = 1000000;
    latencies.reserve(n);

    for (int i = 0; i < 1000000; i++)
    {
        engine.submitOrder(
            100000 + (i % 10000),
            1,
            Side::SELL,
            OrderType::LIMIT);
    }
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < n; i++)
    {
        unsigned long long start = readTSC();

        engine.submitOrder(
            999999,
            1,
            Side::BUY,
            OrderType::LIMIT);

        unsigned long long end = readTSC();

        latencies.push_back(end - start);
    }
    auto end = std::chrono::steady_clock::now();

    auto ns =
        std::chrono::duration_cast<
            std::chrono::nanoseconds>(
            end - start)
            .count();
    double seconds = ns / 1e9;

    std::cout << "Orders: " << n << '\n';
    std::cout << "Time: " << seconds << " s\n";
    std::cout << "Orders/sec: "
              << n / seconds
              << '\n';
    std::cout << "Trades: "
              << engine.getTrades().size()
              << '\n';

    std::cout << "Trade capacity: "
              << engine.getTradeCapacity()
              << '\n';

    std::sort(latencies.begin(), latencies.end());

    std::cout << "P50: " << latencies[n / 2] << " cycles\n";
    std::cout << "P95: " << latencies[n * 95 / 100] << " cycles\n";
    std::cout << "P99: " << latencies[n * 99 / 100] << " cycles\n";
    std::cout << "Max: " << latencies[n - 1] << " cycles\n";

    return 0;
}
