#include "Order.hpp"

long long Order::getId() const
{
    return id;
}

long long Order::getPrice() const
{
    return price;
}

long long Order::getQuantity() const
{
    return quantity;
}

long long Order::getRemainingQuantity() const
{
    return remainingQuantity;
}

Side Order::getSide() const
{
    return side;
}

OrderType Order::getType() const
{
    return type;
}

OrderState Order::getState() const
{
    return state;
}

void Order::reduceQuantity(long long value)
{
    remainingQuantity -= value;

    if (remainingQuantity == 0)
    {
        state = OrderState::FILLED;
    }
    else if (remainingQuantity < quantity)
    {
        state = OrderState::PARTIALLY_FILLED;
    }
}

void Order::setState(OrderState newState)
{
    state = newState;
}