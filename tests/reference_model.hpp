#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include <fluxmatch/OrderState.hpp>
#include <fluxmatch/OrderType.hpp>
#include <fluxmatch/Side.hpp>

// A deliberately simple, obviously-correct matching model used only by tests.
// It scans the whole order list for the best price and ignores performance.
// The production engine must produce exactly the same trades and final states.
struct ReferenceOrder
{
    long long id = 0;
    long long price = 0;
    long long quantity = 0;
    long long remaining = 0;
    Side side = Side::BUY;
    OrderType type = OrderType::LIMIT;
    OrderState state = OrderState::NEW;
    bool resting = false;
};

struct ReferenceTrade
{
    long long buyOrderId = 0;
    long long sellOrderId = 0;
    long long price = 0;
    long long quantity = 0;
};

class ReferenceEngine
{
public:
    long long submit(long long price,
                     long long quantity,
                     Side side,
                     OrderType type)
    {
        if (quantity <= 0)
            return 0;
        if (type == OrderType::LIMIT && price <= 0)
            return 0;

        const long long id = static_cast<long long>(orders.size()) + 1;
        orders.push_back(
            ReferenceOrder{id, price, quantity, quantity, side, type,
                           OrderState::NEW, false});
        auto &incoming = orders.back();

        while (incoming.remaining > 0)
        {
            const long long bestId = findBestOpposite(incoming);
            if (bestId == 0)
                break;

            auto &resting = orders[static_cast<std::size_t>(bestId - 1)];

            const bool crosses =
                incoming.type == OrderType::MARKET ||
                (incoming.side == Side::BUY && incoming.price >= resting.price) ||
                (incoming.side == Side::SELL && incoming.price <= resting.price);
            if (!crosses)
                break;

            const long long tradeQuantity =
                std::min(incoming.remaining, resting.remaining);
            const long long tradePrice = resting.price;

            incoming.remaining -= tradeQuantity;
            resting.remaining -= tradeQuantity;

            incoming.state = incoming.remaining == 0
                                 ? OrderState::FILLED
                                 : OrderState::PARTIALLY_FILLED;
            resting.state = resting.remaining == 0
                                ? OrderState::FILLED
                                : OrderState::PARTIALLY_FILLED;
            if (resting.remaining == 0)
                resting.resting = false;

            trades.push_back(ReferenceTrade{
                incoming.side == Side::BUY ? incoming.id : resting.id,
                incoming.side == Side::SELL ? incoming.id : resting.id,
                tradePrice,
                tradeQuantity});
        }

        if (incoming.remaining > 0)
        {
            if (incoming.type == OrderType::MARKET)
                incoming.state = OrderState::EXPIRED;
            else
                incoming.resting = true;
        }

        return id;
    }

    void cancel(long long orderId)
    {
        if (orderId <= 0 ||
            static_cast<unsigned long long>(orderId) > orders.size())
            return;

        auto &order = orders[static_cast<std::size_t>(orderId - 1)];
        if (order.state != OrderState::NEW &&
            order.state != OrderState::PARTIALLY_FILLED)
            return;

        order.state = OrderState::CANCELLED;
        order.resting = false;
    }

    std::optional<long long> bestBuy() const
    {
        return best(Side::BUY);
    }

    std::optional<long long> bestSell() const
    {
        return best(Side::SELL);
    }

    const std::vector<ReferenceOrder> &getOrders() const
    {
        return orders;
    }

    const std::vector<ReferenceTrade> &getTrades() const
    {
        return trades;
    }

    // Invariants for the reference model itself, so a bug in the model does not
    // silently become the expected answer.
    bool checkInvariants() const
    {
        for (const auto &order : orders)
        {
            if (order.remaining < 0)
                return false;
            if (order.state == OrderState::FILLED && order.remaining != 0)
                return false;
            if (order.state == OrderState::PARTIALLY_FILLED &&
                (order.remaining <= 0 || order.remaining >= order.quantity))
                return false;
            if (order.state == OrderState::NEW &&
                order.remaining != order.quantity)
                return false;
            if (order.resting &&
                (order.type != OrderType::LIMIT ||
                 order.state != OrderState::NEW &&
                     order.state != OrderState::PARTIALLY_FILLED))
                return false;
        }

        const auto buy = bestBuy();
        const auto sell = bestSell();
        if (buy && sell)
        {
            const auto &bid = orders[static_cast<std::size_t>(*buy - 1)];
            const auto &ask = orders[static_cast<std::size_t>(*sell - 1)];
            if (bid.price >= ask.price)
                return false;
        }

        return true;
    }

private:
    std::vector<ReferenceOrder> orders;
    std::vector<ReferenceTrade> trades;

    long long findBestOpposite(const ReferenceOrder &incoming) const
    {
        long long bestId = 0;
        for (const auto &order : orders)
        {
            if (!order.resting || order.side == incoming.side)
                continue;
            if (bestId == 0)
            {
                bestId = order.id;
                continue;
            }

            const auto &best = orders[static_cast<std::size_t>(bestId - 1)];
            const bool better =
                incoming.side == Side::BUY ? order.price < best.price
                                           : order.price > best.price;
            if (better)
                bestId = order.id;
            // Equal prices keep the earlier (lower) ID: time priority.
        }
        return bestId;
    }

    std::optional<long long> best(Side side) const
    {
        long long bestId = 0;
        for (const auto &order : orders)
        {
            if (!order.resting || order.side != side)
                continue;
            if (bestId == 0)
            {
                bestId = order.id;
                continue;
            }

            const auto &current = orders[static_cast<std::size_t>(bestId - 1)];
            const bool better =
                side == Side::BUY ? order.price > current.price
                                  : order.price < current.price;
            if (better)
                bestId = order.id;
        }
        return bestId == 0 ? std::nullopt
                           : std::optional<long long>(bestId);
    }
};
