# FluxMatch order and book invariants

This document is the contract the matching engine must satisfy. Tests enforce
it, the invariant checker verifies it after every operation, and the reference
model produces the expected answer for randomized command streams.

If an invariant is false, the engine is broken even if it did not crash.

## 1. Order lifecycle

Every order has exactly one state:

| State | Meaning | Terminal |
| --- | --- | --- |
| `NEW` | Accepted, nothing has traded yet | No |
| `PARTIALLY_FILLED` | Some quantity traded, some remains | No |
| `FILLED` | Entire quantity traded | Yes |
| `CANCELLED` | The unfilled remainder was cancelled | Yes |
| `EXPIRED` | A market order ran out of liquidity | Yes |

Allowed transitions:

```
NEW -> PARTIALLY_FILLED -> FILLED
NEW -> FILLED
NEW -> CANCELLED
NEW -> EXPIRED
PARTIALLY_FILLED -> CANCELLED
PARTIALLY_FILLED -> EXPIRED   (market orders only)
```

Terminal states have no outgoing transitions. In particular, cancelling a
filled order is a no-op: its trades already happened and must not be rewritten.

Quantity rules:

- `NEW`: `remaining == quantity`.
- `PARTIALLY_FILLED`: `0 < remaining < quantity`.
- `FILLED`: `remaining == 0`.
- `CANCELLED` and `EXPIRED`: the remaining quantity is recorded but the order is
  no longer live.

## 2. Submission and validation

`submitOrder(price, quantity, side, type)`:

- `quantity <= 0` is rejected.
- A `LIMIT` order with `price <= 0` is rejected.
- A `MARKET` order ignores `price`.
- A rejected order returns `0`, consumes no order ID, and does not touch the
  book or the trade history.

Accepted orders receive sequential IDs starting at `1`.

## 3. Price-time priority

- The best buy is the highest-priced resting buy.
- The best sell is the lowest-priced resting sell.
- At the same price, the earliest order (lowest ID) trades first.
- An incoming order matches against the best opposite price, level by level,
  until it is filled, would not cross, or the opposite side is empty.

## 4. Trade price

A trade executes at the **resting order's price**, not the incoming order's
price.

Reason: the resting order was in the book first and set the price. The incoming
order is the aggressor and accepts it.

Trade records contain the buy order ID, sell order ID, price, and quantity.

## 5. Cancellation

- Only live orders (`NEW` or `PARTIALLY_FILLED`) can be cancelled.
- Cancelling removes the **unfilled remainder** from the book.
- Quantity already filled stays in the trade history.
- Cancelling a terminal order is a no-op.
- Cancelling the same order twice is harmless.

Cancellation never edits or removes a trade.

## 6. Market orders

- A market order never rests in the book.
- It matches any available opposite price.
- If it cannot fill completely, the leftover is discarded and the order becomes
  `EXPIRED`.
- A market order with an empty opposite side expires immediately.

## 7. Book invariants

Checked by `OrderBook::checkInvariants()`:

1. Buy price levels are strictly ascending, and sell price levels are strictly
   ascending.
2. Every price level has a non-empty doubly linked list with consistent
   `head`, `tail`, `previous`, and `next` links.
3. Every resting order is a `LIMIT` order in a live state with positive
   remaining quantity, and its side and price match its level.
4. An order appears at most once in the book; links contain no cycles.
5. A non-resting order has zeroed book links.
6. Order IDs match their storage position: ID `n` is at index `n - 1`.
7. The book is not crossed after a command completes:
   `best bid price < best ask price` whenever both sides exist.

## 8. Determinism

- The same command sequence produces the same order IDs, trades, and final
  states.
- The benchmark workload is a pure function of workload name, command count,
  and seed. `hashCommands` is the quick identity check.
- Timing is **not** deterministic; only the logical result is.

## 9. Explicit non-goals and limitations

- Self-trade prevention is not implemented: an account can trade with itself.
- Orders cannot be modified; cancel-and-resubmit is the only change.
- Cancellation is all-or-nothing for the remainder; there is no partial cancel.
- There is no persistence, recovery, or concurrency protection yet.
- Prices are integer ticks; the engine does not define a tick size.
- Historical orders remain in memory for lookup and audit.

## 10. How the contract is verified

| Tool | What it checks |
| --- | --- |
| `tests/EdgeCaseTests.cpp` | Boundary behavior: empty book, partial fills, terminal cancels, invalid input, price-time priority, market sweeps |
| `tests/reference_model.hpp` | Independent, deliberately slow implementation used as an oracle |
| `tests/DifferentialTests.cpp` | 40 seeds × 750 random operations comparing trades, order states, quantities, and best prices against the reference |
| `OrderBook::checkInvariants()` | Internal structure and lifecycle consistency after every random operation |
| `tests/OrderBookTests.cpp` | Existing focused regression tests |

A failure in the differential test is reproducible from its seed.

## 11. Complexity (current implementation)

| Operation | Cost |
| --- | --- |
| Submit a limit order | `O(log L)` level search + `O(1)` list append; inserts may shift level storage |
| Match | `O(1)` per trade at the best level |
| Cancel | `O(log L)` level search + `O(1)` unlink |
| Look up an order by ID | `O(1)` |

`L` is the number of distinct price levels on that side. Order entries are stored
in chunks of 4,096, so a submit does not normally allocate. These are the costs
Phase 2.8 measures before changing.
