# Changelog — tl_diff (Fortran)

All notable changes to the Fortran tl_diff program are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-02-13

Delivered as part of NSPE v6.2 extras. This is the baseline Fortran
implementation of tl_diff (originally named `tldiff`). The Fortran version
is retained for development reference and posterity; active development
continues in the C++ version.

### Summary
- Point-by-point TL file comparison with 3-argument CLI (`FILE1 FILE2 THRESH`)
- Default threshold: 0.1 dB; effective error = threshold + comp_diff (0.05)
- Exit code 0 (pass) if no differences exceed the effective error threshold
- Exit code 1 (fail) if any difference exceeds the threshold
- Subnormal/ignore detection for TL values above 138.47 dB

---

*Active development continues in [`src/cpp/CHANGELOG.md`](../cpp/CHANGELOG.md).*
