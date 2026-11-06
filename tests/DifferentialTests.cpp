#include <fluxmatch/MatchingEngine.hpp>

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

#include "reference_model.hpp"

void runEdgeCaseTests();

namespace
{

void check(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error("Differential check failed: " + message);
}

std::string describe(std::size_t seed, int step)
{
    return "seed " + std::to_string(seed) + ", step " + std::to_string(step);
}

void compareState(const MatchingEngine &engine,
                  const ReferenceEngine &reference,
                  std::size_t seed,
                  int step)
{
    const auto &book = engine.getOrderBook();
    const auto &refOrders = reference.getOrders();

    check(book.getBestBuy() == reference.bestBuy(),
          "best bid differs at " + describe(seed, step));
    check(book.getBestSell() == reference.bestSell(),
          "best ask differs at " + describe(seed, step));

    for (std::size_t index = 0; index < refOrders.size(); ++index)
    {
        const auto &expected = refOrders[index];
        const Order *actual = book.getOrder(expected.id);
        check(actual != nullptr,
              "engine is missing order " + std::to_string(expected.id) +
                  " at " + describe(seed, step));
        check(actual->getPrice() == expected.price,
              "price differs for order " + std::to_string(expected.id) +
                  " at " + describe(seed, step));
        check(actual->getQuantity() == expected.quantity,
              "quantity differs for order " + std::to_string(expected.id) +
                  " at " + describe(seed, step));
        check(actual->getRemainingQuantity() == expected.remaining,
              "remaining quantity differs for order " +
                  std::to_string(expected.id) + " at " + describe(seed, step));
        check(actual->getSide() == expected.side,
              "side differs for order " + std::to_string(expected.id) +
                  " at " + describe(seed, step));
        check(actual->getType() == expected.type,
              "type differs for order " + std::to_string(expected.id) +
                  " at " + describe(seed, step));
        check(actual->getState() == expected.state,
              "state differs for order " + std::to_string(expected.id) +
                  " at " + describe(seed, step));
    }

    const auto &refTrades = reference.getTrades();
    const auto &actualTrades = engine.getTrades();
    check(actualTrades.size() == refTrades.size(),
          "trade count differs at " + describe(seed, step));

    for (std::size_t index = 0; index < refTrades.size(); ++index)
    {
        const auto &expected = refTrades[index];
        const auto &actual = actualTrades[index];
        check(actual.getBuyOrderId() == expected.buyOrderId,
              "trade buyer differs at index " + std::to_string(index) +
                  " at " + describe(seed, step));
        check(actual.getSellOrderId() == expected.sellOrderId,
              "trade seller differs at index " + std::to_string(index) +
                  " at " + describe(seed, step));
        check(actual.getPrice() == expected.price,
              "trade price differs at index " + std::to_string(index) +
                  " at " + describe(seed, step));
        check(actual.getQuantity() == expected.quantity,
              "trade quantity differs at index " + std::to_string(index) +
                  " at " + describe(seed, step));
    }

    std::string error;
    check(book.checkInvariants(&error),
          "engine invariant failed at " + describe(seed, step) + ": " + error);
    check(reference.checkInvariants(),
          "reference invariant failed at " + describe(seed, step));
}

void runStream(std::size_t seed, int steps)
{
    MatchingEngine engine;
    ReferenceEngine reference;
    std::mt19937_64 random(seed);

    std::uniform_int_distribution<int> sideDistribution(0, 1);
    std::uniform_int_distribution<int> typeDistribution(0, 9);
    std::uniform_int_distribution<int> quantityDistribution(1, 20);
    std::uniform_int_distribution<int> priceDistribution(90, 110);
    std::uniform_int_distribution<int> operationDistribution(0, 9);

    for (int step = 0; step < steps; ++step)
    {
        const bool submit = reference.getOrders().empty() ||
                            operationDistribution(random) < 7;

        if (submit)
        {
            const Side side = sideDistribution(random) == 0 ? Side::BUY : Side::SELL;
            const OrderType type = typeDistribution(random) == 0
                                       ? OrderType::MARKET
                                       : OrderType::LIMIT;
            const long long price = priceDistribution(random);
            const long long quantity = quantityDistribution(random);

            const long long actualId = engine.submitOrder(price, quantity, side, type);
            const long long expectedId = reference.submit(price, quantity, side, type);
            check(actualId == expectedId,
                  "order ID differs at " + describe(seed, step));
        }
        else
        {
            const auto &orders = reference.getOrders();
            std::uniform_int_distribution<std::size_t> orderDistribution(
                0, orders.size() - 1);
            const long long id = orders[orderDistribution(random)].id;

            engine.cancelOrder(id);
            reference.cancel(id);
        }

        compareState(engine, reference, seed, step);
    }
}

void runDifferentialTests()
{
    for (std::size_t seed = 1; seed <= 40; ++seed)
        runStream(seed, 750);

    std::cout << "Differential checks passed (40 seeds x 750 operations)\n";
}

} // namespace

int main()
{
    try
    {
        runEdgeCaseTests();
        runDifferentialTests();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    std::cout << "Model checks passed\n";
    return 0;
}
