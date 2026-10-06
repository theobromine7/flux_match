#pragma once

enum class OrderState
{
    NEW,
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED,
    EXPIRED
};