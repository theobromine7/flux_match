#include <iostream>
#include <string>

#include <fluxmatch/MatchingEngine.hpp>

int main()
{
    MatchingEngine engine;

    // Two resting sells. The second has the worse price.
    const auto sell100 = engine.submitOrder(100, 2, Side::SELL, OrderType::LIMIT);
    const auto sell101 = engine.submitOrder(101, 3, Side::SELL, OrderType::LIMIT);

    // Incoming buy sweeps the 100 level, then fills against 101.
    const auto buy = engine.submitOrder(101, 4, Side::BUY, OrderType::LIMIT);

    std::cout << "FluxMatch demo\n";
    std::cout << "Trades:\n";
    for (const auto &trade : engine.getTrades())
    {
        std::cout << "  buy=" << trade.getBuyOrderId()
                  << " sell=" << trade.getSellOrderId()
                  << " price=" << trade.getPrice()
                  << " qty=" << trade.getQuantity()
                  << '\n';
    }

    const auto &book = engine.getOrderBook();
    const auto bestSell = book.getBestSell();
    std::cout << "Best sell: "
              << (bestSell ? std::to_string(*bestSell) : "none")
              << '\n';

    // A non-crossing bid rests and becomes the best buy.
    const auto bid = engine.submitOrder(99, 1, Side::BUY, OrderType::LIMIT);
    const auto bestBuy = book.getBestBuy();
    std::cout << "Best buy: "
              << (bestBuy ? std::to_string(*bestBuy) : "none")
              << " (expected " << bid << ")\n";

    // Reference the unused ids so the intent is explicit without extra output.
    (void)sell100;
    (void)sell101;
    (void)buy;

    return 0;
}
