# Fortran Source — Comparison Utilities

This directory contains the Fortran source code for the NSPE output
comparison utilities (tldiff, prsdiff, tsdiff).

## Directory Structure

```
src/fortran/
├── main/           Source for standalone programs
│   ├── tldiff.f90
│   ├── prsdiff.f90
│   ├── tsdiff.f90
│   └── ...
├── modules/        Shared modules
│   └── file_diff_utils.f90
├── extras/         Delivery package for NSPE (see below)
│   ├── makefile    Standalone/drop-in makefile (tracked)
│   └── README.md   Program documentation (tracked)
├── makefile        Development build (builds all programs + tests)
└── README.md       This file
```

## Development Workflow

1. **Edit source files** in `main/` and `modules/`.
2. **Build and test** with `make` (uses the development makefile here).
3. **Package for delivery** with `make package`.
4. **Copy `extras/`** to the NSPE `extras/` folder for integration testing.

## `make package`

Copies the three program files and the shared module into `extras/`:

```
make package
```

This assembles the delivery package:

```
extras/
├── makefile          ← tracked (hardened, adaptive makefile)
├── README.md         ← tracked (program documentation)
├── file_diff_utils.f90  ← copied by make package (not tracked)
├── tldiff.f90           ← copied by make package (not tracked)
├── prsdiff.f90          ← copied by make package (not tracked)
└── tsdiff.f90           ← copied by make package (not tracked)
```

The `.f90` files in `extras/` are excluded from version control via
`.gitignore`. They are regenerated from `main/` and `modules/` each
time `make package` is run.

## NSPE Integration

The `extras/` makefile adapts to its environment:

- **Standalone** (tarball with just the 5 delivery files): builds the
  3 diff programs only.
- **In NSPE `extras/`** (with existing `.f` programs and parent modules):
  builds all programs — both the legacy `.f` utilities and the new `.f90`
  diff tools.

No modifications to the makefile are needed in either case.
