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
// timer module and measures its overhead in detail.
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

#ifdef FLUXMATCH_HAS_RDTSCP
constexpr const char *kTimerUnit = "cycles";
#else
constexpr const char *kTimerUnit = "ticks";
#endif

unsigned long long readTSC()
{
#ifdef FLUXMATCH_HAS_RDTSCP
    unsigned int aux;
    return __rdtscp(&aux);
#else
    return static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

enum class Mode
{
    Throughput,
    Latency,
    Both
};

struct Options
{
    fluxmatch::bench::WorkloadKind workload =
        fluxmatch::bench::WorkloadKind::OneLevelFill;
    std::size_t commands = 1000000;
    std::uint64_t seed = 42;
    Mode mode = Mode::Both;
    double warmupSeconds = 1.0;
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
        << "  --mode <mode>       throughput, latency, or both (default: both)\n"
        << "  --warmup <seconds>  minimum warm-up duration, 0 disables (default: 1.0)\n"
        << "  --help              show this message\n";
}

std::optional<Mode> parseMode(const std::string &value)
{
    if (value == "throughput")
        return Mode::Throughput;
    if (value == "latency")
        return Mode::Latency;
    if (value == "both")
        return Mode::Both;
    return std::nullopt;
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
            else if (arg == "--mode")
            {
                const auto parsed = parseMode(value);
                if (!parsed)
                {
                    std::cerr << "Unknown mode: " << value << '\n';
                    return std::nullopt;
                }
                options.mode = *parsed;
            }
            else if (arg == "--warmup")
            {
                options.warmupSeconds = std::stod(value);
                if (options.warmupSeconds < 0.0)
                {
                    std::cerr << "Warm-up must not be negative\n";
                    return std::nullopt;
                }
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

// Throughput: one clock reading around the whole stream. No per-command timer,
// so this number includes almost no measurement overhead.
double runThroughput(const std::vector<fluxmatch::bench::Command> &commands,
                     std::size_t &trades)
{
    MatchingEngine engine;
    std::vector<long long> orderIds(commands.size(), 0);

    const auto start = std::chrono::steady_clock::now();
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
    const auto end = std::chrono::steady_clock::now();

    trades = engine.getTrades().size();
    const auto ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return static_cast<double>(ns) / 1e9;
}

// Warm-up runs the stream repeatedly on throwaway engines. It stops when the
// minimum duration has passed AND the last three pass throughputs are within
// 5% of each other, or when a hard safety limit is reached.
struct WarmUpResult
{
    std::size_t passes = 0;
    double seconds = 0.0;
    double firstThroughput = 0.0;
    double lastThroughput = 0.0;
    double recentSpread = 0.0;
    bool steady = false;
};

WarmUpResult runWarmUp(const std::vector<fluxmatch::bench::Command> &commands,
                       double minSeconds)
{
    WarmUpResult result;
    if (minSeconds <= 0.0 || commands.empty())
        return result;

    constexpr double kTolerance = 0.05;
    constexpr double kMaxSeconds = 10.0;
    constexpr std::size_t kMaxPasses = 1000;

    const auto start = std::chrono::steady_clock::now();
    std::vector<double> recent;

    while (true)
    {
        std::size_t trades = 0;
        const double seconds = runThroughput(commands, trades);
        const double throughput =
            seconds > 0.0 ? static_cast<double>(commands.size()) / seconds : 0.0;

        if (result.passes == 0)
            result.firstThroughput = throughput;
        result.lastThroughput = throughput;
        ++result.passes;

        recent.push_back(throughput);
        if (recent.size() > 3)
            recent.erase(recent.begin());

        result.seconds = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - start)
                             .count();

        if (recent.size() == 3)
        {
            double sorted[3] = {recent[0], recent[1], recent[2]};
            std::sort(sorted, sorted + 3);
            const double median = sorted[1];
            result.recentSpread =
                median > 0.0 ? (sorted[2] - sorted[0]) / median : 0.0;
            result.steady = result.recentSpread <= kTolerance;
        }

        if (result.seconds >= minSeconds && result.steady)
            break;
        if (result.seconds >= kMaxSeconds || result.passes >= kMaxPasses)
            break;
    }

    return result;
}

// Engine-only latency: argument preparation (reading the command, resolving a
// cancel target) happens before the timer starts.
std::vector<unsigned long long> runLatencyEngineOnly(
    const std::vector<fluxmatch::bench::Command> &commands,
    std::size_t &trades)
{
    MatchingEngine engine;
    std::vector<long long> orderIds(commands.size(), 0);
    std::vector<unsigned long long> latencies;
    latencies.reserve(commands.size());

    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const auto &command = commands[i];

        if (command.kind == fluxmatch::bench::CommandKind::Submit)
        {
            const long long price = command.price;
            const long long quantity = command.quantity;
            const Side side = command.side;
            const OrderType orderType = command.orderType;

            const auto start = readTSC();
            const long long id = engine.submitOrder(price, quantity, side, orderType);
            const auto end = readTSC();

            orderIds[i] = id;
            latencies.push_back(end - start);
        }
        else
        {
            const long long targetId = orderIds[command.cancelTarget];

            const auto start = readTSC();
            engine.cancelOrder(targetId);
            const auto end = readTSC();

            latencies.push_back(end - start);
        }
    }

    trades = engine.getTrades().size();
    return latencies;
}

// End-to-end latency: the timer includes the command dispatch and cancel-target
// lookup, so it is closer to what a caller of the engine actually pays.
std::vector<unsigned long long> runLatencyEndToEnd(
    const std::vector<fluxmatch::bench::Command> &commands,
    std::size_t &trades)
{
    MatchingEngine engine;
    std::vector<long long> orderIds(commands.size(), 0);
    std::vector<unsigned long long> latencies;
    latencies.reserve(commands.size());

    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const auto &command = commands[i];

        const auto start = readTSC();
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
        const auto end = readTSC();

        if (command.kind == fluxmatch::bench::CommandKind::Submit)
            orderIds[i] = id;
        latencies.push_back(end - start);
    }

    trades = engine.getTrades().size();
    return latencies;
}

// Lower bound on timer cost: two consecutive readings with nothing between.
std::vector<unsigned long long> measureTimerOverhead(std::size_t samples)
{
    std::vector<unsigned long long> overhead;
    overhead.reserve(samples);
    for (std::size_t i = 0; i < samples; ++i)
    {
        const auto start = readTSC();
        const auto end = readTSC();
        overhead.push_back(end - start);
    }
    return overhead;
}

unsigned long long printStats(const char *label,
                              std::vector<unsigned long long> &values)
{
    if (values.empty())
        return 0;

    std::sort(values.begin(), values.end());
    const std::size_t n = values.size();

    std::cout << "  " << label << ":\n";
    std::cout << "    P50: " << values[n / 2] << ' ' << kTimerUnit << '\n';
    std::cout << "    P95: " << values[n * 95 / 100] << ' ' << kTimerUnit << '\n';
    std::cout << "    P99: " << values[n * 99 / 100] << ' ' << kTimerUnit << '\n';
    std::cout << "    Max: " << values[n - 1] << ' ' << kTimerUnit << '\n';
    return values[n / 2];
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

    std::cout << "\n[warm-up]\n";
    if (options.warmupSeconds > 0.0 && !commands.empty())
    {
        const auto warm = runWarmUp(commands, options.warmupSeconds);
        std::cout << "  passes: " << warm.passes << '\n';
        std::cout << "  time: " << warm.seconds << " s\n";
        std::cout << "  first pass: " << warm.firstThroughput << " commands/sec\n";
        std::cout << "  last pass: " << warm.lastThroughput << " commands/sec\n";
        std::cout << "  last-3 spread: " << warm.recentSpread * 100.0
                  << " % (target <= 5 %)\n";
        std::cout << "  status: " << (warm.steady ? "steady" : "not steady") << '\n';
    }
    else
    {
        std::cout << "  disabled\n";
    }

    if (options.mode == Mode::Throughput || options.mode == Mode::Both)
    {
        std::size_t trades = 0;
        const double seconds = runThroughput(commands, trades);

        std::cout << "\n[throughput]\n";
        std::cout << "  Time: " << seconds << " s\n";
        if (seconds > 0.0)
            std::cout << "  Commands/sec: "
                      << static_cast<double>(commands.size()) / seconds << '\n';
        else
            std::cout << "  Commands/sec: n/a\n";
        std::cout << "  Trades: " << trades << '\n';
    }

    if (options.mode == Mode::Latency || options.mode == Mode::Both)
    {
        std::size_t engineTrades = 0;
        std::size_t endToEndTrades = 0;

        auto engineLatencies = runLatencyEngineOnly(commands, engineTrades);
        auto endToEndLatencies = runLatencyEndToEnd(commands, endToEndTrades);

        std::cout << "\n[latency: engine only]\n";
        std::cout << "  Trades: " << engineTrades << '\n';
        const auto engineMedian = printStats("latency", engineLatencies);

        std::cout << "\n[latency: end to end]\n";
        std::cout << "  Trades: " << endToEndTrades << '\n';
        const auto endToEndMedian = printStats("latency", endToEndLatencies);

        auto overhead = measureTimerOverhead(100000);
        std::cout << "\n[timer]\n";
        const auto overheadMedian = printStats("empty timed region", overhead);

        std::cout << "\n[harness overhead estimate]\n";
        const long long delta =
            static_cast<long long>(endToEndMedian) -
            static_cast<long long>(engineMedian);
        std::cout << "  End-to-end P50 minus engine-only P50: "
                  << delta << ' ' << kTimerUnit << '\n';
        std::cout << "  Timer floor P50 (two back-to-back readings): "
                  << overheadMedian << ' ' << kTimerUnit << '\n';
        std::cout << "  Note: single run. Repeat-run variability is added in\n"
                     "  Phase 1.6.\n";
    }

    return 0;
}
