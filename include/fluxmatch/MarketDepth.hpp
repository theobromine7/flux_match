#pragma once
#include <cstdint>
#include <fluxmatch/MarketData.hpp>
#include <map>

namespace fluxmatch
{
    class MarketDepth
    {

    private:
        std::map<long long, long long> bids;
        std::map<long long, long long> asks;
        std::uint64_t lastUpdateId = 0;

    public:
        void apply(const DepthUpdate &update);
        void printTop() const;
        void initialize(const DepthSnapshot &snapshot);
        std::uint64_t getLastUpdateId() const;
    };
}