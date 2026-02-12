# Comparison Utilities for NSPE Output Validation

Three Fortran programs are provided to aid in comparing outputs between
different NSPE runs using the same input file. These programs scan through
files and compare outputs element-by-element against a specified error
threshold.

## Programs

| Program  | Extension | Description                                          |
|----------|-----------|------------------------------------------------------|
| tldiff   | .tl, .rtl | Difference between two transmission loss files       |
| prsdiff  | .prs      | Difference between two complex pressure files        |
| tsdiff   | .ts       | Difference between two time series files             |

## Building

```
make
```

The default compiler is `gfortran`. To use a different compiler, set
`your_f77`:

```
make your_f77=ifort
```

## Usage

All three programs accept the same argument format:

```
tldiff FILE1 FILE2 [THRESHOLD]
```

### Arguments

- **FILE1** — test candidate output (e.g., `std_case1r_01.asc`)
- **FILE2** — STD reference output (e.g., `std/std_case1r.tl`)
- **THRESHOLD** — acceptable error threshold in dB (default: 0.1)

### Example

To validate Test Case 1, compare the local output with the reference:

```
./tldiff std_case1r_01.asc std/std_case1r.tl
```

To validate Test Case 2 (range-averaged TL) with a custom threshold:

```
./tldiff std_case2r_02.asc std/std_case2r.rtl 0.01
```

## Method

1. The dimensions of the two files are compared. The number of lines and
   the number of columns (delimiters) must match.
2. The ranges are compared (first column). Each range value must match.
3. The TL values are compared element by element and a summary is printed.

## Error Definitions

Four error categories are tallied for `tldiff`:

| Counter | Condition                                                   |
|---------|-------------------------------------------------------------|
| NERR    | Any difference > 0.05 dB                                   |
| NERR2   | Difference > threshold + 0.05, TL any value                |
| NERR3   | Difference > threshold + 0.05, both TL values < 110 dB     |
| NERR4   | Difference > threshold + 0.05, both TL values < 138.5 dB   |

The program returns an error (exit code 1) if NERR3 > 0. Differences at
TL values exceeding 138.5 dB are ignored, as they exceed the limits of
32-bit single-precision floating-point representation.

## File Requirements

Input files are assumed to be formatted with range in the first column and
data values (TL, pressure, or time series) in the remaining columns. This
matches the format of NSPE ASCII output files (.asc, .tl, .rtl, .ftl).

## References

See OAML-STD-22M, Section 4.1.5 (Criteria for Evaluated Results) and
Section 4.1.6 (Test Procedure) for the evaluation criteria implemented
by these programs.
