#pragma once

#include <vector>

#include <fluxmatch/Order.hpp>
#include <fluxmatch/OrderBook.hpp>
#include <fluxmatch/Trade.hpp>

class MatchingEngine
{
    OrderBook orderBook;
    std::vector<Trade> trades;

public:
    long long submitOrder(
        long long price,
        long long quantity,
        Side side,
        OrderType type);

    void cancelOrder(long long orderId);
    MatchingEngine();
    void match(long long incomingOrderId);
    void executeTrade(
        Order *buyOrder,
        Order *sellOrder,
        long long tradeQuantity,
        long long tradePrice);
    std::size_t getTradeCapacity() const;
    const std::vector<Trade> &getTrades() const;

    const OrderBook &getOrderBook() const;
};