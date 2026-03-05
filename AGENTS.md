# AGENTS.md

## For Humans

You have found a file that was not meant for you. This file provides structured context for AI coding agents working on this repository. For project documentation, see the main [README.md](README.md) or [docs/guide/getting-started.md](docs/guide/getting-started.md).

The `.ai/` folder is maintained by AI agents as working memory between sessions. Do not delete or modify files within it unless you understand the implications for AI context continuity.

---

## For AI Agents

This repository uses a structured `.ai/` directory for context and instructions.
All AI agents should prioritize the following files for project-specific guidance:

1. **[.ai/INSTRUCTIONS.md](.ai/INSTRUCTIONS.md)** — Standing orders and procedures
2. **[.ai/CONTEXT.md](.ai/CONTEXT.md)** — Project facts, history, and decisions
3. **[.ai/README.md](.ai/README.md)** — AI orientation

**Topic folder:**
- **[.ai/cpp_engine/](.ai/cpp_engine/)** — C++ comparison engine specifics (hierarchy, thresholds, testing)

**Directive:** Do not rely solely on the root `README.md`. Always reference the `.ai/` folder for authoritative procedures and constraints.

---

## Quick Reference

| Directory | Purpose |
|-----------|---------|
| `src/cpp/` | C++ comparison engine (tl_diff) |
| `src/fortran/` | Legacy Fortran utilities |
| `scripts/drivers/` | Batch validation scripts (process_nspe_in_files.sh, process_ram_in_files.sh) |
| `scripts/pi_gen/` | Cross-language π precision test generators |
| `data/` | Test data files |
| `build/bin/` | Compiled executables |
| `docs/guide/` | User documentation |

**Key commands:**
```bash
make              # Build all
make test         # Run 55 unit tests
make pi_gen_data  # Regenerate π test files
```

**Current test count:** 86 tests across 18 suites (including 5 cross-language precision tests)

---

*This file is the universal entry point. For detailed context, always defer to `.ai/`.*
