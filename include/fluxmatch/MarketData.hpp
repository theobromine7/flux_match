#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fluxmatch
{
    struct PriceLevel
    {
        long long price;
        long long quantity;
    };

    struct Instrument
    {
        long long priceScale;
        long long quantityScale;
    };

    struct DepthUpdate
    {
        std::uint64_t firstUpdateId;
        std::uint64_t lastUpdateId;

        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
    };

    struct DepthSnapshot
    {
        std::uint64_t lastUpdateId;

        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
    };
    
}