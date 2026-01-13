# Codebase Architecture & Structure

## Directory Organization

```
diff_utils/
├── src/
│   ├── cpp/                          # Modern C++ comparison engine (primary)
│   │   ├── include/
│   │   │   ├── difference_analyzer.h     # Core comparison logic
│   │   │   ├── file_comparator.h         # File I/O & validation
│   │   │   ├── line_parser.h             # Format parsing & decimal tracking
│   │   │   └── error_accumulation_analyzer.h
│   │   ├── src/
│   │   │   ├── difference_analyzer.cpp   # Numeric comparison, thresholds
│   │   │   ├── file_comparator.cpp       # File reading, column validation
│   │   │   ├── line_parser.cpp           # Decimal place detection
│   │   │   └── error_accumulation_analyzer.cpp
│   │   ├── tests/
│   │   │   ├── test_difference_analyzer.cpp
│   │   │   ├── test_semantic_invariants.cpp  # Zero-threshold contracts
│   │   │   ├── test_sub_lsb_boundary.cpp     # Sub-LSB edge cases
│   │   │   └── test_unit_mismatch.cpp
│   │   └── main/
│   │       └── uband_diff.cpp              # Main executable
│   │
│   ├── fortran/                      # Legacy Fortran programs
│   │   ├── programs/
│   │   │   ├── cpddiff.f90               # Ducting probability
│   │   │   ├── prsdiff.f90               # Pressure files
│   │   │   ├── tldiff.f90                # Transmission loss
│   │   │   └── tsdiff.f90                # Time series
│   │   ├── modules/
│   │   │   ├── constants.f90
│   │   │   ├── file_io.f90
│   │   │   └── (other shared modules)
│   │   └── makefile
│   │
│   └── java/                         # Java utilities (if present)
│       ├── src/
│       ├── tests/
│       └── build config
│
├── build/                            # Compiled artifacts
│   ├── bin/                          # Executables
│   │   ├── uband_diff               # Primary C++ comparison tool
│   │   ├── cpddiff, prsdiff, tldiff, tsdiff  # Fortran tools
│   │   └── tests/                   # Test executables
│   ├── obj/                          # Object files
│   ├── classes/                      # Java classes
│   └── deps/                         # Dependency files
│
├── data/                             # Test data
│   ├── *.txt                         # Reference and test files
│   ├── *.asc                         # ASCII format test files
│   ├── *.prs, *.ts, *.out            # Domain-specific formats
│   └── run_data.sh                   # Data generation script
│
├── docs/                             # Comprehensive documentation
│   ├── IMPLEMENTATION_SUMMARY.md     # Core fix documentation
│   ├── DISCRIMINATION_HIERARCHY.md   # Six-level system
│   ├── SUB_LSB_DETECTION.md          # Precision handling
│   ├── DIFF_LEVEL_OPTION.md          # CLI option reference
│   ├── ERROR_ACCUMULATION_ANALYSIS.md
│   ├── TL_METRICS_IMPLEMENTATION.md
│   ├── PI_PRECISION_TEST_SUITE.md
│   ├── RAW_DIFF_FIX_SUMMARY.md
│   ├── docs/report/UBAND_DIFF_TECHNICAL_REPORT.tex  # Formal documentation
│   └── (other technical reports)
│
├── scripts/                          # Utility scripts
│   ├── pi_gen_python.py              # Pi precision generator
│   ├── spherical_earth_acoustic.py
│   ├── run_pf_ram_batch.sh
│   └── pi_gen/                       # Language-specific generators
│
├── makefile                          # Main build configuration
├── makefile.modern                   # Modernized build options
├── README.md                         # Project overview
└── .ai/                              # AI development context (this folder)
    ├── instructions.md               # This file
    ├── codebase.md                   # Architecture
    ├── dev-workflow.md               # Development process
    └── patterns.md                   # Coding patterns
```

## Core Architecture

### Data Flow

```
Input File
    ↓
FileComparator::validate()
    ├─ Check column count consistency
    ├─ Detect decimal places per column
    └─ Parse numeric values
    ↓
LineParser::analyze_line()
    ├─ Extract numbers
    ├─ Track precision (decimal places)
    └─ Compute shared minimum precision
    ↓
DifferenceAnalyzer::classify_difference()
    ├─ Level 1: Check if raw_diff != 0
    ├─ Level 2: Sub-LSB detection (with FP tolerance)
    ├─ Level 3: Significance threshold (user or derived)
    ├─ Level 4: Marginal band (110 dB TL)
    ├─ Level 5: Critical threshold check
    └─ Level 6: Error classification
    ↓
Output
    ├─ Difference table (capped + truncated if needed)
    ├─ Summary report
    └─ Consistency checks (invariants)
```

### Key Components

#### DifferenceAnalyzer
**Purpose**: Core numeric comparison logic
**Key Responsibilities**:
- Classify differences across 6 levels
- Apply sub-LSB detection with floating-point tolerance
- Enforce immutable Level 2 filtering (trivial differences)
- Maintain semantic invariants (especially zero-threshold behavior)
- Track marginal, critical, error counts

**Key Methods**:
- `classify_difference()` - Main entry point, returns classification
- `apply_sub_lsb_detection()` - Checks if raw_diff ≤ LSB/2 (with FP tolerance)
- `apply_significance_threshold()` - Level 3 filtering
- `check_invariants()` - Validates semantic contracts

**Critical Fields**:
```cpp
const double FP_TOLERANCE = 1e-12;
const double MARGINAL_TL = 110.0;
const double IGNORE_TL = 138.47;
int max_decimal_places = 17;
```

#### FileComparator
**Purpose**: Validation and file I/O
**Key Responsibilities**:
- Validate structural consistency (column counts match)
- Detect format issues before comparison
- Parse numeric values with precision tracking
- Report summary statistics

#### LineParser
**Purpose**: Extract and analyze line content
**Key Responsibilities**:
- Parse delimited text lines
- Detect decimal places per column
- Compute shared minimum precision across all columns
- Handle edge cases (exponential notation, special values)

#### ErrorAccumulationAnalyzer (Experimental Diagnostics)
**Purpose**: Cross-file statistics and exploratory diagnostics (not authoritative for pass/fail)
**Key Responsibilities**:
- Accumulate differences across multiple comparisons
- Diagnostic pattern analysis (run tests, autocorrelation, regression)
- Identify transient spikes or systematic biases (for investigation only)

**Status**: Experimental. Use for analysis; do not rely on for authoritative decisions. See `docs/future-work.md` for research roadmap.

## Precision Handling

### Decimal Place Detection
- **Algorithm**: Count digits after decimal point for each number
- **Storage**: Per-column tracking in `line_parser`
- **Use**: Determines rounding behavior for Level 2 sub-LSB detection
- **Limit**: 17 decimal places (single to extended precision range)

### Sub-LSB Detection Formula

```
shared_min_dp = min(decimal_places[column_a], decimal_places[column_b])
lsb = 10^(-shared_min_dp)
big_zero = lsb / 2  // half-LSB threshold

is_trivial = (raw_diff < big_zero) OR
             (abs(raw_diff - big_zero) < FP_TOLERANCE * max(raw_diff, big_zero))
```

### Floating-Point Robustness
- FP_TOLERANCE = 1e-12 handles binary representation edge cases
- Example: `0.05` might be represented as `0.05000000000000071054`
- Tolerance prevents false positive "differences" from FP rounding

## Threshold Logic

### Operational Thresholds

| Level | Threshold | Value | Notes |
|-------|-----------|-------|-------|
| 2 | Sub-LSB (derived) | LSB/2 | Immutable filtering |
| 3 | Significant | User or derived | 0.0 = maximum sensitivity |
| 3 | Ignore TL | 138.47 dB | Physics: numerically meaningless |
| 4 | Marginal | 110 dB | Operational relevance boundary |
| 5 | Critical | User-set | Triggers fast-fail |

### TL Comparison Methodology
- **Authoritative**: IEEE peer-reviewed weighted TL difference (Fabre & Zingarelli / Goodman) implemented in TL metrics
- **Experimental**: Phase/stretch detection concepts, extended diagnostics — see `docs/future-work.md`
| User | Print | Minimum to emit row | Still counted internally |

### Zero-Threshold Special Case
When `user_significant == 0.0`:
```
diff_significant = diff_non_trivial - diff_high_ignore

Where:
  diff_non_trivial = total differences that pass Level 2
  diff_high_ignore = non-trivial differences where BOTH TL > 138.47
```

This maintains "maximum sensitivity" by only filtering meaningless high-TL region.

## Language-Specific Patterns

### C++ (Primary Implementation)
- **Modern**: C++11 and later features
- **STL Use**: `vector`, `map`, `string` for data structures
- **Memory**: Smart pointers preferred (but legacy code may use raw pointers)
- **Compilation**: GCC (g++) with -Wall, -g flags
- **Testing**: Custom unit test framework in `tests/`

### Fortran (Legacy Maintenance)
- **Standard**: Free-form Fortran (`.f90`)
- **Style**: Structured, module-based
- **Interfaces**: Explicit for all procedures
- **Naming**: UPPERCASE for keywords, MixedCase for identifiers
- **Formatting**: Tasks available for auto-formatting

### Build System
- **Primary**: GNU make with `.d` dependency files
- **Modern**: `makefile.modern` for Fortran-specific improvements
- **Parallel**: `-j` flag supported for faster builds
- **Clean**: `make clean` removes build artifacts

## Data Formats

### Supported Input Formats
- **Whitespace-delimited**: Primary (columns separated by spaces/tabs)
- **CSV**: Potential (requires adaptation)
- **Fixed-width**: Legacy Fortran outputs

### Columns
- **Typical Structure**: [ID] [Value1] [Value2] ... [ValueN]
- **Validation**: Column count must match across reference and test files
- **Precision**: Each column can have different decimal places

### Domain Formats
- `.asc` - ASCII TL (transmission loss) files
- `.prs` - Pressure files
- `.out` - General output
- `.ts` - Time series
- `.txt` - Generic numeric files

## Testing Strategy

### Test Organization
```
src/cpp/tests/
├── test_difference_analyzer.cpp       # Unit tests for comparison logic
├── test_semantic_invariants.cpp       # Zero-threshold contracts
├── test_sub_lsb_boundary.cpp          # Precision edge cases
├── test_sub_lsb_detection.cpp
├── test_trivial_exclusion.cpp
├── test_unit_mismatch.cpp
├── test_file_comparator.cpp
├── test_line_parser.cpp
└── test_main.cpp                      # Test runner
```

### Test Types
1. **Unit Tests**: Individual function behavior (e.g., `classify_difference()`)
2. **Semantic Invariants**: Contract verification (e.g., zero-threshold sensitivity)
3. **Additive Invariants**: Bookkeeping integrity (sum checks)
4. **Integration**: Full file comparison workflows
5. **Regression**: Prevent re-introduction of known bugs

### Key Test Cases
- Cross-precision pairs (1dp vs 2dp)
- Zero-threshold mode with various inputs
- Marginal & critical boundary conditions
- High-TL ignore region filtering
- Floating-point edge cases

## Build Artifacts

### Executables
```
build/bin/
├── uband_diff               # Primary C++ tool
├── cpddiff                  # Fortran (ducting probability)
├── prsdiff                  # Fortran (pressure)
├── tldiff                   # Fortran (TL files)
├── tsdiff                   # Fortran (time series)
└── tests/                   # Unit test runner
```

### Build Products
- `build/obj/` - Object files
- `build/deps/` - Dependency tracking (`.d` files)
- `build/mod/` - Fortran module files (`.mod`)
- `build/classes/` - Java classes (if applicable)

## Critical Coupling

### High-Risk Dependencies
1. **Sub-LSB & Decimal Place Tracking**: LineParser → DifferenceAnalyzer
   - Changes to decimal detection affect comparison thresholds
2. **Level 2 Immutability**: DifferenceAnalyzer → Summary reporting
   - Cannot re-filter trivial differences in later stages
3. **Zero-Threshold Logic**: Significance threshold → Ignore TL filtering
   - Order matters; bypass precision floor only at Level 3

### Low-Risk Dependencies
- File I/O (FileComparator) relatively isolated
- Output formatting (can be changed safely)
- Test data (can be added without affecting core logic)

## Performance Characteristics

### Complexity
- **File Reading**: O(n × m) where n = lines, m = columns
- **Comparison**: O(1) per line (constant work per row)
- **Overall**: Linear in total cells

### Bottlenecks
- File I/O (disk access)
- Decimal place detection (string parsing)
- Summary output printing (for large difference tables)

### Optimization Opportunities
- Batch file reading (currently line-by-line)
- Parallel comparison (rows are independent)
- Early termination on critical threshold
