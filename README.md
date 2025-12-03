# diff_utils

**Numerical and text-based comparison utilities for NSPE and related acoustics data.**

> **Note:** For plotting and visualization tools, see the separate repository at `~/utils/nspe_python_plot_utils`.
> This repository focuses on numerical comparison, difference calculations, and text-based analysis only.

## programs - .f90 files

   | file                     | program | ext     | description                                                |
   | ------------------------ | ------- | ------- | ---------------------------------------------------------- |
   | [`cpddiff`](cpddiff.f90) | uBand   | .out    | calculate difference between two ducting probability files |
   | [`prsdiff`](prsdiff.f90) | NSPE    | .prs    | calculate difference between two .prs pressure files       |
   | [`tldiff`](tldiff.f90)   | NSPE    | _02.asc | calcuate the difference between 02.asc and rtl files       |
   | [`tsdiff`](tsdiff.f90)   | BBPP    | .ts     | calculate difference between two time series files         |

## scripts - .sh files

   | file                                      | description               |
   | ----------------------------------------- | ------------------------- |
   | [`install_packages`](install_packages.sh) | install dependencies      |
   | [`make_links`](make_links.sh)             | link executables to ~/bin |

# cpp

## Overview (C++ diff utilities)

This repository provides comparison tools for a family of acoustics / TL (Transmission Loss) related data products. The modernized C++ core implements a rich six-level difference classification hierarchy to distinguish between: raw vs rounded differences, trivially representational vs physically meaningful discrepancies, marginal operating-range warnings, and truly critical failures.

### Executable of interest

`uband_diff` (C++) — refactored comparison engine replacing / augmenting several earlier Fortran utilities. It performs:

1. Format & structural validation (column count & decimal precision tracking)
2. Multi-threshold numeric comparison
3. Hierarchical classification & flagging
4. Controlled table output (with cap + truncation notice)
5. Summary reporting with consistency (invariant) checks

### Implementation summary (short)
The C++ `uband_diff` engine now includes a precision-aware "sub‑LSB" detection fix: differences smaller than or equal to half the shared least-significant decimal (LSB/2) are treated as trivial (informationally equivalent) rather than failing comparisons. This improves cross-platform robustness (for example, `30.8` vs `30.85`), adds a small floating-point tolerance for edge cases, raises the decimal-place limit to 17, and includes 6 new unit tests. For full technical details, tests, and a file-level change list see `docs/IMPLEMENTATION_SUMMARY.md`.

### Six-Level Hierarchy (Conceptual)

Level 1: Non-zero vs zero raw differences
Level 2: Trivial vs non‑trivial (after rounding to shared minimum precision; half‑ULP rule)
Level 3: Insignificant vs significant (filter out numerically meaningless high TL values or diffs below user/precision threshold)
Level 4: Marginal (operational warning band) vs non‑marginal
Level 5: Critical vs non‑critical (exceeds hard threshold)
Level 6: Error vs non‑error (user threshold subdivision inside non‑marginal, non‑critical significant set)

#### Clarifying the Discrimination Flow

The stages are intentionally **short‑circuiting**:

* A difference classified as trivial at Level 2 never re-enters later semantic buckets. It is excluded from both `diff_significant` and `diff_insignificant` counts. This guarantees that formatting / representational artifacts (e.g. values differing only beyond the shared displayed precision) cannot be “rescued” or later promoted by threshold tuning.
* Level 3 decisions operate **only on the non‑trivial remainder**. Thus, any override or special behavior (like zero-threshold sensitivity) *starts after* trivial filtering has permanently removed display‑noise.
* “Insignificant” at Level 3 therefore means: physically or numerically unimportant given thresholds (high TL ignore region, or below effective significance floor) — **not** trivial. Trivial is a stricter, earlier exclusion.

Why this matters: without this separation, a user setting a very low (or zero) significant threshold could unintentionally reintroduce representational chatter. The design prevents that by making Level 2 an immutable filter.

##### Zero-Threshold vs Trivial Filtering

When `user_significant == 0.0`, the intent is *maximum sensitivity*: count every physically meaningful non‑trivial difference. The engine therefore bypasses the precision-derived floor (`dp_threshold`) **only for the Level 3 significant/insignificant split**. It does **not** revisit Level 2, and does **not** permit trivial differences to become significant. The semantic invariant introduced is:

```
diff_significant = diff_non_trivial - diff_high_ignore   (when user_significant == 0)
```

Where `diff_high_ignore` counts non‑trivial differences whose *both* TL values exceed the ignore threshold (physically meaningless region). Trivial differences (`diff_trivial`) are already excluded before this equation applies.

This yields a clean mental model:

1. Remove pure formatting noise (trivial) — irrevocable.
2. Among the rest, if in sensitive (zero) mode: count all except those physically screened by the high-ignore band.
3. Apply further partitioning (marginal, critical, error) on that significant core.

##### Semantic vs Structural Invariants

*Structural* (additive) invariants: ensure bookkeeping integrity (e.g. `non_trivial == insignificant + marginal + non_marginal`). They cannot detect a logic bug that mislabels many small, meaningful diffs as insignificant so long as the mislabeling is consistent.
*Semantic* invariants: encode meaning. Example: when in zero-threshold mode, no non-trivial difference below ignore should be demoted due to a formatting precision floor; equivalently, the equation above must hold. Failing this indicates erosion of the “maximum sensitivity” contract, even if additive sums still balance.

The test suite now includes semantic invariant tests to lock these expectations in, preventing silent regression that would otherwise pass pure additive checks.

### Threshold Definitions

| Symbol             | Meaning                                                                                                                       | Source                           |
| ------------------ | ----------------------------------------------------------------------------------------------------------------------------- | -------------------------------- |
| significant (user) | Lower bound for counting a meaningful difference. If set to 0.0, activates maximum sensitivity semantics (see below).         | CLI arg 3                        |
| critical (hard)    | Fails fast logically (flags) and halts table printing after first occurrence, but analysis continues for summary.             | CLI arg 4                        |
| print              | Minimum raw difference required to emit a row in the difference table (still counted internally).                             | CLI arg 5                        |
| marginal (110)     | Upper TL at which differences remain operationally relevant. Above this, still significant but tracked as marginal.           | Fixed (domain)                   |
| ignore (~138.47)   | Above this TL the values are numerically meaningless for physics → differences are classed insignificant (unless both below). | Fixed (single precision derived) |
| zero (≈1.19e‑7)    | Single precision epsilon used as a practical raw zero threshold.                                                              | Derived                          |

### Zero (0.0) User Significant Threshold Behavior

When the user supplies a significant threshold of exactly 0.0 the intent is “count every non‑trivial, physically meaningful difference.” However raw binary precision would otherwise impose a format (decimal place) lower bound (the dp-based `dp_threshold`). To avoid having formatting silence differences, the engine now applies:

If (user_significant == 0.0): treat every non‑trivial difference as significant unless BOTH values exceed the ignore threshold.

This yields: significant = non_trivial − (both values > ignore). It is an explicit semantic override of the precision floor. Previously, the precision floor (e.g. 0.1) incorrectly demoted many borderline 0.1 differences to “insignificant,” producing the wrong 29/10 split instead of 12/27.

### Table Output Control

* Rows suppressed if rounded difference == 0 (trivial display noise) unless debug.
* A configurable cap (`max_print_rows_`) prints a truncation notice once.
* After first critical difference, further rows are suppressed (analysis still runs).
* Summary message only claims “All significant differences are printed.” when no suppression mechanisms intervened.

### Consistency / Invariant Checks

The summary performs additive sanity checks (e.g. non_trivial == insignificant + marginal + non_marginal). These guarantee structural consistency but (by design) did not validate semantic expectations tied to zero-threshold sensitivity.

### New Unit Test (Sensitive Threshold)

`SensitiveThresholdTest.CanonicalZeroThresholdClassification` asserts the canonical dataset distribution:

* diff_non_trivial = 39
* diff_significant = 27
* diff_insignificant = 12
* diff_marginal = 24
* non-marginal, non-critical significant = 3
* diff_critical = 0

This locks in the corrected semantics for future refactors.

### Rationale For Additional (Proposed) Tests

1. Zero-Threshold Semantic Invariant Test
   - Assert: if user_significant == 0 then diff_significant == diff_non_trivial − diff_high_ignore.
   - Catches regression if future precision logic reintroduces dp-floor inflation.

2. High Ignore Band Isolation Test
   - Feed only TL values > ignore; assert diff_significant == 0 but diff_non_trivial > 0. Guards the separation between physical insignificance and formatting.

3. Mixed Band Distribution Test
   - Construct synthetic lines with values intentionally straddling marginal & ignore thresholds to assert marginal vs non-marginal partition counts.
   - Ensures no off-by-one inequality drift (>, >=) in threshold boundary logic.

4. Critical Suppression Behavior Test
   - Introduce one row exceeding critical threshold early, followed by additional would-be printable differences.
   - Assert: truncation notice present once, diff_significant counts include post-critical rows, diff_print stops at the triggering row.

5. Print Threshold Interaction Test
   - Use print > 0 with small user threshold to ensure internal counting (diff_significant) continues while table emission drops some rows.
   - Prevent silent coupling errors between diff_print and diff_significant.

6. Sensitive vs Non-Sensitive (Future Flag) A/B Test (if a --sensitive flag is added)
   - Compare runs with (user_significant=small_nonzero) vs (user_significant=0 & sensitive mode) to document and guard intended differential behavior.

Each proposed test targets a semantic dimension not covered by current additive invariants. Collectively they reduce the risk of subtle classification regressions that still “add up” numerically.

### Future Enhancement: Explicit Sensitive Mode

Consider adding an explicit `--sensitive` option rather than overloading threshold=0. This would:
* Make intent explicit in logs and summaries.
* Allow threshold=0 to retain literal meaning if semantics evolve.
* Provide an A/B test axis (see test 6 above).

---

Development / maintenance topics continue below.

## WSL 1

EACCES: permission denied, rename home .vscode-server extensions ms-vscode cpptools linux

* try add polling, fail
* try chmod, fail
* try restarting

## Make

run `Makefile:Configure`
this will add settings to `.vscode/settings.json`
a launch target still needs to be set
run `Makefile:Set launch`... and select target

## SonarLint for VS Code

### Compilation Database

1. you will get the error:
   SonarLint is unable to analyze C and C++ files(s) because there is no configured compilation database
   * click on the button `Configure compile commands`
      * this will make the file `.vscode/settings.json`
      * then error
   * Ctrl-Shift-P SonarLint Configure the compilation...
3. No compilation databases were found in the workspace
   * link-> How to generate compile commands
4. Generate a Compilation Database
   * create a file named .vscode/compile_commands.json
   * settings
   * Makefile Tools
      add the line
      `"makefile.compileCommandsPath": ".vscode/compile_commands.json"`
      to `.vscode/settings.json`

### Node JS

* download latest package from <https://nodejs.org/en/download/>
  * Linux Binaries (x64)

* untar with `sudo tar -C /usr/local --strip-components=1 -xJf node-v20.11.1-linux-x64.tar.xz`
* then set execution directory to `/usr/local/bin/node`

### Connected Mode

#### Sonar Cloud

Ctrl-Shift P
SonarLint: Connect to SonarCloud
Configure

#### SonarQube

* install sonarqube
* run sonar console
* open webpage
