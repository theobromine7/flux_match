#pragma once

#include <fluxmatch/OrderState.hpp>
#include <fluxmatch/OrderType.hpp>
#include <fluxmatch/Side.hpp>

class Order
{
    long long id;
    long long price;

    long long quantity;          // Original quantity
    long long remainingQuantity; // Quantity left

    Side side;
    OrderType type;
    OrderState state;

public:
    Order(long long id,
          long long price,
          long long quantity,
          Side side,
          OrderType type,
          OrderState state)
        : id(id),
          price(price),
          quantity(quantity),
          remainingQuantity(quantity),
          side(side),
          type(type),
          state(state)
    {
    }

    long long getId() const;
    long long getPrice() const;
    long long getQuantity() const;
    long long getRemainingQuantity() const;

    Side getSide() const;
    OrderType getType() const;
    OrderState getState() const;

    void reduceQuantity(long long value);
    void setState(OrderState newState);
};