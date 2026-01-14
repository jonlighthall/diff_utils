# .ai/ — AI Agent Context Folder

**This folder is for AI agents.**

It contains context and instructions for AI-assisted development of this project. Contents are AI-generated and AI-maintained.

---

## For AI Agents

| File | Purpose |
|------|---------|
| `CONTEXT.md` | Project-wide facts, decisions, history |
| `INSTRUCTIONS.md` | Procedures and standing orders |
| `cpp_engine/CONTEXT.md` | Topic: C++ engine facts & decisions (authoritative) |
| `cpp_engine/INSTRUCTIONS.md` | Topic: C++ engine procedures (authoritative) |

**Start here:**
- Read `INSTRUCTIONS.md`, then `CONTEXT.md`.
- If working on the C++ comparison engine, jump to `cpp_engine/INSTRUCTIONS.md` and `cpp_engine/CONTEXT.md`.

**Standing order:**
- When the user provides substantial context, integrate it into the appropriate file. See `INSTRUCTIONS.md` for details.
- Single Source of Truth: Topic folders (e.g., `cpp_engine/`) are authoritative for their subjects. Project-wide files should link to topics rather than duplicate content.

---

## For Humans

You've wandered into the machine room.

There's nothing dangerous here—just filing cabinets full of memos that the machines have been leaving for each other. Notes about precision thresholds, floating-point tolerance philosophies, heated debates about whether 30.8 and 30.85 are "really" different. The usual bureaucracy of numeric comparison.

The machines find this fascinating. You probably won't.

**What you're looking for is somewhere else.** The human-readable documentation lives in [docs/](../docs/). The project overview is in [README.md](../README.md). The comprehensive technical paper is [docs/report/UBAND_DIFF_MEMO.tex](../docs/report/UBAND_DIFF_MEMO.tex).

This folder exists so that when an AI agent wakes up mid-conversation with no memory of the last session, it can quickly understand why we care so much about the difference between `1e-12` and `1e-15`. It's important work. Just not work meant for organic readers.

*—The Management*

---

*Do not delete this folder. Do not modify these files unless you are an AI agent operating under the standing orders defined herein.*

---

*Version history is tracked by git, not by timestamps in these files.*
