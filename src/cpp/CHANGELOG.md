# Changelog — tl_diff (C++)

All notable changes to the C++ tl_diff program are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed
- Critical threshold no longer sets `error_found`; critical diffs cause failure
  through the normal Level 3 significance path (`files_are_close_enough`)
- `error_found` flag now reserved for actual errors (file access, parse failures)

### Fixed
- Documentation: updated stale references to renamed function
  `process_rounded_values()` → `process_hierarchy()` and variable
  `rounded_diff` → `raw_diff` in discrimination-hierarchy.md
- Test comment: clarified critical threshold causes print truncation, not exit

## [2.0.0] - 2026-02-12

Complete C++ rewrite of the Fortran tl_diff. This is a new implementation,
not a port — the comparison algorithm was redesigned from scratch.

### Added
- Six-level discrimination hierarchy (raw → trivial → significant → marginal → critical → error)
- Sub-LSB detection: differences ≤ half the shared LSB classified as trivial
- Floating-point tolerance (1e-12) for boundary classification
- Percent-mode threshold (`-p` flag)
- Critical threshold for detecting catastrophic model failures
- Range-data auto-detection (column 0 monotonic with fixed delta)
- Colored diff-like output with significance annotations
- 55 unit tests across 15 Google Test suites
- Cross-language precision tests (C++, Fortran, Java, Python π data)
- Pi precision test data generators (4 languages)
- Comprehensive documentation (discrimination hierarchy, getting started, future work)

### Changed
- CLI expanded from 3 to 7 arguments (FILE1 FILE2 THRESH [CRITICAL] [PRINT] [VERBOSE] [DEBUG])
- Threshold semantics: user threshold applied directly (no comp_diff addition)
- Sub-LSB replaces the Fortran comp_diff heuristic — strictly better precision handling
- Pass/fail: strict per-point — any significant difference = FAIL (no aggregate 2% rule)
- Exit codes: 0 = pass, 1 = fail or error (same as Fortran, cleaner semantics)

### Removed
- Aggregate 2% threshold (moved to future tl_analysis scope)
- RMSE and TL metrics from output (moved to future tl_metric scope)
- Weighted difference column from table output
- Error accumulation analysis (preserved in code for future tl_analysis)

### Compatibility
- Accepts the same 3 positional arguments as the Fortran version
- Additional C++ arguments are optional with sensible defaults
- Threshold behavior is an intentional improvement, not a bug

---

## [0.1.0] - 2025-10-27

### Added
- 6 unit tests covering sub-LSB boundary and cross-precision scenarios
- Documentation: `docs/SUB_LSB_DETECTION.md` and `docs/IMPLEMENTATION_SUMMARY.md`
- Pi Precision Test Suite documentation and scripts

### Changed
- Sub-LSB detection: differences ≤ half the shared LSB (LSB/2) classified as trivial
- Introduced floating-point tolerance in comparisons
- Raised maximum decimal places to 17

### Fixed
- Corrected logic that previously compared `rounded_diff` to `big_zero`;
  now uses raw difference with FP tolerance for boundary cases

---

*Fortran baseline documented in [`src/fortran/CHANGELOG.md`](../fortran/CHANGELOG.md).*
