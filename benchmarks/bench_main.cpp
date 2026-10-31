#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fluxmatch/MatchingEngine.hpp>

#include "workloads.hpp"

#if !defined(_WIN32)
#include <sys/utsname.h>
#include <unistd.h>
#endif

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

#ifndef FLUXMATCH_GIT_COMMIT
#define FLUXMATCH_GIT_COMMIT "unknown"
#endif
#ifndef FLUXMATCH_COMPILER_ID
#define FLUXMATCH_COMPILER_ID "unknown"
#endif
#ifndef FLUXMATCH_COMPILER_VERSION
#define FLUXMATCH_COMPILER_VERSION "unknown"
#endif
#ifndef FLUXMATCH_CXX_STANDARD
#define FLUXMATCH_CXX_STANDARD "unknown"
#endif
#ifndef FLUXMATCH_SYSTEM_NAME
#define FLUXMATCH_SYSTEM_NAME "unknown"
#endif
#ifndef FLUXMATCH_SYSTEM_PROCESSOR
#define FLUXMATCH_SYSTEM_PROCESSOR "unknown"
#endif
#ifndef FLUXMATCH_BUILD_CONFIG
#define FLUXMATCH_BUILD_CONFIG "unknown"
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

std::string environmentValue(const char *name)
{
    const char *value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

std::string utcTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

std::string cpuBrand()
{
#ifdef _WIN32
    const std::string identifier = environmentValue("PROCESSOR_IDENTIFIER");
    return identifier.empty() ? "unknown" : identifier;
#else
    std::ifstream info("/proc/cpuinfo");
    std::string line;
    while (std::getline(info, line))
    {
        if (line.rfind("model name", 0) != 0)
            continue;
        const auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        std::string value = line.substr(colon + 1);
        const auto first = value.find_first_not_of(" \t");
        return first == std::string::npos ? "unknown" : value.substr(first);
    }
    return "unknown";
#endif
}

std::string hostName()
{
#ifdef _WIN32
    const std::string name = environmentValue("COMPUTERNAME");
    return name.empty() ? "unknown" : name;
#else
    char buffer[256]{};
    return gethostname(buffer, sizeof(buffer)) == 0 ? std::string(buffer)
                                                    : std::string("unknown");
#endif
}

std::string osName()
{
#ifdef _WIN32
    return "Windows";
#else
    struct utsname info
    {
    };
    if (uname(&info) == 0)
        return std::string(info.sysname) + " " + info.release;
    return "unknown";
#endif
}

struct RunMetadata
{
    std::string generatedAt = utcTimestamp();
    std::string gitCommit = FLUXMATCH_GIT_COMMIT;
    std::string compilerId = FLUXMATCH_COMPILER_ID;
    std::string compilerVersion = FLUXMATCH_COMPILER_VERSION;
    std::string cxxStandard = FLUXMATCH_CXX_STANDARD;
    std::string buildConfig = FLUXMATCH_BUILD_CONFIG;
    std::string systemName = FLUXMATCH_SYSTEM_NAME;
    std::string systemProcessor = FLUXMATCH_SYSTEM_PROCESSOR;
    std::string cpu = cpuBrand();
    std::string host = hostName();
    std::string os = osName();
    unsigned logicalCores = std::thread::hardware_concurrency();
};

struct Stats
{
    unsigned long long p50 = 0;
    unsigned long long p95 = 0;
    unsigned long long p99 = 0;
    unsigned long long max = 0;
};

Stats computeStats(std::vector<unsigned long long> &values)
{
    Stats stats;
    if (values.empty())
        return stats;

    std::sort(values.begin(), values.end());
    const std::size_t n = values.size();
    stats.p50 = values[n / 2];
    stats.p95 = values[n * 95 / 100];
    stats.p99 = values[n * 99 / 100];
    stats.max = values[n - 1];
    return stats;
}

void printStats(const char *label, const Stats &stats)
{
    std::cout << "  " << label << ":\n";
    std::cout << "    P50: " << stats.p50 << ' ' << kTimerUnit << '\n';
    std::cout << "    P95: " << stats.p95 << ' ' << kTimerUnit << '\n';
    std::cout << "    P99: " << stats.p99 << ' ' << kTimerUnit << '\n';
    std::cout << "    Max: " << stats.max << ' ' << kTimerUnit << '\n';
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
    std::string jsonPath;
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
        << "  --json <path>       also write a machine-readable result file\n"
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
            else if (arg == "--json")
            {
                options.jsonPath = value;
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

struct ThroughputResult
{
    double seconds = 0.0;
    double commandsPerSecond = 0.0;
    std::size_t trades = 0;
};

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

struct LatencyResult
{
    std::size_t trades = 0;
    Stats stats;
};

LatencyResult runLatencyEngineOnly(
    const std::vector<fluxmatch::bench::Command> &commands)
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

    return {engine.getTrades().size(), computeStats(latencies)};
}

LatencyResult runLatencyEndToEnd(
    const std::vector<fluxmatch::bench::Command> &commands)
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

    return {engine.getTrades().size(), computeStats(latencies)};
}

Stats measureTimerOverhead(std::size_t samples)
{
    std::vector<unsigned long long> overhead;
    overhead.reserve(samples);
    for (std::size_t i = 0; i < samples; ++i)
    {
        const auto start = readTSC();
        const auto end = readTSC();
        overhead.push_back(end - start);
    }
    return computeStats(overhead);
}

std::string jsonEscape(const std::string &value)
{
    std::string out;
    out.reserve(value.size() + 8);
    for (const char c : value)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                std::ostringstream escape;
                escape << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(static_cast<unsigned char>(c));
                out += escape.str();
            }
            else
            {
                out += c;
            }
        }
    }
    return out;
}

std::string jsonString(const std::string &value)
{
    return "\"" + jsonEscape(value) + "\"";
}

std::string statsJson(const Stats &stats)
{
    std::ostringstream out;
    out << "{\"p50\":" << stats.p50
        << ",\"p95\":" << stats.p95
        << ",\"p99\":" << stats.p99
        << ",\"max\":" << stats.max << '}';
    return out.str();
}

bool writeJson(const std::string &path,
               const RunMetadata &meta,
               const Options &options,
               std::size_t commands,
               std::uint64_t streamHash,
               const WarmUpResult &warmup,
               const std::optional<ThroughputResult> &throughput,
               const std::optional<LatencyResult> &engineLatency,
               const std::optional<LatencyResult> &endToEndLatency,
               const std::optional<Stats> &timerFloor)
{
    const std::filesystem::path target(path);
    if (target.has_parent_path())
    {
        std::error_code error;
        std::filesystem::create_directories(target.parent_path(), error);
        if (error)
        {
            std::cerr << "Could not create result directory: "
                      << error.message() << '\n';
            return false;
        }
    }

    std::ofstream out(path);
    if (!out)
    {
        std::cerr << "Could not open result file: " << path << '\n';
        return false;
    }

    out << std::setprecision(9);

    out << "{\n";
    out << "  \"schema\": 1,\n";
    out << "  \"generated_at\": " << jsonString(meta.generatedAt) << ",\n";
    out << "  \"git_commit\": " << jsonString(meta.gitCommit) << ",\n";

    out << "  \"build\": {\n";
    out << "    \"config\": " << jsonString(meta.buildConfig) << ",\n";
    out << "    \"compiler_id\": " << jsonString(meta.compilerId) << ",\n";
    out << "    \"compiler_version\": " << jsonString(meta.compilerVersion) << ",\n";
    out << "    \"cxx_standard\": " << jsonString(meta.cxxStandard) << ",\n";
    out << "    \"system\": " << jsonString(meta.systemName) << ",\n";
    out << "    \"processor\": " << jsonString(meta.systemProcessor) << "\n";
    out << "  },\n";

    out << "  \"machine\": {\n";
    out << "    \"cpu\": " << jsonString(meta.cpu) << ",\n";
    out << "    \"logical_cores\": " << meta.logicalCores << ",\n";
    out << "    \"host\": " << jsonString(meta.host) << ",\n";
    out << "    \"os\": " << jsonString(meta.os) << "\n";
    out << "  },\n";

    out << "  \"workload\": {\n";
    out << "    \"name\": " << jsonString(fluxmatch::bench::toString(options.workload)) << ",\n";
    out << "    \"commands\": " << commands << ",\n";
    out << "    \"seed\": " << options.seed << ",\n";
    out << "    \"stream_hash\": " << jsonString([&]
                                                    {
                                                        std::ostringstream hash;
                                                        hash << std::hex << streamHash;
                                                        return hash.str();
                                                    }())
        << "\n";
    out << "  },\n";

    out << "  \"config\": {\n";
    out << "    \"mode\": ";
    switch (options.mode)
    {
    case Mode::Throughput:
        out << "\"throughput\"";
        break;
    case Mode::Latency:
        out << "\"latency\"";
        break;
    case Mode::Both:
        out << "\"both\"";
        break;
    }
    out << ",\n";
    out << "    \"warmup_seconds\": " << options.warmupSeconds << ",\n";
    out << "    \"timer_unit\": " << jsonString(kTimerUnit) << "\n";
    out << "  },\n";

    out << "  \"warmup\": {\n";
    out << "    \"enabled\": " << (options.warmupSeconds > 0.0 ? "true" : "false") << ",\n";
    out << "    \"passes\": " << warmup.passes << ",\n";
    out << "    \"seconds\": " << warmup.seconds << ",\n";
    out << "    \"first_commands_per_sec\": " << warmup.firstThroughput << ",\n";
    out << "    \"last_commands_per_sec\": " << warmup.lastThroughput << ",\n";
    out << "    \"recent_spread\": " << warmup.recentSpread << ",\n";
    out << "    \"steady\": " << (warmup.steady ? "true" : "false") << "\n";
    out << "  },\n";

    out << "  \"throughput\": ";
    if (throughput)
    {
        out << "{\n";
        out << "    \"seconds\": " << throughput->seconds << ",\n";
        out << "    \"commands_per_sec\": " << throughput->commandsPerSecond << ",\n";
        out << "    \"trades\": " << throughput->trades << "\n";
        out << "  },\n";
    }
    else
    {
        out << "null,\n";
    }

    out << "  \"latency\": ";
    if (engineLatency && endToEndLatency && timerFloor)
    {
        out << "{\n";
        out << "    \"unit\": " << jsonString(kTimerUnit) << ",\n";
        out << "    \"engine_only\": {\"trades\": " << engineLatency->trades
            << ", \"stats\": " << statsJson(engineLatency->stats) << "},\n";
        out << "    \"end_to_end\": {\"trades\": " << endToEndLatency->trades
            << ", \"stats\": " << statsJson(endToEndLatency->stats) << "},\n";
        out << "    \"harness_delta_p50\": "
            << (static_cast<long long>(endToEndLatency->stats.p50) -
                static_cast<long long>(engineLatency->stats.p50))
            << ",\n";
        out << "    \"timer_floor\": " << statsJson(*timerFloor) << "\n";
        out << "  }\n";
    }
    else
    {
        out << "null\n";
    }

    out << "}\n";
    return true;
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
    const std::uint64_t streamHash = fluxmatch::bench::hashCommands(commands);
    const RunMetadata metadata;

    std::cout << "Workload: " << fluxmatch::bench::toString(options.workload) << '\n';
    std::cout << "Seed: " << options.seed << '\n';
    std::cout << "Commands: " << commands.size() << '\n';
    std::cout << "Stream hash: " << std::hex << streamHash << std::dec << '\n';
    std::cout << "Mode: ";
    switch (options.mode)
    {
    case Mode::Throughput:
        std::cout << "throughput\n";
        break;
    case Mode::Latency:
        std::cout << "latency\n";
        break;
    case Mode::Both:
        std::cout << "both\n";
        break;
    }

    std::cout << "\n[metadata]\n";
    std::cout << "  git commit: " << metadata.gitCommit << '\n';
    std::cout << "  build: " << metadata.buildConfig << '\n';
    std::cout << "  compiler: " << metadata.compilerId << ' '
              << metadata.compilerVersion << '\n';
    std::cout << "  c++ standard: " << metadata.cxxStandard << '\n';
    std::cout << "  system: " << metadata.systemName << " / "
              << metadata.systemProcessor << '\n';
    std::cout << "  cpu: " << metadata.cpu << '\n';
    std::cout << "  logical cores: " << metadata.logicalCores << '\n';
    std::cout << "  host: " << metadata.host << '\n';
    std::cout << "  os: " << metadata.os << '\n';
    std::cout << "  time: " << metadata.generatedAt << '\n';

    std::cout << "\n[warm-up]\n";
    const WarmUpResult warmup = runWarmUp(commands, options.warmupSeconds);
    if (options.warmupSeconds > 0.0 && !commands.empty())
    {
        std::cout << "  passes: " << warmup.passes << '\n';
        std::cout << "  time: " << warmup.seconds << " s\n";
        std::cout << "  first pass: " << warmup.firstThroughput << " commands/sec\n";
        std::cout << "  last pass: " << warmup.lastThroughput << " commands/sec\n";
        std::cout << "  last-3 spread: " << warmup.recentSpread * 100.0
                  << " % (target <= 5 %)\n";
        std::cout << "  status: " << (warmup.steady ? "steady" : "not steady") << '\n';
    }
    else
    {
        std::cout << "  disabled\n";
    }

    std::optional<ThroughputResult> throughput;
    std::optional<LatencyResult> engineLatency;
    std::optional<LatencyResult> endToEndLatency;
    std::optional<Stats> timerFloor;

    if (options.mode == Mode::Throughput || options.mode == Mode::Both)
    {
        std::size_t trades = 0;
        const double seconds = runThroughput(commands, trades);

        ThroughputResult result;
        result.seconds = seconds;
        result.trades = trades;
        result.commandsPerSecond =
            seconds > 0.0 ? static_cast<double>(commands.size()) / seconds : 0.0;
        throughput = result;

        std::cout << "\n[throughput]\n";
        std::cout << "  Time: " << result.seconds << " s\n";
        if (result.commandsPerSecond > 0.0)
            std::cout << "  Commands/sec: " << result.commandsPerSecond << '\n';
        else
            std::cout << "  Commands/sec: n/a\n";
        std::cout << "  Trades: " << result.trades << '\n';
    }

    if (options.mode == Mode::Latency || options.mode == Mode::Both)
    {
        engineLatency = runLatencyEngineOnly(commands);
        endToEndLatency = runLatencyEndToEnd(commands);
        timerFloor = measureTimerOverhead(100000);

        std::cout << "\n[latency: engine only]\n";
        std::cout << "  Trades: " << engineLatency->trades << '\n';
        printStats("latency", engineLatency->stats);

        std::cout << "\n[latency: end to end]\n";
        std::cout << "  Trades: " << endToEndLatency->trades << '\n';
        printStats("latency", endToEndLatency->stats);

        std::cout << "\n[timer]\n";
        printStats("empty timed region", *timerFloor);

        std::cout << "\n[harness overhead estimate]\n";
        const long long delta =
            static_cast<long long>(endToEndLatency->stats.p50) -
            static_cast<long long>(engineLatency->stats.p50);
        std::cout << "  End-to-end P50 minus engine-only P50: "
                  << delta << ' ' << kTimerUnit << '\n';
        std::cout << "  Timer floor P50 (two back-to-back readings): "
                  << timerFloor->p50 << ' ' << kTimerUnit << '\n';
        std::cout << "  Note: single run. Repeat-run variability is added in\n"
                     "  Phase 1.6.\n";
    }

    if (!options.jsonPath.empty())
    {
        if (writeJson(options.jsonPath, metadata, options, commands.size(),
                      streamHash, warmup, throughput, engineLatency,
                      endToEndLatency, timerFloor))
        {
            std::cout << "\nWrote JSON result: " << options.jsonPath << '\n';
        }
        else
        {
            return 3;
        }
    }

    return 0;
}
