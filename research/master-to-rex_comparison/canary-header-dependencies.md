# Canary GPU headers → what they depend on outside the GPU folder

Analysis of every `#include` in the `canary-gpu/` **headers** that points *outside*
`src/xenia/gpu`, mapped to whether ReXGlue already has an equivalent (elsewhere in
`include/rex/`) or whether it must be added manually.

Checked against ReXGlue on branch `canary-rex`. Legend:
**✅ have** (same concept, usable) · **🔤 renamed** (exists, different path/name — rexform maps it) ·
**➕ add** (symbol/feature missing inside an otherwise-present header) · **❌ missing** (no equivalent).

## 1. External header → ReXGlue equivalent

### `xenia/base/*` (the base utility layer)
| canary include (uses) | ReXGlue | status |
|---|---|---|
| `base/assert.h` (20×) | `rex/assert.h` | ✅ have |
| `base/math.h` (10×) | `rex/math.h` | ➕ has most (`clamp_float`,`saturate`,`xenos_half_to_float`,`align`,`log2_floor`,`countof`,`bit_count`,`lzcnt`) but **missing** `roundToNearestOrderOfMagnitude`, `ArchReciprocalRefined`/`RefineReciprocal`/`ArchReciprocal`, `divisors::MagicDiv` |
| `base/hash.h` (6×) | `rex/hash.h` | ✅ have |
| `base/xxhash.h` (3×) | `rex/hash.h` | 🔤 renamed (xxhash folded into hash) |
| `base/threading.h` (4×) | `rex/thread.h` (namespace `xe::threading`→`rex::thread`) | 🔤 renamed |
| `base/platform.h` (4×) | `rex/platform.h` | ➕ has `REX_*` platform/arch macros but **missing the `XE_*` attribute macros** (`XE_FORCEINLINE/NOINLINE/RESTRICT/COLD/NOALIAS/MAYBE_UNUSED/LIKELY_IF/MSVC_OPTIMIZE_*`) |
| `base/memory.h` (4×) | `rex/memory.h` (`rex::memory::` load/store/copy_and_swap) | ✅ have |
| `base/cvar.h` (4×) | `rex/cvar.h` | ➕ has `REXCVAR_*` but the API differs — canary uses `DEFINE_*`/`DECLARE_*` + `cvars::name`; ReXGlue uses `REXCVAR_DEFINE_*`(reordered args) + `REXCVAR_GET(name)`. Needs macro/name translation |
| `base/string_buffer.h` (3×) | `rex/string/buffer.h` (`StringBuffer`) | 🔤 renamed (different path) |
| `base/mutex.h` (2×) | `rex/thread/mutex.h` (`global_critical_region`) | 🔤 renamed |
| `base/byte_order.h` (2×) | `rex/types.h` (`byte_swap`) | 🔤 renamed |
| `base/ring_buffer.h` (1×) | `rex/memory/ring_buffer.h` | ➕ have `RingBuffer` but **missing** `BeginPrefetchedRead` + `swcache::` prefetch |
| `base/mapped_memory.h` (1×) | `rex/memory/mapped_memory.h` | ✅ have |
| `base/logging.h` (1×) | `rex/logging.h` | ➕ has `REXGPU_*`/`REXLOG_*` but **missing** the `XELOG*` names and the `LoggerBatch<LogLevel>` class (used by PM4 disasm) |
| `base/literals.h` (1×) | `rex/literals.h` | ✅ have |
| `base/filesystem.h` (1×) | `rex/filesystem.h` | ✅ have |
| `base/profiling.h` (via .cc) | `rex/dbg.h` (`SCOPE_profile_*`) | 🔤 renamed |

### `xenia/*` (emulator root)
| canary include (uses) | ReXGlue | status |
|---|---|---|
| `xenia/memory.h` (14×) — guest `Memory` | `rex/system/xmemory.h` (`Memory`, `TranslatePhysical`) | 🔤 renamed (moved under system/) |
| `xenia/xbox.h` (1×) — `X_STATUS` etc. | `rex/system/xtypes.h` | 🔤 renamed |
| `xenia/emulator.h` (2×) — `Emulator` | — | ❌ **missing.** ReXGlue has no `Emulator`; it uses the runtime + plugin ABI (`rex::system::IGraphicsSystem`, `FunctionDispatcher`, `KernelState`). Headers that reach for `Emulator*` must be re-pointed at the ReXGlue boundary. |

### `xenia/kernel/*` and `xenia/cpu/*`
| canary include | ReXGlue | status |
|---|---|---|
| `kernel/kernel_state.h` (3×) | `rex/system/kernel_state.h` | 🔤 renamed (kernel→system) |
| `kernel/xthread.h` (2×) | `rex/system/xthread.h` | 🔤 renamed |
| `kernel/user_module.h` (1×) | `rex/system/user_module.h` | 🔤 renamed |
| `cpu/processor.h` (1×) — `Processor` | `rex/system/function_dispatcher.h` (`FunctionDispatcher`) | 🔤 renamed (Processor→FunctionDispatcher) |

### `xenia/ui/*` (presentation)
`ui/vulkan/vulkan_X.h` → `rex/ui/vulkan/X.h` (ReXGlue **de-prefixes** `vulkan_`). Present:
`vulkan_api`→`api` ✅, `vulkan_device`→`device` ✅, `vulkan_provider`→`provider` ✅,
`vulkan_presenter`→`presenter` ✅, `vulkan_mem_alloc`→`mem_alloc` ✅,
`vulkan_upload_buffer_pool`→`upload_buffer_pool` ✅, `single_layout_descriptor_set_pool` ✅,
`linked_type_descriptor_set_allocator` ✅. Generic `ui/presenter.h`,`ui/graphics_provider.h`,
`ui/window*.h`,`ui/immediate_drawer.h`,`ui/imgui_*` all map to `rex/ui/*` ✅.
**Missing:** `ui/vulkan/vulkan_gpu_completion_timeline.h` → ❌ no `rex/ui/vulkan/*completion*` equivalent.

### third_party
`third_party/fmt/include/fmt/format.h` → `<fmt/format.h>` (ReXGlue vendors fmt) ✅.

## 2. What must be ADDED MANUALLY (the real work)

These are the pieces with **no drop-in equivalent** — either a missing symbol inside a
present header, or a missing header entirely:

1. **`rex/math.h` additions** — `roundToNearestOrderOfMagnitude`, the `ArchReciprocal`/
   `RefineReciprocal`/`ArchReciprocalRefined` family (arch-gated), and the `divisors::MagicDiv`
   magic-division helpers. ~150 lines, additive, ported verbatim from Canary `base/math.h`.
2. **`XE_*` attribute macros** — `XE_FORCEINLINE/NOINLINE/RESTRICT/COLD/NOALIAS/MAYBE_UNUSED/
   LIKELY_IF/MSVC_OPTIMIZE_SMALL/REVERT`. ReXGlue keeps `XE_GPU_REG_*` names, so keep the `XE_`
   spelling; define them (mirroring `base/platform.h`).
3. **Logging shims** — alias `XELOGE/XELOGW/XELOGD/XELOGGPU` to `REXGPU_*`, and provide a
   `LoggerBatch<LogLevel>` (or stub the PM4 structured-disassembly that needs it).
4. **Cvar translation** — bridge Xenia's `DEFINE_*`/`DECLARE_*` (name, default, **desc, cat**)
   to ReXGlue's `REXCVAR_DEFINE_*`/`REXCVAR_DECLARE` (name, default, **cat, desc**), and
   `cvars::name` reads to `REXCVAR_GET(name)`. Cross-file cvars also need `REXCVAR_DECLARE`s
   added to `rex/graphics/flags.h`.
5. **`swcache` prefetch + `RingBuffer::BeginPrefetchedRead`** — perf-only; can be dropped
   initially (it's `BeginRead` + a prefetch hint).
6. **`Emulator`** — no equivalent. Only reached by the boundary path; keep ReXGlue's
   plugin-ABI/runtime wiring instead of porting `xenia/emulator.h`.
7. **`vulkan_gpu_completion_timeline.h`** — a Canary Vulkan helper ReXGlue lacks; port the
   header (and its .cc) 1:1, or stub the timeline usage.

## 3. Bottom line

The base/support surface is **~85% already present** in ReXGlue — mostly under renamed
paths (`kernel/`→`system/`, `ui/vulkan/vulkan_X`→`ui/vulkan/X`, `string_buffer`→`string/buffer`,
`Processor`→`FunctionDispatcher`, guest `Memory`→`rex/system/xmemory.h`), which a codemod
rewrites mechanically. The genuine **manual additions** are small and enumerated in §2:
a handful of base-math functions, the `XE_*`/`XELOG*` macros, the cvar-macro bridge, and two
missing headers (`Emulator` — skip via the boundary; `vulkan_gpu_completion_timeline` — port).
None of it is a rewrite; it's a bounded compatibility layer plus two file ports.
