# master ↔ canary GPU comparison

Generated file-level comparison of the two extracted Xenia GPU trees in this folder.

| tree | commit | date |
|------|--------|------|
| `../master-gpu/` (Xenia master — ReXGlue's base) | `dfa1b3f` | 2025-12-14 |
| `../canary-gpu/` (Xenia canary_experimental — port target) | `7010c86` | 2026-07-30 |

## Files only in canary (added since master)
```
d3d12/d3d12_zpd_query_pool.cc
d3d12/d3d12_zpd_query_pool.h
pipeline_util.h
pm4_command_processor_declare.h
pm4_command_processor_implement.h
shader_storage.h
spirv_compatibility.h
texture_address.h
texture_info_formats.inl
vulkan/vulkan_zpd_query_pool.cc
vulkan/vulkan_zpd_query_pool.h
xenos_zpd_report.h
```

## Files only in master (removed/renamed in canary)
```
texture_conversion.cc
texture_conversion.h
```

## Changed common files — by churn (added+removed lines)
```
  churn  file
   2597  d3d12/d3d12_command_processor.cc
   1896  command_processor.cc
   1724  vulkan/vulkan_command_processor.cc
   1547  vulkan/vulkan_pipeline_cache.cc
   1276  d3d12/pipeline_cache.cc
   1171  spirv_shader_translator_rb.cc
   1092  d3d12/d3d12_render_target_cache.cc
    954  vulkan/vulkan_texture_cache.cc
    833  d3d12/d3d12_texture_cache.cc
    716  spirv_shader_translator.cc
    711  vulkan/vulkan_render_target_cache.cc
    527  dxbc_shader_translator_om.cc
    509  texture_cache.cc
    463  draw_util.cc
    440  d3d12/d3d12_texture_cache.h
    420  packet_disassembler.cc
    394  command_processor.h
    376  shared_memory.cc
    265  d3d12/d3d12_command_processor.h
    250  draw_util.h
    235  spirv_shader_translator_fetch.cc
    235  graphics_system.cc
    232  vulkan/vulkan_pipeline_cache.h
    197  dxbc_shader_translator_fetch.cc
    190  spirv_shader_translator_memexport.cc
    167  render_target_cache.cc
    165  d3d12/pipeline_cache.h
    164  packet_disassembler.h
    164  dxbc_shader_translator.cc
    161  spirv_shader_translator.h
    160  d3d12/d3d12_shared_memory.cc
    152  texture_info.cc
    147  vulkan/vulkan_texture_cache.h
    143  texture_cache.h
    136  vulkan/vulkan_command_processor.h
    135  vulkan/deferred_command_buffer.h
    129  d3d12/deferred_command_list.h
    120  texture_info.h
    117  texture_info_formats.cc
    109  render_target_cache.h
```
