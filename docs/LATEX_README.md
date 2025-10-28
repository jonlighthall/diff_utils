# uband_diff Technical Report

## Overview

This directory contains a comprehensive technical report on the `uband_diff` utility, combining documentation from:
- DISCRIMINATION_HIERARCHY.md
- SUB_LSB_DETECTION.md
- IMPLEMENTATION_SUMMARY.md

## Files

### Main Document
- **UBAND_DIFF_TECHNICAL_REPORT.tex** - Main LaTeX document (31 pages)
- **UBAND_DIFF_TECHNICAL_REPORT.pdf** - Compiled PDF output

### Sections (in `sections/`)
1. **introduction.tex** - Motivation, problem statement, design goals
2. **design_philosophy.tex** - Core principles and architectural patterns
3. **mathematical_foundation.tex** - Threshold derivations, LSB/sub-LSB math
4. **discrimination_hierarchy.tex** - Six-level classification algorithm
5. **sub_lsb_detection.tex** - Cross-platform robustness theory
6. **implementation.tex** - Code structure, algorithms, data structures
7. **validation.tex** - Unit tests, pi precision suite, regression testing
8. **usage_examples.tex** - Command-line usage and output interpretation
9. **conclusion.tex** - Summary, lessons learned, future work
10. **appendix_thresholds.tex** - Threshold reference tables
11. **appendix_code_structure.tex** - Directory structure, build system

## Compiling

### Requirements
- LaTeX distribution (TeX Live 2023 or later)
- Packages: amsmath, amssymb, listings, xcolor, geometry, hyperref, booktabs, graphicx

### Build Commands

```bash
# Single compilation
cd docs
pdflatex UBAND_DIFF_TECHNICAL_REPORT.tex

# Full build (for references and TOC)
pdflatex UBAND_DIFF_TECHNICAL_REPORT.tex
pdflatex UBAND_DIFF_TECHNICAL_REPORT.tex

# Using LaTeX Workshop in VS Code
Open UBAND_DIFF_TECHNICAL_REPORT.tex and press Ctrl+Alt+B
or click the green "Build LaTeX project" button
```

## Document Structure

```
UBAND_DIFF_TECHNICAL_REPORT.pdf
|-- Abstract
|-- Table of Contents
|
|-- 1. Introduction
|   |-- 1.1 Motivation
|   |-- 1.2 Problem Statement
|   |-- 1.3 Application Domain
|   |-- 1.4 Design Goals
|   `-- 1.5 Document Organization
|
|-- 2. Design Philosophy
|   |-- 2.1 Core Principles
|   |-- 2.2 Architectural Design Pattern
|   `-- 2.3 Fail-Fast vs. Fail-Complete
|
|-- 3. Mathematical Foundation
|   |-- 3.1 Machine Precision Constants
|   |-- 3.2 Domain-Specific Threshold Derivations
|   `-- 3.3 LSB and Sub-LSB Criteria
|
|-- 4. Six-Level Discrimination Hierarchy
|   |-- 4.1 Overview
|   |-- 4.2 Level 0: Structure Validation
|   |-- 4.3 Level 1: Raw Difference Detection
|   |-- 4.4 Level 2: Precision-Based Trivial Detection
|   |-- 4.5 Level 3: Significance Assessment
|   |-- 4.6 Level 4: Marginal vs Non-Marginal
|   |-- 4.7 Level 5: Critical vs Non-Critical
|   `-- 4.8 Complete Hierarchy Diagram
|
|-- 5. Sub-LSB Detection
|   |-- 5.1 The Problem
|   |-- 5.2 Canonical Example (30.8 vs 30.85)
|   |-- 5.3 Information-Theoretic Justification
|   |-- 5.4 Cross-Platform Robustness
|   |-- 5.5 Implementation: The Bug and The Fix
|   `-- 5.6 Mathematical Formulation
|
|-- 6. Implementation
|   |-- 6.1 Code Organization
|   |-- 6.2 Data Structures
|   |-- 6.3 Analysis Flow
|   `-- 6.4 Key Algorithms
|
|-- 7. Validation and Testing
|   |-- 7.1 Unit Test Suite
|   |-- 7.2 Pi Precision Test Suite
|   `-- 7.3 Regression Testing
|
|-- 8. Usage Examples
|   |-- 8.1 Basic Usage
|   |-- 8.2 Output Interpretation
|   |-- 8.3 Exit Codes
|   `-- 8.4 Integration with Automated Testing
|
|-- 9. Conclusion
|   |-- 9.1 Summary of Contributions
|   |-- 9.2 Operational Benefits
|   |-- 9.3 Lessons Learned
|   |-- 9.4 Future Enhancements
|   `-- 9.5 Final Remarks
|
|-- Appendix A: Threshold Reference
|   |-- A.1 Default Threshold Values
|   |-- A.2 Threshold Derivation Details
|   `-- A.3 Recommended Thresholds by Use Case
|
`-- Appendix B: Code Structure Reference
    |-- B.1 Directory Structure
    |-- B.2 Key Function Reference
    |-- B.3 Build System
    `-- B.4 Testing Infrastructure
```

## Key Features

- **Comprehensive**: Combines all uband_diff documentation into one cohesive report
- **Mathematical Rigor**: Detailed derivations of thresholds and algorithms
- **Cross-Referenced**: Internal hyperlinks for easy navigation
- **Production Quality**: Publication-ready LaTeX formatting
- **Modular**: Separated into logical sections for maintainability

## Viewing the PDF

### In VS Code with LaTeX Workshop
- Open the .tex file
- Click "View LaTeX PDF" button (top-right)
- Or press `Ctrl+Alt+V`

### Command Line
```bash
# Linux
xdg-open UBAND_DIFF_TECHNICAL_REPORT.pdf

# WSL with Windows PDF viewer
/mnt/c/Windows/System32/cmd.exe /c start UBAND_DIFF_TECHNICAL_REPORT.pdf
```

## Maintenance

### Adding New Sections
1. Create `sections/new_section.tex`
2. Add `\input{sections/new_section}` to main document
3. Recompile

### Updating Content
- Edit individual section files in `sections/`
- Main document structure remains unchanged
- Rebuild to incorporate changes

## Notes

- The `.gitignore` excludes all LaTeX auxiliary files (`.aux`, `.log`, etc.)
- PDF output is also gitignored but can be committed if desired
- All box-drawing characters have been converted to ASCII for LaTeX compatibility
- Mathematical symbols use proper LaTeX commands (not Unicode)
