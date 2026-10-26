#include <fluxmatch/MatchingEngine.hpp>
#include <algorithm>
#include <iostream>
MatchingEngine::MatchingEngine()
{
    trades.reserve(1000000);
}
const OrderBook &MatchingEngine::getOrderBook() const
{
    return orderBook;
}
long long MatchingEngine::submitOrder(
    long long price,
    long long quantity,
    Side side,
    OrderType type)
{
    long long orderId =
        orderBook.addOrder(price, quantity, side, type, OrderState::NEW);

    match(orderId);

    return orderId;
}

void MatchingEngine::cancelOrder(long long orderId)
{
    orderBook.cancelOrder(orderId);
}

void MatchingEngine::match(long long incomingOrderId)
{
    while (true)
    {
        Order *incomingOrder =
            orderBook.getOrder(incomingOrderId);

        if (!incomingOrder)
            return;

        // =================================================
        // INCOMING BUY
        // =================================================

        if (incomingOrder->getSide() == Side::BUY)
        {
            auto bestSellId = orderBook.getBestSell();

            if (!bestSellId)
            {
                // Market order cannot rest.
                if (incomingOrder->getType() == OrderType::MARKET)
                {
                    orderBook.removeFromBook(incomingOrderId);
                    incomingOrder->setState(OrderState::EXPIRED);
                }

                return;
            }

            Order *sellOrder =
                orderBook.getOrder(*bestSellId);

            // LIMIT BUY does not cross.
            if (incomingOrder->getType() == OrderType::LIMIT &&
                incomingOrder->getPrice() < sellOrder->getPrice())
            {
                return;
            }

            long long tradeQuantity =
                std::min(
                    incomingOrder->getRemainingQuantity(),
                    sellOrder->getRemainingQuantity());

            long long buyOrderId =
                incomingOrder->getId();

            long long sellOrderId =
                sellOrder->getId();

            long long tradePrice =
                sellOrder->getPrice();

            executeTrade(incomingOrder, sellOrder, tradeQuantity, tradePrice);
            if (incomingOrder->getRemainingQuantity() == 0)
                return;
        }

        // =================================================
        // INCOMING SELL
        // =================================================

        else
        {
            auto bestBuyId =
                orderBook.getBestBuy();

            if (!bestBuyId)
            {
                // Market order cannot rest.
                if (incomingOrder->getType() == OrderType::MARKET)
                {
                    orderBook.removeFromBook(incomingOrderId);
                    incomingOrder->setState(OrderState::EXPIRED);
                }
                return;
            }

            Order *buyOrder =
                orderBook.getOrder(*bestBuyId);

            // LIMIT SELL does not cross.
            if (incomingOrder->getType() == OrderType::LIMIT &&
                incomingOrder->getPrice() > buyOrder->getPrice())
            {
                return;
            }

            long long tradeQuantity =
                std::min(
                    incomingOrder->getRemainingQuantity(),
                    buyOrder->getRemainingQuantity());

            long long buyOrderId =
                buyOrder->getId();

            long long sellOrderId =
                incomingOrder->getId();

            long long tradePrice =
                buyOrder->getPrice();

            executeTrade(buyOrder, incomingOrder, tradeQuantity, tradePrice);
            if (incomingOrder->getRemainingQuantity() == 0)
                return;
        }
    }
}
std::size_t MatchingEngine::getTradeCapacity() const
{
    return trades.capacity();
}
void MatchingEngine::executeTrade(
    Order *buyOrder,
    Order *sellOrder,
    long long tradeQuantity,
    long long tradePrice)
{
    if (!buyOrder || !sellOrder)
        return;

    // 1. Reduce quantities
    buyOrder->reduceQuantity(tradeQuantity);
    sellOrder->reduceQuantity(tradeQuantity);

    // 2. Record trade

    trades.emplace_back(
        buyOrder->getId(),
        sellOrder->getId(),
        tradePrice,
        tradeQuantity);

    // 3. Update BUY state
    if (buyOrder->getRemainingQuantity() == 0)
        buyOrder->setState(OrderState::FILLED);
    else
        buyOrder->setState(OrderState::PARTIALLY_FILLED);

    // 4. Update SELL state
    if (sellOrder->getRemainingQuantity() == 0)
        sellOrder->setState(OrderState::FILLED);
    else
        sellOrder->setState(OrderState::PARTIALLY_FILLED);

    // 5. Remove FILLED orders from active book
    if (buyOrder->getState() == OrderState::FILLED)
        orderBook.removeFromBook(buyOrder->getId());

    if (sellOrder->getState() == OrderState::FILLED)
        orderBook.removeFromBook(sellOrder->getId());
}
const std::vector<Trade> &MatchingEngine::getTrades() const
{
    return trades;
}