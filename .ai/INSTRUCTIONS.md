# Instructions

**Purpose:** Procedures and standing orders for AI agents working on this project.

---

## Quick Start

1. Read this file for procedures
2. Read `CONTEXT.md` for project facts and decisions

---

## Topics

- C++ Engine: [cpp_engine/INSTRUCTIONS.md](cpp_engine/INSTRUCTIONS.md) and [cpp_engine/CONTEXT.md](cpp_engine/CONTEXT.md)

## Table of Contents

- [Topics](#topics)
- [Context Maintenance (Standing Order)](#context-maintenance-standing-order)
- [General Quality Standards](#general-quality-standards)
- [Critical Domain Knowledge](#critical-domain-knowledge)
- [Code Quality Standards](#code-quality-standards)
    - [Testing Requirements](#testing-requirements)
    - [C++ Conventions](#c-conventions)
    - [Fortran Conventions](#fortran-conventions)
    - [Documentation](#documentation)
    - [Scripting Best Practices](#scripting-best-practices)
    - [Changelog](#changelog)
- [Harvest Snippet](#harvest-snippet-for-chat-reuse)
- [Conflicts and Overrides](#conflicts-and-overrides)
- [Processing Order (multiple chats)](#processing-order-multiple-chats)

---

## Context Maintenance (Standing Order)

When the user provides substantial clarifying information, **integrate it into the appropriate `.ai/` file without being asked**.

### Where to put new information:

| Type of Information | Destination |
|---------------------|-------------|
| Project-wide decisions, facts, history | `CONTEXT.md` |
| Project-wide procedures, standing orders | `INSTRUCTIONS.md` |

### When to update:

**DO update when:**
- User provides ≥2-3 sentences of explanatory context
- User answers clarifying questions about the project
- User makes a decision that should persist across sessions
- User corrects a misconception (especially if AI-generated)

**DON'T update for:**
- Routine edits, minor corrections
- Conversational exchanges
- Information already documented

### If you cannot write to these files:

Summarize what should be added and ask the user to update the files manually.

---

## General Quality Standards

### Before Editing:
1. Verify you have sufficient context
2. Check terminology against `CONTEXT.md`
3. Use the author's preferred voice (see `CONTEXT.md`)

### After Editing:
1. Verify the edit didn't break anything (compilation, syntax, etc.)
2. Update `CONTEXT.md` if you made decisions that should persist
3. Check for errors introduced

### When Uncertain:
- Ask clarifying questions before making changes
- Document assumptions in `CONTEXT.md`
- Prefer minimal changes over extensive rewrites

---

## Critical Domain Knowledge

This section is maintained in the topic file for the C++ engine. See:
- Six-level hierarchy, sub-LSB detection, zero-threshold semantics
- Critical thresholds and Fabre’s method guidance
- Range data detection specifics

[cpp_engine/INSTRUCTIONS.md](cpp_engine/INSTRUCTIONS.md)

---

## Code Quality Standards

### Testing Requirements
- **Unit Tests**: Every numeric comparison logic change requires corresponding unit tests
- **Semantic Invariants**: Test that zero-threshold behavior maintains maximum sensitivity contract
- **Additive Invariants**: Ensure bookkeeping integrity (sum of categories equals total)
- **Cross-Precision Tests**: Validate handling of mixed decimal place comparisons
- Test location: `src/cpp/tests/`

### C++ Conventions
See engine conventions in [cpp_engine/INSTRUCTIONS.md](cpp_engine/INSTRUCTIONS.md).

### Fortran Conventions
- Programs in `src/fortran/programs/`
- Modules in `src/fortran/modules/`
- Style: Free-form Fortran (`.f90` extension)
- Use explicit interfaces for all procedures
- Require `IMPLICIT NONE`
- Use VS Code tasks for formatting: "Break and Normalize Fortran (Global)"

### Documentation
- Update corresponding `.md` file in `docs/` for any algorithmic changes
- Include mathematical notation where precision/thresholds matter
- Document why, not just what (especially for numerical edge cases)

**Standing order (cross-linking):**
- Keep long-form technical writeups in `docs/` (e.g., `docs/IMPLEMENTATION_SUMMARY.md`, `docs/SUB_LSB_DETECTION.md`).
- Add a concise, user-facing summary to the root `README.md` when a change materially affects usage or behavior (3–8 lines) and link to the detailed doc.
- Avoid duplicating full technical content in the root `README.md`; prefer a short summary + link.

### Scripting Best Practices

For batch processing drivers (`process_ram_in_files.sh`, `process_nspe_in_files.sh`):

**Input File Validation:**
- If logic depends on file content (markers, structure), always print a status message
- Example: "Columns 76-80: [blank] — overriding checks" or "Content marker found"
- This provides visibility into why files are accepted or skipped

**File Filtering:**
- When implementing override logic (e.g., columns 76–80), print the decision 
- Include relevant column/marker values in output
- Default to requiring markers unless explicitly overridden (fail-safe approach)

**Syntax Validation:**
- After substantial script modifications, run `bash -n` to check for syntax errors
- Test with sample input files before running on full batch
- Use `--dry-run` flag to preview changes before executing

### Changelog

Maintain a project changelog in the repository root: [CHANGELOG.md](CHANGELOG.md).

- Format: Follow "Keep a Changelog" style with sections `Unreleased` and versioned entries `X.Y.Z - YYYY-MM-DD`.
- Categories: Use `Added`, `Changed`, `Fixed`, `Removed`, `Deprecated`, `Security` as applicable.
- Inclusion criteria: Add entries for user-facing behavior changes, CLI flags/threshold updates, output format changes, and classification semantics adjustments. Skip trivial internal refactors.
- Versioning: Use Semantic Versioning. Bump:
    - MAJOR for breaking behavior changes (e.g., classification semantics or thresholds with incompatible impact)
    - MINOR for backward-compatible feature additions
    - PATCH for bug fixes
- Cross-links: Link to detailed docs (e.g., [docs/IMPLEMENTATION_SUMMARY.md](docs/IMPLEMENTATION_SUMMARY.md)) and reference PRs/commits when helpful.
- README tie-in: Keep README concise. Optionally highlight top 1–3 bullets from the latest release; defer full detail to the changelog and docs.
- Process: Update `Unreleased` on merge of qualifying changes; date and tag a release when publishing.

### Harvest Snippet (for chat reuse)

When a chat contains substantial decisions or facts, capture them without asking permission:

```
Harvest this chat: Read `.ai/CONTEXT.md` and `.ai/INSTRUCTIONS.md`, review this conversation, and update the files with any decisions, facts, or constraints discovered here. Don't ask permission — just update and show what changed.
```

Use chronologically (oldest → newest) if processing multiple chats; later decisions naturally override earlier ones. Skip trivial Q&A or abandoned explorations.

### Conflicts and Overrides

Topic-specific files may override project-wide guidance, but overrides must be explicit.

- If a topic file documents an exception (e.g., "This component uses X despite project-wide Y"), follow the topic file.
- If a conflict exists without an explicit override, ask the user which guidance applies before proceeding.
- When creating an intentional override, add a note explaining what is overridden and why.

### Processing Order (multiple chats)

When harvesting context from multiple chats or notes:

- Prefer chronological order: process oldest → newest so later decisions naturally override earlier ones. Mark earlier guidance as "Superseded" when helpful.
- If chronology is unclear, process by importance: prioritize critical decisions first, then capture remaining context. Document any overrides explicitly.
- Tag harvested chats (e.g., add a "[harvested]" note) to avoid reprocessing later.
- Skip trivial Q&A or abandoned explorations that produced no decisions.

---

## Build & Test Procedures

### Building

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
├── column_analyzer         # Utility
├── tl_metrics              # Utility
├── cpddiff                 # Fortran: Ducting probability
├── prsdiff                 # Fortran: Pressure files
├── tldiff                  # Fortran: Transmission loss
├── tsdiff                  # Fortran: Time series
└── tests/
    ├── test_difference_analyzer
    ├── test_semantic_invariants
    ├── test_sub_lsb_boundary
    └── (other unit tests)
```

### Running Unit Tests

**Prerequisite:** Google Test must be installed (`sudo apt-get install libgtest-dev`)

```bash
# Run all tests
make test

# Build tests without running
make tests

# Check if gtest is available
make printvars | grep GTEST_AVAILABLE
# Output should show: GTEST_AVAILABLE = yes

# Run specific test executable
./bin/tests/run_tests

# Run with filter (using gtest command-line flags)
./bin/tests/run_tests --gtest_filter="ZeroThreshold*"
./bin/tests/run_tests --gtest_filter="*SubLSB*"

# List all available tests
./bin/tests/run_tests --gtest_list_tests
```

**If Google Test is Not Installed:**

The makefile will show a clear error:
```
ERROR: Google Test is not installed.
Please install libgtest-dev to run tests:
  sudo apt-get install libgtest-dev
```

Regular builds (`make all`) will succeed—only test targets require gtest.

### Integration Testing

```bash
# Simple test
echo "1 30.8" > /tmp/ref.txt
echo "1 30.85" > /tmp/test.txt
./build/bin/uband_diff /tmp/ref.txt /tmp/test.txt 0 1 0
# Expected: Classified as EQUIVALENT (sub-LSB)

# Cross-precision test
echo "A 10.1 20.2" > /tmp/ref.txt
echo "A 10.15 20.25" > /tmp/test.txt
./build/bin/uband_diff /tmp/ref.txt /tmp/test.txt 0 1 0
# Expected: Both differences classified as sub-LSB (EQUIVALENT)

# Zero-threshold maximum sensitivity
./build/bin/uband_diff data/bbtl.pe02.ref.txt data/bbtl.pe02.test.txt 0 1 0
# Expected: Every non-trivial, non-ignored difference counted as significant
```

---

## Common Patterns

### Floating-Point Comparison (Critical)

```cpp
// ❌ WRONG - Direct equality
if (raw_diff == big_zero) { /* ... */ }

// ✅ CORRECT - With tolerance
const double FP_TOLERANCE = 1e-12;
if (raw_diff < big_zero ||
    std::abs(raw_diff - big_zero) < FP_TOLERANCE * std::max(raw_diff, big_zero)) {
    // Correct: handles both very small and normal-sized values
}
```

### Sub-LSB Detection Pattern

```cpp
// Calculate shared minimum decimal places
int shared_dp = std::min(decimal_places_col_a, decimal_places_col_b);

// Calculate LSB threshold
double lsb = std::pow(10.0, -shared_dp);
double big_zero = lsb / 2.0;

// Check if raw difference is sub-LSB
const double FP_TOLERANCE = 1e-12;
bool sub_lsb_diff = (raw_diff < big_zero) ||
                   (std::abs(raw_diff - big_zero) <
                    FP_TOLERANCE * std::max(raw_diff, big_zero));

// Classify
bool trivial_after_rounding = (rounded_diff == 0.0) || sub_lsb_diff;
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

// Verification (invariant in zero-threshold mode)
if (user_significant == 0.0) {
    int expected = diff_non_trivial - diff_high_ignore;
    int actual = diff_significant;
    assert(actual == expected);  // If fails: zero-threshold contract broken
}
```

### Test Pattern

```cpp
#include <gtest/gtest.h>

TEST(ComponentName, TestName) {
    // Arrange
    DifferenceAnalyzer analyzer;
    analyzer.set_user_significant(0.0);  // Zero-threshold mode

    // Act
    int classification = analyzer.classify_difference(10.0, 10.05);

    // Assert
    ASSERT_EQ(classification, TRIVIAL);
}
```

---

## Common Modifications

### Adding a New Threshold
1. Add constant to relevant header (e.g., `difference_analyzer.h`)
2. Document in `docs/` with physical/domain justification
3. Update `--help` text in main program
4. Add unit tests for threshold boundary conditions
5. Update semantic invariant tests if applicable

### Fixing Numeric Comparison Issues
1. Check if sub-LSB detection applies
2. Verify floating-point tolerance is applied (FP_TOLERANCE = 1e-12)
3. Test with cross-precision pairs (1dp vs 2dp, etc.)
4. Test zero-threshold mode explicitly
5. Add test cases to regression suite

### Refactoring Fortran Code
1. Use VS Code tasks: "Break and Normalize Fortran (Global)" or "Break Fortran Lines Only (Global)"
2. Verify compilation passes after formatting
3. Run tests to confirm no behavior change

---

## Debugging Procedures

### Compilation Errors

```bash
make clean && make 2>&1 | head -50
# Common issues: Missing #include, undefined reference, C++ version mismatch
```

### Runtime Errors

```bash
# Simplify test file first
echo "1 10.0" > /tmp/simple.txt
echo "1 10.01" > /tmp/simple_test.txt
./build/bin/uband_diff /tmp/simple.txt /tmp/simple_test.txt 0 1 0

# GDB if needed
gdb --args ./build/bin/uband_diff /tmp/simple.txt /tmp/simple_test.txt 0 1 0
```

### Test Failures

```bash
./build/bin/tests/test_difference_analyzer
# Or under GDB
gdb ./build/bin/tests/test_difference_analyzer
(gdb) run
(gdb) bt  # backtrace if crash
```

### Emergency Reset

```bash
cd /home/jlighthall/utils/diff_utils
rm -rf build/
make clean
make all -j4
cd build && make test

# Nuclear option
git clean -fdx
git checkout .
make all -j4
```

---

## File Navigation

### Core Source Files

| File | Purpose |
|------|---------|
| `src/cpp/src/difference_analyzer.cpp` | Core comparison logic |
| `src/cpp/src/line_parser.cpp` | Decimal place tracking |
| `src/cpp/src/file_comparator.cpp` | File I/O, validation |
| `src/cpp/tests/test_semantic_invariants.cpp` | Invariant tests |

### Headers

| File | Contains |
|------|----------|
| `src/cpp/include/difference_analyzer.h` | DifferenceAnalyzer class |
| `src/cpp/include/file_comparator.h` | FileComparator methods |
| `src/cpp/include/line_parser.h` | LineParser methods |

### Documentation

| File | Use When |
|------|----------|
| `docs/report/UBAND_DIFF_MEMO.tex` | Primary paper (complete) |
| `docs/future-work.md` | Experimental diagnostics, research roadmap |

---

## Tool Configuration

### VS Code Settings

```json
{
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/src/cpp/include",
        "${workspaceFolder}/src/cpp/src"
    ],
    "[fortran]": {
        "editor.rulers": [80]
    },
    "fortran.linter.maxLineLength": 80,
    "fortran.includePaths": ["${workspaceFolder}/build/mod"]
}
```

### GitHub Copilot

Copy this file's content to `.github/copilot-instructions.md` if needed.

---

## Common Mistakes to Avoid

### ❌ Mistake 1: Using `==` for float comparison

```cpp
if (raw_diff == big_zero) { }  // WRONG
```

✅ **Fix:** Use tolerance

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

✅ **Fix:** Level 2 is immutable

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

✅ **Fix:** Explicit zero-threshold handling

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

✅ **Fix:** Include cross-precision tests

```cpp
TEST(SubLSB, CrossPrecision) {
    analyzer.analyze(30.8, 30.85);   // 1dp vs 2dp - should be TRIVIAL
    analyzer.analyze(50.12, 50.123); // 2dp vs 3dp - should be TRIVIAL
}
```

### ❌ Mistake 5: Single-column test files

```cpp
// WRONG: Single column gets treated as column 0 (range) and skipped
write_file(f1, "100.5");
write_file(f2, "100.0");
// No comparison happens! Column 0 is always skipped as range column
```

✅ **Fix:** Include range column so data becomes column 1

```cpp
// CORRECT: Range column + data column
write_file(f1, "101.5 100.5");  // Column 0 (range) skipped, column 1 compared
write_file(f2, "101.5 100.0");
```

### ❌ Mistake 6: FileComparator constructor parameter order

```cpp
// WRONG: Missing debug_level parameter
FileComparator comp(0.0, 10.0, 1.0, 0,
                    /*significant_is_percent*/ true,
                    /*significant_percent*/ 0.01);
// "true" becomes debug_level, 0.01 becomes significant_is_percent (truncated to 0)!
```

✅ **Fix:** Include all positional parameters

```cpp
// CORRECT: Include both verbosity and debug_level
FileComparator comp(/*user_thresh*/ 0.0, /*critical*/ 10.0, /*print*/ 1.0,
                    /*verbosity*/ 0, /*debug*/ 0,
                    /*significant_is_percent*/ true,
                    /*significant_percent*/ 0.01);
```

---

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| `undefined reference to ...` | Missing object file | Run `make clean && make -j4` |
| Fortran compilation fails | Format issues | Use "Break and Normalize Fortran" task |
| Tests fail after pull | Dependency mismatch | `make clean && make -j4` |
| Output differs on Windows vs Linux | FP precision | Check sub-LSB tolerance is applied |
| New test case fails | Logic regression | Review diff since last passing build |
| `cannot open module file...` | Missing Fortran module path | Add `build/mod` to include paths |

---

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

---

*Version history is tracked by git, not by timestamps in this file.*
