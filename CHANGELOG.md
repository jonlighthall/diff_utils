# Changelog

All notable changes to this project are documented in this file.

The format is based on Keep a Changelog, and this project adheres to Semantic Versioning.

## [Unreleased]
- Placeholder for upcoming changes. Add entries under Added/Changed/Fixed as they merge.

## [0.1.0] - 2025-10-27

### Added
- 6 unit tests covering sub‑LSB boundary and cross‑precision scenarios.
- Documentation: `docs/SUB_LSB_DETECTION.md` (concepts, math, examples) and `docs/IMPLEMENTATION_SUMMARY.md` (code‑level details, tests, file changes).
- Pi Precision Test Suite documentation and scripts.

### Changed
- Sub‑LSB detection: differences ≤ half the shared LSB (LSB/2) are classified as trivial (informationally equivalent). Introduced floating‑point tolerance in comparisons and raised maximum decimal places to 17. See `docs/IMPLEMENTATION_SUMMARY.md` for rationale and implementation notes.
- Root README: added a short "Implementation summary" subsection with a link to `docs/IMPLEMENTATION_SUMMARY.md`.

### Fixed
- Corrected logic that previously compared `rounded_diff` to `big_zero`; now uses raw difference with FP tolerance for boundary cases (e.g., `30.8` vs `30.85`).

---

Link references and additional release notes may be added as versions are tagged.
