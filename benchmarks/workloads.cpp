#include "workloads.hpp"

#include <random>
#include <stdexcept>

namespace fluxmatch::bench
{
namespace
{

constexpr long long kMid = 100000;

// Resting prices never cross: bids stay strictly below kMid, asks strictly
// above it. This keeps the "resting" and "cancel" workloads fill-free.
Command makeRestingCommand(std::mt19937_64 &rng)
{
    Command command;
    command.kind = CommandKind::Submit;
    command.side = (rng() % 2) != 0 ? Side::BUY : Side::SELL;
    command.orderType = OrderType::LIMIT;

    const long long offset = 1 + static_cast<long long>(rng() % 1000);
    command.price = command.side == Side::BUY ? kMid - offset : kMid + offset;
    command.quantity = 1 + static_cast<long long>(rng() % 10);
    return command;
}

Command makeMarketCommand(std::mt19937_64 &rng)
{
    Command command;
    command.kind = CommandKind::Submit;
    command.side = (rng() % 2) != 0 ? Side::BUY : Side::SELL;
    command.orderType = OrderType::MARKET;
    command.price = 0;
    command.quantity = 1 + static_cast<long long>(rng() % 5);
    return command;
}

Command makeLimitCommand(Side side, long long price, long long quantity)
{
    Command command;
    command.kind = CommandKind::Submit;
    command.side = side;
    command.orderType = OrderType::LIMIT;
    command.price = price;
    command.quantity = quantity;
    return command;
}

Command makeCancelCommand(std::size_t target)
{
    Command command;
    command.kind = CommandKind::Cancel;
    command.cancelTarget = target;
    return command;
}

void mix(std::uint64_t &hash, std::uint64_t value)
{
    for (int i = 0; i < 8; ++i)
    {
        hash ^= (value >> (i * 8)) & 0xFFu;
        hash *= 1099511628211ull;
    }
}

std::vector<Command> generateResting(std::size_t count, std::uint64_t seed)
{
    std::mt19937_64 rng(seed);
    std::vector<Command> commands;
    commands.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        commands.push_back(makeRestingCommand(rng));
    return commands;
}

std::vector<Command> generateOneLevelFill(std::size_t count, std::uint64_t seed)
{
    // Every pair is one resting sell followed by a buy that fills exactly it.
    const long long seedOffset = static_cast<long long>(seed % 50);
    std::vector<Command> commands;
    commands.reserve(count);

    std::size_t pair = 0;
    while (commands.size() < count)
    {
        const long long price =
            kMid + (seedOffset + static_cast<long long>(pair % 100)) % 100;
        commands.push_back(makeLimitCommand(Side::SELL, price, 1));
        if (commands.size() == count)
            break;
        commands.push_back(makeLimitCommand(Side::BUY, price, 1));
        ++pair;
    }

    return commands;
}

std::vector<Command> generateMultiLevelSweep(std::size_t count, std::uint64_t seed)
{
    // Ten resting sells on consecutive prices, then one buy that consumes all
    // ten levels.
    const std::size_t seedOffset = static_cast<std::size_t>(seed % 50);
    std::vector<Command> commands;
    commands.reserve(count);

    std::size_t cycle = 0;
    while (commands.size() < count)
    {
        const long long start =
            kMid + static_cast<long long>((cycle + seedOffset) % 50) * 100;
        for (long long level = 1; level <= 10; ++level)
        {
            if (commands.size() == count)
                break;
            commands.push_back(makeLimitCommand(Side::SELL, start + level, 1));
        }
        if (commands.size() == count)
            break;
        commands.push_back(makeLimitCommand(Side::BUY, start + 10, 10));
        ++cycle;
    }

    return commands;
}

std::vector<Command> generateCancel(std::size_t count, std::uint64_t seed)
{
    // Three out of every four commands rest an order; the fourth cancels a
    // randomly chosen order that is still tracked as active.
    std::mt19937_64 rng(seed);
    std::vector<Command> commands;
    commands.reserve(count);
    std::vector<std::size_t> active;

    for (std::size_t i = 0; i < count; ++i)
    {
        const bool cancel = !active.empty() && (i % 4 == 3);
        if (cancel)
        {
            const std::size_t pick = static_cast<std::size_t>(rng() % active.size());
            const std::size_t target = active[pick];
            active[pick] = active.back();
            active.pop_back();
            commands.push_back(makeCancelCommand(target));
        }
        else
        {
            commands.push_back(makeRestingCommand(rng));
            active.push_back(commands.size() - 1);
        }
    }

    return commands;
}

std::vector<Command> generateMixed(std::size_t count, std::uint64_t seed)
{
    // 50% resting limits, 20% cancels, 30% market orders. Market orders make
    // the mix include fills without simulating the book in the generator.
    std::mt19937_64 rng(seed);
    std::vector<Command> commands;
    commands.reserve(count);
    std::vector<std::size_t> active;

    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint64_t roll = rng() % 100;
        if (roll < 50)
        {
            commands.push_back(makeRestingCommand(rng));
            active.push_back(commands.size() - 1);
        }
        else if (roll < 70 && !active.empty())
        {
            const std::size_t pick = static_cast<std::size_t>(rng() % active.size());
            const std::size_t target = active[pick];
            active[pick] = active.back();
            active.pop_back();
            commands.push_back(makeCancelCommand(target));
        }
        else
        {
            commands.push_back(makeMarketCommand(rng));
        }
    }

    return commands;
}

} // namespace

const char *toString(WorkloadKind kind)
{
    switch (kind)
    {
    case WorkloadKind::Resting:
        return "resting";
    case WorkloadKind::OneLevelFill:
        return "one_level_fill";
    case WorkloadKind::MultiLevelSweep:
        return "multi_level_sweep";
    case WorkloadKind::Cancel:
        return "cancel";
    case WorkloadKind::Mixed:
        return "mixed";
    }
    return "unknown";
}

std::optional<WorkloadKind> parseWorkload(std::string_view name)
{
    if (name == "resting")
        return WorkloadKind::Resting;
    if (name == "one_level_fill")
        return WorkloadKind::OneLevelFill;
    if (name == "multi_level_sweep")
        return WorkloadKind::MultiLevelSweep;
    if (name == "cancel")
        return WorkloadKind::Cancel;
    if (name == "mixed")
        return WorkloadKind::Mixed;
    return std::nullopt;
}

const char *workloadList()
{
    return "resting, one_level_fill, multi_level_sweep, cancel, mixed";
}

std::vector<Command> generateWorkload(WorkloadKind kind,
                                      std::size_t commandCount,
                                      std::uint64_t seed)
{
    switch (kind)
    {
    case WorkloadKind::Resting:
        return generateResting(commandCount, seed);
    case WorkloadKind::OneLevelFill:
        return generateOneLevelFill(commandCount, seed);
    case WorkloadKind::MultiLevelSweep:
        return generateMultiLevelSweep(commandCount, seed);
    case WorkloadKind::Cancel:
        return generateCancel(commandCount, seed);
    case WorkloadKind::Mixed:
        return generateMixed(commandCount, seed);
    }
    throw std::invalid_argument("unknown workload kind");
}

std::uint64_t hashCommands(const std::vector<Command> &commands)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (const auto &command : commands)
    {
        mix(hash, static_cast<std::uint64_t>(command.kind));
        mix(hash, static_cast<std::uint64_t>(command.side));
        mix(hash, static_cast<std::uint64_t>(command.orderType));
        mix(hash, static_cast<std::uint64_t>(command.price));
        mix(hash, static_cast<std::uint64_t>(command.quantity));
        mix(hash, static_cast<std::uint64_t>(command.cancelTarget));
    }
    return hash;
}

} // namespace fluxmatch::bench
