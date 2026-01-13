# Development Workflow & Build Guide

## Environment Setup

**Scope**: Production workflows rely on peer-reviewed, authoritative methods (IEEE weighted TL difference, sub-LSB hierarchy). Experimental diagnostics (pattern analysis, phase/stretch ideas) are for investigation only; see `docs/FUTURE_WORK.md` before using them in decisions.

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install build-essential gcc g++ gfortran make

# macOS (with Homebrew)
brew install gcc gfortran

# Windows (via WSL2)
# Install WSL2 with Ubuntu, then install above packages
```

### Build Verification
```bash
# Check compilers are available
gcc --version
g++ --version
gfortran --version
which make
```

## Build Process

### Quick Start
```bash
cd /home/jlighthall/utils/diff_utils
make clean          # Clean previous build artifacts
make all            # Build all targets
make -j4            # Parallel build with 4 jobs (faster)
```

### What `make all` Produces
```
build/bin/
├── uband_diff              # C++ main executable
├── column_analyzer         # (utility)
├── tl_metrics              # (utility)
├── pi_gen_cpp              # (test generator)
├── pi_gen_fortran          # (test generator)
├── pi_gen_java             # (test generator)
├── cpddiff                 # Fortran executable
├── prsdiff                 # Fortran executable
├── tldiff                  # Fortran executable
├── tsdiff                  # Fortran executable
└── tests/
    ├── test_sqrt2
    ├── test_difference_analyzer
    ├── test_semantic_invariants
    └── (other unit tests)
```

### Incremental Builds
```bash
# Build only C++ components
make -C src/cpp

# Build only Fortran components
make -C src/fortran

# Rebuild a specific target
make uband_diff     # If makefile supports specific targets
make clean && make  # Force complete rebuild
```

## Testing

### Running Unit Tests
```bash
# After building, unit tests are in build/bin/tests/
./build/bin/tests/test_difference_analyzer
./build/bin/tests/test_semantic_invariants
./build/bin/tests/test_sub_lsb_boundary

# Or via makefile
cd build && make test
```

### Test Categories

#### 1. Semantic Invariant Tests (`test_semantic_invariants.cpp`)
**Purpose**: Verify zero-threshold contracts and domain rules
**Key Tests**:
- Zero-threshold mode enables maximum sensitivity
- Trivial filtering is immutable (Level 2 cannot be revisited)
- Ignore TL region filters correctly
- Non-trivial count = significant + insignificant (in normal mode)

**Run**:
```bash
./build/bin/tests/test_semantic_invariants
# Expected: All assertions pass, verifying contracts
```

#### 2. Sub-LSB Boundary Tests (`test_sub_lsb_boundary.cpp`)
**Purpose**: Edge cases in floating-point precision
**Key Tests**:
- `30.8` vs `30.85` classified as TRIVIAL (not SIGNIFICANT)
- Rounding to shared minimum precision works correctly
- FP tolerance handles binary representation edge cases
- Decimal place detection for all columns

**Test Data**:
```
# data/test_sub_lsb_files/
# Contains pairs with known sub-LSB differences
```

#### 3. Trivial Exclusion Tests (`test_trivial_exclusion.cpp`)
**Purpose**: Ensure Level 2 filtering persists through pipeline
**Key Tests**:
- Trivial differences never counted as significant
- Trivial differences never counted as insignificant
- Summary counts maintain invariant: total = trivial + nontrivial

#### 4. Difference Analyzer Tests (`test_difference_analyzer.cpp`)
**Purpose**: Core comparison logic
**Key Tests**:
- All six levels classify correctly
- Thresholds applied in correct order
- Marginal band (110 dB) applied correctly
- Critical threshold triggers appropriately

#### 5. File Comparator Tests (`test_file_comparator.cpp`)
**Purpose**: File I/O and validation
**Key Tests**:
- Column count validation
- Decimal place detection
- Format error detection
- Proper error messages

### Integration Testing (Manual)

#### Test Case 1: Basic Numeric Comparison
```bash
# Create simple test files
echo "1 30.8" > /tmp/ref.txt
echo "1 30.85" > /tmp/test.txt

# Run with threshold=0 (zero-threshold mode)
./build/bin/uband_diff /tmp/ref.txt /tmp/test.txt 0 1 0
# Expected: Classified as EQUIVALENT (sub-LSB)
```

#### Test Case 2: Cross-Precision Files
```bash
# Reference has 1 decimal place
echo "A 10.1 20.2" > /tmp/ref.txt

# Test has 2 decimal places
echo "A 10.15 20.25" > /tmp/test.txt

./build/bin/uband_diff /tmp/ref.txt /tmp/test.txt 0 1 0
# Expected: Both differences classified as sub-LSB (EQUIVALENT)
```

#### Test Case 3: Real Data
```bash
# Using provided test data
./build/bin/uband_diff data/bbpp.rc.ref.txt data/bbpp.rc.test.txt 0 10.0 0
# Outputs: Difference table + summary with counts
```

#### Test Case 4: Zero-Threshold Maximum Sensitivity
```bash
./build/bin/uband_diff data/bbtl.pe02.ref.txt data/bbtl.pe02.test.txt 0 1 0
# Expected: Every non-trivial, non-ignored difference counted as significant
```

## Development Workflow

### 1. Identify Change Needed
- Example: "Fix incorrect zero-threshold counting"
- Example: "Add new significance threshold option"

### 2. Create Tests First
```bash
# Add test case to appropriate file in src/cpp/tests/
# Example: test_semantic_invariants.cpp

TEST(SemanticInvariants, ZeroThresholdMaxSensitivity) {
    // Setup reference and test data
    // Assert that counting matches expected zero-threshold contract
    ASSERT_EQ(analyzer.diff_significant(), expected_count);
}
```

### 3. Understand Current Behavior
```bash
# Run existing tests to establish baseline
make clean && make -j4
./build/bin/tests/test_semantic_invariants

# If tests fail, understand why before proceeding
```

### 4. Implement the Fix
- Modify source files in `src/cpp/src/` or `src/fortran/`
- Keep changes focused and minimal
- Reference domain knowledge from `.ai/instructions.md`

### 5. Test the Fix
```bash
# Rebuild (will only recompile changed files + dependents)
make -j4

# Run unit tests
./build/bin/tests/test_semantic_invariants

# If new test was added, it should now pass
# If regression test, verify old issue is fixed

# Run full test suite
cd build && make test
```

### 6. Integration Testing
```bash
# Run against provided test data
./build/bin/uband_diff data/bbpp.rc.ref.txt data/bbpp.rc.test.txt 0 10.0 0

# Compare output with known baseline (if available)
# Manual inspection of difference table and summary counts
```

### 7. Documentation
- Update relevant `.md` file in `docs/`
- Add comments to code explaining why (not just what)
- Update `README.md` if user-facing behavior changed

### 8. Commit
```bash
git add -A
git commit -m "Fix: [Issue] - [Explanation]"
# Example: "Fix: zero-threshold mode counts trivial differences incorrectly"
```

## Common Development Tasks

### Adding a New Comparison Metric

1. **Add calculation to DifferenceAnalyzer**:
   ```cpp
   // src/cpp/src/difference_analyzer.cpp

   double DifferenceAnalyzer::calculate_new_metric(double val1, double val2) {
       // Implementation
       return result;
   }
   ```

2. **Update header**:
   ```cpp
   // src/cpp/include/difference_analyzer.h

   double calculate_new_metric(double val1, double val2);
   ```

3. **Add unit test**:
   ```cpp
   // src/cpp/tests/test_difference_analyzer.cpp

   TEST(NewMetric, BasicCalculation) {
       DifferenceAnalyzer analyzer;
       ASSERT_EQ(analyzer.calculate_new_metric(10.0, 12.0), 2.0);
   }
   ```

4. **Rebuild and test**:
   ```bash
   make -j4
   ./build/bin/tests/test_difference_analyzer
   ```

### Modifying Threshold Logic

1. **Update constant** (if applicable):
   ```cpp
   // src/cpp/include/difference_analyzer.h
   const double NEW_THRESHOLD = 123.45;
   ```

2. **Modify threshold application** in `difference_analyzer.cpp`:
   ```cpp
   if (condition_for_new_threshold) {
       // Apply logic
   }
   ```

3. **Add semantic invariant test** (critical for threshold changes):
   ```cpp
   // src/cpp/tests/test_semantic_invariants.cpp
   // Verify that the invariant still holds with new threshold
   ```

4. **Update documentation**:
   ```markdown
   # docs/IMPLEMENTATION_SUMMARY.md
   Added NEW_THRESHOLD = 123.45 for [domain reason]
   ```

### Formatting Fortran Code

**Use provided build tasks**:
- VS Code: Command Palette → "Break and Normalize Fortran (Global)"
- Or: "Break Fortran Lines Only (Global)"

These automatically format using `break-and-normalize.ps1` script.

**Manual formatting** (if needed):
```bash
# After editing Fortran file
task "Break and Normalize Fortran (Global)"
# Verify compilation still succeeds
make -C src/fortran clean && make -C src/fortran
```

### Cross-Platform Testing (Windows/WSL)

The Fortran formatting tasks use WSL/PowerShell:
```powershell
# Runs via: wsl powershell.exe -File break-and-normalize.ps1 ${file}
# Files are auto-formatted to Fortran standard
```

**Test on both platforms if possible**:
```bash
# Linux
make clean && make all
./build/bin/uband_diff data/bbpp.rc.ref.txt data/bbpp.rc.test.txt 0 1 0

# Then test on Windows/WSL with same data
# Ensure output is byte-for-byte identical (floating-point precision may vary)
```

## Debugging Workflow

### Compilation Errors
```bash
# Get full compiler output
make clean && make 2>&1 | head -50

# Common issues:
# - Missing #include
# - Undefined reference (rebuild after header changes)
# - C++ version mismatch (ensure C++11 compatible)
```

### Runtime Errors
```bash
# Run with simpler test file first
echo "1 10.0" > /tmp/simple.txt
echo "1 10.01" > /tmp/simple_test.txt
./build/bin/uband_diff /tmp/simple.txt /tmp/simple_test.txt 0 1 0

# Check with GDB if needed
gdb ./build/bin/uband_diff
(gdb) run data/test.txt data/test.txt 0 1 0
(gdb) bt  # backtrace
```

### Test Failures
```bash
# Run single test with verbose output
./build/bin/tests/test_difference_analyzer

# Or run under GDB
gdb ./build/bin/tests/test_difference_analyzer
(gdb) run
(gdb) bt  # if it crashes
```

### Verifying Sub-LSB Detection
```bash
# Create test case with known sub-LSB difference
echo "ID_1 30.8 100.0" > /tmp/test_sub_lsb.txt
echo "ID_1 30.85 100.0" > /tmp/test_sub_lsb_ref.txt

# Run with threshold=0 (maximum sensitivity)
./build/bin/uband_diff /tmp/test_sub_lsb_ref.txt /tmp/test_sub_lsb.txt 0 999 0

# Expected output shows difference as TRIVIAL/EQUIVALENT
# If it shows SIGNIFICANT, sub-LSB detection may be broken
```

## Performance Profiling (Optional)

For large file comparisons:

```bash
# Build with optimization (if profiling)
CXXFLAGS="-O3" make -j4

# Measure execution time
time ./build/bin/uband_diff large_file1.txt large_file2.txt 0 1 0

# Profile with perf (Linux)
perf record -g ./build/bin/uband_diff large_file1.txt large_file2.txt 0 1 0
perf report
```

## Continuous Integration (Optional)

If setting up CI/CD:

```bash
#!/bin/bash
# Simple CI script
set -e  # exit on error

make clean
make -j4
cd build && make test
cd ..

# Run integration tests
./build/bin/uband_diff data/bbpp.rc.ref.txt data/bbpp.rc.test.txt 0 10.0 0
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| `undefined reference to ...` | Missing object file | Run `make clean && make -j4` |
| Fortran compilation fails | Format issues | Use "Break and Normalize Fortran" task |
| Tests fail after pull | Dependency mismatch | `make clean && make -j4` |
| Output differs on Windows vs Linux | FP precision | Check sub-LSB tolerance is applied |
| New test case fails | Logic regression | Review diff since last passing build |

## Clean Builds & Debugging

```bash
# Full clean rebuild (use when stuck)
make clean
make -j4

# Verify all tests pass
cd build && make test

# Run manual integration tests with sample data
../build/bin/uband_diff ../data/bbpp.rc.ref.txt ../data/bbpp.rc.test.txt 0 1 0
```

## Code Review Checklist

Before committing changes:

- [ ] New tests added (if logic modified)
- [ ] All existing tests pass
- [ ] No compiler warnings (`-Wall`)
- [ ] Documentation updated (`.md` files)
- [ ] Semantic invariants maintained (if threshold-related)
- [ ] Cross-precision test cases included
- [ ] Comments explain why (not just what)
- [ ] Code follows project conventions
- [ ] Fortran code formatted (if modified)
