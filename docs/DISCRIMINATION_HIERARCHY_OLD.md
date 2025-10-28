# Discrimination Hierarchy: Algorithm Design and Implementation

## Overview

This document describes the multi-level binary discrimination algorithm used for numerical file comparison in the `uband_diff` utility. The algorithm implements a progressive refinement pipeline that classifies numeric differences between two files into hierarchical categories, enabling domain-specific and precision-aware comparison.

## Design Philosophy

The comparison algorithm is designed around three core principles:

1. **Progressive Refinement**: Each level partitions one set into exactly two subsets, with one subset undergoing further subdivision at the next level
2. **Precision Awareness**: Differences are evaluated in the context of printed precision, machine epsilon, and floating-point representation
3. **Domain Specificity**: Special handling for transmission loss (TL) data, including physics-based thresholds derived from acoustic pressure limits and single-precision arithmetic constraints

The algorithm implements a six-level hierarchy where each level applies specific numerical criteria to progressively categorize differences from raw comparison through domain-specific significance assessment.

## Implementation Architecture

**Key source files:**
- `src/cpp/include/uband_diff.h` — Threshold definitions, data structures (`Thresholds`, `CountStats`, `DiffStats`, `Flags`)
- `src/cpp/include/difference_analyzer.h` — Core analyzer interface
- `src/cpp/src/difference_analyzer.cpp` — Discrimination logic implementation
- `src/cpp/src/file_comparator.cpp` — File parsing, orchestration, summary generation
- `src/cpp/main/uband_diff.cpp` — Command-line interface

**Runtime data structures:**
- **`ColumnValues`**: Per-element structure containing `value1`, `value2`, `min_dp` (minimum decimal places), `max_dp`, range information
- **`Thresholds`**: Configuration including `zero`, `significant`, `critical`, `marginal`, `ignore`, plus percent-mode fields
- **`CountStats`**: Counters for each discrimination level
- **`DiffStats`**: Maximum differences and precision tracking
- **`Flags`**: Boolean state tracking for comparison outcomes

## Mathematical Foundation

### Machine Precision Constants

```cpp
SINGLE_PRECISION_EPSILON = 2^-23 ≈ 1.19e-07
```

This value represents the smallest resolvable difference in single-precision floating-point arithmetic and forms the basis for domain-specific thresholds.

### Domain-Specific Thresholds

**Ignore Threshold (TL Domain)**:
```cpp
thresh.ignore = -20 × log₁₀(SINGLE_PRECISION_EPSILON) ≈ 138.47 dB
```

This represents the transmission loss value corresponding to the smallest representable pressure in single-precision arithmetic. Values above this threshold are numerically unreliable.

**Marginal Threshold (TL Domain)**:
```cpp
thresh.marginal = 110 dB
```

Based on operational research (https://doi.org/10.23919/OCEANS.2009.5422312), transmission loss values above 110 dB are weighted to zero in acoustic propagation analysis. This threshold delineates the boundary of operational significance.

### Sub-LSB Detection

The algorithm uses half-ULP (Unit in the Last Place) criterion for detecting trivial differences:

```cpp
LSB = 10^(-minimum_decimal_places)
big_zero = LSB / 2.0
trivial = (raw_diff < big_zero) OR (|raw_diff - big_zero| < FP_TOLERANCE × max(raw_diff, big_zero))
```

Where `FP_TOLERANCE = 1e-12` provides floating-point comparison robustness.

## Six-Level Discrimination Hierarchy

Each level implements a binary partition, with one subset undergoing further subdivision.

---

### LEVEL 0: Structure Validation
- Purpose: Count total number of elements to compare and detect structural mismatches between files.
- Decision rule: total = sum over column groups = (lines × columns) per parsed file structure. If file element counts differ, the comparison is treated as a failure for structure but processing continues up to the difference in structure.
- Where implemented: file parsing and element accounting in `src/cpp/src/file_comparator.cpp` (functions: `compare_files`, `parse_line`, `process_line`, `extract_column_values`).

LEVEL 1 — Zero vs Non-zero (raw difference)
- Purpose: Partition elements into elements with raw difference == 0 (within `thresh.zero`) and raw non-zero differences.
- Decision rule: raw_diff = |value1 - value2|; element is "non-zero" if raw_diff > thresh.zero.
- Where implemented: `DifferenceAnalyzer::process_raw_values` in `src/cpp/src/difference_analyzer.cpp`.
- Counters updated: `counter.diff_non_zero` and `differ.max_non_zero`.

LEVEL 2 — Trivial vs Non-trivial (sub-LSB / printed-precision)
- Purpose: Distinguish differences that are due solely to printed-format precision (trivial) from substantive differences (non-trivial).
- Decision rule: compute rounded values using the minimum printed decimal places (min_dp). Compute raw_diff and LSB = 10^-min_dp; half-ULP (big_zero) = LSB/2. If raw_diff < big_zero (or within a small FP tolerance) OR rounded values compare equal, classify as TRIVIAL; otherwise NON-TRIVIAL.
- Where implemented: `DifferenceAnalyzer::process_rounded_values` (sub-LSB detection and `counter.diff_trivial` / `counter.diff_non_trivial`) in `src/cpp/src/difference_analyzer.cpp`.
- Notes: This level enforces cross-platform robustness by treating tiny format-driven differences as equivalent.

LEVEL 3 — Insignificant vs Significant (ignore threshold + user significant threshold)
- Purpose: Among non-trivial differences, discard differences that are semantically meaningless (insignificant) and mark the remainder as significant.
- Decision rule (code):
  - If both values exceed the `thresh.ignore` value (domain-dependent ignore threshold, e.g., TL ~138), mark as INSIGNIFICANT (counts toward `diff_high_ignore`).
  - Otherwise, apply significance test:
    - If `thresh.significant_is_percent` == true (percent-mode): compare fractional rounded_diff/reference where reference = |value2| (second file is reference). If `ref <= thresh.zero` then percent comparison is undefined and the implementation conservatively treats any non-trivial diff as exceeding the percent cutoff.
    - Else if `thresh.significant == 0.0` (sensitive mode): treat all non-trivial diffs (below ignore) as SIGNIFICANT.
    - Otherwise: compare rounded_diff > threshold (strict '>' as in original semantics).
- Where implemented: `DifferenceAnalyzer::process_rounded_values` (the significance branch) in `src/cpp/src/difference_analyzer.cpp`.
- Counters updated: `counter.diff_insignificant`, `counter.diff_significant`.

LEVEL 4 — Marginal vs Non-marginal (TL/marginal handling)
- Purpose: Partition significant differences into those inside a marginal (warning) operational band vs those outside it (non-marginal).
- Decision rule: If both values are in (marginal, ignore) range (i.e., > marginal && < ignore) then mark MARGINAL; else NON-MARGINAL.
- Where implemented: `DifferenceAnalyzer::process_rounded_values` (marginal check) in `src/cpp/src/difference_analyzer.cpp`.
- Counters updated: `counter.diff_marginal` and later counters derived from non-marginal set.

LEVEL 5 — Critical vs Non-critical
- Purpose: Detect very large differences (critical) that may indicate model failure and set flags so the program exits non-zero.
- Decision rule: If rounded_diff > `thresh.critical` and both values are <= `thresh.ignore`, mark CRITICAL. The analyzer sets flags (`flags.has_critical_diff`, `flags.error_found`) but does not abort analysis immediately—the rest of the stream is still processed so counts remain consistent.
- Where implemented: `DifferenceAnalyzer::process_difference` (top-level critical check) and `process_rounded_values` (per-element critical check) in `src/cpp/src/difference_analyzer.cpp`. `print_hard_threshold_error` reports the first critical occurrence.
- Counters updated: `counter.diff_critical`.

LEVEL 6 — Error vs Non-error (user threshold split)
- Purpose: Among non-critical non-marginal significant differences, decide whether the user-specified threshold marks the difference as an "error" (counts toward failure) or merely a non-error significant difference.
- Decision rule: If `thresh.significant_is_percent` is enabled, perform the percent-mode comparison (rounded_diff / |value2| > significant_percent, with conservative fallback when ref <= zero). Otherwise: rounded_diff > `thresh.significant` → error, else non-error.
- Where implemented: tail portion of `DifferenceAnalyzer::process_rounded_values` in `src/cpp/src/difference_analyzer.cpp` (the code explicitly applies percent-mode here and updates `counter.diff_error` / `counter.diff_non_error`).

---

## Additional implemented behaviors and diagnostics

- Maximum percent error and per-difference percent: non-trivial differences compute a percent error (100 * raw_diff / |value2|) when `|value2| > thresh.zero`; otherwise a sentinel (+INF) is used for display. The maximum finite percent error is recorded in `DiffStats::max_percent_error` and printed in summary. Implementation: percent-tracking added inside `process_rounded_values` in `difference_analyzer.cpp` and printed from `file_comparator.cpp` (`print_table`, `print_rounded_summary`).
- Printing and truncation: the file comparator controls printing of the diff table and may truncate the table when many differences are present or a critical threshold triggers abbreviated printing. See `FileComparator::print_table` and higher-level printing helpers in `src/cpp/src/file_comparator.cpp`.
- Sensitive / zero-threshold mode: when the user supplies a zero user-threshold, the implementation treats all non-trivial, non-ignored differences as significant (sensitive mode). This is controlled by `thresh.significant == 0.0` in `difference_analyzer.cpp`.

---

## Differences between `data/notes.md` and the implementation

- Level numbering and description: The notes and code express the same conceptual pipeline, but the code is explicit about the TRIVIAL/non-trivial split (LEVEL 2 in code) and implements percent-mode semantics (negative user threshold) that use the second file as the reference. The notes propose an almost-identical six-level hierarchy; the code implements the levels with precise numeric and FP-tolerance rules.
- Percent-mode and near-zero references: `data/notes.md` suggests negative threshold becomes a percent rule (e.g., -10 -> 10%). The code implements this. The notes raise the question of how to handle very small reference values; the code uses a conservative fallback: if |reference| <= `thresh.zero`, percent comparison is undefined and implementation treats the non-trivial difference as exceeding the percent cutoff (and displays an INF sentinel). This is documented in `difference_analyzer.cpp` and is deliberate to avoid silently missing large relative errors when reference is near-zero.
- Critical handling: notes suggest potentially aborting table printing at a critical hit but continuing counts; the implementation prints a concise critical notification upon first critical difference and sets flags (it does not immediately abort internal processing), letting summaries remain consistent. Table truncation and printing policies are handled in the comparator printing code.

---

## Edge cases, numerical considerations, and recommendations

- Sub-LSB detection: the code uses half-ULP (LSB/2) with a tiny FP tolerance to classify sub-LSB differences as trivial. This is sensible for format-driven equivalence, but be aware this depends on `min_dp` accuracy provided by the parser.
- Percent comparisons: comparing rounded differences to a fractional percent uses the second file as reference. Alternatives (sometimes preferable) are:
  - Use symmetric relative difference: |v1 - v2| / max(|v1|, |v2|) to avoid asymmetry when reference may be noisy.
  - Combined absolute+relative rule: treat difference significant when abs_diff > abs_tol OR abs_diff / max(|v1|,|v2|) > rel_tol.
  - ULP-based comparisons for scale-aware machine-precision assessments.
If you prefer any of the above semantics, I can implement it and add corresponding unit tests.

- Near-zero reference fallback: current conservative fallback (treat as exceeding percent cutoff and print INF) is explicit and safe but may produce false positives for numeric noise. Consider adopting the symmetric or hybrid rules above if you want fewer INF cases.

---

## Where to look in the code (quick references)

- `src/cpp/include/uband_diff.h` — Thresholds, DiffStats, CountStats definitions and `FileComparator` member declarations.
- `src/cpp/include/difference_analyzer.h` — DifferenceAnalyzer class declaration.
- `src/cpp/src/difference_analyzer.cpp` — `process_difference`, `process_raw_values`, `process_rounded_values`, `round_to_decimals`, `print_hard_threshold_error`. This file contains the core discrimination logic and percent-mode handling.
- `src/cpp/src/file_comparator.cpp` — file parsing, `process_line`, `process_column`, `process_difference` orchestration, printing routines (`print_table`, `print_rounded_summary`, summary helpers). The comparator drives per-element calls to the analyzer and collects/prints the global summary.
- `src/cpp/main/uband_diff.cpp` — CLI parsing and construction of `FileComparator` with thresholds.

---

If you want, I can:

- Update the document to adopt any of the alternative percent/near-zero semantics described above and change the code accordingly (with unit tests).
- Add a short appendix with exact struct and field names (copy-paste from headers) so the document is an even more precise developer reference.

Requested next step? (pick one)
- Keep the current policy and finalize this document. or
- Switch percent-mode to a symmetric/hybrid rule and update code + tests. or
- Move the sensitive test temporary file location into the repo test directory for reproducible artifacts.
