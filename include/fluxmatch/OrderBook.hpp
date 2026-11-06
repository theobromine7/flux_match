#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <fluxmatch/Order.hpp>

class OrderBook
{
public:
    long long addOrder(long long price, long long quantity, Side side,
                       OrderType type, OrderState state);
    void cancelOrder(long long orderId);
    std::optional<long long> getBestBuy() const;
    std::optional<long long> getBestSell() const;
    Order *getOrder(long long orderId);
    const Order *getOrder(long long orderId) const;
    void removeFromBook(long long orderId);
    void printOrderBook() const;

    // Validates the internal book structure and every order's lifecycle state.
    // Returns true when the book satisfies docs/INVARIANTS.md; on failure,
    // writes a human-readable reason to *error when error is not null.
    bool checkInvariants(std::string *error = nullptr) const;

private:
    struct PriceLevel
    {
        long long price;
        long long head = 0;
        long long tail = 0;
    };
    struct OrderEntry
    {
        Order order;
        long long previous = 0;
        long long next = 0;
        bool resting = false;
    };

    // Chunked storage keeps Order* pointers stable while amortizing allocation:
    // one heap allocation per chunk instead of one per order.
    class OrderPool
    {
    public:
        OrderEntry &create(long long price, long long quantity, Side side,
                           OrderType type, OrderState state);
        OrderEntry &at(std::size_t index);
        const OrderEntry &at(std::size_t index) const;
        std::size_t size() const;
        void discardLast();

    private:
        static constexpr std::size_t kChunkSize = 4096;
        struct Chunk
        {
            std::unique_ptr<std::byte[]> storage;
            std::size_t used = 0;
        };
        std::vector<Chunk> chunks;
        std::size_t count = 0;
    };

    // Ascending prices. Creating/deleting levels shifts contiguous entries.
    std::vector<PriceLevel> buyOrders;
    std::vector<PriceLevel> sellOrders;
    // Dense sequential IDs. Chunk storage preserves pointers on insertion.
    // Historical orders remain available, as in the original implementation.
    OrderPool orders;
    static std::vector<PriceLevel>::iterator findLevel(
        std::vector<PriceLevel> &levels, long long price);
    void printLevels(const std::vector<PriceLevel> &levels) const;
};
