#include <fluxmatch/OrderBook.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <unordered_set>

namespace
{

bool fail(std::string *error, const std::string &message)
{
    if (error)
        *error = message;
    return false;
}

bool isActive(OrderState state)
{
    return state == OrderState::NEW || state == OrderState::PARTIALLY_FILLED;
}

} // namespace

std::vector<OrderBook::PriceLevel>::iterator OrderBook::findLevel(
    std::vector<PriceLevel> &levels, long long price)
{
    return std::lower_bound(levels.begin(), levels.end(), price,
        [](const PriceLevel &level, long long value) { return level.price < value; });
}

void OrderBook::printLevels(const std::vector<PriceLevel> &levels) const
{
    for (auto level = levels.rbegin(); level != levels.rend(); ++level)
    {
        std::cout << "Price: " << level->price << " | ";
        for (long long id = level->head; id != 0;)
        {
            const auto &entry = orders[static_cast<std::size_t>(id - 1)];
            std::cout << "[ID=" << id << ", Qty="
                      << entry.order.getRemainingQuantity() << "] ";
            id = entry.next;
        }
        std::cout << '\n';
    }
}

void OrderBook::printOrderBook() const
{
    std::cout << "\n--- SELL ORDERS ---\n";
    printLevels(sellOrders);
    std::cout << "\n--- BUY ORDERS ---\n";
    printLevels(buyOrders);
}

long long OrderBook::addOrder(long long price, long long quantity, Side side,
                            OrderType type, OrderState state)
{
    // Rejected orders do not consume an ID and do not touch the book.
    if (quantity <= 0)
        return 0;
    if (type == OrderType::LIMIT && price <= 0)
        return 0;

    const auto id = static_cast<long long>(orders.size()) + 1;
    orders.push_back(OrderEntry{Order(id, price, quantity, side, type, state)});
    if (type == OrderType::LIMIT)
    {
        auto &levels = side == Side::BUY ? buyOrders : sellOrders;
        auto level = levels.end();
        // Frequent best-price inserts avoid a binary search.
        if (!levels.empty() && levels.back().price == price)
            level = levels.end() - 1;
        else if (!levels.empty() && levels.front().price == price)
            level = levels.begin();
        else
            level = findLevel(levels, price);
        try
        {
            if (level == levels.end() || level->price != price)
                level = levels.insert(level, PriceLevel{price});
        }
        catch (...)
        {
            orders.pop_back();
            throw;
        }
        auto &entry = orders.back();
        entry.previous = level->tail;
        entry.resting = true;
        if (level->tail != 0)
            orders[static_cast<std::size_t>(level->tail - 1)].next = id;
        else
            level->head = id;
        level->tail = id;
    }
    return id;
}

std::optional<long long> OrderBook::getBestBuy() const
{
    if (buyOrders.empty()) return std::nullopt;
    return buyOrders.back().head;
}
std::optional<long long> OrderBook::getBestSell() const
{
    if (sellOrders.empty()) return std::nullopt;
    return sellOrders.front().head;
}
Order *OrderBook::getOrder(long long orderId)
{
    if (orderId <= 0 || static_cast<unsigned long long>(orderId) > orders.size())
        return nullptr;
    return &orders[static_cast<std::size_t>(orderId - 1)].order;
}
const Order *OrderBook::getOrder(long long orderId) const
{
    if (orderId <= 0 || static_cast<unsigned long long>(orderId) > orders.size())
        return nullptr;
    return &orders[static_cast<std::size_t>(orderId - 1)].order;
}
void OrderBook::cancelOrder(long long orderId)
{
    auto *order = getOrder(orderId);
    if (!order)
        return;

    // Terminal states are final: cancelling a filled or expired order is a
    // no-op. Only live orders transition to CANCELLED.
    if (!isActive(order->getState()))
        return;

    order->setState(OrderState::CANCELLED);
    removeFromBook(orderId);
}
void OrderBook::removeFromBook(long long orderId)
{
    if (!getOrder(orderId)) return;
    auto &entry = orders[static_cast<std::size_t>(orderId - 1)];
    if (!entry.resting) return;
    auto &levels = entry.order.getSide() == Side::BUY ? buyOrders : sellOrders;
    auto level = findLevel(levels, entry.order.getPrice());
    if (entry.previous != 0)
        orders[static_cast<std::size_t>(entry.previous - 1)].next = entry.next;
    else
        level->head = entry.next;
    if (entry.next != 0)
        orders[static_cast<std::size_t>(entry.next - 1)].previous = entry.previous;
    else
        level->tail = entry.previous;
    entry.previous = entry.next = 0;
    entry.resting = false;
    if (level->head == 0) levels.erase(level);
}

bool OrderBook::checkInvariants(std::string *error) const
{
    const auto checkLevels = [&](const std::vector<PriceLevel> &levels,
                                 Side side,
                                 const char *sideName) -> bool
    {
        bool first = true;
        long long previousPrice = 0;
        std::unordered_set<long long> seen;

        for (const auto &level : levels)
        {
            if (!first && level.price <= previousPrice)
                return fail(error, std::string(sideName) +
                                      " price levels are not strictly ascending");
            first = false;
            previousPrice = level.price;

            if (level.head == 0 || level.tail == 0)
                return fail(error, std::string(sideName) +
                                      " price level is empty");
            if (level.price <= 0)
                return fail(error, std::string(sideName) +
                                      " price level has a non-positive price");

            long long id = level.head;
            long long previousId = 0;
            std::size_t count = 0;

            while (id != 0)
            {
                if (id < 0 || static_cast<unsigned long long>(id) > orders.size())
                    return fail(error, std::string(sideName) +
                                          " order chain contains an invalid ID");

                const auto &entry = orders[static_cast<std::size_t>(id - 1)];

                if (!entry.resting)
                    return fail(error, std::string(sideName) +
                                          " order chain contains a non-resting order");
                if (entry.previous != previousId)
                    return fail(error, std::string(sideName) +
                                          " order chain has a broken previous link");
                if (entry.order.getSide() != side)
                    return fail(error, std::string(sideName) +
                                          " order chain contains the wrong side");
                if (entry.order.getType() != OrderType::LIMIT)
                    return fail(error, std::string(sideName) +
                                          " order chain contains a non-limit order");
                if (entry.order.getPrice() != level.price)
                    return fail(error, std::string(sideName) +
                                          " order chain contains the wrong price");
                if (!isActive(entry.order.getState()))
                    return fail(error, std::string(sideName) +
                                          " order chain contains a terminal order");
                if (entry.order.getRemainingQuantity() <= 0)
                    return fail(error, std::string(sideName) +
                                          " order chain contains a zero-quantity order");
                if (!seen.insert(id).second)
                    return fail(error, std::string(sideName) +
                                          " order chain contains a cycle");

                previousId = id;
                id = entry.next;
                ++count;
                if (count > orders.size())
                    return fail(error, std::string(sideName) +
                                          " order chain contains a cycle");
            }

            if (previousId != level.tail)
                return fail(error, std::string(sideName) +
                                      " order chain does not end at the level tail");
        }

        return true;
    };

    if (!checkLevels(buyOrders, Side::BUY, "buy"))
        return false;
    if (!checkLevels(sellOrders, Side::SELL, "sell"))
        return false;

    for (std::size_t index = 0; index < orders.size(); ++index)
    {
        const auto &entry = orders[index];
        const auto &order = entry.order;
        const long long expectedId = static_cast<long long>(index) + 1;

        if (order.getId() != expectedId)
            return fail(error, "order ID does not match its storage position");

        if (entry.resting)
        {
            if (!isActive(order.getState()))
                return fail(error, "resting order is in a terminal state");
            if (order.getRemainingQuantity() <= 0)
                return fail(error, "resting order has no remaining quantity");
            if (order.getType() != OrderType::LIMIT)
                return fail(error, "resting order is not a limit order");
        }
        else if (entry.previous != 0 || entry.next != 0)
        {
            return fail(error, "non-resting order has non-zero book links");
        }

        switch (order.getState())
        {
        case OrderState::NEW:
            if (order.getRemainingQuantity() != order.getQuantity())
                return fail(error, "NEW order has an inconsistent quantity");
            break;
        case OrderState::PARTIALLY_FILLED:
            if (order.getRemainingQuantity() <= 0 ||
                order.getRemainingQuantity() >= order.getQuantity())
                return fail(error, "PARTIALLY_FILLED order has an inconsistent quantity");
            break;
        case OrderState::FILLED:
            if (order.getRemainingQuantity() != 0)
                return fail(error, "FILLED order still has remaining quantity");
            break;
        case OrderState::CANCELLED:
        case OrderState::EXPIRED:
            break;
        }
    }

    if (!buyOrders.empty() && !sellOrders.empty() &&
        buyOrders.back().price >= sellOrders.front().price)
    {
        return fail(error, "book is crossed");
    }

    return true;
}
