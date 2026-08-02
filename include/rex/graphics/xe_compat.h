/**
 * @file        graphics/xe_compat.h
 * @brief       xe:: -> rex:: translation layer for verbatim Canary GPU sources
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 *
 * @remarks     ReXGlue ports Xenia Canary GPU files as close to 1:1 as possible.
 *              rexform rewrites `xe::gpu` -> `rex::graphics` and include paths;
 *              this header supplies the remaining `xe::` base-utility names as
 *              aliases over their `rex::` equivalents so the ported code compiles
 *              unchanged. rexform injects an include of this header into every
 *              translated file. Add aliases here as new `xe::` symbols surface --
 *              this is the one place the base-layer divergence lives.
 */

#ifndef REX_GRAPHICS_XE_COMPAT_H_
#define REX_GRAPHICS_XE_COMPAT_H_

#include <bit>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

#include <rex/assert.h>
#include <rex/bit.h>
#include <rex/chrono/clock.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/graphics/gpu_attributes.h>
#include <rex/hash.h>
#include <rex/literals.h>
#include <rex/math.h>
#include <rex/memory.h>
#include <rex/memory/ring_buffer.h>
#include <rex/string/buffer.h>
#include <rex/string/utf8.h>
#include <rex/system.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/mmio_handler.h>
#include <rex/thread.h>
#include <rex/thread/mutex.h>
#include <rex/types.h>

// Forward-declared so `xe::ByteStream` resolves without pulling the full header
// (the boundary trace path owns the real include).
namespace rex::stream {
class ByteStream;
}  // namespace rex::stream

// Ensure the rex ui namespaces exist even when no ui header is included yet, so
// the using-directives in the xe::ui bridge below always have a target.
namespace rex {
namespace ui {
namespace vulkan {}
namespace d3d12 {}
}  // namespace ui
}  // namespace rex

namespace xe {

// --- bit utilities ---------------------------------------------------------
using ::rex::bit_scan_forward;
using ::rex::countof;
// xe::bit_range:: -> rex::bit:: (consolidated). ReXGlue renamed NextUnsetRange
// to GetNextRangeUnset; bridge the Xenia name.
namespace bit_range {
using namespace ::rex::bit;  // SetRange, GetNextRangeUnset, ...
template <typename Block>
inline std::pair<size_t, size_t> NextUnsetRange(const Block* bits, size_t first,
                                                size_t length) {
  return ::rex::bit::GetNextRangeUnset(bits, first, length);
}
}  // namespace bit_range

template <typename T>
inline uint32_t bit_count(T value) {
  return static_cast<uint32_t>(std::popcount(static_cast<std::make_unsigned_t<T>>(value)));
}
template <typename T>
inline uint32_t lzcnt(T value) {
  return static_cast<uint32_t>(std::countl_zero(static_cast<std::make_unsigned_t<T>>(value)));
}
template <typename T>
inline uint32_t tzcnt(T value) {
  return static_cast<uint32_t>(std::countr_zero(static_cast<std::make_unsigned_t<T>>(value)));
}
template <typename T>
inline T clear_lowest_bit(T value) {
  return static_cast<T>(value & (value - 1));
}
template <size_t N>
XE_FORCEINLINE void smallcpy_const(void* dst, const void* src) {
  std::memcpy(dst, src, N);
}

// --- math helpers ----------------------------------------------------------
using ::rex::align;
using ::rex::byte_swap;
using ::rex::clamp_float;
using ::rex::log2_ceil;
using ::rex::log2_floor;
using ::rex::next_pow2;
using ::rex::round_up;
using ::rex::saturate;
using ::rex::xenos_half_to_float;

// --- string builder + path/text utilities ----------------------------------
using ::rex::path_to_utf8;
using ::rex::to_path;
using ::rex::string::StringBuffer;
using ::rex::string::to_utf16;

// --- guest memory + endian-aware access (rex::memory) ----------------------
using ::rex::memory::MappedMemory;
using ::rex::memory::Memory;
using ::rex::memory::RingBuffer;
using ::rex::memory::copy_and_swap;
using ::rex::memory::fourcc_t;
using ::rex::memory::load;
using ::rex::memory::load_and_swap;
using ::rex::memory::make_fourcc;
using ::rex::memory::store;
using ::rex::memory::store_and_swap;
// Guest heap types + allocation/protection flags (Vulkan sparse shared memory).
using ::rex::memory::HeapAllocationInfo;
using ::rex::memory::VirtualHeap;
using ::rex::memory::kMemoryAllocationCommit;
using ::rex::memory::kMemoryAllocationReserve;
using ::rex::memory::kMemoryProtectRead;
using ::rex::memory::kMemoryProtectWrite;

// --- clock / threading / diagnostics ---------------------------------------
using ::rex::chrono::Clock;
using ::rex::FatalError;
using ::rex::ShowSimpleMessageBox;
using ::rex::SimpleMessageBoxType;
using ::rex::stream::ByteStream;
using ::rex::thread::global_critical_region;
// ReXGlue's global_critical_region locks a std::recursive_mutex (rex/thread/mutex.h).
using global_unique_lock_type = std::unique_lock<std::recursive_mutex>;

// --- logging: Xenia's structured/batched logger (PM4 disasm + register-write
//     initiator traces). ReXGlue routes GPU logging through spdlog and has no
//     batched-logger API, so this is a compile-only stub (ShouldLog=false, all
//     no-ops). The structured disassembly is perf/debug-only; disabling it does
//     not affect emulation. ---
enum class LogLevel { Trace, Debug, Info, Warning, Error };
namespace logging {
inline bool ShouldLog(LogLevel) { return false; }
template <LogLevel kLevel>
struct LoggerBatch {
  template <typename... Args>
  void operator()(Args&&...) {}
  void submit(char) {}
};
namespace internal {
inline std::pair<char*, size_t> GetThreadBuffer() { return {nullptr, size_t(0)}; }
inline void AppendLogLine(LogLevel, char, size_t) {}
}  // namespace internal
}  // namespace logging

// --- config: Xenia persists emulator settings; in ReXGlue the host owns config
//     persistence, so the plugin's save request is a no-op. ---
namespace config {
inline void SaveConfig() {}
}  // namespace config

// --- swcache: software-prefetch hints (perf only; no-op on ReXGlue) --------
namespace swcache {
enum class PrefetchTag { Level1, Level2, Level3, NonTemporal };
inline void Prefetch(const void*) {}
template <PrefetchTag tag>
inline void Prefetch(const void*) {}
inline void PrefetchL1(const void*) {}
inline void WriteFence() {}
}  // namespace swcache

// --- FixedVMemVector -------------------------------------------------------
// Xenia's fixed virtual-memory-backed byte buffer (capacity in bytes; callers
// reinterpret_cast data() to the element type and write in place). ReXGlue backs
// it with a std::vector<uint8_t> reserved to the full capacity -- functionally
// equivalent, without the lazy-commit vmem optimization.
template <size_t kCapacityBytes>
class FixedVMemVector {
 public:
  FixedVMemVector() : storage_(kCapacityBytes) {}
  uint8_t* data() { return storage_.data(); }
  const uint8_t* data() const { return storage_.data(); }
  void clear() { size_ = 0; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  static constexpr size_t capacity() { return kCapacityBytes; }

 private:
  std::vector<uint8_t> storage_;
  size_t size_ = 0;
};

// --- namespace aliases for subsystems ReXGlue renamed ----------------------
namespace threading = ::rex::thread;
namespace filesystem = ::rex::filesystem;
namespace memory = ::rex::memory;
namespace literals = ::rex::literals;
namespace hash {
using ::rex::IdentityHasher;
using ::rex::XXHasher;
}  // namespace hash

// --- ui:: bridge -----------------------------------------------------------
// Canary GPU code references the presentation layer as `ui::X` / `ui::vulkan::X`
// / `ui::d3d12::X` (and, unqualified from xe::gpu, as `xe::ui::X`). ReXGlue's own
// ui namespacing is inconsistent -- some classes live in bare `rex::` (VulkanDevice,
// Presenter, ...), some in `rex::ui::` (RawImage) or `rex::ui::vulkan::`
// (VulkanSubmissionTracker, util) or `rex::ui::d3d12::` (D3D12Provider). Pull all
// the relevant roots in so every spelling resolves. Defining this under `namespace
// xe::ui` makes `xe::ui::X` work directly, and the bare `ui::X` spelling resolves
// via the `using namespace ::xe` pulled into rex::graphics below.
namespace ui {
using namespace ::rex;
using namespace ::rex::ui;
namespace vulkan {
using namespace ::rex;
using namespace ::rex::ui::vulkan;
}  // namespace vulkan
namespace d3d12 {
using namespace ::rex;
using namespace ::rex::ui::d3d12;
}  // namespace d3d12
}  // namespace ui

}  // namespace xe

// Canary GPU code lives in `xe::gpu` and uses base-namespace names (StringBuffer,
// bit_scan_forward, ...) unqualified. rexform collapses `xe::gpu` -> `rex::graphics`,
// so pull the xe aliases into rex::graphics for bare-name lookup.
namespace rex::graphics {
using namespace ::xe;

// `kernel::X` -> rex::system (KernelState, XThread, XHostThread, object_ref, ...).
namespace kernel = ::rex::system;

// `cpu::X` seam names. ReXGlue renamed Processor -> FunctionDispatcher; the
// boundary files (graphics_system / command_processor) still do the real seam
// bridging in the wiring phase, but the type names resolve here.
namespace cpu {
using Processor = ::rex::runtime::FunctionDispatcher;
using ::rex::runtime::MMIOReadCallback;
using ::rex::runtime::MMIOWriteCallback;
}  // namespace cpu
}  // namespace rex::graphics

// Cvar definitions/declarations: Xenia's macros take (name, default, DESCRIPTION,
// CATEGORY); ReXGlue's take (name, default, CATEGORY, DESCRIPTION). These aliases
// bridge the ordering so ported Canary DEFINE_*/DECLARE_* lines compile as-is.
#ifndef DEFINE_bool
#define DEFINE_bool(name, def, desc, cat) REXCVAR_DEFINE_BOOL(name, def, cat, desc)
#define DEFINE_int32(name, def, desc, cat) REXCVAR_DEFINE_INT32(name, def, cat, desc)
#define DEFINE_int64(name, def, desc, cat) REXCVAR_DEFINE_INT64(name, def, cat, desc)
#define DEFINE_uint32(name, def, desc, cat) REXCVAR_DEFINE_UINT32(name, def, cat, desc)
#define DEFINE_uint64(name, def, desc, cat) REXCVAR_DEFINE_UINT64(name, def, cat, desc)
#define DEFINE_double(name, def, desc, cat) REXCVAR_DEFINE_DOUBLE(name, def, cat, desc)
#define DEFINE_string(name, def, desc, cat) REXCVAR_DEFINE_STRING(name, def, cat, desc)
// ReXGlue's cvar system has no filesystem-path type; Xenia's path cvars map onto
// the string variant (std::string <-> std::filesystem::path interconvert at use
// sites). trace/shader path cvars only.
#define DEFINE_path(name, def, desc, cat) REXCVAR_DEFINE_STRING(name, def, cat, desc)
#define DECLARE_bool(name) REXCVAR_DECLARE(bool, name)
#define DECLARE_int32(name) REXCVAR_DECLARE(int32_t, name)
#define DECLARE_int64(name) REXCVAR_DECLARE(int64_t, name)
#define DECLARE_uint32(name) REXCVAR_DECLARE(uint32_t, name)
#define DECLARE_uint64(name) REXCVAR_DECLARE(uint64_t, name)
#define DECLARE_double(name) REXCVAR_DECLARE(double, name)
#define DECLARE_string(name) REXCVAR_DECLARE(std::string, name)
#define DECLARE_path(name) REXCVAR_DECLARE(std::string, name)
// Xenia's UPDATE_from_* re-default a cvar as of a date. ReXGlue has no
// equivalent; no-op (the cvar keeps its DEFINE_* default). Perf/policy only.
#define UPDATE_from_bool(...)
#define UPDATE_from_int32(...)
#define UPDATE_from_int64(...)
#define UPDATE_from_uint32(...)
#define UPDATE_from_uint64(...)
#define UPDATE_from_double(...)
#define UPDATE_from_string(...)
// Xenia's OVERRIDE_* set a cvar's current value at runtime. Map to a direct
// assignment on ReXGlue's cvar storage.
#define OVERRIDE_bool(name, val) (REXCVAR_GET(name) = (val))
#define OVERRIDE_int32(name, val) (REXCVAR_GET(name) = (val))
#define OVERRIDE_int64(name, val) (REXCVAR_GET(name) = (val))
#define OVERRIDE_uint32(name, val) (REXCVAR_GET(name) = (val))
#define OVERRIDE_uint64(name, val) (REXCVAR_GET(name) = (val))
#define OVERRIDE_double(name, val) (REXCVAR_GET(name) = (val))
#define OVERRIDE_string(name, val) (REXCVAR_GET(name) = (val))
#endif

#endif  // REX_GRAPHICS_XE_COMPAT_H_
