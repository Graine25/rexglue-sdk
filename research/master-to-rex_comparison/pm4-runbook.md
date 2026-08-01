# PM4 command-processor swap — runbook

Executable steps to adopt Canary's PM4 mixin. Foundation (the `XE_*` attribute
macros, `include/rex/graphics/gpu_attributes.h`) is already landed and building.

## Staged material in this dir
- `pm4_command_processor_declare.h` — rexform'd Canary declarations (inject into class body).
- `pm4_command_processor_implement.h` — rexform'd Canary bodies (`#include` into each `.cc`).
- `command_processor.h.canary` / `.cpp.canary` — full rexform'd Canary base cmd-proc, for reference.
- `command_processor.seam.diff` — `ReXGlue_current − base_master`: the seam bridges to re-apply.

## The mechanism (reverse-engineered from Canary)
- `declare.h` is `#include`d **inside** each command-processor class body. `PM4_OVERRIDE`
  expands to `override` when `OVERRIDING_BASE_CMDPROCESSOR` is defined (backends) vs empty (base).
- `implement.h` is `#include`d at the **end of each backend `.cc`**, after
  `#define COMMAND_PROCESSOR <ClassName>`. That macro (116 uses) is how the same
  bodies compile & inline separately per backend — the whole point.
- The ring reader is a **member** `RingBuffer reader_;` (chrispy moved it off-stack
  for cache locality), NOT a threaded `RingBuffer* reader` param. This is the
  breaking difference vs ReXGlue's current handlers.

## Infra gap (small)
- ReXGlue `RingBuffer` already has `read_count/read_offset/read_ptr/set_write_offset/BeginRead`.
- Missing: `BeginPrefetchedRead<swcache::PrefetchTag>()` + `swcache::` (2 call sites,
  results discarded). It's `BeginRead` + a cache-prefetch hint — **drop the 2 calls**
  in `implement.h` for the initial port (perf-only; restore later via a
  `RingBuffer::BeginPrefetchedRead` shim). No other swcache use.
- ZPD query calls all have base no-op virtual defaults — porting the ZPD pool is NOT required.
- `ReportHandle`/`QueryOpenResult` come with `command_processor.h`.

## Steps
1. Move `pm4_command_processor_{declare,implement}.h` to `include/rex/graphics/`.
2. In `implement.h`: delete the 2 `reader_.BeginPrefetchedRead<...>(...)` statements.
3. `include/rex/graphics/command_processor.h`:
   - add member `memory::RingBuffer reader_;`
   - add `ReportHandle`/`QueryOpenResult` types + ZPD no-op virtuals (copy from `.h.canary`).
   - replace the master-style PM4 decl block (the `ExecutePacketType*(RingBuffer* reader, …)`
     run, ~lines 186–235) with `#include <rex/graphics/pm4_command_processor_declare.h>`.
4. `src/graphics/command_processor.cpp`:
   - delete the existing PM4 handler bodies (`ExecutePrimaryBuffer`, `ExecutePacket*`,
     `ExecutePacketType*`) — `implement.h` replaces them.
   - at end of file: `#define COMMAND_PROCESSOR CommandProcessor` then
     `#include <rex/graphics/pm4_command_processor_implement.h>`.
   - re-apply the seam bridges from `command_processor.seam.diff` — the ~62 seam lines,
     chiefly:
       * `ExecutePacketType3_INTERRUPT` → `graphics_system_->DispatchInterruptCallback(1, n)`
       * `ExecutePacketType3_XE_SWAP` → ReXGlue VdSwap hook path
       * host thread via `system::XHostThread` + `kernel_state_`
       * keep `FunctionDispatcher`, plugin ABI, `readback_resolve`, null-guards.
5. Repeat 3–4 for `d3d12/` and `vulkan/` command processors, each with
   `#define OVERRIDING_BASE_CMDPROCESSOR` before the declare include and
   `#define COMMAND_PROCESSOR <Backend>CommandProcessor` before the implement include.
6. Build: `cmake --build out/build/mac-arm64 --target rexgpu-xenos` (core + Vulkan on mac).

## Verification caveats — do NOT skip
- A green compile is necessary but **not sufficient**. The re-bridged
  `INTERRUPT`/`XE_SWAP` paths are correctness-critical and only exercised at runtime.
  **Capture GPU trace goldens first** (trace_writer→trace_player) and diff a few
  titles before/after. Without that, a compiling swap is unverified.
- **d3d12 cannot be built on macOS** (backend off here). The d3d12 command-processor
  half must be compiled/tested on Windows before it can be called done.
