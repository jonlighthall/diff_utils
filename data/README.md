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

why is the "range" different between 1 and 3?
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


#### History - Output Formatting
**Status**: Fixed
**Issue**: Complex output formatting caused parsing failures
**Resolution**: Improved format handling robustness in file parser

   STATUS: breaks
   FIX: crazy output formatting

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

#### History - Decimal Place Handling
   test for tsdiff functionality
   FIX: can't handle number of decimal places
**Issue**: Time series comparison couldn't properly handle varying decimal places across columns
**Resolution**: Implemented column-specific decimal place tracking (`min_dp`, `max_dp` per column)

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

#### History - Message and Percentage Display
   FIX: bad message
   FIX: no percentage, but probably is printed
**Issues**:
- Bad error message formatting
- Percentage not displayed when expected
**Resolution**: Updated message formatting and ensured percent error calculation for all non-trivial differences

---

### `calcpduct.std0` — Header Line Handling

**Files**: Files with `.std0` naming

**Description**: Tests comparison utility `cpddiff` functionality with files containing header lines.

**Characteristics**:
- Contains header row
- Tests header skip logic
- Validates multi-column processing

very close but fails regular "diff"

history
   test cpddiff
   has header line

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

history

$ ./bin/uband-diff data/case1r.tl data/case1r_01.txt 0 1 0.01 1

with a threshold of zero, there are 39 differences (correctly tallied). there are 12 differences where the corresponding values are above the ignore threshold of 138.47; these are trivial and should be ignored. there are 24 differences where the corresponding values exceed the marginal threshold. these should be counted, but not cause a error or failure in the difference (by themselves). there are 3 differences where the values are less than the marginal threshold and should therefore be counted as significant differences.

I think there are supposed to be 39-12=27 significant differences, not 13, so I don't know where that number is coming from. It looks like the non-zero differences are being double counted (39x2=78)

    the number of elements is obvious, that should be the total number of elements compared. in this case the file has 353 lines with 11 columns per line (1 range with 10 data columns), so the total number of elements is 3883

 In this case all of the 39 non-zero differences are non-trivial.

 By my count, there are 12 "insignificant" differences where the magnitude of both vales exceeds the ignore threshold of ~138 (corresponding to pressure values less than the single precision epsilon of ~1.19E-7), which means there should be 27 significant differences (those differences that can't be written off as being due to machine precision and correspond to differences greater than the user threshold (first argument after file names)).

 In this example, of the 27 significant differences, I count 24 differences that have both values in the marginal range.

 The marginal/non-marginal count should be 24 and 3, which adds up to the significant difference tally of 27.

 in this case there are 3 non-marginal significant errors.

e.g. 353 lines * 11 columns = 3883 elements
 In this case 3 out of the 3883 differences correspond to 0.0773%.
 e.g. 353 lines * 11 columns = 3883 elements
          e.g. 3883 = 3844 + 39 = 353 * 11
                   e.g. 39 = 0 + 39 (no trivial differences)
                            e.g. 39 = 12 + 27
         e.g. 27 = 24 3
   e.g. 3 = 0 + 3
    e.g. 3 = 3 + 0

    there are 39 real differences in the file plus the 9 trivial and 1 non-trival differences I introduced
         therefore, there are 48 non-zero differences int his file
         e.g. 3883 = 3835 + 48
         Here, the 9 trivial differences I added are identified
         e.g. 48 = 9 + 39 (no trivial differences)
         In this example, there are 9 non-trivial differences that I introduced manually on line 1.
                  e.g. 39 = 12 + 27

In this example there are 27 significant differences
         e.g. 27 = 24 3

         there are 3 non-marginal differences in this example
                 e.g. 3 = 0 + 3
                       e.g. 3 = 3 + 0
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

history
file (case 2)
   odd case
   starts out the same

nspe.std2.
   another great example
   this should "fail" but should pass with the 2% rule
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

history
   considering the ouptut of ./bin/uband-diff data/pe.std3.test.nspe01.txt data/pe.std3.refe.nspe01.txt

   this is a realistic example of data being compared.
   in this example, the two files should "fail" a simple diff

#### Range Precision and Monotonic Detection
**Complex issue**: Range column had step size 0.0122 but printed precision was insufficient to capture it at every step (e.g., values printed with fewer decimal places). This caused monotonic increasing test to fail because consecutive values appeared identical.

**Root cause**: Monotonic increasing test was too strict—it should allow for unchanged values (monotonically non-decreasing).

**Resolution**: Updated range detection to be more forgiving of precision-limited format while maintaining fixed-delta detection



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
