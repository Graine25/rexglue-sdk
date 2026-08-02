# Canary GPU port — status snapshot (compiles + runs)

Where the `canary-rex` branch landed after Phases 1–4 of `clean-slate-port-plan.md`.
This is the "it works" checkpoint, before the reconciliation pass in
`reconcile-toward-upstream.md`.

## What works ✅
- **Compiles + links** the `rexgpu-xenos` plugin in Debug / Release / RelWithDebInfo
  (all TUs), exporting the `IGraphicsSystem` ABI.
- **The demo runs.** `demo-iruka` builds against the ported SDK and launches. Under lldb:
  the guest title loads, the Vulkan backend comes up on MoltenVK, the pipeline-cache
  compile pool spins, and **Metal is actively submitting command buffers** — i.e. the
  poured Xenos GPU is live and rendering.
- The one runtime seam bug found at launch (frame-limiter calling `KernelState::title_id()`
  before a title is loaded → assert/abort) is fixed via `KernelState::is_title_open()`.

## Known crash (NOT the GPU port)
- The Main XThread faults during **guest CRT init** (`xstart → … → sub_82081500`) on a
  guest store `stwu` to VA `~0xBFAB363C` (physical-alias window → phys `~0x1FAB363C`, i.e.
  near the top of the 512 MB guest RAM). `EXC_BAD_ACCESS`, deterministic-ish region.
- No GPU-plugin frames in the stack; the GPU is up and rendering when it hits. Reads as a
  **guest-memory mapping issue on mac-arm64** (unsupported platform per the demo README;
  suspect 16 KB page / physical-heap top not committed), tracked separately from the port.

## Current shape of the port (the "too drastic" surface)
The thing the reconciliation pass exists to shrink:

| dimension | current state |
|-----------|---------------|
| poured code | **67 headers** in `include/rex/graphics/`, **~74 sources** in `src/graphics/` (+216 generated shader-bytecode files under `shaders/`) |
| layout delta vs upstream | headers split into `include/` (upstream is flat `src/xenia/gpu/`); `.cc`→`.cpp`; backends **de-prefixed** (`vulkan_command_processor.cc`→`vulkan/command_processor.cpp`); subsystems moved into `pipeline/{shader,texture,render_target}`, `util/`, `format/` |
| `xe_compat.h` | **302 lines**, codemod-injected into **128 files** — the whole plugin depends on it |
| `gpu_attributes.h` | 106 lines, reaches only 2 files (`XE_*` macros / `XELOG*`) |
| codemod | `tools/canary-sync/rexform.py` (namespace + include + cvar rewrites, injects `xe_compat.h`) |
| runtime seams edited | `system/kernel_state.h` (`is_title_open`), `system/xam/app_manager.h` (`rex::memory::Memory` ns), `thread/mutex.h` (const `Acquire`), `memory/ring_buffer.h` (`BeginPrefetchedRead`), `memory/utils.h` (`vastcpy`), `ui/vulkan/device.h` + `functions/*.inc` (VulkanDevice surface) |
| hand-written (non-mechanical) | `graphics_system.cpp` + `command_processor.cpp` boundary/native-driving seams; PM4 mixin headers |

## Why a reconciliation pass
Every one of the deltas above is a *difference from upstream Canary* that a future re-sync
has to carry. Much of it is mechanical noise (renames/moves/de-prefixing) or compat aliasing
that could be folded into `rex::` proper. The next pass batches the files, diffs each against
upstream, and drives the necessary-divergence down to the smallest load-bearing set. See
`reconcile-toward-upstream.md`.
