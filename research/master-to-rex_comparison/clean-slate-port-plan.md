# Clean-slate Canary GPU port — plan

Replace ReXGlue's diverged GPU fork with Canary's GPU code **1:1**, then wire it to
ReXGlue's runtime. Canary is source-of-truth; ReXGlue's divergence lives in three
well-defined places (a codemod, a translation layer, a thin boundary) — never scattered
per-file. This is the "pour, don't merge" method, applied from a clean slate.

## Phase 0 — Preserve  ✅ DONE
- `research/rexglue-gpu/` — the **pristine ReXGlue GPU** (`include/` 54 headers, `src/` 61
  cpp + CMakeLists; generated shader bytecode dropped). This is the reference for wiring:
  the plugin ABI, the memory/dispatcher/kernel seams, the CMake, the native-driving code.
- `research/canary-gpu/` + `research/master-gpu/` — the source trees to paste from.
- `research/master-to-rex_comparison/` — the delta + dependency notes (see
  `canary-header-dependencies.md`).

## Phase 1 — Remove
Delete ReXGlue's current GPU entirely:
- `include/rex/graphics/`  (54 files)
- `src/graphics/`          (268 files)

**Do NOT delete** (these are the boundary, they live outside the GPU folder):
- `include/rex/system/interfaces/graphics.h` — the `IGraphicsSystem` ABI
- `include/rex/system/gpu_plugin.h` — the plugin loader ABI
Everything under `include/rex/graphics/` and `src/graphics/` is fair game — recoverable from
git and from `research/rexglue-gpu/`.

## Phase 2 — Paste Canary 1:1

**Layout decision (recommended): mirror Canary's `src/xenia/gpu/` exactly** into
`src/graphics/` — headers and `.cc` together, keeping `d3d12/`, `vulkan/`, `null/`, `shaders/`
subdirs. Rationale: it's a true 1:1 paste, so every future re-sync is a drop-in; the GPU
internals are plugin-private and don't need to sit in `include/`. Add `src/graphics/` to the
target's include path so internal cross-includes resolve.
> Alternative (proven but not flat-1:1): keep ReXGlue's `include/` vs `src/` split and let the
> codemod map paths into it. Chosen only if we want to preserve the existing include convention.

Paste mechanically:
1. Copy `research/canary-gpu/**` → `src/graphics/` (verbatim).
2. Run the **codemod** on every file: `xe::gpu`→`rex::graphics`, rewrite includes
   (`xenia/…`→`rex/…`, de-prefix `ui/vulkan/vulkan_X`, `kernel/`→`system/`, `cpu/processor`→
   `system/function_dispatcher`, `string_buffer`→`string/buffer`, etc.), `cvars::`→
   `REXCVAR_GET`, and auto-inject the translation header. (Reuse `rexform.py` from
   `canary-rendering`.)

## Phase 3 — Translation layer (all the divergence, in ONE place)
Reuse from the `canary-rendering` branch, then complete the manual additions from
`canary-header-dependencies.md` §2:
- **`xe_compat.h`** — `namespace xe` aliases over `rex::` (bit ops, `Memory`, `StringBuffer`,
  endian load/store, `threading`→`thread`, `filesystem`, `memory`), pulled into `rex::graphics`.
- **`rex/math.h`** += `roundToNearestOrderOfMagnitude`, `ArchReciprocal*`, `divisors::MagicDiv`.
- **`XE_*` attribute macros** (FORCEINLINE/NOINLINE/RESTRICT/COLD/NOALIAS/MAYBE_UNUSED/…).
- **Logging** — `XELOG*`→`REXGPU_*` aliases + a `LoggerBatch` (or stub PM4 disasm).
- **Cvar bridge** — `DEFINE_*`/`DECLARE_*` arg-reorder macros + missing `REXCVAR_DECLARE`s in
  `flags.h`. **`spirv_compatibility.h`** made self-protecting.
- **glslang 16.0.0** submodule bump + the one-line CMake fix (drop removed `OGLCompiler`).
- Two file ports: `vulkan_gpu_completion_timeline.{h,cc}`; skip `xenia/emulator.h` (boundary).

## Phase 4 — Wire (do this once the paste is in)
Using `research/rexglue-gpu/` as the reference:
1. **CMake** — rebuild `rexgpu-xenos` from the new tree: source list, include dirs, glslang16 +
   spirv-tools/vma/volk links, shader-build step. (Diff against the preserved CMakeLists.)
2. **Plugin boundary** — re-add `plugin_main.cpp` + the `IGraphicsSystem` implementation that
   bridges Canary's `GraphicsSystem`/`Vd*` surface to `rex::system` (interrupt callback, ring
   buffer, presenter). Reference the preserved boundary code.
3. **Seams** — point Canary's `Memory`→`rex/system/xmemory.h`, `Processor`→`FunctionDispatcher`,
   `KernelState`→`rex::system::KernelState`; wire interrupt/swap and the native-driving path in
   `command_processor`. **This is the one genuinely hand-written file** — diff the preserved
   `command_processor.cpp` for the exact seams.
4. **Shaders** — generate Canary's precompiled shaders (tessellation + host_depth_store) into
   the expected path (the build step).

## Order of operations
`Phase 1 → 2 → 3` gets it compiling subsystem-by-subsystem (pour → build → add missing alias).
`Phase 4` is the wiring, done against the `research/rexglue-gpu/` reference. The only file that
is not a mechanical pour is `command_processor` (native-driving boundary).

## Why this beats the previous attempts
Merging Canary onto the diverged fork produced conflicts, duplicate members, and phantom
"feature ports". Pouring 1:1 from a clean slate removes all of that — every difference between
the two sides *is* the divergence, and the divergence now lives only in the codemod +
`xe_compat.h` + the boundary, not in 130 individually-reconciled files.
