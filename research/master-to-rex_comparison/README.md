# master-to-rex comparison + porting notes

Compares the two GPU implementations in `../master-gpu/` (Xenia master `dfa1b3f`,
ReXGlue's fork base) and `../canary-gpu/` (Xenia canary_experimental `7010c86`, the
target), and collects the notes from the porting effort.

## Contents

| file | what it is |
|------|-----------|
| `clean-slate-port-plan.md` | **The plan** — remove ReXGlue's GPU folder and paste Canary 1:1, phase by phase (preserve → remove → paste → translation layer → wire). |
| `canary-port-status.md` | **Where it landed** — the port compiles + renders (Metal is submitting command buffers). Snapshot of what works, the known guest-CRT crash (not the GPU), and the current "too drastic" surface (compat headers, renames, seams) with numbers. |
| `reconcile-toward-upstream.md` | **Phase 5** — shrink the divergence. Batch the poured files, diff each against upstream Canary, classify every hunk (mechanical / seam / gratuitous / compat-alias), revert the gratuitous, and dissolve `xe_compat.h` into `rex::` proper + the codemod. |
| `master-vs-canary-comparison.md` | Generated file-level diff: new files in canary, removed files, and every changed common file ranked by churn. Start here for the shape of the delta. |
| `canary-header-dependencies.md` | What the canary GPU **headers** pull in from *outside* the GPU folder (base/ui/kernel/cpu), mapped to whether ReXGlue has an equivalent, a rename, or nothing (must add manually). This is the compatibility-layer checklist. |
| `DRIFT-notes.md` | The main working notes — worksheet, per-subsystem status, the seam taxonomy, the PM4 command-processor decision, the base-layer findings, the glslang-16 scoping, and the **approach pivot** (pour Canary files 1:1 + one translation layer, instead of 3-way merging onto the diverged fork). |
| `harness-method.md` | How the sync harness / 3-way-merge method works (the original approach, superseded by the 1:1 pour but still useful context). |
| `pm4-runbook.md` | Step-by-step runbook for adopting Canary's PM4 command-processor mixin, incl. the reverse-engineered mechanism and seam bridges. |

## TL;DR of the findings
- Merging Canary onto ReXGlue's **diverged** fork is the source of all the pain (conflicts,
  stale cherry-picks, duplicate members, "SystemConstants feature ports"). Proven: replacing
  a file 1:1 with Canary collapses a whole "feature port" to a wrong include path.
- **The method that works:** treat Canary as source-of-truth, pour files ~1:1 through a
  codemod, and keep all divergence in ONE translation layer (`xe::`→`rex::` aliases + macros).
  Pour by **subsystem**, not file.
- **glslang** is the one hard infra dependency: Canary needs **glslang 16.0.0**; ReXGlue is
  on ~11.0. That upgrade is the keystone unblock for the SPIRV shader path.
- The only genuine hand-reconciliation is the **command_processor** boundary (Canary GPU
  logic mixed with ReXGlue's native-driving seams: PM4 mixin, FunctionDispatcher,
  interrupt/swap). Everything else pours in.

(The working implementation of the pivot — the codemod, the `xe_compat.h` translation layer,
the glslang-16 bump, and the poured subsystems — lives on the `canary-rendering` branch.)
