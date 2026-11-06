# Experiment 006 — reference model and differential testing

| Field | Fill in |
| --- | --- |
| Question | How do we know the matching engine is correct beyond the cases we thought to test? |
| Hypothesis | An independent, obviously correct reference model plus seeded random command streams will expose disagreements that hand-written tests miss. |
| Change | Added edge-case tests, a vector-scan reference model, randomized differential tests, and a book invariant checker. Invalid orders are rejected with ID 0. Terminal orders can no longer be cancelled. |
| Correctness | 40 seeds × 750 operations. Every step compares order IDs, states, quantities, best prices, and full trade records. Engine and reference invariants are checked after every operation. |
| Measurement | `ctest --preset windows-release`: 3/3 tests pass. Model tests take about 55 s locally. |
| Conclusion | The engine agreed with the reference for 30,000 randomized operations. The only failure found was in the old regression test, which used negative limit prices that are now correctly rejected. |

## What was compared

For every operation:

1. Returned order ID.
2. Best bid and best ask.
3. Every order's state, price, original quantity, and remaining quantity.
4. Every trade's buyer, seller, price, and quantity.
5. Book structure and lifecycle invariants.

## Why a slow model is useful

The reference model scans the whole order list. The real engine uses sorted
price levels and linked lists. The two implementations share almost no code, so a
bug in one is unlikely to be reproduced by the same bug in the other.

The model is not a proof. If both implementations share a misunderstanding, both
will agree and both will be wrong. The edge-case tests and the invariant checker
are independent defenses against that.

## What the differential test found

- No engine-versus-reference mismatch in 30,000 operations.
- One outdated test assumption: the existing regression test generated limit
  prices from −100 to 100. Negative and zero limit prices are now rejected, so
  the generator was changed to produce positive prices.

## Limitations

- The random price range is small (90–110), which creates frequent matches. It
  does not stress very sparse books.
- Cancels are unrestricted: the test can cancel any existing order, including
  filled ones, which verifies the no-op rule.
- The reference model is `O(n)` per match, so large streams are slow. This is
  acceptable for a test oracle.
- The differential test is single-threaded; concurrency arrives in Phase 4.
