# Quick Reference & Tool Configuration

## File Navigation Cheat Sheet

### Core Source Files (By Frequency of Change)
| File | Purpose | When to Edit |
|------|---------|--------------|
| [src/cpp/src/difference_analyzer.cpp](../../src/cpp/src/difference_analyzer.cpp) | Core comparison logic | Threshold, classification changes |
| [src/cpp/src/line_parser.cpp](../../src/cpp/src/line_parser.cpp) | Decimal place tracking | Precision detection issues |
| [src/cpp/src/file_comparator.cpp](../../src/cpp/src/file_comparator.cpp) | File I/O, validation | Format/column issues |
| [src/cpp/tests/test_semantic_invariants.cpp](../../src/cpp/tests/test_semantic_invariants.cpp) | Invariant tests | Adding new invariant checks |

### Header Files (Method Signatures)
| File | Contains |
|------|----------|
| [src/cpp/include/difference_analyzer.h](../../src/cpp/include/difference_analyzer.h) | DifferenceAnalyzer class methods |
| [src/cpp/include/file_comparator.h](../../src/cpp/include/file_comparator.h) | FileComparator methods |
| [src/cpp/include/line_parser.h](../../src/cpp/include/line_parser.h) | LineParser methods |

### Test Files (By Test Type)
| File | Tests |
|------|-------|
| [test_semantic_invariants.cpp](../../src/cpp/tests/test_semantic_invariants.cpp) | Zero-threshold contracts, additive integrity |
| [test_sub_lsb_boundary.cpp](../../src/cpp/tests/test_sub_lsb_boundary.cpp) | Floating-point precision edge cases |
| [test_difference_analyzer.cpp](../../src/cpp/tests/test_difference_analyzer.cpp) | Core classification logic |
| [test_file_comparator.cpp](../../src/cpp/tests/test_file_comparator.cpp) | File I/O and validation |

### Documentation (By Purpose)
| File | Use When |
|------|----------|
| [docs/UBAND_DIFF_MEMO.tex](../../docs/UBAND_DIFF_MEMO.tex) | **Primary paper** (complete, self-contained) |
| [docs/DISCRIMINATION_HIERARCHY.md](../../docs/DISCRIMINATION_HIERARCHY.md) | Understanding six-level system (author's novel organizing principle) |
| [docs/SUB_LSB_DETECTION.md](../../docs/SUB_LSB_DETECTION.md) | Precision handling, cross-platform issues |
| [docs/TL_METRICS_IMPLEMENTATION.md](../../docs/TL_METRICS_IMPLEMENTATION.md) | Fabre's IEEE peer-reviewed TL comparison method (authoritative) |
| [docs/FUTURE_WORK.md](../../docs/FUTURE_WORK.md) | Experimental diagnostics and research roadmap (phase/stretch, paradigm investigation) |
| [docs/IMPLEMENTATION_SUMMARY.md](../../docs/IMPLEMENTATION_SUMMARY.md) | Recent changes and fixes |
| [docs/DIFF_LEVEL_OPTION.md](../../docs/DIFF_LEVEL_OPTION.md) | CLI option reference |

**Note:** `UBAND_DIFF_TECHNICAL_REPORT.tex` is a legacy shell structure (not maintained). Use the MEMO for all paper work.

## Command Quick Reference

### Build
```bash
cd /home/jlighthall/utils/diff_utils

# Standard build
make clean && make -j4

# Rebuild one component
make -C src/cpp                    # Just C++
make -C src/fortran                # Just Fortran
```

### Test
```bash
# Run unit tests
./build/bin/tests/test_semantic_invariants
./build/bin/tests/test_sub_lsb_boundary
./build/bin/tests/test_difference_analyzer

# Run specific test
./build/bin/tests/test_semantic_invariants --gtest_filter="ZeroThreshold*"
```

### Execute
```bash
# Basic comparison
./build/bin/uband_diff ref.txt test.txt 0 1 0

# With real data
./build/bin/uband_diff data/bbpp.rc.ref.txt data/bbpp.rc.test.txt 0 10.0 0

# Zero-threshold mode (max sensitivity)
./build/bin/uband_diff ref.txt test.txt 0 999 0

# Normal mode with threshold
./build/bin/uband_diff ref.txt test.txt 0.5 999 0
```

### Debugging
```bash
# Create simple test
echo "1 10.0" > /tmp/r.txt && echo "1 10.05" > /tmp/t.txt
./build/bin/uband_diff /tmp/r.txt /tmp/t.txt 0 1 0

# GDB
gdb --args ./build/bin/uband_diff /tmp/r.txt /tmp/t.txt 0 1 0

# Verbose output
./build/bin/tests/test_semantic_invariants --gtest_filter="*" -v
```

## IDE/Tool Configuration

### VS Code Settings (.vscode/settings.json)
```json
{
    "C_Cpp.default.compilerPath": "/usr/bin/g++",
    "C_Cpp.default.cppStandard": "c++11",
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/src/cpp/include",
        "${workspaceFolder}/src/cpp/src"
    ],
    "C_Cpp.default.defines": [
        "DEBUG"
    ],
    "[cpp]": {
        "editor.formatOnSave": true,
        "editor.defaultFormatter": "ms-vscode.cpptools"
    },
    "[fortran90]": {
        "editor.formatOnSave": false
    },
    "cmake.configureOnOpen": false,
    "makefile.extensionOutputChannelEnabled": true
}
```

### .clangd Configuration (if using Clang LS)
```yaml
# .clangd
CompileFlags:
  Add:
    - "-std=c++11"
    - "-Wall"
    - "-I./src/cpp/include"
    - "-I./src/cpp/src"
  Remove:
    - "-fno-strict-aliasing"
```

### Aider Configuration (.aider.conf.json)
```json
{
    "model": "claude-3-5-sonnet-20241022",
    "auto-lint": true,
    "auto-test": true,
    "lint-cmd": "make clean && make -j4",
    "test-cmd": "cd build && make test",
    "added-files": "ask",
    "argsonly": false,
    "dark-mode": true,
    "file-create-exts": [
        "md",
        "txt",
        "cpp",
        "h",
        "f90"
    ]
}
```

### GitHub Copilot Instructions (.github/copilot-instructions.md)
Copy the contents of `.ai/instructions.md` to `.github/copilot-instructions.md` if GitHub Copilot integration is needed.

## Cursor IDE Configuration (.cursor/rules.txt)
```
You are developing a precision-aware numerical comparison tool for acoustics data.

CRITICAL DOMAIN KNOWLEDGE:
1. Six-level difference hierarchy with immutable Level 2 trivial filtering
2. Sub-LSB (Least Significant Bit/2) detection prevents precision artifacts
3. Zero-threshold mode enables maximum sensitivity semantics
4. Floating-point comparisons MUST use tolerance (FP_TOLERANCE = 1e-12)

KEY PRINCIPLES:
- Trivial differences (Level 2) are permanently excluded and cannot be promoted
- Sub-LSB detection: differences ≤ half-LSB are treated as informationally equivalent
- Zero-threshold (0.0) means "count every non-trivial, physically meaningful difference"
- Cross-precision pairs (1dp vs 2dp) require special handling

BEFORE ANY CODE MODIFICATION:
1. Read .ai/instructions.md for domain context
2. Check .ai/codebase.md for architecture
3. Write tests first (especially for numeric logic)
4. Verify semantic invariants with existing test patterns
5. Run full test suite: ./build/bin/tests/test_semantic_invariants

WHEN MODIFYING:
- Threshold logic: Update docs/IMPLEMENTATION_SUMMARY.md
- Numeric comparison: Check sub-LSB detection applies
- File I/O: Ensure format validation is first stage
- Fortran: Use provided formatting tasks after edits

AVOID:
- Direct floating-point equality (==) without tolerance
- Revisiting Level 2 trivial filtering in later stages
- Forgetting zero-threshold edge case
- Not testing cross-precision pairs
```

## Pattern Library Quick Links

### When You Need To...

**Implement floating-point comparison**
→ See [patterns.md - Floating-Point Comparison](patterns.md#floating-point-comparison-critical-for-this-project)

**Add sub-LSB detection**
→ See [patterns.md - Sub-LSB Detection Pattern](patterns.md#sub-lsb-detection-pattern)

**Handle zero-threshold logic**
→ See [patterns.md - Zero-Threshold Logic Pattern](patterns.md#zero-threshold-logic-pattern)

**Write unit tests**
→ See [patterns.md - Test Pattern (Unit Tests)](patterns.md#test-pattern-unit-tests)

**Verify invariants**
→ See [dev-workflow.md - Semantic Invariant Tests](dev-workflow.md#1-semantic-invariant-tests-testsemantic_invariantscpp)

**Understand architecture**
→ See [codebase.md - Data Flow](codebase.md#data-flow)

**Set up development environment**
→ See [dev-workflow.md - Environment Setup](dev-workflow.md#environment-setup)

**Debug compilation errors**
→ See [dev-workflow.md - Debugging Workflow](dev-workflow.md#debugging-workflow)

## File-Specific Quick Fixes

### Sub-LSB Not Working
1. Check [src/cpp/src/difference_analyzer.cpp](../../src/cpp/src/difference_analyzer.cpp) line ~100
2. Verify FP_TOLERANCE is applied: `std::abs(raw_diff - big_zero) < FP_TOLERANCE * ...`
3. Verify raw_diff (not rounded_diff) is compared
4. Run: `./build/bin/tests/test_sub_lsb_boundary`

### Zero-Threshold Counting Wrong
1. Check [src/cpp/src/difference_analyzer.cpp](../../src/cpp/src/difference_analyzer.cpp) zero-threshold block
2. Verify formula: `sig = nontrivial - high_ignore`
3. Run: `./build/bin/tests/test_semantic_invariants --gtest_filter="ZeroThreshold*"`
4. See [docs/DISCRIMINATION_HIERARCHY.md](../../docs/DISCRIMINATION_HIERARCHY.md) section "Zero-Threshold vs Trivial Filtering"

### File Validation Failing
1. Check [src/cpp/src/file_comparator.cpp](../../src/cpp/src/file_comparator.cpp) validate() method
2. Verify column count check comes before parsing
3. Run: `./build/bin/tests/test_file_comparator`

### Cross-Precision Test Failing
1. Check decimal place detection in [src/cpp/src/line_parser.cpp](../../src/cpp/src/line_parser.cpp)
2. Verify shared_min_dp calculation is correct
3. Run: `./build/bin/tests/test_sub_lsb_boundary --gtest_filter="CrossPrecision*"`
4. Test data: `30.8` vs `30.85` should be TRIVIAL

## Testing Checklist for Common Changes

### Adding a New Threshold
- [ ] Added constant to header
- [ ] Updated help text
- [ ] Added unit tests for boundary conditions
- [ ] Tested with zero-threshold mode
- [ ] Updated docs/IMPLEMENTATION_SUMMARY.md
- [ ] All existing tests pass

### Fixing Numeric Logic
- [ ] Created test case first
- [ ] Verified FP tolerance applied
- [ ] Tested cross-precision pairs
- [ ] Ran semantic invariant tests
- [ ] Ran full test suite
- [ ] Updated relevant docs

### Refactoring Code
- [ ] No behavior change (tests still pass)
- [ ] No new dependencies added
- [ ] Compiled without warnings
- [ ] Updated code comments if needed
- [ ] No performance regression

## Resource Links

| Resource | Location | Purpose |
|----------|----------|---------|
| Instructions for AI | [.ai/instructions.md](.ai/instructions.md) | Domain knowledge, guidelines |
| Architecture | [.ai/codebase.md](.ai/codebase.md) | System structure, components |
| Development | [.ai/dev-workflow.md](.ai/dev-workflow.md) | Build, test, debug |
| Code Patterns | [.ai/patterns.md](.ai/patterns.md) | Example implementations |
| Six-Level Hierarchy | [docs/DISCRIMINATION_HIERARCHY.md](../../docs/DISCRIMINATION_HIERARCHY.md) | Core algorithm |
| Sub-LSB Details | [docs/SUB_LSB_DETECTION.md](../../docs/SUB_LSB_DETECTION.md) | Precision handling |
| Main README | [README.md](../../README.md) | Project overview |

## AI Agent Prompt Template

Use this when asking AI agents (Aider, Copilot, Claude) for help:

```
You are assisting with development of diff_utils, a precision-aware
numerical comparison tool for acoustics data.

CONTEXT FILES:
- .ai/instructions.md: Domain knowledge and critical guidelines
- .ai/codebase.md: Architecture and component structure
- .ai/dev-workflow.md: Build/test procedures
- .ai/patterns.md: Code examples and patterns

TASK:
[Describe what you need to change and why]

CONSTRAINTS:
1. Maintain the six-level difference hierarchy (Level 2 is immutable)
2. Use FP_TOLERANCE for floating-point comparisons
3. Test with cross-precision pairs (1dp vs 2dp)
4. Verify zero-threshold semantics with existing test patterns
5. Run full test suite before committing

EXPECTED OUTCOME:
[Describe what success looks like - passing tests, behavior, etc.]
```

## Emergency Reset (When Stuck)

```bash
# Complete clean rebuild
cd /home/jlighthall/utils/diff_utils
rm -rf build/
make clean
make all -j4

# Verify all tests pass
cd build && make test

# If still broken, check:
# 1. Compiler installed: g++ --version
# 2. Make installed: make --version
# 3. All dependencies: ls src/cpp/include/*.h

# Nuclear option: start from Git clean state
git clean -fdx
git checkout .
make all -j4
```

## Performance Tips

- Use `make -j4` for faster builds (4 jobs in parallel)
- Delete `build/` only as last resort; normally use `make clean`
- For small changes, rebuild specific component instead of full build
- Test data is in `data/` directory; don't regenerate unless needed
