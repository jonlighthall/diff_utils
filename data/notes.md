# Development Notes and Algorithm Evolution

This document contains historical development notes, bug fixes, and algorithmic insights that informed the design of the discrimination hierarchy. For complete algorithm documentation, see `../docs/DISCRIMINATION_HIERARCHY.md`. For test case descriptions, see `README.md` in this directory.

## Historical Bug Fixes and Issues

### bbpp.rc Output Formatting
**Status**: Fixed  
**Issue**: Complex output formatting caused parsing failures  
**Resolution**: Improved format handling robustness in file parser

### bbpp.ts Decimal Place Handling
**Issue**: Time series comparison couldn't properly handle varying decimal places across columns  
**Resolution**: Implemented column-specific decimal place tracking (`min_dp`, `max_dp` per column)

### bbtl.nspe02 Message and Percentage Display
**Issues**:
- Bad error message formatting
- Percentage not displayed when expected
**Resolution**: Updated message formatting and ensured percent error calculation for all non-trivial differences

### edge_case Zero Threshold
**Issue**: Comparison failed to trigger error when threshold set to zero  
**Resolution**: Implemented sensitive mode logic where `significant == 0.0` treats all non-trivial differences as significant

### nspe.std3 Range Precision and Monotonic Detection
**Complex issue**: Range column had step size 0.0122 but printed precision was insufficient to capture it at every step (e.g., values printed with fewer decimal places). This caused monotonic increasing test to fail because consecutive values appeared identical.

**Root cause**: Monotonic increasing test was too strict—it should allow for unchanged values (monotonically non-decreasing).

**Resolution**: Updated range detection to be more forgiving of precision-limited format while maintaining fixed-delta detection

**Command that exposed issue**:
```bash
make && ./bin/uband-diff data/nspe.std3.test.nspe01.txt data/nspe.std3.refe.nspe01.txt 0.2 3 0.6
```

**Expected behavior**: With 905 significant differences out of 3554 total (25%), should fail the 2% threshold

## Algorithmic Design Decisions

### Zero Difference Printing
**Decision**: Trivial differences should not be printed in the table unless debug/print level is non-zero

**Rationale**: Trivial differences (LEVEL 2) are format artifacts, not substantive numerical differences. Printing them clutters output and obscures meaningful differences.

**Implementation**: Print threshold comparison uses `>` not `>=` to exclude zero-difference elements

### 2% Pass-With-Warning Rule
**Concept**: If non-marginal, non-critical significant differences represent less than 2% of total elements, comparison passes with warning message "files are close enough but not identical"

**Rationale**: In realistic operational scenarios, files may have minor numerical differences that don't indicate model failure. Strict failure on any significant difference would be overly conservative.

**History**: This rule was removed in an earlier commit, then re-introduced after analyzing realistic comparison cases like `nspe.std2`

**Implementation**: Applied after LEVEL 6 discrimination, before final pass/fail determination

### Percentage Threshold Mode
**Design question**: "Is there a way to estimate what the minimum difference between two numbers could be at a certain scale in single precision arithmetic?"

**Answer**: Yes, using relative epsilon:
```
meaningful_threshold = max(|v1|, |v2|) × machine_epsilon × safety_factor
```

**Implementation approach**: Introduce percentage thresholds through negative argument values. If third argument is negative (e.g., `-10`), interpret as 10% relative threshold rather than absolute threshold.

**Rationale**: Minimum resolvable difference varies with magnitude:
- Time series/pressure: O(10⁻⁵)
- Transmission loss: O(10²)
- Range data: O(10⁵)

Absolute thresholds fail to capture this scale dependence. Percentage thresholds adapt automatically.

**Example**:
```bash
# Absolute threshold: 0.05
./bin/uband_diff file1.txt file2.txt 0.05 3 0.6 1

# Relative threshold: 10%
./bin/uband_diff file1.txt file2.txt -10 3 0.6 1
```

### Reference Value Selection for Percent Mode
**Decision**: Use `value2` (second file, typically reference) as denominator for percent error calculation

**Rationale**: 
- Asymmetric but consistent
- Second file typically represents "ground truth" or expected output
- Avoids ambiguity in reference selection

**Edge case handling**: When `|value2| <= thresh.zero`, percent error is undefined. Display `INF` as sentinel and conservatively treat as exceeding threshold.

**Alternative approaches considered** (not implemented):
- Symmetric: `|v1 - v2| / max(|v1|, |v2|)`
- Combined: Significant when `abs_diff > abs_tol OR rel_diff > rel_tol`

### Critical Difference Early Termination
**Original behavior**: Program stopped on first critical difference

**Issue**: Prevented complete file analysis and accurate statistics

**Resolution**: Continue processing after critical difference detected, but set `error_found` flag and potentially abbreviate table printing. This allows:
- Complete statistics (all discrimination levels)
- Multiple critical differences to be counted
- Consistent unit test validation

### Double Counting Bug (case1r)
**Issue**: Non-zero differences were being counted twice (39 × 2 = 78)

**Root cause**: Differences were incremented in both raw processing and rounded processing

**Resolution**: Ensured each difference is counted exactly once at appropriate discrimination level

**Validation**: Unit tests verify summation invariants at each level

### Printed Differences Count
**Feature request**: Add count of printed differences to summary for cases where table overflows terminal

**Rationale**: User needs to know how many differences were shown vs total differences found

**Implementation**: Track `printed_diff_count` separately from discrimination counters

## Discrimination Hierarchy Refinement History

### Evolution from 4 to 6 Levels
**Original design** (4 levels):
1. Structure validation
2. Zero vs non-zero (raw)
3. Significant vs insignificant (ignore threshold)
4. Marginal vs non-marginal (marginal threshold)

**Realized missing levels**:
- **LEVEL 2** (Trivial vs non-trivial): Essential for format-independent comparison
- **LEVEL 5** (Critical vs non-critical): Sanity check for model failure
- **LEVEL 6** (Error vs non-error): User threshold application

**Insight**: Each level must create exactly two partitions, with one partition undergoing further subdivision. This ensures clean hierarchical structure and unambiguous accounting.

### Binary Partition Principle
**Key design constraint**: "At each level, (a subset of) the data is divided into two groups. One of the two divisions is further divided in the next step."

**Why this matters**:
- Prevents overlapping categories
- Ensures summation invariants hold
- Makes unit testing straightforward
- Clarifies logical flow for users

**Counter-example** (rejected): Dividing non-zero into three groups (trivial, insignificant, significant) violates binary partition principle and creates accounting ambiguity

### Summation Invariants as Unit Tests
**Insight**: The hierarchical invariants (e.g., `total = zero + trivial + insignificant + marginal + critical + error + non_error`) can be directly encoded as unit tests

**Implementation**: Each test verifies summation at specific discrimination level

**Benefit**: Mathematical structure of algorithm is enforced at compile/test time, preventing regression

## Machine Precision and Domain Knowledge Integration

### Ignore Threshold Derivation (138.47 dB)
**Physical basis**: 
```
TL = -20 × log₁₀(p / p_ref)
```
Where `p` is pressure and `p_ref` is reference pressure (1 μPa).

**Machine precision limit**:
```
p_min = SINGLE_PRECISION_EPSILON ≈ 1.19e-7
TL_max = -20 × log₁₀(1.19e-7 / 1e-6) ≈ 138.47 dB
```

**Interpretation**: Transmission loss values above 138.47 dB correspond to pressures smaller than single-precision can reliably represent. Differences at these values are artifacts of numerical precision, not physical differences.

### Marginal Threshold Justification (110 dB)
**Research basis**: https://doi.org/10.23919/OCEANS.2009.5422312

**Operational finding**: In acoustic propagation analysis, TL values exceeding 110 dB are weighted to zero because:
- Signal is effectively undetectable
- Values are outside operational interest
- Numerical artifacts dominate

**Algorithm application**: Differences where both values are in [110, 138.47] dB range are classified as "marginal"—technically computable but operationally unimportant

## Open Questions and Future Enhancements

### Alternative Relative Threshold Approaches
**NumPy-style combined threshold**:
```python
close = np.allclose(a, b, rtol=1e-5, atol=1e-8)
# Equivalent to: |a - b| <= atol + rtol × |b|
```

**ULP-based comparison**:
```cpp
ulp_threshold = std::nextafter(max(|v1|, |v2|), INFINITY) - max(|v1|, |v2|)
meaningful_diff = diff > ulp_threshold × safety_factor
```

**Adaptive epsilon**:
```cpp
scale = max(|v1|, |v2|)
adaptive_threshold = scale × EPSILON × 100  // 100 ULP threshold
```

### Per-Column Threshold Specification
**Motivation**: Different columns may represent different physical quantities (range, depth, TL) with different natural scales

**Proposed syntax**:
```bash
./bin/uband_diff file1.txt file2.txt 0.05:0.1:0.02 ...
# Column 1: 0.05, Column 2: 0.1, Column 3: 0.02
```

### Error Run Length Detection
**Concept**: Even if total error percentage is high, if errors are localized to a few consecutive lines, it may indicate isolated numerical instability rather than systematic model failure

**Proposed logic**: If no run of consecutive errors exceeds X lines, pass with warning

**Example**: 100 errors spanning 5 lines vs 100 errors spanning 50 lines → different interpretations

### Enhanced Range Detection
**Current limitation**: Range detection uses simple criteria (monotonic + fixed delta + start < 100)

**Potential improvements**:
- Detect geometric progressions (e.g., frequency data)
- Detect logarithmic spacing
- Auto-detect column semantic type (range, depth, frequency, angle)

### Percent Error Statistics
**Current**: Track maximum percent error

**Enhancement**: Track distribution of percent errors (mean, median, std dev, percentiles)

**Use case**: Understanding whether errors are uniform or concentrated at specific values
