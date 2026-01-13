# Common Coding Patterns & Examples

## C++ Patterns

### Floating-Point Comparison (Critical for this project)

#### ❌ WRONG - Direct equality
```cpp
if (raw_diff == big_zero) {
    // WRONG: FP representation may differ slightly
}
```

#### ✅ CORRECT - With tolerance
```cpp
const double FP_TOLERANCE = 1e-12;

// Basic tolerance check
if (std::abs(raw_diff - big_zero) < FP_TOLERANCE) {
    // Correct but simple; doesn't scale well
}

// Relative tolerance (better for varying magnitudes)
if (std::abs(raw_diff - big_zero) < FP_TOLERANCE * std::max(raw_diff, big_zero)) {
    // Correct: scales with magnitude
}

// Combined absolute + relative (most robust)
if (raw_diff < big_zero ||
    std::abs(raw_diff - big_zero) < FP_TOLERANCE * std::max(raw_diff, big_zero)) {
    // Handles both very small and normal-sized values
}
```

### Sub-LSB Detection Pattern

```cpp
// Step 1: Calculate shared minimum decimal places
int shared_dp = std::min(decimal_places_col_a, decimal_places_col_b);

// Step 2: Calculate LSB threshold
double lsb = std::pow(10.0, -shared_dp);
double big_zero = lsb / 2.0;

// Step 3: Check if raw difference is sub-LSB
const double FP_TOLERANCE = 1e-12;
bool sub_lsb_diff = (raw_diff < big_zero) ||
                   (std::abs(raw_diff - big_zero) <
                    FP_TOLERANCE * std::max(raw_diff, big_zero));

// Step 4: Classify
bool trivial_after_rounding = (rounded_diff == 0.0) || sub_lsb_diff;
```

**Real Example** (from `difference_analyzer.cpp`):
```cpp
double calculate_lsb_threshold(int shared_decimal_places) {
    return std::pow(10.0, -shared_decimal_places) / 2.0;
}

bool is_sub_lsb(double raw_diff, double big_zero) {
    constexpr double FP_TOLERANCE = 1e-12;
    return (raw_diff < big_zero) ||
           (std::abs(raw_diff - big_zero) <
            FP_TOLERANCE * std::max(raw_diff, big_zero));
}
```

### Zero-Threshold Logic Pattern

```cpp
// Normal mode: Use significance threshold
if (user_significant > 0.0) {
    is_significant = std::abs(diff) >= user_significant;
}
// Zero-threshold mode: Maximum sensitivity
else if (user_significant == 0.0) {
    // Count all non-trivial except those in ignore region
    is_significant = !is_trivial && !both_tl_values_in_ignore_region;
}
```

**Verification Pattern**:
```cpp
// After classification, verify invariant in zero-threshold mode
if (user_significant == 0.0) {
    // Check: sig = nontrivial - high_ignore
    int expected = diff_non_trivial - diff_high_ignore;
    int actual = diff_significant;

    assert(actual == expected);
    // If fails: zero-threshold contract is broken
}
```

### Error Handling Pattern

```cpp
// File validation with clear error messages
if (columns_ref != columns_test) {
    throw std::runtime_error(
        "Column mismatch: reference has " + std::to_string(columns_ref) +
        ", test has " + std::to_string(columns_test)
    );
}

// Return error codes (for non-throwing contexts)
enum class Status {
    OK = 0,
    FILE_NOT_FOUND = 1,
    FORMAT_ERROR = 2,
    COLUMN_MISMATCH = 3,
    PARSE_ERROR = 4
};
```

### Vector Operations Pattern

```cpp
#include <vector>
#include <algorithm>

// Reading lines
std::vector<std::string> lines;
std::string line;
while (std::getline(file, line)) {
    lines.push_back(line);
}

// Processing with indices
for (size_t i = 0; i < lines.size(); ++i) {
    // Process lines[i]
}

// Filtering
auto significant_diffs = std::vector<Difference>();
std::copy_if(diffs.begin(), diffs.end(),
             std::back_inserter(significant_diffs),
             [](const Difference& d) { return d.is_significant; });
```

### Test Pattern (Unit Tests)

```cpp
#include <gtest/gtest.h>

// Basic test structure
TEST(ComponentName, TestName) {
    // Arrange: Setup
    DifferenceAnalyzer analyzer;
    analyzer.set_user_significant(0.0);  // Zero-threshold mode

    // Act: Execute
    int classification = analyzer.classify_difference(10.0, 10.05);

    // Assert: Verify
    ASSERT_EQ(classification, TRIVIAL);
}

// Parameterized test (for multiple similar cases)
class SubLSBTests : public ::testing::TestWithParam<std::tuple<double, double, bool>> {};

TEST_P(SubLSBTests, Classify) {
    auto [val1, val2, expected_trivial] = GetParam();
    DifferenceAnalyzer analyzer;
    int result = analyzer.classify_difference(val1, val2);

    ASSERT_EQ(result == TRIVIAL, expected_trivial);
}

INSTANTIATE_TEST_SUITE_P(SubLSBEdgeCases, SubLSBTests,
    ::testing::Values(
        std::make_tuple(30.8, 30.85, true),    // Should be trivial
        std::make_tuple(10.0, 12.0, false),    // Should not be trivial
        std::make_tuple(100.123456, 100.123457, true)  // Cross-precision
    )
);
```

### Struct/Class Pattern

```cpp
// Data-centric (lightweight, no methods)
struct LineData {
    std::string id;
    std::vector<double> values;
    std::vector<int> decimal_places;
    int column_count;
};

// Analyzer (methods + state)
class DifferenceAnalyzer {
public:
    // Main entry point
    int classify_difference(double val1, double val2);

    // Configuration
    void set_user_significant(double threshold);
    void set_critical_threshold(double threshold);

    // Results
    int get_count(DiffLevel level) const;

private:
    // Thresholds
    double user_significant_ = 0.0;
    double critical_threshold_ = 999.0;

    // Statistics
    int diff_trivial_ = 0;
    int diff_significant_ = 0;
    int diff_critical_ = 0;

    // Implementation helpers
    bool is_sub_lsb_(double raw, double lsb_half) const;
    bool is_ignored_region_(double tl_val) const;
};
```

## Fortran Patterns

### Module Structure

```fortran
MODULE comparison_module
    IMPLICIT NONE
    PRIVATE

    ! Public procedures
    PUBLIC :: classify_difference, apply_threshold

    ! Module constants
    INTEGER, PARAMETER :: MAX_DECIMAL_PLACES = 17
    REAL, PARAMETER :: MARGINAL_TL = 110.0
    REAL, PARAMETER :: IGNORE_TL = 138.47

CONTAINS

    SUBROUTINE classify_difference(val1, val2, classification)
        REAL, INTENT(IN) :: val1, val2
        INTEGER, INTENT(OUT) :: classification

        ! Implementation
    END SUBROUTINE classify_difference

END MODULE comparison_module
```

### File I/O Pattern

```fortran
PROGRAM read_and_compare
    USE comparison_module, ONLY: classify_difference
    IMPLICIT NONE

    CHARACTER(LEN=256) :: ref_file, test_file
    INTEGER :: ref_unit, test_unit, io_status

    ! Open files
    OPEN(NEWUNIT=ref_unit, FILE=ref_file, STATUS='OLD', &
         ACTION='READ', IOSTAT=io_status)
    IF (io_status /= 0) THEN
        WRITE(*,*) 'Error opening reference file'
        STOP
    END IF

    ! Read and process
    ! ...

    ! Close files
    CLOSE(ref_unit)
    CLOSE(test_unit)

END PROGRAM read_and_compare
```

### Floating-Point Comparison

```fortran
REAL, PARAMETER :: FP_TOLERANCE = 1.0E-12

FUNCTION is_approximately_equal(a, b, tol) RESULT(equal)
    REAL, INTENT(IN) :: a, b, tol
    LOGICAL :: equal

    equal = ABS(a - b) < tol * MAX(ABS(a), ABS(b))
END FUNCTION is_approximately_equal
```

## Python Patterns (Scripts)

### Data Generation Pattern

```python
import math

def generate_precision_test_data(base_value, precision_levels):
    """Generate test values at different decimal precisions."""
    test_values = []

    for precision in precision_levels:
        # Round to specified decimal places
        rounded = round(base_value, precision)
        test_values.append({
            'value': rounded,
            'decimal_places': precision,
            'precision_string': f"{rounded:.{precision}f}"
        })

    return test_values

# Usage
data = generate_precision_test_data(30.85, [1, 2, 3])
# Returns values: [30.8, 30.85, 30.850] with metadata
```

### File Writing Pattern

```python
import csv

def write_comparison_file(filename, id_col, values):
    """Write data in whitespace-delimited format."""
    with open(filename, 'w') as f:
        for id_val, value_list in zip(id_col, values):
            line = f"{id_val} " + " ".join(f"{v:.10f}" for v in value_list)
            f.write(line + '\n')
```

## Build System Patterns

### Makefile Dependency Pattern

```makefile
# Source files
SRCS = src/main.cpp src/analyzer.cpp src/parser.cpp
HDRS = include/analyzer.h include/parser.h

# Object files (auto-generate from SRCS)
OBJS = $(SRCS:src/%.cpp=obj/%.o)
DEPS = $(SRCS:src/%.cpp=deps/%.d)

# Main target
bin/uband_diff: $(OBJS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Automatic dependency generation
deps/%.d: src/%.cpp
	$(CXX) $(DEPFLAGS) $< -o $@

# Include dependency files
-include $(DEPS)

# Compilation rule
obj/%.o: src/%.cpp | deps/%.d
	$(CXX) $(CXXFLAGS) $(UFLAGS) -c $< -o $@

# Directory creation
$(OBJS) $(DEPS): | obj/ deps/

obj/ deps/ bin/:
	mkdir -p $@
```

### Fortran Compilation Pattern

```makefile
# Fortran programs with modules
MODS = src/modules/constants.f90 src/modules/io.f90
PROGS = src/programs/cpddiff.f90 src/programs/tldiff.f90

OBJS = $(MODS:.f90=.o) $(PROGS:.f90=.o)
EXES = $(patsubst src/programs/%.f90,bin/%,$(PROGS))

# Fortran module dependency (must compile modules first)
src/programs/%.o: $(MODS:.f90=.o)
	$(FC) $(FFLAGS) -c $< -o $@

# Fortran executable needs object files
bin/%: src/programs/%.o
	$(FC) $(LDFLAGS) $^ -o $@
```

## Testing Patterns

### Test Data Setup

```cpp
// Create temporary test file
class SubLSBTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Create reference file
        ref_file = "/tmp/test_ref.txt";
        std::ofstream ref(ref_file);
        ref << "ID1 30.8 100.0\n";
        ref << "ID2 50.5 150.0\n";
        ref.close();

        // Create test file
        test_file = "/tmp/test_test.txt";
        std::ofstream test(test_file);
        test << "ID1 30.85 100.0\n";  // sub-LSB difference
        test << "ID2 50.6 150.0\n";   // 0.1 difference
        test.close();
    }

    void TearDown() override {
        std::remove(ref_file.c_str());
        std::remove(test_file.c_str());
    }

    std::string ref_file, test_file;
};

TEST_F(SubLSBTestFixture, ClassifySubLSBDifference) {
    FileComparator comparator;
    auto results = comparator.compare(ref_file, test_file);

    // First line should be trivial (30.8 vs 30.85)
    ASSERT_EQ(results[0].classification, TRIVIAL);

    // Second line should be significant (50.5 vs 50.6)
    ASSERT_EQ(results[1].classification, SIGNIFICANT);
}
```

### Invariant Testing Pattern

```cpp
class InvariantTests : public ::testing::Test {
protected:
    void check_additive_invariant(const DifferenceAnalyzer& analyzer) {
        // Verify: total = trivial + non_trivial
        int total = analyzer.get_total_lines();
        int trivial = analyzer.get_count(DiffLevel::TRIVIAL);
        int non_trivial = total - trivial;

        int marginal = analyzer.get_count(DiffLevel::MARGINAL);
        int critical = analyzer.get_count(DiffLevel::CRITICAL);
        int error = analyzer.get_count(DiffLevel::ERROR);

        ASSERT_EQ(non_trivial, marginal + critical + error);
        SUCCEED() << "Additive invariant holds";
    }

    void check_zero_threshold_semantics(const DifferenceAnalyzer& analyzer) {
        // Verify: sig = nontrivial - high_ignore (only in zero-threshold mode)
        int expected = analyzer.get_non_trivial_count() -
                      analyzer.get_high_ignore_count();
        int actual = analyzer.get_significant_count();

        ASSERT_EQ(actual, expected)
            << "Zero-threshold maximum sensitivity contract violated";
    }
};

TEST_F(InvariantTests, AllInvariantsAfterComparison) {
    // Run comparison
    analyzer.compare_files(ref_file, test_file);

    // Check both invariants
    check_additive_invariant(analyzer);
    check_zero_threshold_semantics(analyzer);
}
```

## Documentation Patterns

### Code Comment Pattern

```cpp
// ❌ BAD: Describes what, not why
bool is_trivial = (raw_diff <= big_zero);

// ✅ GOOD: Explains domain reasoning
// A difference is trivial if it's within one half-LSB (Least Significant Bit),
// which represents the effective precision of the shared minimum decimal places.
// This prevents formatting artifacts (e.g., 30.8 vs 30.85) from being counted
// as meaningful physical differences in acoustic calculations.
// See docs/guide/sub-lsb-detection.md for detailed explanation.
bool is_trivial = (raw_diff <= big_zero);
```

### Function Documentation Pattern

```cpp
/// Classifies a single numeric difference across the six-level hierarchy.
///
/// @param val1 Reference value
/// @param val2 Test value
/// @param shared_decimal_places Common minimum decimal places (affects LSB threshold)
/// @param user_significant User-provided significance threshold (0.0 = maximum sensitivity)
/// @return Classification level (1-6)
///
/// @details
/// The classification follows the six-level hierarchy:
/// 1. Raw difference non-zero check
/// 2. Sub-LSB trivial filtering (IMMUTABLE - never revisited)
/// 3. Significance threshold application
/// 4. Marginal band (110 dB) application
/// 5. Critical threshold check
/// 6. Error classification
///
/// CRITICAL: Level 2 trivial filtering is immutable. Differences classified
/// as trivial are permanently excluded and cannot be promoted by threshold
/// tuning. This guarantees formatting artifacts do not corrupt results.
///
/// @see SUB_LSB_DETECTION.md, DISCRIMINATION_HIERARCHY.md
int classify_difference(double val1, double val2, int shared_decimal_places,
                       double user_significant);
```

## Common Mistakes to Avoid

### ❌ Mistake 1: Using `==` for float comparison
```cpp
if (raw_diff == big_zero) { }  // WRONG
```
✅ Fix: Use tolerance
```cpp
if (std::abs(raw_diff - big_zero) < FP_TOLERANCE * std::max(raw_diff, big_zero)) { }
```

### ❌ Mistake 2: Revisiting Level 2 trivial filtering
```cpp
// Level 2: mark as trivial
if (is_sub_lsb) classifier.mark_trivial();

// Later (WRONG): trying to upgrade trivial to significant
if (some_threshold_condition) classifier.mark_significant();  // NO!
```
✅ Fix: Level 2 is immutable
```cpp
if (!is_trivial && satisfies_significance_threshold()) classifier.mark_significant();
```

### ❌ Mistake 3: Forgetting zero-threshold special case
```cpp
// Normal mode logic
if (std::abs(diff) >= user_significant) {
    count_as_significant();
}
// Oops: doesn't handle user_significant == 0.0 correctly
```
✅ Fix: Explicit zero-threshold handling
```cpp
if (user_significant > 0.0) {
    if (std::abs(diff) >= user_significant) count_as_significant();
} else { // user_significant == 0.0
    if (!is_trivial && !both_in_ignore_region()) count_as_significant();
}
```

### ❌ Mistake 4: Not testing cross-precision pairs
```cpp
// Only tests same decimal places
TEST(Differences, AllSameDP) {
    analyzer.analyze(10.5, 10.6);  // Both 1dp
}
// Misses sub-LSB handling across different precisions
```
✅ Fix: Include cross-precision tests
```cpp
TEST(SubLSB, CrossPrecision) {
    analyzer.analyze(30.8, 30.85);   // 1dp vs 2dp - should be TRIVIAL
    analyzer.analyze(50.12, 50.123); // 2dp vs 3dp - should be TRIVIAL
}
```
