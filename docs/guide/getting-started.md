# Getting Started with diff_utils

This guide covers building, running, and testing the diff_utils suite.

## What `tl_diff` Does

When validating scientific software, you often need to compare two output
files and answer: *do these results agree?* A simple `diff` is useless
here — formatting differences, compiler variations, and floating-point
rounding produce thousands of textual changes that have no physical
meaning. `tl_diff` eliminates that noise by understanding what
numerical differences actually signify.

### The core idea

A printed value like `30.8` doesn't mean exactly 30.8 — it means "some
value that rounds to 30.8 at one decimal place," i.e., the interval
[30.75, 30.85). If another file prints `30.85` (two decimal places), that
value falls within the first value's representational interval. The two
numbers are informationally equivalent — you can't distinguish them from
the printed output alone. `tl_diff` detects this automatically by
examining the decimal precision of each value and classifying
sub-interval differences as **trivial**.

### Three layers of analysis

`tl_diff` processes differences through three distinct layers:

1. **Point-by-point comparison** — Each pair of corresponding values is
   classified through a six-level hierarchy: zero vs non-zero, trivial vs
   non-trivial, significant vs insignificant, marginal vs non-marginal,
   critical vs non-critical, and error vs non-error. The trivial filter
   (sub-LSB detection) is the key innovation — it permanently removes
   formatting artifacts before any threshold logic runs.

2. **Physics-based filtering** — Values above 138.47 dB (the noise floor
   of IEEE 754 single-precision floating point) are ignored, since
   differences there have no physical meaning. An operational marginal
   band flags differences that are technically significant but outside the
   range of practical interest.

3. **Curve-level metrics** (informational) — When enabled with `-v 1`,
   the program reports TL curve comparison metrics (weighted RMSE,
   correlation, combined score) based on the methodology of
   [Fabre et al. (2009)](https://doi.org/10.23919/OCEANS.2009.5422312).
   These metrics are displayed for reference but **do not affect the
   pass/fail determination**.

### Pass/fail decision

The default pass/fail is based entirely on layers 1 and 2. If fewer than
2% of compared elements show non-marginal, non-critical significant
differences, the files are considered to agree. Critical threshold
exceedances cause immediate failure regardless of percentage.

## Prerequisites

### Required
- **C++ compiler:** GCC 7+ or Clang 5+ with C++17 support
- **Make:** GNU Make 4.0+
- **gfortran:** For Fortran components and pi generators

### Optional
- **libgtest-dev:** For unit tests (`sudo apt install libgtest-dev`)
- **Java JDK:** For Java pi generator
- **Python 3:** For Python pi generator

## Building

### Build Everything
```bash
make              # Build all executables
```

### Build Specific Targets
```bash
make diff         # Build tl_diff only
make test         # Build and run unit tests
make pi_gen       # Build pi precision generators (C++, Fortran, Java)
```

### Clean Build
```bash
make clean        # Remove all build artifacts
make remake       # Clean + rebuild
```

## Running tl_diff

### Basic Usage
```bash
./build/bin/tl_diff <ref_file> <test_file> <threshold> <critical> <print>
```

### Arguments
| Arg | Name | Description |
|-----|------|-------------|
| 1 | ref_file | Reference file path |
| 2 | test_file | Test file path |
| 3 | threshold | Significance threshold (0 = maximum sensitivity) |
| 4 | critical | Critical threshold (halt printing on first exceedance) |
| 5 | print | Minimum difference to print (0 = print all) |

### Examples

**Maximum sensitivity comparison:**
```bash
./build/bin/tl_diff data/pe.std1.pe01.ref.txt data/pe.std1.pe01.test.txt 0 1 0
```

**Standard comparison with thresholds:**
```bash
./build/bin/tl_diff ref.txt test.txt 0.1 2.0 0.05
```

**Cross-language pi validation:**
```bash
./build/bin/tl_diff data/pi_cpp_asc.txt data/pi_fortran_asc.txt 0 1 0
```

## Running Tests

### Prerequisites
Install Google Test:
```bash
# Ubuntu/Debian
sudo apt install libgtest-dev

# Build gtest libraries (if not pre-built)
cd /usr/src/gtest
sudo cmake .
sudo make
sudo cp lib/*.a /usr/lib/
```

### Run All Tests
```bash
make test
```

### Run Tests Directly
```bash
./build/bin/tests/run_tests
```

### Run Specific Test Suite
```bash
./build/bin/tests/run_tests --gtest_filter="SemanticInvariants.*"
./build/bin/tests/run_tests --gtest_filter="SubLSBBoundaryTest.*"
./build/bin/tests/run_tests --gtest_filter="CrossLanguagePrecisionTest.*"
```

### Run Single Test
```bash
./build/bin/tests/run_tests --gtest_filter="SemanticInvariants.ZeroThresholdInvariant"
```

### List All Tests
```bash
./build/bin/tests/run_tests --gtest_list_tests
```

## Test Suites Overview

| Suite | Tests | Purpose |
|-------|-------|---------|
| `CrossLanguagePrecisionTest` | 5 | Validate C++/Fortran/Python/Java produce identical π |
| `SemanticInvariants` | 4 | Zero-threshold, high-ignore, critical suppression |
| `SubLSBBoundaryTest` | 6 | Sub-LSB detection edge cases |
| `FileComparatorSummationTest` | 8 | 6-level hierarchy, counter invariants |
| `SensitiveThresholdTest` | 1 | Zero-threshold canonical classification |
| `TrivialExclusionTest` | 1 | Trivial differences excluded at all thresholds |
| `UnitMismatchTest` | 4 | Detect meters vs nautical miles |
| `PercentThresholdTest` | 3 | Percentage-based significance |

**Total: 55 tests across 15 suites**

## Generating Test Data

### Pi Precision Test Data
The pi generators create test files for cross-language validation:

```bash
# Build generators
make pi_gen

# Generate test data in data/
make pi_gen_data
```

**Generated files:**
- `data/pi_cpp_asc.txt` / `data/pi_cpp_desc.txt`
- `data/pi_fortran_asc.txt` / `data/pi_fortran_desc.txt`
- `data/pi_python_asc.txt` / `data/pi_python_desc.txt`
- `data/pi_java_asc.txt` / `data/pi_java_desc.txt`

### Manual Pi Generation
```bash
# C++
./build/bin/pi_gen_cpp > my_pi_cpp.txt
./build/bin/pi_gen_cpp desc > my_pi_cpp_desc.txt

# Fortran
./build/bin/pi_gen_fortran > my_pi_fortran.txt

# Python
python3 scripts/pi_gen/pi_gen_python.py > my_pi_python.txt

# Java
./build/bin/pi_gen_java > my_pi_java.txt
```

## Project Structure

```
diff_utils/
├── build/
│   ├── bin/           # Executables
│   │   ├── tl_diff
│   │   ├── pi_gen_cpp
│   │   ├── pi_gen_fortran
│   │   ├── pi_gen_java
│   │   └── tests/run_tests
│   └── test/          # Test data files (generated by tests)
├── data/              # Static test data (pi files, reference data)
├── docs/
│   └── guide/         # User guides (you are here)
├── scripts/
│   └── pi_gen/        # Pi generator source files
├── src/
│   └── cpp/
│       ├── src/       # Core implementation
│       ├── include/   # Headers
│       └── tests/     # Unit tests
└── makefile
```

## Troubleshooting

### "libgtest not found"
```bash
# Install development package
sudo apt install libgtest-dev

# If still failing, build from source
cd /usr/src/gtest
sudo cmake .
sudo make
sudo cp lib/*.a /usr/lib/
```

### "gfortran: command not found"
```bash
sudo apt install gfortran
```

### "javac: command not found"
```bash
sudo apt install default-jdk
```

### Tests fail with "file not found"
Ensure test data exists:
```bash
make pi_gen_data   # Regenerate pi test files
ls data/pi_*.txt   # Verify files exist
```

### Build fails with C++17 errors
Ensure GCC 7+ or Clang 5+:
```bash
g++ --version
# If too old, update:
sudo apt install g++-9
```

## Next Steps

- **Technical details:** See [implementation-summary.md](../implementation-summary.md)
- **Hierarchy explanation:** See [discrimination-hierarchy.md](discrimination-hierarchy.md)
- **Sub-LSB detection:** See [sub-lsb-detection.md](sub-lsb-detection.md)
- **AI context:** See [.ai/README.md](../../.ai/README.md) for AI assistant context
