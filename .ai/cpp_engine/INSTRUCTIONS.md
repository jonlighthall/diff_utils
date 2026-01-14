# Instructions — C++ Engine (DifferenceAnalyzer)

Purpose: Procedures and standing orders when working on the core numeric comparison engine.

---

## Table of Contents

- [Critical Domain Knowledge](#critical-domain-knowledge)
- [Testing Requirements](#testing-requirements)
- [C++ Conventions](#c-conventions)
- [Patterns](#patterns)
- [Common Mistakes](#common-mistakes)
- [Links](#links)

## Critical Domain Knowledge

### The Six-Level Difference Hierarchy
1. Level 1: Non-zero vs zero raw differences
2. Level 2: Trivial vs non-trivial (sub-LSB rule with FP tolerance)
3. Level 3: Insignificant vs significant (user threshold)
4. Level 4: Marginal band (110 dB TL)
5. Level 5: Critical threshold breaches
6. Level 6: Statistical/error-classification (experimental)

Key Principle: Level 2 trivial filtering is immutable.

### Sub-LSB Detection
- Shared minimum precision determines LSB
- big_zero = LSB/2
- Trivial if raw_diff < big_zero or within FP_TOLERANCE * max(raw_diff, big_zero)
- Example: 30.8 vs 30.85 → EQUIVALENT under zero threshold

### Zero-Threshold Semantics
When user_significant == 0.0, count every non-trivial, non-ignored difference; do not revisit Level 2.

---

## Testing Requirements
- Unit tests for all engine changes (Google Test)
- Semantic invariants (zero-threshold contracts)
- Sub-LSB boundary tests (cross-precision pairs)
- Additive invariants (bookkeeping integrity)
- Test location: src/cpp/tests/

---

## C++ Conventions
- Use std::abs for doubles
- Floating-point comparison uses FP_TOLERANCE = 1e-12
- Decimal-place tracking (not bit-based)
- Validate input before processing (format checks early)

---

## Patterns

### Floating-Point Comparison
```cpp
const double FP_TOLERANCE = 1e-12;
if (std::abs(raw_diff - big_zero) < FP_TOLERANCE * std::max(raw_diff, big_zero)) {
    // treat as equal within tolerance
}
```

### Sub-LSB Detection
```cpp
int shared_dp = std::min(decimal_places_col_a, decimal_places_col_b);
double lsb = std::pow(10.0, -shared_dp);
double big_zero = lsb / 2.0;
const double FP_TOLERANCE = 1e-12;
bool sub_lsb = (raw_diff < big_zero) ||
               (std::abs(raw_diff - big_zero) < FP_TOLERANCE * std::max(raw_diff, big_zero));
```

### Zero-Threshold Logic
```cpp
if (user_significant > 0.0) {
    is_significant = std::abs(diff) >= user_significant;
} else { // zero-threshold mode
    is_significant = !is_trivial && !both_tl_values_in_ignore_region;
}
```

---

## Common Mistakes
- Using == for float equality
- Revisiting Level 2 trivial filtering later in pipeline
- Forgetting zero-threshold special case
- Not testing cross-precision pairs
- Single-column test files (column 0 is skipped)
- Misordered FileComparator constructor arguments (include debug_level)

---

## Links
- Project-wide context: ../../CONTEXT.md
- Project-wide instructions: ../../INSTRUCTIONS.md
