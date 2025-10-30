# Diff Level Option Documentation

## Overview

The `--diff-level` option allows you to control which diff tool is used for file comparison in the process drivers, overriding the default hierarchical behavior.

## Default Behavior (No --diff-level)

By default, the diff process uses a **hierarchical fallback system**:

1. **diff** (standard diff) - Fast, strict text comparison
   - If files match → **PASS**
   - If files differ → Try next level

2. **tldiff** (TL-aware diff) - Numerical comparison for TL data
   - If files match within threshold → **PASS**
   - If files differ → Try next level

3. **uband_diff** (Universal band diff) - Advanced numerical comparison
   - If files match within threshold → **PASS**
   - If files differ → **FAIL**

## Diff Level Options

### Level 0 (Default - Auto)
```bash
./process_nspe_in_files.sh diff std
# or explicitly:
./process_nspe_in_files.sh diff std --diff-level 0
```

**Behavior**: Hierarchical fallback (standard behavior described above)
- Stops at first PASS
- Tries all three levels if previous levels fail

**Use Case**: Normal operation - let the system find the appropriate comparison level

---

### Level 1 (diff only)
```bash
./process_nspe_in_files.sh diff std --diff-level 1
```

**Behavior**:
- Only runs standard `diff`
- No fallback to tldiff or uband_diff
- Files must match exactly to pass

**Use Case**:
- Strict validation where exact text match is required
- Testing for platform-dependent output differences
- Quick pre-screening before detailed analysis

**Example Output**:
```
Diffing file1.asc and file1.tl:
  (Level 1: diff only - no fallback)
diff done with return code 1
Difference found between file1.asc and file1.tl
diff FAILED
Stopping at level 1 (diff only)
```

---

### Level 2 (max tldiff)
```bash
./process_nspe_in_files.sh diff std --diff-level 2
```

**Behavior**:
- Runs `diff` first
- If diff fails, tries `tldiff`
- **Stops at tldiff** - does NOT try uband_diff even if tldiff fails

**Use Case**:
- When you want TL-aware comparison but don't want the full analysis from uband_diff
- Faster execution when uband_diff is not needed
- Testing tldiff thresholds specifically

**Example Output** (tldiff passes):
```
Diffing file1.asc and file1.tl:
  (Level 2: max tldiff - stop at tldiff)
diff done with return code 1
Difference found between file1.asc and file1.tl
diff FAILED
Trying tldiff...
tldiff OK
```

**Example Output** (tldiff fails):
```
Diffing file1.asc and file1.tl:
  (Level 2: max tldiff - stop at tldiff)
diff done with return code 1
Difference found between file1.asc and file1.tl
diff FAILED
Trying tldiff...
tldiff FAILED
Stopping at level 2 (max tldiff)
```

---

### Level 3 (force uband_diff)
```bash
./process_nspe_in_files.sh diff std --diff-level 3
```

**Behavior**:
- Runs `diff` first
- Always continues to `tldiff` even if diff passes
- Always continues to `uband_diff` even if tldiff passes
- Final result is based on uband_diff

**Use Case**:
- When you always want the full uband_diff analysis with RMSE, TL metrics, etc.
- Generating comprehensive comparison reports
- Benchmarking all diff tools
- Collecting statistics even when files match closely

**Example Output** (all pass):
```
Diffing file1.asc and file1.tl:
  (Level 3: force uband_diff - run all comparisons)
diff done with return code 0
diff OK
Continuing to level 3 (force uband_diff)...
Trying tldiff...
tldiff OK
Trying uband_diff...
[Full uband_diff output with RMSE, TL metrics, etc.]
uband_diff OK
```

## Usage Examples

### Example 1: Strict Text Comparison
```bash
# Only accept files that match exactly
./process_nspe_in_files.sh diff std --diff-level 1 --pattern "case1*"
```

### Example 2: TL-Aware Without Advanced Analysis
```bash
# Use tldiff but skip uband_diff for faster processing
./process_nspe_in_files.sh test std --diff-level 2
```

### Example 3: Force Full Analysis
```bash
# Always get comprehensive uband_diff output
./process_nspe_in_files.sh diff std --diff-level 3 --pattern "pe.std*"
```

### Example 4: Combined with Other Options
```bash
# Process specific files with level 3 diff and keep binary outputs
./process_nspe_in_files.sh test std \
    --diff-level 3 \
    --pattern "case*" \
    --exclude "case_old*" \
    --keep-bin
```

## Comparison Table

| Level | diff | tldiff | uband_diff | Stop Condition | Use Case |
|-------|------|--------|------------|----------------|----------|
| 0 (Auto) | ✓ | If diff fails | If tldiff fails | First PASS | Normal operation |
| 1 | ✓ | ✗ | ✗ | After diff | Strict validation |
| 2 | ✓ | ✓ | ✗ | After tldiff | TL-aware only |
| 3 | ✓ | ✓ | ✓ | After uband_diff | Full analysis |

## Technical Details

### Function Signature
```bash
diff_files <test_file> <reference_file> [threshold1] [threshold2] [diff_level]
```

### Implementation
The diff_level is passed as the 5th parameter to the `diff_files` function in `lib_diff_utils.sh`:

```bash
# In process_nspe_in_files.sh
diff_files "$test" "$ref" "" "" "$diff_level"
```

### Validation
The diff_level must be 0, 1, 2, or 3. Invalid values will cause the script to exit with an error:

```
Error: Invalid diff level '5'.
Valid diff levels are:
  0 - Auto (default hierarchical behavior)
  1 - diff only (no fallback to tldiff or uband_diff)
  2 - max tldiff (stop at tldiff, don't try uband_diff)
  3 - force uband_diff (always run all three diff tools)
```

## Summary Reports

When using diff levels, the summary report shows which files passed at which level:

### With --diff-level 1
```
Diff Details:
-------------
(Level 1 only - no advanced tools used)

Diff Results:
=============
Passed files: 5
Failed files: 3
```

### With --diff-level 3
```
Diff Details:
-------------
Files that passed with tldiff or uband_diff (simple diff failed): 2
Files that passed with uband_diff (tldiff failed): 1

Diff Results:
=============
Passed files: 8
Failed files: 0
```

## Notes

1. **Thresholds**: The threshold parameters (threshold1, threshold2) can still be passed to diff_files if needed, but this requires modifying the script to accept them as command-line arguments.

2. **Performance**:
   - Level 1 is fastest (single diff)
   - Level 2 is moderate (diff + tldiff)
   - Level 3 is slowest (all three tools)

3. **Compatibility**: The --diff-level option is available in both `test` and `diff` modes. It has no effect in `make` or `copy` modes.

4. **Default Behavior**: If --diff-level is not specified, the system defaults to level 0 (auto hierarchical behavior).

## Related Files

- **Driver Script**: `scripts/drivers/process_nspe_in_files.sh`
- **Library Function**: `scripts/drivers/lib_diff_utils.sh` (diff_files function)
- **Diff Tools**:
  - `diff` (system command)
  - `tldiff` (custom TL comparison tool)
  - `uband_diff` (universal numerical comparison tool)
