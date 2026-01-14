# Context — C++ Engine (DifferenceAnalyzer)

Purpose: Focused context for the numeric comparison engine used by uband_diff and related tools. Use this when working on core semantics, precision handling, and classification logic.

---

## Table of Contents

- [Key Components](#key-components)
- [Data Flow](#data-flow)
- [Key Decisions](#key-decisions)
- [Critical Paradigm Question (PENDING)](#critical-paradigm-question-pending)
- [Test Categories](#test-categories)
- [Links](#links)

## Key Components

### DifferenceAnalyzer

Purpose: Core numeric comparison logic

Responsibilities:
- Classify differences across 6 levels
- Apply sub-LSB detection with floating-point tolerance
- Enforce immutable Level 2 filtering (trivial differences)
- Maintain semantic invariants (especially zero-threshold behavior)
- Track marginal, critical, error counts

Critical Fields:
```cpp
const double FP_TOLERANCE = 1e-12;
const double MARGINAL_TL = 110.0;
const double IGNORE_TL = 138.47;
int max_decimal_places = 17;
```

### FileComparator

Purpose: File I/O and validation

Responsibilities:
- Check column count consistency
- Detect decimal places per column
- Parse numeric values
- Validate format before processing

Critical Behavior: Column 0 is Always Skipped
- Column index 0 is unconditionally treated as the range column (e.g., distance, time) and is excluded from numerical comparison. Data columns start at index 1.

Implications:
- Single-column test files will have their only value skipped (column 0)
- Test files must have at least 2 columns: `range_value data_value`

### LineParser

Purpose: Parse individual lines, track precision

Responsibilities:
- Extract numbers from lines
- Track decimal places per value
- Compute shared minimum precision

---

## Data Flow

```
Input File
    ↓
FileComparator::validate()
    ├─ Check column count consistency
    ├─ Detect decimal places per column
    └─ Parse numeric values
    ↓
LineParser::analyze_line()
    ├─ Extract numbers
    ├─ Track precision (decimal places)
    └─ Compute shared minimum precision
    ↓
DifferenceAnalyzer::classify_difference()
    ├─ Level 1: Check if raw_diff != 0
    ├─ Level 2: Sub-LSB detection (with FP tolerance)
    ├─ Level 3: Significance threshold
    ├─ Level 4: Marginal band (110 dB TL)
    ├─ Level 5: Critical threshold check
    └─ Level 6: Error classification
    ↓
Output
    ├─ Difference table
    ├─ Summary report
    └─ Consistency checks (invariants)
```

---

## Key Decisions

### Level 2 Filtering is Immutable
Differences classified as trivial at Level 2 are permanently excluded from later semantic buckets.

Rationale: Prevents the zero-threshold paradox where formatting artifacts are promoted to significant by threshold tuning.

### FP_TOLERANCE = 1e-12
Well below double-precision epsilon (~2.2e-16) while allowing for accumulated floating-point error.

### Maximum Decimal Places = 17
Supports full double-precision representation. Beyond 17 digits, precision is noise.

### Fabre's Method is Authoritative
Peer-reviewed, IEEE-weighted TL curve comparison is the only authoritative quantitative method. This engine enables Fabre by filtering formatting artifacts.

Citation: Fabre & Zingarelli, "A synthesis of transmission loss comparison methods," IEEE OCEANS 2009, doi:10.23919/OCEANS.2009.5422312

---

## Critical Paradigm Question (PENDING)

Fabre's Method Optimization Paradigm: Requires careful reading of Fabre et al. to determine:

1. Tactical Equivalence: Curves sufficiently similar for operational decision-making
2. Theoretical/Computational Equivalence: Curves match at numerical/phase error analysis level

Observations from Fabre Figures 2–6: Curves with similar gross structure but apparent range-offset differences (possible accumulated phase error). Whether these score high or low in Fabre's method determines the paradigm.

Why it matters: This distinction affects interpretation of all subsequent phase-error and horizontal-stretch comparison work. See docs/future-work.md for investigation plan.

---

## Test Categories

### Semantic Invariant Tests (test_semantic_invariants.cpp)

Purpose: Verify zero-threshold contracts and domain rules

Key Tests:
- Zero-threshold mode enables maximum sensitivity
- Trivial filtering is immutable (Level 2 cannot be revisited)
- Ignore TL region filters correctly
- Non-trivial count = significant + insignificant (in normal mode)

### Sub-LSB Boundary Tests (test_sub_lsb_boundary.cpp)

Purpose: Edge cases in floating-point precision

Key Tests:
- 30.8 vs 30.85 classified as TRIVIAL (not SIGNIFICANT)
- Rounding to shared minimum precision works correctly
- FP tolerance handles binary representation edge cases

### Trivial Exclusion Tests (test_trivial_exclusion.cpp)

Purpose: Ensure Level 2 filtering persists through pipeline

Key Tests:
- Trivial differences never counted as significant
- Trivial differences never counted as insignificant
- Total = trivial + nontrivial

## Links
- Project-wide context: ../../CONTEXT.md
- Project-wide instructions: ../../INSTRUCTIONS.md
