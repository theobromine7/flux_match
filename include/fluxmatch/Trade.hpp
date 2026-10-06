#pragma once
#include <vector>
class Trade
{
    long long buyOrderId;
    long long sellOrderId;
    long long price;
    long long quantity;

public:
    Trade(long long buyOrderId,
          long long sellOrderId,
          long long price,
          long long quantity)
        : buyOrderId(buyOrderId),
          sellOrderId(sellOrderId),
          price(price),
          quantity(quantity)
    {
    }

    long long getBuyOrderId() const;
    long long getSellOrderId() const;
    long long getPrice() const;
    long long getQuantity() const;
};