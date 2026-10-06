#include "MarketDepth.hpp"
#include <iostream>

namespace fluxmatch
{
    std::uint64_t MarketDepth::getLastUpdateId() const
    {
        return lastUpdateId;
    }
    void MarketDepth::apply(const DepthUpdate &update)
    {
        for (const auto &x : update.bids)
        {
            if (x.quantity == 0)
                bids.erase(x.price);
            else
                bids[x.price] = x.quantity;
        }

        for (const auto &x : update.asks)
        {
            if (x.quantity == 0)
                asks.erase(x.price);
            else
                asks[x.price] = x.quantity;
        }

        lastUpdateId = update.lastUpdateId;
    }

    void MarketDepth::printTop() const
    {
        if (!bids.empty())
        {
            auto it = bids.rbegin();

            std::cout << "BEST BID "
                      << it->first << " "
                      << it->second << '\n';
        }

        if (!asks.empty())
        {
            auto it = asks.begin();

            std::cout << "BEST ASK "
                      << it->first << " "
                      << it->second << '\n';
        }
    }
    void MarketDepth::initialize(
        const DepthSnapshot &snapshot)
    {
        bids.clear();
        asks.clear();

        for (const auto &x : snapshot.bids)
            bids[x.price] = x.quantity;

        for (const auto &x : snapshot.asks)
            asks[x.price] = x.quantity;

        lastUpdateId = snapshot.lastUpdateId;
    }
}