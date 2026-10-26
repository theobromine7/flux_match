#include <fluxmatch/Trade.hpp>

long long Trade::getBuyOrderId() const
{
    return buyOrderId;
}

long long Trade::getSellOrderId() const
{
    return sellOrderId;
}

long long Trade::getPrice() const
{
    return price;
}

long long Trade::getQuantity() const
{
    return quantity;
}