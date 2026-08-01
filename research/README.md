# research/

Reference material for porting ReXGlue's GPU (Xenos) implementation toward Xenia
Canary. Three folders:

| folder | what it is |
|--------|-----------|
| `master-gpu/` | Xenia **master** `src/xenia/gpu` at `dfa1b3f` (2025-12-14) — the exact commit ReXGlue's `src/graphics` was forked from. Headers + `.cc` only (generated shader bytecode pruned). |
| `canary-gpu/` | Xenia **canary_experimental** `src/xenia/gpu` at `7010c86` (2026-07-30, latest tip) — the port target. Same structure, so files line up 1:1 for diffing. |
| `master-to-rex_comparison/` | The analysis: a generated master↔canary file comparison plus the notes from the porting effort. |

## The three-way relationship
- **master** = the baseline ReXGlue forked from and diverged (renamed to `rex::graphics`,
  rex-abstracted memory/dispatcher/kernel, wrapped behind a runtime plugin ABI, cherry-picked
  some Canary fixes).
- **canary** = where we want ReXGlue's rendering to end up.
- **ReXGlue** = a master-based fork. The port takes it from "master + local divergence" to
  "Canary", and the notes capture the strategy that works.

## Quick diff
```bash
diff -rq master-gpu canary-gpu           # which files differ / are new
diff master-gpu/<file> canary-gpu/<file> # a specific file's Canary changes
```
