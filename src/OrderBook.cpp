#include "OrderBook.hpp"
#include <algorithm>
#include <iostream>

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
void OrderBook::cancelOrder(long long orderId)
{
    if (auto *order = getOrder(orderId))
    {
        order->setState(OrderState::CANCELLED);
        removeFromBook(orderId);
    }
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
