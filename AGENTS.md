# AGENTS.md

## For Humans

You've found the machine-readable project brief. This file exists because AI coding assistants (GitHub Copilot, Cursor, Claude, etc.) are increasingly trained to look for `AGENTS.md` at the repository root for onboarding context.

**What you're looking for is probably:**
- [README.md](README.md) — Project overview, quick start, technical details
- [docs/guide/getting-started.md](docs/guide/getting-started.md) — Build instructions, test commands, troubleshooting

This file won't help you. It's a pointer to the `.ai/` folder where AI agents find their working notes. Please don't edit it—the robots will get confused.

—*The AI Context Management Bureau*

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
| `src/cpp/` | C++ comparison engine (uband_diff) |
| `src/fortran/` | Legacy Fortran utilities |
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

**Current test count:** 55 tests across 15 suites (including 5 cross-language precision tests)

---

*This file is the universal entry point. For detailed context, always defer to `.ai/`.*
