#include <fluxmatch/MatchingEngine.hpp>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

static void check(bool condition)
{
    if (!condition)
        throw std::runtime_error("Order book regression check failed");
}

static void testBook()
{
    OrderBook book;
    struct Expected
    {
        long long id;
        long long price;
        Side side;
        bool active;
    };
    std::vector<Expected> reference;
    std::mt19937 random(42);
    Order *first = nullptr;
    for (int step = 0; step < 20000; ++step)
    {
        if (reference.empty() || random() % 3 != 0)
        {
            const long long price = static_cast<long long>(random() % 201) - 100;
            const auto side = random() % 2 ? Side::BUY : Side::SELL;
            const auto type = random() % 10 ? OrderType::LIMIT : OrderType::MARKET;
            auto id = book.addOrder(price, 5, side, type, OrderState::NEW);
            reference.push_back({id, price, side, type == OrderType::LIMIT});
            if (!first)
                first = book.getOrder(id);
            check(first == book.getOrder(1));
        }
        else
        {
            auto &order = reference[random() % reference.size()];
            if (random() % 2)
                book.cancelOrder(order.id);
            else
                book.removeFromBook(order.id);
            order.active = false;
            book.removeFromBook(order.id); // Repeated removal must be harmless.
        }
        std::optional<long long> buy, sell;
        long long buyPrice = 0, sellPrice = 0;
        for (const auto &order : reference)
        {
            if (!order.active)
                continue;
            if (order.side == Side::BUY && (!buy || order.price > buyPrice))
            {
                buy = order.id;
                buyPrice = order.price;
            }
            if (order.side == Side::SELL && (!sell || order.price < sellPrice))
            {
                sell = order.id;
                sellPrice = order.price;
            }
        }
        check(book.getBestBuy() == buy);
        check(book.getBestSell() == sell);
    }
    for (const auto &order : reference)
        book.removeFromBook(order.id);
    check(!book.getBestBuy() && !book.getBestSell());
    check(!book.getOrder(0) && !book.getOrder(-1) && !book.getOrder(999999));
    book.cancelOrder(-1);
    book.removeFromBook(999999);
    const auto low = book.addOrder(std::numeric_limits<long long>::min(), 1,
                                   Side::BUY, OrderType::LIMIT, OrderState::NEW);
    const auto high = book.addOrder(std::numeric_limits<long long>::max(), 1,
                                    Side::BUY, OrderType::LIMIT, OrderState::NEW);
    check(book.getBestBuy() == high);
    book.removeFromBook(high);
    check(book.getBestBuy() == low);
}

static void testMatching()
{
    MatchingEngine engine;
    const auto a = engine.submitOrder(101, 3, Side::SELL, OrderType::LIMIT);
    const auto b = engine.submitOrder(100, 2, Side::SELL, OrderType::LIMIT);
    const auto c = engine.submitOrder(100, 4, Side::SELL, OrderType::LIMIT);
    const auto incoming = engine.submitOrder(101, 7, Side::BUY, OrderType::LIMIT);
    const auto &trades = engine.getTrades();
    check(trades.size() == 3);
    check(trades[0].getSellOrderId() == b && trades[0].getQuantity() == 2);
    check(trades[1].getSellOrderId() == c && trades[1].getQuantity() == 4);
    check(trades[2].getSellOrderId() == a && trades[2].getQuantity() == 1);
    check(trades[0].getPrice() == 100 && trades[2].getPrice() == 101);
    check(trades[2].getBuyOrderId() == incoming);
    check(engine.getOrderBook().getBestSell() == a);
    engine.submitOrder(0, 5, Side::BUY, OrderType::MARKET);
    check(!engine.getOrderBook().getBestSell());
    check(!engine.getOrderBook().getBestBuy());
    check(trades.back().getQuantity() == 2);

    const auto bid1 = engine.submitOrder(99, 2, Side::BUY, OrderType::LIMIT);
    const auto bid2 = engine.submitOrder(99, 3, Side::BUY, OrderType::LIMIT);
    engine.cancelOrder(bid1);
    engine.submitOrder(0, 1, Side::SELL, OrderType::MARKET);
    check(trades.back().getBuyOrderId() == bid2);
    check(engine.getOrderBook().getBestBuy() == bid2);
    engine.cancelOrder(bid2);
    check(!engine.getOrderBook().getBestBuy());
}

int main()
{
    testBook();
    testMatching();
    std::cout << "Order book regression checks passed\n";
}
