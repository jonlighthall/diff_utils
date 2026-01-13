# AI Context Framework for diff_utils

**Created:** January 2026
**Purpose:** Enable efficient AI-assisted development while maintaining scientific rigor through peer-reviewed methods

## ⚠️ Critical Principles

**Novel Contribution - Hierarchical Discrimination:** The six-level hierarchy is the author's original organizing principle, not a taxonomy imposed on existing methods. It implements early-exit logic where the program continues through subsequent levels only upon match or critical failure. This is the core innovation.

**Authoritative Method - Fabre's TL Comparison:** Any quantitative TL curve comparison MUST use Fabre et al.'s peer-reviewed algorithm \cite{fabre2009}. This tool enables that algorithm by filtering formatting artifacts.

**Critical Paradigm Question (PENDING):** Fabre's paper requires careful reading to determine whether the method optimizes for tactical equivalence (sufficient for decision-making) or theoretical/computational equivalence (numerical/phase error analysis). Observed in Fabre Figures 2--6: curves with similar structure but apparent range-offset differences (possible accumulated phase error) that may score high or low depending on Fabre's optimization paradigm.

**For AI agents and developers**:
- Do not replace, remove, or deprecate Fabre's method
- If new algorithms emerge during development, distinguish them clearly as novel contributions
- Validate new methods independently before integration
- Document research ideas in `docs/future-work.md`

## What's Included

This `.ai/` directory contains a complete knowledge base for AI agents assisting with diff_utils development. Production-ready methods are peer-reviewed (Fabre's TL difference algorithm), while diagnostics and research ideas are marked experimental and captured in FUTURE_WORK.md.

### 📋 Files Overview

1. **[instructions.md](instructions.md)** — START HERE for first-time AI agents
   - Project overview and critical domain knowledge
   - The six-level difference hierarchy (THE core concept)
   - Sub-LSB detection and zero-threshold semantics
   - Code quality standards and common patterns
   - When to ask for clarification

2. **[codebase.md](codebase.md)** — Understand the architecture
   - Complete directory structure with descriptions
   - Data flow diagram (input → output)
   - Component responsibilities (DifferenceAnalyzer, FileComparator, etc.)
   - Precision handling mechanics
   - Critical coupling and dependencies

3. **[dev-workflow.md](dev-workflow.md)** — Execute changes correctly
   - Environment setup and prerequisites
   - Complete build process (`make all`, parallel builds)
   - Testing strategy (semantic invariants, unit tests, integration tests)
   - Development workflow (test-first, verify, document)
   - Debugging procedures and common issues
   - Code review checklist

4. **[patterns.md](patterns.md)** — Copy-paste working examples
   - C++ patterns (floating-point comparison, sub-LSB detection, zero-threshold logic)
   - Fortran patterns (module structure, file I/O)
   - Python patterns (data generation)
   - Test patterns (invariant testing, parameterized tests)
   - Common mistakes and how to avoid them

5. **[quick-reference.md](quick-reference.md)** — Fast lookup
   - File navigation cheat sheet
   - Command quick reference
   - Tool configuration (VS Code, Aider, GitHub Copilot, Cursor)
   - File-specific quick fixes
   - Emergency reset procedures

6. **[FUTURE_WORK.md](../docs/future-work.md)** — Research and roadmap (phase-aware TL comparison, experimental diagnostics, uncertainty, multi-resolution)

## Using This Framework

### For First-Time AI Agents

1. Read [instructions.md](instructions.md) **completely** (15 min)
2. Skim [codebase.md](codebase.md) architecture section (10 min)
3. Refer to [patterns.md](patterns.md) for code examples
4. Use [dev-workflow.md](dev-workflow.md) when building/testing
5. Bookmark [quick-reference.md](quick-reference.md) for quick lookups

### For Specific Tasks

| Task | Reference |
|------|-----------|
| Implement numeric comparison fix | [instructions.md](instructions.md#fixing-numeric-comparison-issues) → [patterns.md](patterns.md#sub-lsb-detection-pattern) |
| Add new threshold | [instructions.md](instructions.md#adding-a-new-threshold) → [dev-workflow.md](dev-workflow.md#adding-a-new-comparison-metric) |
| Debug failing test | [dev-workflow.md](dev-workflow.md#debugging-workflow) → [quick-reference.md](quick-reference.md#file-specific-quick-fixes) |
| Understand architecture | [codebase.md](codebase.md) → Look at specific component |
| Write unit test | [patterns.md](patterns.md#test-pattern-unit-tests) → [dev-workflow.md](dev-workflow.md#running-unit-tests) |
| Set up environment | [dev-workflow.md](dev-workflow.md#environment-setup) |

### For Tool Integration

- **GitHub Copilot**: Copy [instructions.md](instructions.md) to `.github/copilot-instructions.md`
- **Aider**: Use [quick-reference.md](quick-reference.md#aider-configuration) settings
- **Cursor IDE**: Use [quick-reference.md](quick-reference.md#cursor-ide-configuration) rules
- **Claude/Other APIs**: Include `.ai/instructions.md` in system prompt

## Core Concepts (Abbreviated)

### The Six-Level Hierarchy
Differences are classified at six levels, with **Level 2 being immutable**:
1. Non-zero raw difference?
2. **Trivial** (sub-LSB) or non-trivial? ← IMMUTABLE
3. Significant or insignificant?
4. Marginal or non-marginal?
5. Critical or non-critical?
6. Error or non-error? (Level 6 diagnostics are experimental)

**Key**: Trivial differences can NEVER be promoted to significant. This prevents formatting artifacts from corrupting analysis. Level 6 diagnostic tools (pattern analysis) are experimental and not used for authoritative pass/fail.

### TL Comparison Methods
- **Authoritative, production-ready**: IEEE peer-reviewed weighted TL difference (Fabre & Zingarelli / Goodman) implemented in TL metrics
- **Experimental diagnostics**: Pattern analysis, phase/stretch detection concepts — see [FUTURE_WORK.md](../docs/future-work.md)


### Sub-LSB Detection
Differences ≤ half the LSB (Least Significant Bit) are treated as informationally equivalent:
```
Example: 30.8 vs 30.85 with threshold=0 → TRIVIAL (not SIGNIFICANT)
```

This prevents cross-platform comparison failures from floating-point rounding.

### Zero-Threshold Semantics
When user provides `significant_threshold == 0.0`:
- Intent: Count every non-trivial, physically meaningful difference
- Implementation: Skip precision-derived floor, but DON'T bypass Level 2
- Formula: `significant = non_trivial - high_ignore_region`

## Quick Access by Role

### I'm an AI Assistant Helping with Development
→ Start with [instructions.md](instructions.md), refer to [patterns.md](patterns.md) for examples

### I'm a Developer on This Project
→ Bookmark [dev-workflow.md](dev-workflow.md) and [quick-reference.md](quick-reference.md)

### I'm Fixing a Specific Bug
→ [quick-reference.md](quick-reference.md#file-specific-quick-fixes) then [dev-workflow.md](dev-workflow.md#debugging-workflow)

### I'm Adding a New Feature
→ [instructions.md](instructions.md#common-modifications--patterns) then [dev-workflow.md](dev-workflow.md#development-workflow)

### I'm Just Starting and Want Overview
→ [codebase.md](codebase.md) architecture section, then read main [README.md](../../README.md)

## Key Files in Project

| Critical File | Why | When |
|---------------|-----|------|
| `src/cpp/src/difference_analyzer.cpp` | Core comparison logic | Modifying thresholds, classification |
| `src/cpp/tests/test_semantic_invariants.cpp` | Invariant verification | Validating changes, debugging |
| `docs/guide/sub-lsb-detection.md` | Precision handling | Understanding FP edge cases |
| `docs/guide/discrimination-hierarchy.md` | Six-level system | Understanding classification order |
| `makefile` | Build configuration | Compilation issues |

## Testing is Mandatory

Every code change requires:
1. ✅ **New unit test** (if logic modified)
2. ✅ **Existing tests pass** (run full suite)
3. ✅ **Semantic invariants verified** (if threshold-related)
4. ✅ **Cross-precision pairs tested** (if numeric logic)

See [dev-workflow.md#testing](dev-workflow.md#testing) for detailed procedures.

## Common Pitfalls & How to Avoid

| Pitfall | Cause | Solution |
|---------|-------|----------|
| FP comparison bug | Using `==` instead of tolerance | See [patterns.md](patterns.md#floating-point-comparison-critical-for-this-project) |
| Zero-threshold broken | Forgetting special case handling | See [patterns.md](patterns.md#zero-threshold-logic-pattern) |
| Cross-precision fails | Missing decimal place handling | Test with `30.8` vs `30.85` pairs |
| Trivial filter broken | Attempting to revisit Level 2 | See [instructions.md](instructions.md#avoid-common-pitfalls) |
| Build issues | Stale object files | Run `make clean && make -j4` |

## Getting Help

### Q: How do I understand the six-level hierarchy?
→ Read [codebase.md - Threshold Logic](codebase.md#threshold-logic) and [docs/guide/discrimination-hierarchy.md](../../docs/guide/discrimination-hierarchy.md)

### Q: My test is failing, how do I debug?
→ See [dev-workflow.md#debugging-workflow](dev-workflow.md#debugging-workflow) and [quick-reference.md#file-specific-quick-fixes](quick-reference.md#file-specific-quick-fixes)

### Q: How do I add a new feature?
→ See [instructions.md#common-modifications--patterns](instructions.md#common-modifications--patterns)

### Q: What's the build process?
→ See [dev-workflow.md#build-process](dev-workflow.md#build-process)

### Q: Where are the test patterns?
→ See [patterns.md - Testing Patterns](patterns.md#testing-patterns)

## Updating This Framework

As the project evolves, keep these files updated:

- **New threshold added?** → Update [instructions.md](instructions.md#critical-thresholds) and [codebase.md](codebase.md#operational-thresholds)
- **New component created?** → Add to [codebase.md](codebase.md#core-architecture) and [quick-reference.md](quick-reference.md#core-source-files)
- **New test added?** → Document in [dev-workflow.md](dev-workflow.md#test-organization)
- **Bug pattern discovered?** → Add to [patterns.md](patterns.md#common-mistakes-to-avoid)
- **New build configuration?** → Update [dev-workflow.md](dev-workflow.md#build-process)

## Version History

| Date | Changes |
|------|---------|
| Jan 2026 | Initial framework created with 5 core files |

---

**Total Reading Time**: ~1 hour for full context, 15 min for quick start
**Maintenance Effort**: Low; update only when project structure changes
**Value Delivered**: 10x faster AI-assisted development with fewer errors
