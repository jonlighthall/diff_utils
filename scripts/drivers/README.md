# Batch Processing and Analysis Drivers

Shell and Python scripts for running, comparing, and analyzing NSPE validation
results.

## Scripts

| Script | Purpose |
|--------|---------|
| `process_nspe_in_files.sh` | Batch runner: build, run, diff NSPE input files |
| `process_ram_in_files.sh` | Batch runner for RAM-specific input files |
| `process_timed_subset.sh` | Run only cases below a runtime threshold |
| `diff_stats.sh` | Aggregate and display `_diff.log` statistics |
| `timing_stats.sh` | Per-file timing statistics from `_timing.log` |
| `parse_nspe_input.py` | Parse `.in` files, estimate complexity, predict runtime |
| `lib_diff_utils.sh` | Shared library (sourced by other scripts) |

---

## parse_nspe_input.py

Parse NSPE input files and estimate computational complexity from physics
parameters.

### Basic usage

```bash
# Parameter table sorted by grid work
parse_nspe_input.py std_new/ --sort work

# With predicted runtimes
parse_nspe_input.py std_new/ --predict --sort time

# Joined with timing data (shows actual vs predicted)
parse_nspe_input.py std_new/ --timing std_new/_timing.log --predict --sort work

# TSV export for analysis (pandas, Excel, etc.)
parse_nspe_input.py std_new/ --timing std_new/_timing.log --tsv > analysis.tsv

# Summary statistics
parse_nspe_input.py std_new/ --sort work --summary
```

### Sort options

| `--sort` value | Sorts by |
|----------------|----------|
| `file` | Filename (alphabetical) |
| `freq` | Frequency (Hz) |
| `sd` / `speeddial` | SpeedDial value |
| `rmax` | Maximum range |
| `bathy_max` | Maximum bathymetry depth |
| `work` | Estimated grid work (Nr × Nz) |
| `time` / `predict` | Predicted runtime |

### Prediction model

The `--predict` flag adds estimated runtime using a power-law regression fitted
to 195 Fortran v6.2 timing records:

$$T \approx 10^{-4.68} \times \text{grid\_work}^{0.82} \times 2.91^{[\text{is\_slow}]}$$

- $R^2 = 0.886$; 96% of predictions within 5× of actual
- "Slow" bottom types: ABSORBING, GEOACOUSTIC, LVA, LFBL-10SCF, LFBL-1015LVA
- When combined with `--timing`, an A/P (actual/predicted) ratio column appears

---

## timing_stats.sh

Statistical summary of `_timing.log` files produced by `process_nspe_in_files.sh`.

### Single-file mode

```bash
# Basic per-file statistics
timing_stats.sh std_new/_timing.log

# Sorted by mean runtime (slowest last)
timing_stats.sh std_new/_timing.log --sort mean

# List files completing in under 5 seconds
timing_stats.sh std_new/_timing.log --list-below 5

# CSV output
timing_stats.sh std_new/_timing.log --csv
```

### Compare mode (two logs)

```bash
# Side-by-side Fortran vs C++ comparison
timing_stats.sh fortran/_timing.log cpp/_timing.log --compare

# Sorted by B/A ratio (C++ fastest relative to Fortran first)
timing_stats.sh fortran/_timing.log cpp/_timing.log --compare --sort ratio

# Sorted by mean runtime
timing_stats.sh fortran/_timing.log cpp/_timing.log --compare --sort mean

# Sorted by statistical significance of the timing difference
timing_stats.sh fortran/_timing.log cpp/_timing.log --compare --sort sigma
```

### Sort options

| `--sort` value | Sorts by | Modes |
|----------------|----------|-------|
| `name` | Filename (default) | single, compare |
| `mean` | Mean runtime | single, compare |
| `count` | Number of runs | single |
| `stddev` | Standard deviation | single |
| `min` | Minimum runtime | single |
| `max` | Maximum runtime | single |
| `ratio` | B/A ratio | compare only |
| `sigma` | Welch t-statistic (σ) | compare only |

The **sigma** column shows how many standard errors apart the two means are
(Welch's t-statistic). Color coding: ≥|3|σ → red/green (highly significant),
≥|2|σ → yellow (significant). Positive = B slower than A, negative = B faster.

### Filters

```bash
--exe <name>       # Filter by executable basename
--status <s>       # Filter by status (default: PASS)
--all              # Include all statuses
--host <name>      # Filter by hostname
```

---

## diff_stats.sh

Display and aggregate `_diff.log` comparison results.

```bash
# Per-file diff statistics
diff_stats.sh std_new/_diff.log

# Per-basename summary (worst case across variants)
diff_stats.sh std_new/_diff.log --basename

# Sorted by maximum error
diff_stats.sh std_new/_diff.log --sort max_err

# Filter by diff method
diff_stats.sh std_new/_diff.log --method tl_diff

# CSV export
diff_stats.sh std_new/_diff.log --csv
```

---

## process_timed_subset.sh

Run `process_nspe_in_files.sh` on only the files whose mean runtime falls below
a threshold (based on a previous `_timing.log`).

```bash
# Run only cases under 5 seconds
process_timed_subset.sh --timing-log std_new/_timing.log --max-time 5 make std_new

# Run only cases under 30 seconds, with diff
process_timed_subset.sh --timing-log std_new/_timing.log --max-time 30 diff std_new
```
