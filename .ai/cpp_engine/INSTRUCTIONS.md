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

## Code Path Debugging (January 2026)

### Critical: Map Execution Paths Before Implementing Logic

When implementing features (especially hierarchy counters), **verify the code path is actually executed** before investing in implementation. Multiple counting systems may exist; don't assume a subsystem is used just because it exists in the architecture.

**Effective debug technique:**
```cpp
std::ofstream debug_file("/tmp/debug_<function>.txt", std::ios::app);
debug_file << "Function called: v1=" << value1 << " v2=" << value2 << std::endl;
debug_file.close();
```

**Advantages:**
- Survives process termination and output redirection
- Proves function execution without interfering with stdout/stderr
- Leaves audit trail for verification

**Verification process:**
1. Add file-based debug to the function you're testing
2. Run test or comparison
3. Check for file creation: `ls /tmp/debug_<function>.txt`
4. If file exists: path confirmed, proceed with implementation
5. If file doesn't exist: function is NOT called; find the actual path being used

### Known Architecture Issues

**DifferenceAnalyzer status:** Currently not verified to be called during test comparisons. The `process_rounded_values()` function containing the 6-level hierarchy logic may not be executed during tests; actual counting may happen in a different code path (FileComparator, FileReader, or legacy system).

**Before implementing in DifferenceAnalyzer:**
- Trace the full path: `FileComparator::compare_files()` → `process_line()` → `process_column()` → `process_difference()` → `DifferenceAnalyzer::process_difference()` → `DifferenceAnalyzer::process_rounded_values()`
- Verify each step with debug file output
- If any step fails to execute, the hierarchy logic won't run
- Document which path is actually responsible for populating `CountStats` counters

---

## Links
- Project-wide context: ../../CONTEXT.md
- Project-wide instructions: ../../INSTRUCTIONS.md
