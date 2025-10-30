# Test Data Files

This directory contains reference and test files used for validating the `uband_diff` numerical file comparison utility.

## Test File Pairs

### `bbpp.freq` — Scientific Format with Varying Decimal Places

**Files**: `bbpp.freq.ref.txt`, `bbpp.freq.test.txt`, `bbpp.freq.test2.txt`

**Description**: Tests comparison of scientific notation values with fixed data width but varying decimal places. These files exercise format-independent comparison logic.

**Characteristics**:
- Scientific format (e.g., 1.234E+02)
- Fixed column width with variable precision
- Tests precision-aware trivial difference detection

test and ref should pass
test 2 should fail

---

### `bbpp.rc` — Range-Centered Data with Complex Formatting

**Files**: `bbpp.rc.ref.txt`, `bbpp.rc.test.txt`

**Description**: Range-centered propagation data with complex output formatting. Originally exposed output formatting issues.

**Characteristics**:
- Complex multi-column structure
- Mixed precision formats
- Historical test case for format handling robustness

time by 4.83ns, will fail
wildly different results

---

### `bbpp.ts` — Time Series Data

**Files**: `bbpp.ts.ref.txt`, `bbpp.ts.test.txt`

**Description**: Tests time series comparison functionality (tsdiff mode). Originally exposed decimal place handling issues.

**Characteristics**:
- Time-domain acoustic data
- Variable decimal places across columns
- Tests column-specific precision tracking

time offset by 1.36ns, fill fail
complex data

---

### `bbtl.nspe02` — Transmission Loss with NSPE Data

**Files**: `bbtl.pe02.ref.txt`, `bbtl.pe02.test.txt`, `bbtl.pe02.test2.txt`

**Description**: Transmission loss (TL) data from NSPE (Normal Mode Spectral Eigenray) runs. Tests domain-specific thresholds (110 dB marginal, 138.47 dB ignore).

**Characteristics**:
- TL values spanning operational to marginal ranges
- Multiple columns (range + depth points)
- Tests ignore/marginal threshold logic

all different, all fail
very similar tl curves, but different results.
test 1 and test 2 are very close and would pass a more sophisticated test

---

### `calcpduct.std0` — Header Line Handling

**Files**: Files with `.std0` naming

**Description**: Tests comparison utility `cpddiff` functionality with files containing header lines.

**Characteristics**:
- Contains header row
- Tests header skip logic
- Validates multi-column processing

very close but fails regular "diff"

---

### `pe.std1.pe01` — Precision Edge Case

**Files**: `pe.std1.pe01.ref.txt`, `pe.std1.pe01.test.txt`

**Description**: Engineered test case with intentional trivial and non-trivial differences. The reference file was manually modified to add:
- 1 trailing zero (trivial difference)
- 9 extra decimal places with values 1-4 (trivial differences)
- 39 real differences

this is the canonical test
34 lines appear different
maximum difference is 0.1.
the first line of ref file has added precision for testing
real data


**Characteristics**:
- Total elements: 3883 (353 lines × 11 columns)
- Non-zero differences: 48 (9 trivial + 39 non-trivial)
- Tests LEVEL 2 trivial vs non-trivial discrimination

level 0
  file sturctures match, but with different formatting
   353 lines ×
    11 columns
  3883 - Total elements:
level 1
  3835 exact matches
    48 non-zero differences (3835 + 48 = 3883)
level 2
    39 non-trivial
     9 trivial (39 + 9 = 48)
level 3
  29 insignificant differences (differences below meaningful range)
  10 significant differences (in meaningful range) (29 + 10 = 48)
level 4
  marginal differnces 9 (in operationally insignificant range
  1 non-marginal difference (in operational range) (9 + 1 = 10)
level 5
  no critical (>1dB) differences
level 6
  1 significant difference in operational range
  0.025% of total
level 7
   uncorrelated differences

**Expected behavior**:
- 9 trivial differences detected and excluded from significance analysis
- 39 non-trivial differences undergo further discrimination
- Validates format precision awareness



---

### `pe.std2.pe02` — "Close Enough" Scenario

**Files**: `pe.std2.pe02.ref.txt`, `pe.std2.pe02.test.txt`, `pe.std2.pe02.test2.txt`

**Description**: Realistic comparison scenario testing the 2% rule. Files have significant differences but should pass due to low overall percentage of differing elements.

**Characteristics**:
- Contains significant differences
- Tests pass-with-warning logic (< 2% threshold)
- Realistic operational comparison case

divering - should fail all tests
actually very close, again would pass a more sophisticaed test
test 2 and ref are imperceptably close; actually they are identical, but fail regular diff!?


---

### `pe.std3.pe01` — Structure Mismatch and Range Precision

**Files**: `pe.std3.pe01.ref.txt`, `pe.std3.pe01.test.txt`

**Description**: Complex test case exposing multiple edge cases:
- Range column precision insufficient to capture step size (0.0122)
- Monotonically increasing test fails due to formatting
- Different file structures

**Characteristics**:
- Total elements: 3554
- Significant differences: 905 (25% of total)
- Tests structural mismatch handling
- Exposes need for range data detection

interesting case a few large differences
ranges formatted differently
should absolutely pass
test of transient spikes

**Command example**:
```bash
./bin/uband_diff data/nspe.std3.test.nspe01.txt data/nspe.std3.refe.nspe01.txt 0.2 3 0.6
```

**Expected behavior**: Should fail due to >2% significant differences (905/3554 = 25%)

---

### case1

unit scaling error

### `case1r` — Multi-Level Discrimination Validation

**Files**: `case1r.tl`, `case1r_01.txt`

**Description**: Comprehensive test case designed to validate all six discrimination levels. Contains controlled distributions of differences across all categories.

**Characteristics**:
- Total elements: 3883 (353 lines × 11 columns)
- Non-zero differences: 39
- Insignificant (above ignore 138.47 dB): 12
- Marginal (110-138.47 dB range): 24
- Non-marginal significant: 3 (0.077% of total)

**Command example**:
```bash
./bin/uband_diff data/case1r.tl data/case1r_01.txt 0 1 0.01 1
```

**Breakdown by discrimination level**:
- LEVEL 1: 39 non-zero, 3844 zero
- LEVEL 2: 39 non-trivial (all non-zero are non-trivial in this case)
- LEVEL 3: 27 significant, 12 insignificant
- LEVEL 4: 24 marginal, 3 non-marginal
- LEVEL 5: 0 critical, 3 non-critical
- LEVEL 6: 3 error (based on user threshold)

**Expected behavior**: Pass with note (3/3883 = 0.077% < 2% threshold)

---



### `edge_case` — Zero Threshold Failure

**Files**: `edge_case` test files

**Description**: Engineered edge case that should fail when significance threshold is set to zero. Tests sensitive mode operation.

**Characteristics**:
- Tests zero-threshold mode
- Validates sensitive comparison logic
- Ensures any non-trivial difference triggers failure

---

### `file` (case 2) — Identical Start Divergence

**Files**: Various `file` test pairs

**Description**: Files that start identically but diverge later. Tests structural difference detection and partial comparison.

**Characteristics**:
- Identical initial structure
- Diverges mid-file
- Tests early-exit vs complete analysis behavior

---

## Test Execution

### Standard Comparison
```bash
./bin/uband_diff <file1> <file2> <significant_threshold> <group_size> <critical_threshold> <print_threshold>
```

### Example Commands
```bash
# Basic comparison with 0.05 significant threshold
./bin/uband_diff data/bbpp.freq.ref.txt data/bbpp.freq.test.txt 0.05 3 0.6 1

# Zero threshold (sensitive mode)
./bin/uband_diff data/pe.std1.pe01.ref.txt data/pe.std1.pe01.test.txt 0 1 0 1

# Percent mode (10% threshold)
./bin/uband_diff data/pe.std2.pe02.ref.txt data/pe.std2.pe02.test.txt -10 3 0.6 1
```

## File Naming Conventions

- `.ref.txt` — Reference file (expected correct output)
- `.test.txt` — Test file (output being validated)
- `.test2.txt` — Secondary test variant
- `.tl` — Transmission loss data
- `.asc` — ASCII formatted data
- `nspe` prefix — Normal mode spectral eigenray data
- `pe` prefix — Parabolic equation solver output
- `bb` prefix — Broadband propagation data

## Additional Files

- `notes.md` — Development notes and algorithm evolution (historical)
- `plot_bbpp_freq.m` — MATLAB plotting script for frequency domain data
- `pi_cpp_asc.txt`, `pi_cpp_desc.txt` — Pi precision test data generated by C++ version (ascending/descending)
- `pi_fortran_asc.txt`, `pi_fortran_desc.txt` — Pi precision test data generated by Fortran version (ascending/descending)

## See Also

- `../docs/DISCRIMINATION_HIERARCHY.md` — Detailed algorithm documentation
- `../docs/PI_PRECISION_TEST_SUITE.md` — Pi precision test methodology
- `../build/test/` — Generated test files from unit test suite
