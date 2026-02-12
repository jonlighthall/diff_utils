# diff_utils

**Numerical comparison utilities for validating acoustics model output.**

Standard text tools like `diff` can tell you that two files are different,
but not whether the differences *matter*. When comparing numerical output
from scientific codes — especially across platforms, compilers, or
precision levels — many reported "differences" are artifacts of
floating-point formatting, not real physical disagreements.

`uband_diff` (C++) solves this by treating each printed number as
representing an interval of values (determined by its displayed decimal
precision) rather than an exact point. Differences that fall within that
representational interval are classified as trivial and excluded from
consideration. The remaining differences are then filtered through
physics-based thresholds — ignoring values below the noise floor of
single-precision arithmetic — before a pass/fail determination is made.

The result: a clear, defensible answer to "do these two output files
agree?" with far fewer false positives than a naive comparison.

## Quick Start

### Build

```bash
make         # Build uband_diff
```

Requires GCC 7+ (or Clang 5+) with C++17 support and GNU Make.

### Run

```bash
uband_diff <ref_file> <test_file> [threshold] [critical] [print]
```

**Example — compare two PE output files:**
```bash
./build/bin/uband_diff data/pe.std1.pe01.ref.txt data/pe.std1.pe01.test.txt 0 1 0
```

### Arguments

| Arg | Name | Default | Description |
|-----|------|---------|-------------|
| 1 | ref_file | (required) | Reference output file |
| 2 | test_file | (required) | Test candidate file |
| 3 | threshold | 0.05 | Significance threshold in dB (0 = maximum sensitivity) |
| 4 | critical | 10.0 | Critical threshold — differences above this cause failure |
| 5 | print | 1.0 | Minimum difference to display in the output table |

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | Files agree (or differ within acceptable tolerance) |
| 1 | Files differ significantly, or a critical error occurred |

For detailed build instructions, prerequisites, and troubleshooting, see
[docs/guide/getting-started.md](docs/guide/getting-started.md).

## How It Works

`uband_diff` classifies every element-by-element difference through a
six-level hierarchy. Each level is a filter — once a difference is
classified at one level, it cannot be reclassified at a later level.

| Level | Question | Classification |
|-------|----------|----------------|
| 1 | Is the raw difference zero? | identical vs different |
| 2 | Is the difference within the representational interval? | trivial vs non-trivial |
| 3 | Is it physically meaningful? | insignificant vs significant |
| 4 | Is it in the operational range? | marginal vs non-marginal |
| 5 | Does it exceed the critical threshold? | critical vs non-critical |
| 6 | Does it exceed the user threshold? | error vs non-error |

**Pass/fail:** If fewer than 2% of elements show non-marginal, non-critical
significant differences, the comparison passes. Critical threshold
exceedances always cause failure.

### Threshold reference

| Threshold | Value | Source | Purpose |
|-----------|-------|--------|---------|
| significant | CLI arg 3 | User | Lower bound for counting a meaningful difference |
| critical | CLI arg 4 | User | Hard failure threshold |
| print | CLI arg 5 | User | Minimum difference to display |
| marginal | 110 dB | Domain convention | Upper bound for operational relevance |
| ignore | ~138.47 dB | IEEE 754 derived | Noise floor — differences here are physically meaningless |

For the full technical description of the discrimination hierarchy, see
[docs/guide/discrimination-hierarchy.md](docs/guide/discrimination-hierarchy.md).

For the sub-LSB detection algorithm, see
[docs/guide/sub-lsb-detection.md](docs/guide/sub-lsb-detection.md).

## Version History

See [CHANGELOG.md](CHANGELOG.md) for recent updates.

---

## Development

The sections below are for developers working on or maintaining this
codebase. They are not needed to build or run `uband_diff`.

### Legacy Fortran programs

The original Fortran comparison utilities are preserved in `src/fortran/`.
These programs are superseded by `uband_diff` but remain in working order
for use with the current NSPE Fortran distribution.

| Program | Description |
|---------|-------------|
| tldiff | Transmission loss file comparison |
| prsdiff | Complex pressure file comparison |
| tsdiff | Time series file comparison |
| cpddiff | Ducting probability file comparison |

See [src/fortran/README.md](src/fortran/README.md) for the Fortran build
workflow and [src/fortran/extras/README.md](src/fortran/extras/README.md)
for the NSPE delivery package.

### Testing

```bash
make test    # Build and run all 55 unit tests (requires libgtest-dev)
```

The test suite includes sub-LSB boundary tests, semantic invariant tests,
summation checks, six-level hierarchy validation, and cross-language
precision tests. See [docs/guide/getting-started.md](docs/guide/getting-started.md)
for test prerequisites and details.

### Project structure

```
src/cpp/           C++ comparison engine (uband_diff)
src/fortran/       Legacy Fortran utilities
scripts/           Shell scripts (batch processing)
data/              Test data files
build/bin/         Compiled executables
docs/              Documentation and technical reports
```

### Documentation index

| Document | Description |
|----------|-------------|
| [Getting Started](docs/guide/getting-started.md) | Build, run, and test instructions |
| [Discrimination Hierarchy](docs/guide/discrimination-hierarchy.md) | Six-level classification algorithm |
| [Sub-LSB Detection](docs/guide/sub-lsb-detection.md) | Precision-aware trivial difference detection |
| [TL Metrics](docs/guide/tl-metrics-implementation.md) | Curve-level comparison metrics (Fabre et al.) |
| [Diff Level Option](docs/guide/diff-level-option.md) | Batch processing `--diff-level` flag |
| [Future Work](docs/future-work.md) | Research roadmap and backlog |

### Dev environment notes

See [docs/dev-notes.md](docs/dev-notes.md) for WSL configuration,
SonarLint setup, Makefile Tools, and other IDE-specific instructions.
