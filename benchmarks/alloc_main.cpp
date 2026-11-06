#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <fluxmatch/MatchingEngine.hpp>

#include "allocation_tracker.hpp"
#include "workloads.hpp"

namespace
{

struct Options
{
    fluxmatch::bench::WorkloadKind workload =
        fluxmatch::bench::WorkloadKind::Mixed;
    std::size_t commands = 200000;
    std::uint64_t seed = 42;
    bool help = false;
};

void printUsage(const char *program)
{
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --workload <name>   one of: " << fluxmatch::bench::workloadList()
        << " (default: mixed)\n"
        << "  --commands <n>      number of commands (default: 200000)\n"
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

void runPass(const std::vector<fluxmatch::bench::Command> &commands)
{
    MatchingEngine engine;
    std::vector<long long> orderIds(commands.size(), 0);
    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const auto &command = commands[i];
        if (command.kind == fluxmatch::bench::CommandKind::Submit)
        {
            orderIds[i] = engine.submitOrder(
                command.price, command.quantity, command.side, command.orderType);
        }
        else
        {
            engine.cancelOrder(orderIds[command.cancelTarget]);
        }
    }
}

struct KindStats
{
    unsigned long long operations = 0;
    unsigned long long allocations = 0;
    unsigned long long bytes = 0;
    unsigned long long zeroAllocationOperations = 0;
    unsigned long long maxAllocations = 0;
};

void addSample(KindStats &stats, const fluxmatch::bench::AllocationSnapshot &snapshot)
{
    ++stats.operations;
    stats.allocations += snapshot.count;
    stats.bytes += snapshot.bytes;
    if (snapshot.count == 0)
        ++stats.zeroAllocationOperations;
    stats.maxAllocations = std::max(stats.maxAllocations, snapshot.count);
}

void printKind(const char *name, const KindStats &stats)
{
    if (stats.operations == 0)
        return;
    const double mean = static_cast<double>(stats.allocations) /
                        static_cast<double>(stats.operations);
    const double zeroPercent =
        100.0 * static_cast<double>(stats.zeroAllocationOperations) /
        static_cast<double>(stats.operations);

    std::cout << "  " << name << ":\n";
    std::cout << "    operations: " << stats.operations << '\n';
    std::cout << "    allocations: " << stats.allocations << '\n';
    std::cout << "    allocations/op: " << mean << '\n';
    std::cout << "    bytes: " << stats.bytes << '\n';
    std::cout << "    zero-allocation ops: " << zeroPercent << " %\n";
    std::cout << "    max allocations in one op: " << stats.maxAllocations << '\n';
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

    // Warm the allocator and the engine code, then measure a fresh engine.
    runPass(commands);

    MatchingEngine engine;
    std::vector<long long> orderIds(commands.size(), 0);

    fluxmatch::bench::resetAllocationCounters();

    KindStats submit;
    KindStats cancel;
    KindStats total;
    std::array<unsigned long long, 17> histogram{};
    unsigned long long trades = 0;

    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const auto &command = commands[i];
        fluxmatch::bench::resetAllocationCounters();

        long long id = 0;
        if (command.kind == fluxmatch::bench::CommandKind::Submit)
        {
            id = engine.submitOrder(
                command.price, command.quantity, command.side, command.orderType);
        }
        else
        {
            engine.cancelOrder(orderIds[command.cancelTarget]);
        }

        const auto snapshot = fluxmatch::bench::allocationSnapshot();
        orderIds[i] = id;

        addSample(total, snapshot);
        addSample(command.kind == fluxmatch::bench::CommandKind::Submit ? submit
                                                                        : cancel,
                  snapshot);

        const std::size_t bucket =
            snapshot.count < histogram.size() ? snapshot.count : histogram.size() - 1;
        ++histogram[bucket];
    }

    trades = engine.getTrades().size();

    std::cout << "Trades: " << trades << '\n';

    std::cout << "\n[allocation summary]\n";
    std::cout << "  total allocations: " << total.allocations << '\n';
    std::cout << "  total bytes: " << total.bytes << '\n';
    std::cout << "  allocations/command: "
              << static_cast<double>(total.allocations) /
                     static_cast<double>(commands.size())
              << '\n';
    std::cout << "  bytes/command: "
              << static_cast<double>(total.bytes) /
                     static_cast<double>(commands.size())
              << '\n';
    std::cout << "  zero-allocation commands: "
              << (100.0 * static_cast<double>(total.zeroAllocationOperations) /
                  static_cast<double>(commands.size()))
              << " %\n";
    std::cout << "  max allocations in one command: "
              << total.maxAllocations << '\n';

    std::cout << "\n[allocation histogram]\n";
    for (std::size_t bucket = 0; bucket < histogram.size(); ++bucket)
    {
        if (histogram[bucket] == 0)
            continue;
        if (bucket + 1 == histogram.size())
            std::cout << "  " << bucket << "+: " << histogram[bucket] << '\n';
        else
            std::cout << "  " << bucket << ": " << histogram[bucket] << '\n';
    }

    std::cout << "\n[by command kind]\n";
    printKind("submit", submit);
    printKind("cancel", cancel);

    return 0;
}
