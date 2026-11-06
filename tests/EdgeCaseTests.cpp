#include <fluxmatch/MatchingEngine.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

void check(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error("Edge-case check failed: " + message);
}

void checkHealthy(const MatchingEngine &engine, const std::string &where)
{
    std::string error;
    if (!engine.getOrderBook().checkInvariants(&error))
        throw std::runtime_error("Invariant check failed at " + where + ": " + error);
}

void emptyBook()
{
    MatchingEngine engine;
    check(!engine.getOrderBook().getBestBuy(), "empty book has no bid");
    check(!engine.getOrderBook().getBestSell(), "empty book has no ask");
    check(engine.getTrades().empty(), "empty book has no trades");

    const auto market = engine.submitOrder(0, 10, Side::BUY, OrderType::MARKET);
    check(market != 0, "valid market order is accepted");
    check(engine.getTrades().empty(), "market order into empty book does not trade");
    check(engine.getOrderBook().getOrder(market)->getState() == OrderState::EXPIRED,
          "unfilled market order expires");
    checkHealthy(engine, "emptyBook");
}

void partialFillThenCancel()
{
    MatchingEngine engine;
    const auto sell = engine.submitOrder(100, 10, Side::SELL, OrderType::LIMIT);
    const auto buy = engine.submitOrder(100, 4, Side::BUY, OrderType::LIMIT);

    check(engine.getTrades().size() == 1, "one trade");
    check(engine.getTrades()[0].getQuantity() == 4, "trade quantity is the smaller side");
    check(engine.getOrderBook().getOrder(sell)->getRemainingQuantity() == 6,
          "resting order keeps its remainder");
    check(engine.getOrderBook().getOrder(buy)->getState() == OrderState::FILLED,
          "incoming order is filled");

    engine.cancelOrder(sell);
    check(engine.getOrderBook().getOrder(sell)->getState() == OrderState::CANCELLED,
          "partially filled order can be cancelled");
    check(engine.getOrderBook().getOrder(sell)->getRemainingQuantity() == 6,
          "cancel preserves the unfilled quantity");
    check(!engine.getOrderBook().getBestSell(), "cancelled order leaves the book");
    checkHealthy(engine, "partialFillThenCancel");
}

void cancelTwiceAndCancelFilled()
{
    MatchingEngine engine;
    const auto bid = engine.submitOrder(99, 5, Side::BUY, OrderType::LIMIT);
    engine.cancelOrder(bid);
    engine.cancelOrder(bid);
    check(engine.getOrderBook().getOrder(bid)->getState() == OrderState::CANCELLED,
          "second cancel is harmless");
    check(!engine.getOrderBook().getBestBuy(), "cancelled bid is gone");

    const auto ask = engine.submitOrder(100, 3, Side::SELL, OrderType::LIMIT);
    const auto take = engine.submitOrder(100, 3, Side::BUY, OrderType::LIMIT);
    check(engine.getOrderBook().getOrder(ask)->getState() == OrderState::FILLED,
          "ask is filled");
    engine.cancelOrder(ask);
    check(engine.getOrderBook().getOrder(ask)->getState() == OrderState::FILLED,
          "cancelling a filled order does not rewrite history");
    check(engine.getOrderBook().getOrder(take)->getState() == OrderState::FILLED,
          "taking order is filled");
    checkHealthy(engine, "cancelTwiceAndCancelFilled");
}

void invalidOrdersAreRejected()
{
    MatchingEngine engine;

    check(engine.submitOrder(100, 0, Side::BUY, OrderType::LIMIT) == 0,
          "zero quantity is rejected");
    check(engine.submitOrder(100, -1, Side::BUY, OrderType::LIMIT) == 0,
          "negative quantity is rejected");
    check(engine.submitOrder(0, 5, Side::BUY, OrderType::LIMIT) == 0,
          "zero limit price is rejected");
    check(engine.submitOrder(-5, 5, Side::SELL, OrderType::LIMIT) == 0,
          "negative limit price is rejected");
    check(engine.submitOrder(0, 5, Side::BUY, OrderType::MARKET) != 0,
          "market price is ignored");

    check(!engine.getOrderBook().getBestBuy(), "rejected orders do not rest");
    check(!engine.getOrderBook().getBestSell(), "rejected orders do not rest");
    check(engine.getTrades().empty(), "rejected orders do not trade");
    checkHealthy(engine, "invalidOrdersAreRejected");
}

void priceTimePriority()
{
    MatchingEngine engine;
    const auto first = engine.submitOrder(100, 2, Side::SELL, OrderType::LIMIT);
    const auto second = engine.submitOrder(100, 3, Side::SELL, OrderType::LIMIT);
    const auto better = engine.submitOrder(99, 1, Side::SELL, OrderType::LIMIT);

    const auto buyer = engine.submitOrder(100, 6, Side::BUY, OrderType::LIMIT);
    const auto &trades = engine.getTrades();

    check(trades.size() == 3, "three price levels/orders are consumed");
    check(trades[0].getSellOrderId() == better && trades[0].getPrice() == 99,
          "better price trades first");
    check(trades[1].getSellOrderId() == first && trades[1].getPrice() == 100,
          "earlier order wins time priority");
    check(trades[2].getSellOrderId() == second && trades[2].getQuantity() == 3,
          "later order trades after the earlier one");
    check(engine.getOrderBook().getOrder(buyer)->getState() == OrderState::FILLED,
          "buyer is fully filled");
    checkHealthy(engine, "priceTimePriority");
}

void marketOrderSweepsAndExpires()
{
    MatchingEngine engine;
    engine.submitOrder(101, 2, Side::SELL, OrderType::LIMIT);
    engine.submitOrder(102, 2, Side::SELL, OrderType::LIMIT);

    const auto buy = engine.submitOrder(0, 5, Side::BUY, OrderType::MARKET);
    check(engine.getTrades().size() == 2, "market order sweeps two levels");
    check(engine.getTrades()[0].getPrice() == 101, "first trade at best ask");
    check(engine.getTrades()[1].getPrice() == 102, "second trade at next ask");
    check(engine.getOrderBook().getOrder(buy)->getState() == OrderState::EXPIRED,
          "market remainder expires");
    check(engine.getOrderBook().getOrder(buy)->getRemainingQuantity() == 1,
          "market remainder is recorded");
    check(!engine.getOrderBook().getBestSell(), "book is empty after the sweep");
    checkHealthy(engine, "marketOrderSweepsAndExpires");
}

void noCrossingLimitRests()
{
    MatchingEngine engine;
    const auto bid = engine.submitOrder(100, 5, Side::BUY, OrderType::LIMIT);
    const auto ask = engine.submitOrder(101, 5, Side::SELL, OrderType::LIMIT);

    check(engine.getTrades().empty(), "non-crossing limit orders do not trade");
    check(engine.getOrderBook().getBestBuy() == bid, "bid rests");
    check(engine.getOrderBook().getBestSell() == ask, "ask rests");
    check(engine.getOrderBook().getOrder(bid)->getState() == OrderState::NEW,
          "resting bid stays NEW");
    checkHealthy(engine, "noCrossingLimitRests");
}

void extremePrices()
{
    MatchingEngine engine;
    const auto low = engine.submitOrder(1, 1, Side::BUY, OrderType::LIMIT);
    const auto high = engine.submitOrder(std::numeric_limits<long long>::max(),
                                         1, Side::SELL, OrderType::LIMIT);

    check(engine.getOrderBook().getBestBuy() == low, "low bid is best bid");
    check(engine.getOrderBook().getBestSell() == high, "high ask is best ask");
    check(engine.getTrades().empty(), "far-apart orders do not trade");
    checkHealthy(engine, "extremePrices");
}

} // namespace

void runEdgeCaseTests()
{
    emptyBook();
    partialFillThenCancel();
    cancelTwiceAndCancelFilled();
    invalidOrdersAreRejected();
    priceTimePriority();
    marketOrderSweepsAndExpires();
    noCrossingLimitRests();
    extremePrices();
    std::cout << "Edge-case checks passed\n";
}
