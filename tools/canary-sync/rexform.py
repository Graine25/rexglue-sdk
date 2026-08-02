#!/usr/bin/env python3
"""rexform - mechanical Xenia -> ReXGlue source transform.

Turns a stock Xenia GPU source/header file (`src/xenia/gpu/...`) into a
"rexglue-flavored" file: include paths rewritten, `xe::gpu` namespace collapsed
to `rex::graphics`. It deliberately does NOT chase symbol-level namespace moves
(e.g. `xe::load_and_swap` -> `rex::memory::load_and_swap`); those are left alone
on purpose. In the 3-way merge that consumes this output, any line the codemod
misses is identical between BASE-flavored and CANARY-flavored, so git merge-file
keeps ReXGlue's own version ("ours wins when base == theirs"). The codemod only
has to normalize the *context* lines - includes and namespace - so the merge
aligns hunks. Formatting is normalized separately by clang-format.

Usage:
    python3 rexform.py < in.cc > out.cc        # transform text on stdin
    from rexform import transform, XENIA_TO_REX_FILE
"""
import re
import sys

# --- header include map: xenia-relative path -> rex include path (angle form) --
# Explicit renames first; a handful of regex fallbacks cover the long tail.
INCLUDE_MAP = {
    # gpu top-level (kept name)
    "xenia/gpu/command_processor.h": "rex/graphics/command_processor.h",
    "xenia/gpu/graphics_system.h": "rex/graphics/graphics_system.h",
    "xenia/gpu/packet_disassembler.h": "rex/graphics/packet_disassembler.h",
    "xenia/gpu/primitive_processor.h": "rex/graphics/primitive_processor.h",
    "xenia/gpu/register_file.h": "rex/graphics/register_file.h",
    "xenia/gpu/registers.h": "rex/graphics/registers.h",
    "xenia/gpu/sampler_info.h": "rex/graphics/sampler_info.h",
    "xenia/gpu/shared_memory.h": "rex/graphics/shared_memory.h",
    "xenia/gpu/xenos.h": "rex/graphics/xenos.h",
    "xenia/gpu/gpu_flags.h": "rex/graphics/flags.h",
    # gpu -> util/
    "xenia/gpu/draw_util.h": "rex/graphics/util/draw.h",
    "xenia/gpu/draw_extent_estimator.h": "rex/graphics/util/draw_extent_estimator.h",
    # gpu -> pipeline/render_target/
    "xenia/gpu/render_target_cache.h": "rex/graphics/pipeline/render_target/cache.h",
    # gpu -> pipeline/texture/
    "xenia/gpu/texture_cache.h": "rex/graphics/pipeline/texture/cache.h",
    "xenia/gpu/texture_conversion.h": "rex/graphics/pipeline/texture/conversion.h",
    "xenia/gpu/texture_info.h": "rex/graphics/pipeline/texture/info.h",
    "xenia/gpu/texture_util.h": "rex/graphics/pipeline/texture/util.h",
    # gpu -> pipeline/shader/
    "xenia/gpu/shader.h": "rex/graphics/pipeline/shader/shader.h",
    "xenia/gpu/shader_interpreter.h": "rex/graphics/pipeline/shader/interpreter.h",
    "xenia/gpu/shader_translator.h": "rex/graphics/pipeline/shader/translator.h",
    "xenia/gpu/dxbc_shader_translator.h": "rex/graphics/pipeline/shader/dxbc_translator.h",
    "xenia/gpu/dxbc_shader.h": "rex/graphics/pipeline/shader/dxbc.h",
    "xenia/gpu/spirv_shader_translator.h": "rex/graphics/pipeline/shader/spirv_translator.h",
    "xenia/gpu/spirv_shader.h": "rex/graphics/pipeline/shader/spirv.h",
    "xenia/gpu/spirv_builder.h": "rex/graphics/pipeline/shader/spirv_builder.h",
    # gpu -> format/
    "xenia/gpu/dxbc.h": "rex/graphics/format/dxbc.h",
    "xenia/gpu/ucode.h": "rex/graphics/format/ucode.h",
    # --- canary-only new headers: chosen ReXGlue destinations (document in DRIFT) ---
    "xenia/gpu/pipeline_util.h": "rex/graphics/pipeline_util.h",
    "xenia/gpu/pm4_command_processor_declare.h": "rex/graphics/pm4_command_processor_declare.h",
    "xenia/gpu/pm4_command_processor_implement.h": "rex/graphics/pm4_command_processor_implement.h",
    "xenia/gpu/shader_storage.h": "rex/graphics/pipeline/shader/shader_storage.h",
    "xenia/gpu/spirv_compatibility.h": "rex/graphics/pipeline/shader/spirv_compatibility.h",
    "xenia/gpu/texture_address.h": "rex/graphics/pipeline/texture/address.h",
    "xenia/gpu/texture_info_formats.inl": "rex/graphics/pipeline/texture/info_formats.inl",
    "xenia/gpu/xenos_zpd_report.h": "rex/graphics/xenos_zpd_report.h",
    # base rename
    "xenia/base/byte_stream.h": "rex/stream.h",
    # base headers ReXGlue renamed (the generic base-> fallback is wrong for these)
    "xenia/base/byte_order.h": "rex/types.h",
    "xenia/base/profiling.h": "rex/dbg.h",
    "xenia/base/xxhash.h": "rex/hash.h",
    "xenia/base/string_buffer.h": "rex/string/buffer.h",
}

# Ordered regex fallbacks (first match wins). Applied only if no exact hit above.
INCLUDE_REGEX = [
    # backend files: de-prefix d3d12_/vulkan_ and drop the xenia/gpu prefix
    (re.compile(r"^xenia/gpu/(d3d12|vulkan|null)/(?:d3d12_|vulkan_|null_)?(.+)$"),
     r"rex/graphics/\1/\2"),
    # trace_*.h stay at graphics root
    (re.compile(r"^xenia/gpu/(trace_\w+\.h)$"), r"rex/graphics/\1"),
    # glslang SPIR-V headers -> bare <SPIRV/...>
    (re.compile(r"^third_party/glslang/(SPIRV/.+)$"), r"\1"),
    (re.compile(r"^third_party/glslang/(glslang/.+)$"), r"\1"),
    # other third_party -> strip the vendor include dir best-effort (<pkg/...>)
    (re.compile(r"^third_party/[^/]+/include/(.+)$"), r"\1"),
    (re.compile(r"^third_party/([^/]+)/(.+)$"), r"\2"),
    # xenia base utils -> rex root (byte_stream handled explicitly above)
    (re.compile(r"^xenia/base/(.+)$"), r"rex/\1"),
    # guest Memory / emulator-root headers
    (re.compile(r"^xenia/memory\.h$"), r"rex/memory.h"),
    # ui/vulkan files are de-prefixed in ReXGlue (vulkan_device.h -> device.h)
    (re.compile(r"^xenia/ui/vulkan/vulkan_(.+)$"), r"rex/ui/vulkan/\1"),
    (re.compile(r"^xenia/ui/(.+)$"), r"rex/ui/\1"),
    # any remaining xenia/gpu/* we didn't rename -> graphics root, keep leaf
    (re.compile(r"^xenia/gpu/(.+)$"), r"rex/graphics/\1"),
]

_INCLUDE_LINE = re.compile(r'^(\s*#\s*include\s*)(["<])([^">]+)([">])(.*)$')


def rewrite_include(path: str):
    """Return the rex include path for a xenia include, or None to leave as-is."""
    if path in INCLUDE_MAP:
        return INCLUDE_MAP[path]
    for rx, repl in INCLUDE_REGEX:
        if rx.match(path):
            return rx.sub(repl, path)
    return None


def transform_includes(text: str) -> str:
    out = []
    for line in text.splitlines():
        m = _INCLUDE_LINE.match(line)
        if m:
            pre, _open, path, _close, tail = m.groups()
            newpath = rewrite_include(path)
            if newpath is not None:
                out.append(f"{pre}<{newpath}>{tail}")
                continue
        out.append(line)
    return "\n".join(text_tail(text, out))


def text_tail(orig: str, lines):
    # preserve trailing newline
    if orig.endswith("\n"):
        lines = lines + [""]
    return lines


# --- namespace collapse ------------------------------------------------------
_NS_OPEN = [
    (re.compile(r"namespace xe \{\s*\nnamespace gpu \{\s*\nnamespace (d3d12|vulkan|null) \{"),
     r"namespace rex::graphics::\1 {"),
    (re.compile(r"namespace xe \{\s*\nnamespace gpu \{"),
     r"namespace rex::graphics {"),
]
_NS_CLOSE = [
    (re.compile(r"\}\s*//\s*namespace (d3d12|vulkan|null)\s*\n\}\s*//\s*namespace gpu\s*\n\}\s*//\s*namespace xe"),
     r"}  // namespace rex::graphics::\1"),
    (re.compile(r"\}\s*//\s*namespace gpu\s*\n\}\s*//\s*namespace xe"),
     r"}  // namespace rex::graphics"),
]


def transform_namespaces(text: str) -> str:
    for rx, repl in _NS_OPEN:
        text = rx.sub(repl, text)
    for rx, repl in _NS_CLOSE:
        text = rx.sub(repl, text)
    # qualified references that remain in bodies
    text = text.replace("xe::gpu::", "rex::graphics::")
    return text


_CVARS = re.compile(r"\bcvars::(\w+)")


def inject_compat_include(text: str) -> str:
    """Add the xe:: translation-layer header after the file's last #include."""
    lines = text.split("\n")
    last = -1
    for i, l in enumerate(lines):
        if l.lstrip().startswith("#include"):
            last = i
    if last >= 0 and "rex/graphics/xe_compat.h" not in text:
        lines.insert(last + 1, "#include <rex/graphics/xe_compat.h>")
    return "\n".join(lines)


def transform(text: str) -> str:
    text = transform_namespaces(text)
    # cvar reads: Canary `cvars::name` -> ReXGlue `REXCVAR_GET(name)`.
    text = _CVARS.sub(r"REXCVAR_GET(\1)", text)
    text = transform_includes(text)
    text = inject_compat_include(text)
    return text


# --- source-file path map: xenia gpu relpath -> ReXGlue repo relpath ---------
# Stem renames for files that moved/renamed (everything else: strip backend
# prefix, drop the gpu prefix, .cc -> .cpp).
_SOURCE_STEM_RENAME = {
    "gpu_flags": "flags",
    "draw_util": "util/draw",
    "draw_extent_estimator": "util/draw_extent_estimator",
    "render_target_cache": "pipeline/render_target/cache",
    "texture_cache": "pipeline/texture/cache",
    "texture_conversion": "pipeline/texture/conversion",
    "texture_info": "pipeline/texture/info",
    "texture_info_formats": "pipeline/texture/info_formats",
    "texture_util": "pipeline/texture/util",
    "texture_extent": "pipeline/texture/extent",
    "shader": "pipeline/shader/shader",
    "shader_interpreter": "pipeline/shader/interpreter",
    "shader_translator": "pipeline/shader/translator",
    "shader_translator_disasm": "pipeline/shader/translator_disasm",
    "dxbc_shader": "pipeline/shader/dxbc",
    "dxbc_shader_translator": "pipeline/shader/dxbc_translator",
    "dxbc_shader_translator_alu": "pipeline/shader/dxbc_translator_alu",
    "dxbc_shader_translator_fetch": "pipeline/shader/dxbc_translator_fetch",
    "dxbc_shader_translator_memexport": "pipeline/shader/dxbc_translator_memexport",
    "dxbc_shader_translator_om": "pipeline/shader/dxbc_translator_om",
    "spirv_shader": "pipeline/shader/spirv",
    "spirv_builder": "pipeline/shader/spirv_builder",
    "spirv_shader_translator": "pipeline/shader/spirv_translator",
    "spirv_shader_translator_alu": "pipeline/shader/spirv_translator_alu",
    "spirv_shader_translator_fetch": "pipeline/shader/spirv_translator_fetch",
    "spirv_shader_translator_memexport": "pipeline/shader/spirv_translator_memexport",
    "spirv_shader_translator_rb": "pipeline/shader/spirv_translator_rb",
    "spirv_compatibility": "pipeline/shader/spirv_compatibility",
    "shader_storage": "pipeline/shader/shader_storage",
    "ucode": "format/ucode",
    "dxbc": "format/dxbc",
    "texture_address": "pipeline/texture/address",
    # canary-only new top-level files -> chosen destinations
    "pm4_command_processor_declare": "pm4_command_processor_declare",
    "pm4_command_processor_implement": "pm4_command_processor_implement",
    "pipeline_util": "pipeline_util",
    "xenos_zpd_report": "xenos_zpd_report",
}

# xenia gpu files ReXGlue intentionally does not carry (standalone tools, null).
_SOURCE_SKIP = {
    "shader_compiler_main", "texture_dump",
    "d3d12/d3d12_trace_dump_main", "d3d12/d3d12_trace_viewer_main",
    "vulkan/vulkan_trace_dump_main", "vulkan/vulkan_trace_viewer_main",
}


def xenia_source_to_rex(relpath: str):
    """Map a xenia gpu-relative source path to a ReXGlue repo-relative path.

    `relpath` is relative to src/xenia/gpu (e.g. 'spirv_shader_translator_rb.cc'
    or 'd3d12/d3d12_texture_cache.cc'). Returns a path relative to the repo root
    (e.g. 'src/graphics/pipeline/shader/spirv_translator_rb.cpp'), or None for
    files ReXGlue does not carry or anything under null/.
    """
    if relpath.startswith("null/"):
        return None
    dot = relpath.rfind(".")
    stem, ext = relpath[:dot], relpath[dot + 1:]
    ext = "cpp" if ext == "cc" else ext  # .cc -> .cpp; .h/.inl preserved
    if stem in _SOURCE_SKIP:
        return None
    # backend-prefixed files: d3d12/d3d12_foo -> d3d12/foo, vulkan/vulkan_foo -> vulkan/foo
    for be in ("d3d12", "vulkan"):
        pfx = f"{be}/{be}_"
        if stem.startswith(pfx):
            return f"src/graphics/{be}/{stem[len(pfx):]}.{ext}"
    if "/" in stem:  # other backend files (deferred_command_list/buffer, pipeline_cache)
        return f"src/graphics/{stem}.{ext}"
    mapped = _SOURCE_STEM_RENAME.get(stem, stem)
    return f"src/graphics/{mapped}.{ext}"


if __name__ == "__main__":
    sys.stdout.write(transform(sys.stdin.read()))
