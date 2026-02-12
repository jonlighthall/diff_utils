# Future Work and Research Extensions

**Status:** All items listed below are EXPERIMENTAL and distinct from production-ready functionality. These are research directions that may inform future papers, standalone analysis tools, or extensions. Each requires independent validation before integration into tl_diff.

---

## PREREQUISITE: Critical Paradigm Investigation

**Issue:** Fabre's paper (Figures 2--6) shows curves with similar gross structure but apparent range-offset differences (possible accumulated phase error). **Pending determination:** Does Fabre's method score these as high (tactical equivalence) or low (different computation)?

**Action Required:**
1. Read Fabre et al. (2009) carefully to determine optimization paradigm
2. Distinguish: tactical equivalence (sufficient for decision-making) vs. theoretical equivalence (numerical/phase error analysis)
3. Document findings in context files
4. This determination affects all subsequent phase/stretch research

**Implication:** If Fabre optimizes for tactical equivalence, phase error compensation may be unnecessary. If theoretical equivalence, phase-aware comparison becomes critical.

---

## Phase Error and Horizontal Stretch Detection (RESEARCH AREA)

**Motivation:** Observations from Fabre Figures 2--6 suggest range-offset differences that may represent accumulated phase errors. Author has developed horizontal stretch and compare algorithm in separate MATLAB script.

**Proposed Research:**
1. Quantify phase error signatures in TL curve pairs
2. Develop horizontal stretch detection metrics
3. Compare against Fabre's method to understand relationship
4. Determine if phase compensation improves or complicates comparison

**Example:** An oscillatory TL curve stretched by 10% will fail dramatically under RMSE difference metrics, even though:
- The curves "look the same" visually
- They may be tactically equivalent for decision-making purposes
- The shape and character are preserved

**Why it Matters - Two Comparison Paradigms:**
1. **Tactical equivalence**: Are these curves similar enough for operational decision-making?
2. **Implementation verification**: Is the model producing exactly the same output?

These are fundamentally different questions. Fabre's method optimizes for one; the other may require phase-aware comparison.

**Status:** Experimental; requires paradigm determination as prerequisite

---

## Statistical Pattern Analysis (DIAGNOSTIC ONLY)

**Current State:** The tool includes experimental pattern analysis:
- Run tests (randomness of residuals)
- Autocorrelation analysis (error accumulation patterns)
- Linear regression (systematic drift detection)

**Status:** Weak technique; marked experimental; useful for investigation but not authoritative for pass/fail decisions

**Why Not Production-Ready:**
- Not peer-reviewed or published
- Not validated against known standards
- May give misleading results without careful interpretation
- Need more rigorous theoretical foundation

**Future Work:**
- Develop peer-reviewable methodology
- Validate against known test cases
- Establish clear interpretation guidelines
- Consider publishing as standalone contribution

---

## Extended Uncertainty Quantification

**Current State:** Thresholds (MARGINAL_TL=110 dB, IGNORE_TL=138.47 dB) are hard boundaries, not probabilistic uncertainty bounds.

**Proposed Enhancement:** Incorporate actual uncertainty quantification:
- Propagate input uncertainties through models
- Compare curves with confidence bands
- Determine if differences are significant relative to uncertainty

**Challenges:**
- Distinguish hard thresholds (deterministic) from uncertainty intervals (probabilistic)
- Establish domain-appropriate uncertainty models
- Validate against measurements

**Status:** Research only; distinct from current hard-threshold approach

---

## Multi-Resolution Analysis Framework

**Concept:** Analyze differences at multiple frequency/resolution scales:
- Coarse scale: Overall energy/shape
- Medium scale: Major features (convergence zones, etc.)
- Fine scale: Oscillation patterns

**Benefit:** Different scales matter for different purposes (tactical vs verification)

**Status:** Not yet scoped; potential future direction

---

## Extended Domain Support

**Current:** Acoustic propagation models (RAM, NSPE, uBand)

**Potential Extensions:** CFD (Computational Fluid Dynamics), climate models, quantum chemistry

**Prerequisite:** Validate that hierarchical architecture generalizes beyond acoustic domain; establish domain-specific thresholds with peer-reviewed standards

---

## Implementation Backlog (Lower Priority)

These are concrete features that have been considered but not yet implemented:

### Consecutive Differences Counter
**Concept:** Count maximum consecutive differences in a file comparison and report as a metric.
**Purpose:** Detect clustering of errors vs. random distribution.
**Status:** Not implemented; lower priority than core metrics.

### Percentage Error Output
**Concept:** When non-zero, non-trivial error is detected, print maximum percentage error and add to diff table.
**Status:** Not implemented; would enhance summary output.

### Test Mode Enhancements
**Concept:** In test mode:
- Include `-s` flag with diff command so identical files are noted in log
- Save hostname and present working directory to each log file
**Status:** Not implemented; convenience feature for CI/CD integration.

---

## Publishing Strategy

### Current Paper (Production-Ready)
- Sub-LSB detection (implemented, validated)
- Six-level hierarchy with early-exit logic (original organizing principle)
- IEEE weighted TL method via Fabre (peer-reviewed, authoritative)
- Accounting invariants (rigorous validation)

### Follow-On Papers (Research Extensions)
- Phase-aware TL comparison methods (requires paradigm investigation)
- Horizontal curve matching / stretch compensation
- Validated statistical pattern analysis

---

## General Rule

All research extensions must clearly distinguish from production-ready components. When documenting:
- Mark status as EXPERIMENTAL
- Explain validation requirements
- Document assumptions being tested
- Never deprecate peer-reviewed methods like Fabre's algorithm
