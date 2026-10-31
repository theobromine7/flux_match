#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <fluxmatch/MatchingEngine.hpp>

#include "workloads.hpp"

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

namespace
{

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

struct Options
{
    fluxmatch::bench::WorkloadKind workload =
        fluxmatch::bench::WorkloadKind::OneLevelFill;
    std::size_t commands = 1000000;
    std::uint64_t seed = 42;
    bool help = false;
};

void printUsage(const char *program)
{
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --workload <name>   one of: " << fluxmatch::bench::workloadList()
        << " (default: one_level_fill)\n"
        << "  --commands <n>      number of commands to generate (default: 1000000)\n"
        << "  --seed <n>          RNG seed (default: 42)\n"
        << "  --help              show this message\n";
}

std::optional<Options> parseOptions(int argc, char **argv)
{
    Options options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--help")
        {
            options.help = true;
            return options;
        }

        if (i + 1 >= argc)
        {
            std::cerr << "Missing value for " << arg << '\n';
            return std::nullopt;
        }

        const std::string value = argv[++i];

        try
        {
            if (arg == "--workload")
            {
                const auto parsed = fluxmatch::bench::parseWorkload(value);
                if (!parsed)
                {
                    std::cerr << "Unknown workload: " << value << '\n';
                    return std::nullopt;
                }
                options.workload = *parsed;
            }
            else if (arg == "--commands")
            {
                options.commands = static_cast<std::size_t>(std::stoull(value));
            }
            else if (arg == "--seed")
            {
                options.seed = std::stoull(value);
            }
            else
            {
                std::cerr << "Unknown option: " << arg << '\n';
                return std::nullopt;
            }
        }
        catch (const std::exception &)
        {
            std::cerr << "Invalid value for " << arg << ": " << value << '\n';
            return std::nullopt;
        }
    }

    return options;
}

} // namespace

int main(int argc, char **argv)
{
    const auto parsed = parseOptions(argc, argv);
    if (!parsed)
    {
        printUsage(argv[0]);
        return 2;
    }

    const Options options = *parsed;
    if (options.help)
    {
        printUsage(argv[0]);
        return 0;
    }

    const auto commands = fluxmatch::bench::generateWorkload(
        options.workload, options.commands, options.seed);

    std::cout << "Workload: " << fluxmatch::bench::toString(options.workload) << '\n';
    std::cout << "Seed: " << options.seed << '\n';
    std::cout << "Commands: " << commands.size() << '\n';
    std::cout << "Stream hash: " << std::hex
              << fluxmatch::bench::hashCommands(commands) << std::dec << '\n';

    MatchingEngine engine;
    std::vector<long long> orderIds(commands.size(), 0);
    std::vector<unsigned long long> latencies;
    latencies.reserve(commands.size());

    const auto wallStart = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const auto &command = commands[i];
        const auto start = readTSC();

        if (command.kind == fluxmatch::bench::CommandKind::Submit)
        {
            orderIds[i] = engine.submitOrder(
                command.price, command.quantity, command.side, command.orderType);
        }
        else
        {
            engine.cancelOrder(orderIds[command.cancelTarget]);
        }

        const auto end = readTSC();
        latencies.push_back(end - start);
    }

    const auto wallEnd = std::chrono::steady_clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        wallEnd - wallStart)
                        .count();
    const double seconds = static_cast<double>(ns) / 1e9;

    std::cout << "Time: " << seconds << " s\n";
    if (seconds > 0.0)
        std::cout << "Commands/sec: " << static_cast<double>(commands.size()) / seconds << '\n';
    else
        std::cout << "Commands/sec: n/a\n";

    std::cout << "Trades: " << engine.getTrades().size() << '\n';
    std::cout << "Trade capacity: " << engine.getTradeCapacity() << '\n';

    if (!latencies.empty())
    {
        std::sort(latencies.begin(), latencies.end());
        const std::size_t n = latencies.size();
        std::cout << "P50: " << latencies[n / 2] << " cycles\n";
        std::cout << "P95: " << latencies[n * 95 / 100] << " cycles\n";
        std::cout << "P99: " << latencies[n * 99 / 100] << " cycles\n";
        std::cout << "Max: " << latencies[n - 1] << " cycles\n";
    }

    return 0;
}
