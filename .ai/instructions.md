# AI Assistant Development Guidelines for diff_utils

## Project Overview

`uband_diff` solves a specific problem: **comparing numerical results that are visually identical but bitwise different across platforms**.

When two acoustic propagation model implementations produce results that look the same but differ due to:
- Floating-point rounding differences
- Platform/compiler variations
- Precision formatting differences

...traditional tools like `diff` fail completely. `uband_diff` recognizes these formatting artifacts as trivial (informationally equivalent) and filters them out, enabling application of peer-reviewed quantitative comparison methods (Fabre's TL metric).

**The novel contribution**: Enabling cross-platform, cross-precision comparison so that authoritative quantitative analysis (via Fabre's peer-reviewed method) can proceed on results that appear identical.

## Critical Domain Knowledge

### The Six-Level Difference Hierarchy
Understanding this hierarchy is **essential** to all code modifications:

1. **Level 1**: Non-zero vs zero raw differences
2. **Level 2**: Trivial vs non-trivial (after rounding to shared minimum precision; sub-LSB rule)
3. **Level 3**: Insignificant vs significant (threshold-based filtering)
4. **Level 4**: Marginal (operational warning band) vs non-marginal
5. **Level 5**: Critical vs non-critical (hard threshold breaches)

**Note on Level 6 (Statistical Pattern Analysis)**: Experimental diagnostic features exist for pattern analysis (run tests, autocorrelation, regression) but are NOT production-ready. They are useful for investigation but not for authoritative pass/fail decisions.

**Key Principle**: Differences classified as trivial at Level 2 are **permanently excluded** from later semantic buckets. This is immutable filtering to prevent formatting artifacts from being reintroduced by threshold tuning.

### Sub-LSB Detection Fix
A precision-aware sub-LSB (Least Significant Bit/2) detection fix is core to cross-platform robustness:
- Differences ≤ half the shared LSB are treated as trivial (informationally equivalent)
- Example: `30.8` vs `30.85` with threshold=0 → EQUIVALENT (not SIGNIFICANT)
- Uses floating-point tolerance (FP_TOLERANCE = 1e-12) to handle binary representation edge cases
- See `docs/guide/sub-lsb-detection.md` and `docs/implementation-summary.md`

### Zero-Threshold Semantics
When `user_significant == 0.0`:
- Intent: "count every non-trivial, physically meaningful difference"
- The engine bypasses the precision-derived floor (`dp_threshold`) **only for Level 3**
- Does **NOT** revisit Level 2 or permit trivial differences to become significant
- Ensures "maximum sensitivity" contract is maintained

### Critical Thresholds
| Threshold | Value | Purpose |
|-----------|-------|---------|
| marginal | 110 dB TL | Operational relevance boundary |
| ignore | ~138.47 dB TL | Above this, values are numerically meaningless |
| zero | ~1.19e-7 | Single precision epsilon |
| decimal places | 17 | Maximum supported (increased from 15) |

### Novel Contributions Overview

This project contains author-created innovations:

**Hierarchical Six-Level Discrimination (Core Organizing Principle)**:
- Six levels of progressively refined difference classification
- Early-exit architecture: program continues to next level only upon match or critical failure
- Physics-based thresholds from IEEE 754 and peer-reviewed standards
- Not merely a taxonomy, but an operational decision framework
- Location: [docs/guide/discrimination-hierarchy.md](../docs/guide/discrimination-hierarchy.md)

**Sub-LSB Detection (Information-Theoretic)**:
- Recognizes printed value as interval, not point
- Enables robust cross-precision comparison
- Location: [docs/guide/sub-lsb-detection.md](../docs/guide/sub-lsb-detection.md)

**Rigorous Accounting Invariants**:
- Structural and semantic consistency throughout classification
- Ensures reproducibility
- Validated through semantic invariant tests

**Enablement of Fabre's Method**:
- Precision filtering to allow peer-reviewed quantitative comparison on bitwise-different results
- Not a replacement or alternative to Fabre's method

### Critical Paradigm Question (PENDING)

**Fabre's Method Optimization Paradigm:** Requires careful reading of Fabre et al. to determine:

1. **Tactical Equivalence:** Curves sufficiently similar for operational decision-making
2. **Theoretical/Computational Equivalence:** Curves match at numerical/phase error analysis level

**Observations from Fabre Figures 2--6:** Curves with similar gross structure but apparent range-offset differences (possible accumulated phase error). Whether these score high or low in Fabre's method determines the paradigm.

**Why it matters:** This distinction affects interpretation of all subsequent phase-error and horizontal-stretch comparison work. See [docs/future-work.md](../docs/future-work.md) for investigation plan.

## Code Quality Standards

### Testing Requirements
- **Unit Tests**: Every numeric comparison logic change requires corresponding unit tests
- **Semantic Invariants**: Test that zero-threshold behavior maintains maximum sensitivity contract
- **Additive Invariants**: Ensure bookkeeping integrity (sum of categories equals total)
- **Cross-Precision Tests**: Validate handling of mixed decimal place comparisons
- Test location: `src/cpp/tests/`

### C++ Conventions
- Use `std::abs()` for doubles (not `fabs()`)
- Floating-point comparisons use tolerance: `std::abs(a - b) < FP_TOLERANCE * std::max(a, b)`
- Precision tracking is decimal-place based, not bit-based
- Always validate input before processing (format validation is first stage)

### Fortran Conventions
- Programs in `src/fortran/programs/`
- Modules in `src/fortran/modules/`
- Style: Free-form Fortran (files use `.f90` extension)
- Use explicit interfaces for all procedures
- Avoid implicit typing (require IMPLICIT NONE)

### Documentation
- Update corresponding `.md` file in `docs/` for any algorithmic changes
- Include mathematical notation where precision/thresholds matter
- Document why, not just what (especially for numerical edge cases)

## Common Modifications & Patterns

### Adding a New Threshold
1. Add constant to relevant header (e.g., `difference_analyzer.h`)
2. Document in `docs/` with physical/domain justification
3. Update `--help` text in main program
4. Add unit tests for threshold boundary conditions
5. Update semantic invariant tests if applicable

### Fixing Numeric Comparison Issues
1. Check if sub-LSB detection applies (see `SUB_LSB_DETECTION.md`)
2. Verify floating-point tolerance is applied (FP_TOLERANCE = 1e-12)
3. Test with cross-precision pairs (1dp vs 2dp, etc.)
4. Test zero-threshold mode explicitly
5. Add test cases to regression suite

### Refactoring Fortran Code
- Use provided tasks: "Break and Normalize Fortran (Global)" or "Break Fortran Lines Only (Global)"
- These format code using `break-and-normalize.ps1` script (cross-platform via WSL)
- Post-processing: Verify compilation and tests pass

## Build & Test Workflow

### Building
```bash
make clean
make all                    # Builds all C++, Fortran, Java utilities
make -j4                    # Parallel build (4 jobs)
```

### Testing
```bash
cd build && make test       # Runs C++ unit test suite
# Fortran/Java: manual testing via provided data files in data/
```

### Running Utilities
- C++: `./build/bin/uband_diff <ref> <test> [significant] [critical] [print_threshold]`
- Fortran: Similar CLI interface (see README.md)
- Data files: `data/*.txt`, `data/*.asc`, etc.

## Key Files to Understand

### C++ Core (Fortran Replacement)
- `src/cpp/src/difference_analyzer.cpp` - Core numeric comparison logic
- `src/cpp/src/file_comparator.cpp` - File reading & validation
- `src/cpp/src/line_parser.cpp` - Decimal place tracking & format validation
- `src/cpp/tests/` - Unit test suite (semantic & additive invariants)

### Fortran Legacy Programs
- `src/fortran/programs/cpddiff.f90` - Ducting probability comparison
- `src/fortran/programs/prsdiff.f90` - Pressure (.prs) file comparison
- `src/fortran/programs/tldiff.f90` - Transmission loss comparison
- `src/fortran/programs/tsdiff.f90` - Time series comparison

### Documentation Hierarchy
- **Quick Start**: `README.md`, `docs/guide/diff-level-option.md`
- **Theory**: `docs/guide/discrimination-hierarchy.md`, `docs/ERROR_ACCUMULATION_ANALYSIS.md`
- **Implementation**: `docs/implementation-summary.md`, `docs/guide/sub-lsb-detection.md`
- **Technical Reports**: `docs/docs/report/UBAND_DIFF_TECHNICAL_REPORT.tex`

## When to Ask for Clarification

Ask for clarification if:
1. Changes involve threshold values (domain expertise needed)
2. Modifying Level 2 or Level 3 logic (affects invariant contracts)
3. Cross-platform compatibility is affected (Windows vs Linux vs WSL)
4. Decimal place handling changes (could affect precision tracking)
5. Zero-threshold semantics are involved (maximum sensitivity contract)

## Avoid Common Pitfalls

❌ **DON'T**:
- Use `==` for floating-point comparisons without tolerance
- Modify trivial filtering without updating semantic invariants
- Apply thresholds out of order (Level 2 must remain immutable)
- Forget to test with mixed decimal place inputs
- Ignore the zero-threshold edge case

✅ **DO**:
- Check sub-LSB detection applies to your change
- Write tests before modifying comparison logic
- Update docs when changing thresholds or behavior
- Test both zero-threshold and normal modes
- Validate cross-platform builds (especially Fortran)

## Resources
- Full hierarchy explanation: `docs/guide/discrimination-hierarchy.md`
- Sub-LSB deep-dive: `docs/guide/sub-lsb-detection.md`
- Architecture details: `docs/implementation-summary.md`
- Makefile patterns: `docs/MAKEFILE_BEST_PRACTICES.md`
- Precision test suite: `docs/guide/pi-precision-test-suite.md`
