# Phase 5 — Reconcile toward upstream (shrink the divergence)

The port compiles and renders (see `canary-port-status.md`). Now make it *fit* ReXGlue
without the diff being this drastic. Method: **batch the poured files, diff each batch
against its upstream Canary source, and classify every difference** as load-bearing or
gratuitous — then delete the gratuitous ones and fold the load-bearing ones into the
smallest possible surface (ideally `rex::` proper, not a compat shim).

Goal state: a re-sync from Canary is a near-mechanical re-pour, and a reader sees ReXGlue
code — not a 302-line `xe::`→`rex::` translation unit injected into 128 files.

## The classification (apply to every diff hunk)
For each difference between a poured file and its upstream original, tag it:

- **M — mechanical / m  codemod** — namespace `xe::gpu`→`rex::graphics`, include path rewrite,
  `cvars::`→`REXCVAR_GET`. Keep, but it must live *only* in `rexform.py` so it re-applies for free.
- **S — real seam** — genuine ReXGlue divergence (native-driving, FunctionDispatcher, the
  plugin ABI, MoltenVK). Keep, but push it to the **narrowest** place and document why.
- **G — gratuitous** — rename/move/reformat/de-prefix that buys nothing but diff. **Revert**
  unless it earns its keep against the criteria below.
- **A — compat alias** — an `xe::` name aliased to a `rex::` one in `xe_compat.h`. For each:
  can it be **removed** by adding the name to `rex::` directly, or by letting the codemod
  rewrite the use site? Prefer that over carrying the alias.

The win condition is: **maximize M+S, drive G→0, and drive A toward 0** by relocating the few
truly-needed aliases into `rex::` headers.

## Batches (diff one subsystem at a time, in this order)
Ordered easy→hard so the method is proven on small batches first.

| # | batch | files | upstream path (`xenia/gpu/…`) | notes |
|---|-------|-------|-------------------------------|-------|
| 1 | `format/` + `util/` | 3 | `ucode.*`, `draw_util.*`, `draw_extent_estimator.*` | stateless, few seams — prove the method here |
| 2 | root GPU core | ~18 | `register_file`, `registers`, `sampler_info`, `command_processor.*`, `graphics_system.*`, `primitive_processor`, `shared_memory`, `trace_writer`, `packet_disassembler`, PM4 mixin | the seam-heavy core; `command_processor`/`graphics_system` stay hand-written but audit the rest |
| 3 | `pipeline/shader/` | 17 | `shader*`, `spirv*`, `dxbc*`, `ucode_*` | translator core; mostly M, watch the glslang-16 hunks |
| 4 | `pipeline/texture` + `pipeline/render_target` | 6 | `texture_cache*`, `render_target_cache*` | the `rex::memory::Memory` and `global_unique_lock_type` seams live here |
| 5 | `vulkan/` | 12 | `vulkan/*` | de-prefixing (G) concentrated here; VulkanDevice surface seam (S) |
| 6 | `d3d12/` | 12 | `d3d12/*` | can't build on mac — diff-only audit, defer verification to a Windows build |

Per batch: `diff` poured-vs-upstream → tag every hunk (M/S/G/A) → revert G, migrate A,
confirm M is codemod-covered, annotate S. Rebuild the batch. Commit per batch.

## Naming / layout decision to settle first
The G-tag depends on one call the whole pass hinges on — **resolve before batch 1:**

- **`.cc` → `.cpp`**: keep (ReXGlue convention) — cheap, codemod-able, worth the diff.
- **headers `include/` vs flat**: decide whether the `include/`↔`src/` split earns its keep
  or whether a flat mirror (upstream layout) makes re-sync trivial. Currently split.
- **backend de-prefixing** (`vulkan_command_processor`→`vulkan/command_processor`): the
  biggest source of rename-noise. Keep only if the subdir + short name reads clearly; else
  revert to upstream names to zero out batch-5 churn.

Whatever we pick, encode it in `rexform.py` as a deterministic path map so it's mechanical,
not per-file handwork.

## Shrinking `xe_compat.h` (302 lines → as close to 0 as possible)
It reaches 128 files; every alias in it is an **A** to eliminate. Triage its contents:

1. **Utilities that ReXGlue simply lacks** (e.g. `vastcpy`, prefetch tags, `bit_range`
   helpers) → add them to the real `rex::` headers (`memory/utils.h`, `memory/ring_buffer.h`,
   …). Already started (`vastcpy`, `BeginPrefetchedRead`) — finish it so the alias can drop.
2. **Pure renames** (`xe::Foo` = `rex::foo::Foo`) → move the rewrite into `rexform.py` so the
   use sites say `rex::…` directly and the alias disappears.
3. **Namespace bridges** (`namespace xe::ui`, `kernel::`=`rex::system`, `cpu::Processor`=
   `FunctionDispatcher`) → these are real seams (S). Keep, but isolate them in a *small*
   named header (e.g. `graphics/seams.h`), not a 300-line grab-bag.
4. `gpu_attributes.h` (`XE_*` macros, `XELOG*`) already reaches only 2 files — leave it;
   it's the model for how small a compat header should be.

Target: `xe_compat.h` becomes a short seams header, with everything else either in `rex::`
proper or handled by the codemod.

## Definition of done
- Every batch diffs against upstream with only **M** and **S** hunks remaining (no **G**).
- `xe_compat.h` reduced to real seams only; the alias count is small and each is justified in
  a comment.
- `rexform.py` reproduces the entire M-layer (namespace + includes + paths + cvars) from a
  fresh Canary checkout, so re-sync = re-run the codemod + re-apply the S seams.
- A one-page "what diverges and why" table (the surviving S set) — the honest divergence ledger.
