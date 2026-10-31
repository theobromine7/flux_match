#include "workloads.hpp"

#include <iostream>
#include <stdexcept>

namespace
{

using fluxmatch::bench::Command;
using fluxmatch::bench::CommandKind;
using fluxmatch::bench::WorkloadKind;

void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

const WorkloadKind kAllKinds[] = {
    WorkloadKind::Resting,
    WorkloadKind::OneLevelFill,
    WorkloadKind::MultiLevelSweep,
    WorkloadKind::Cancel,
    WorkloadKind::Mixed,
};

void testDeterminism()
{
    for (const auto kind : kAllKinds)
    {
        const auto first = fluxmatch::bench::generateWorkload(kind, 5000, 1234);
        const auto second = fluxmatch::bench::generateWorkload(kind, 5000, 1234);
        check(first == second, "same seed produced a different stream");
        check(fluxmatch::bench::hashCommands(first) ==
                  fluxmatch::bench::hashCommands(second),
              "same seed produced a different hash");
    }
}

void testSeedChangesStream()
{
    const auto a = fluxmatch::bench::generateWorkload(WorkloadKind::Mixed, 5000, 1);
    const auto b = fluxmatch::bench::generateWorkload(WorkloadKind::Mixed, 5000, 2);
    check(a != b, "different seeds produced identical mixed streams");
    check(fluxmatch::bench::hashCommands(a) != fluxmatch::bench::hashCommands(b),
          "different seeds produced the same hash");
}

void testCancelTargets()
{
    for (const auto kind : {WorkloadKind::Cancel, WorkloadKind::Mixed})
    {
        const auto commands = fluxmatch::bench::generateWorkload(kind, 10000, 7);
        for (std::size_t i = 0; i < commands.size(); ++i)
        {
            if (commands[i].kind != CommandKind::Cancel)
                continue;
            check(commands[i].cancelTarget < i,
                  "cancel target must refer to an earlier command");
            check(commands[commands[i].cancelTarget].kind == CommandKind::Submit,
                  "cancel target must be a submit command");
        }
    }
}

void testSweepShape()
{
    const auto commands =
        fluxmatch::bench::generateWorkload(WorkloadKind::MultiLevelSweep, 1100, 0);

    std::size_t sweeps = 0;
    for (const auto &command : commands)
    {
        if (command.kind == CommandKind::Submit &&
            command.side == Side::BUY)
        {
            check(command.quantity == 10, "sweep buy must consume ten levels");
            ++sweeps;
        }
    }
    check(sweeps == 100, "expected 100 sweeps in 1100 commands");
}

void testSizes()
{
    const auto empty =
        fluxmatch::bench::generateWorkload(WorkloadKind::Mixed, 0, 42);
    check(empty.empty(), "zero commands must produce an empty stream");

    const auto one =
        fluxmatch::bench::generateWorkload(WorkloadKind::OneLevelFill, 1, 42);
    check(one.size() == 1, "one command must produce exactly one command");
}

void testNames()
{
    for (const auto kind : kAllKinds)
    {
        const auto parsed = fluxmatch::bench::parseWorkload(
            fluxmatch::bench::toString(kind));
        check(parsed.has_value() && *parsed == kind, "workload name round-trip failed");
    }
    check(!fluxmatch::bench::parseWorkload("nonsense").has_value(),
          "unknown workload must not parse");
}

} // namespace

int main()
{
    testDeterminism();
    testSeedChangesStream();
    testCancelTargets();
    testSweepShape();
    testSizes();
    testNames();

    std::cout << "Workload generator checks passed\n";
    return 0;
}
