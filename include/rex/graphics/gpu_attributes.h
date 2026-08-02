/**
 * @file        graphics/gpu_attributes.h
 * @brief       Compiler attribute macros used by the Xenos GPU hot paths
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 *
 * @remarks     ReXGlue carries Xenia's `XE_GPU_REG_*` register names verbatim,
 *              so the Canary GPU code's `XE_`-prefixed attribute macros are kept
 *              under the same spelling rather than renamed. This lets the PM4
 *              command-processor headers (ported from xenia-canary) compile
 *              unchanged and keeps future 3-way merges clean. Definitions mirror
 *              xenia/base/platform.h.
 */

#ifndef REX_GRAPHICS_GPU_ATTRIBUTES_H_
#define REX_GRAPHICS_GPU_ATTRIBUTES_H_

#if defined(_MSC_VER) && !defined(__clang__)
#define XE_FORCEINLINE __forceinline
#define XE_NOINLINE __declspec(noinline)
#define XE_COLD __declspec(code_seg(".cold"))
#define XE_NOALIAS __declspec(noalias)
#define XE_LIKELY(...) (!!(__VA_ARGS__))
#define XE_UNLIKELY(...) (!!(__VA_ARGS__))
#define XE_LIKELY_IF(...) if (XE_LIKELY(__VA_ARGS__))
#define XE_UNLIKELY_IF(...) if (XE_UNLIKELY(__VA_ARGS__))
#define XE_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define XE_FORCEINLINE __attribute__((always_inline))
#define XE_NOINLINE __attribute__((noinline))
#define XE_COLD __attribute__((cold))
#define XE_NOALIAS
#define XE_LIKELY(...) __builtin_expect(!!(__VA_ARGS__), true)
#define XE_UNLIKELY(...) __builtin_expect(!!(__VA_ARGS__), false)
#define XE_LIKELY_IF(...) if (!!(__VA_ARGS__)) [[likely]]
#define XE_UNLIKELY_IF(...) if (!!(__VA_ARGS__)) [[unlikely]]
#define XE_RESTRICT __restrict
#else
#define XE_FORCEINLINE inline
#define XE_NOINLINE
#define XE_COLD
#define XE_NOALIAS
#define XE_LIKELY(...) (!!(__VA_ARGS__))
#define XE_UNLIKELY(...) (!!(__VA_ARGS__))
#define XE_LIKELY_IF(...) if (!!(__VA_ARGS__))
#define XE_UNLIKELY_IF(...) if (!!(__VA_ARGS__))
#define XE_RESTRICT
#endif

// Xenia log-macro aliases used by the ported PM4 command-processor code. ReXGlue
// logging has no dedicated warning level, so XELOGW maps to the GPU info channel.
#include <rex/logging.h>
#ifndef XE_MAYBE_UNUSED
#define XE_MAYBE_UNUSED [[maybe_unused]]
#endif

// MSVC per-function optimization pragmas; no-ops on other compilers.
#ifndef XE_MSVC_OPTIMIZE_SMALL
#if defined(_MSC_VER) && !defined(__clang__)
#define XE_MSVC_OPTIMIZE_SMALL() __pragma(optimize("s", on))
#define XE_MSVC_OPTIMIZE_REVERT() __pragma(optimize("", on))
#else
#define XE_MSVC_OPTIMIZE_SMALL()
#define XE_MSVC_OPTIMIZE_REVERT()
#endif
#endif

// Host cache-line size, for cache-line alignment of hot data (64 bytes on all
// targets ReXGlue builds for).
#ifndef XE_HOST_CACHE_LINE_SIZE
#define XE_HOST_CACHE_LINE_SIZE 64
#endif

// MSVC compiler reorder barrier (prevents instruction reordering across it);
// no-op on other compilers.
#ifndef XE_MSVC_REORDER_BARRIER
#if defined(_MSC_VER) && !defined(__clang__)
#define XE_MSVC_REORDER_BARRIER() _ReadWriteBarrier()
#else
#define XE_MSVC_REORDER_BARRIER()
#endif
#endif

// Xenia packed-struct macro (shader/register storage headers).
#ifndef XEPACKEDSTRUCT
#if defined(_MSC_VER) && !defined(__clang__)
#define XEPACKEDSTRUCT(name, value) \
  __pragma(pack(push, 1)) struct name value; __pragma(pack(pop))
#else
#define XEPACKEDSTRUCT(name, value) struct __attribute__((packed)) name value;
#endif
#endif

#ifndef XELOGE
#define XELOGE(...) REXGPU_ERROR(__VA_ARGS__)
#define XELOGW(...) REXGPU_INFO(__VA_ARGS__)
#define XELOGI(...) REXGPU_INFO(__VA_ARGS__)
#define XELOGD(...) REXGPU_DEBUG(__VA_ARGS__)
#define XELOGGPU(...) REXGPU_DEBUG(__VA_ARGS__)
#endif

#endif  // REX_GRAPHICS_GPU_ATTRIBUTES_H_
