#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fluxmatch/OrderType.hpp>
#include <fluxmatch/Side.hpp>

namespace fluxmatch::bench
{

enum class CommandKind
{
    Submit,
    Cancel
};

// A single operation in a generated workload. Cancel commands refer to the
// index of an earlier Submit command; the runner maps that index to the order
// id returned by the engine.
struct Command
{
    CommandKind kind = CommandKind::Submit;
    Side side = Side::BUY;
    OrderType orderType = OrderType::LIMIT;
    long long price = 0;
    long long quantity = 0;
    std::size_t cancelTarget = 0;

    bool operator==(const Command &) const = default;
};

enum class WorkloadKind
{
    Resting,
    OneLevelFill,
    MultiLevelSweep,
    Cancel,
    Mixed
};

const char *toString(WorkloadKind kind);
std::optional<WorkloadKind> parseWorkload(std::string_view name);
const char *workloadList();

// Deterministic: the same kind, command count, and seed always produce the
// same sequence, on any platform.
std::vector<Command> generateWorkload(WorkloadKind kind,
                                      std::size_t commandCount,
                                      std::uint64_t seed);

// Order-independent hash used to compare streams and to detect accidental
// changes in generation.
std::uint64_t hashCommands(const std::vector<Command> &commands);

} // namespace fluxmatch::bench
