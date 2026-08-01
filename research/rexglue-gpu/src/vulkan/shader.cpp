/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <cstdint>
#include <vector>

#include <rex/assert.h>
#include <rex/graphics/vulkan/shader.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/ui/vulkan/provider.h>

#if REX_PLATFORM_MAC
#include <spirv-tools/optimizer.hpp>
#endif

namespace rex::graphics::vulkan {

#if REX_PLATFORM_MAC
namespace {

// SPIR-V opcodes we care about (low 16 bits of the first instruction word).
enum : uint16_t {
  kSpvOpFunction = 54,
  kSpvOpFunctionEnd = 56,
  kSpvOpLabel = 248,
  kSpvOpBranchFirst = 249,     // OpBranch
  kSpvOpUnreachableLast = 255  // OpBranch..OpUnreachable are the block terminators
};

// Our SPIR-V translator can emit an extra branch after a block already ended
// (e.g. `OpBranch %loop_continue` immediately followed by a stray
// `OpBranch %selection_merge`). That is invalid SPIR-V — a block may have only
// one terminator — but desktop drivers ignore the unreachable trailer. MoltenVK's
// SPIRV-Cross does not, and fails pipeline creation with "Failed to get vertex
// outputs: Trying to end a non-existing block". Strip any instructions that
// appear after a terminator and before the next OpLabel/function boundary.
// Returns {} if the binary can't be parsed (caller then uses it unchanged).
std::vector<uint32_t> StripDeadCodeAfterTerminators(const uint32_t* words, size_t word_count) {
  constexpr size_t kHeaderWords = 5;
  if (word_count < kHeaderWords) {
    return {};
  }
  std::vector<uint32_t> out;
  out.reserve(word_count);
  out.insert(out.end(), words, words + kHeaderWords);
  bool after_terminator = false;
  for (size_t i = kHeaderWords; i < word_count;) {
    const uint16_t opcode = static_cast<uint16_t>(words[i] & 0xFFFFu);
    const uint16_t length = static_cast<uint16_t>(words[i] >> 16);
    if (length == 0 || i + length > word_count) {
      return {};  // malformed stream, don't touch it
    }
    const bool starts_block = (opcode == kSpvOpLabel);
    const bool function_boundary = (opcode == kSpvOpFunction || opcode == kSpvOpFunctionEnd);
    if (after_terminator && !starts_block && !function_boundary) {
      i += length;  // dead instruction between a terminator and the next block
      continue;
    }
    out.insert(out.end(), words + i, words + i + length);
    if (starts_block || function_boundary) {
      after_terminator = false;
    } else if (opcode >= kSpvOpBranchFirst && opcode <= kSpvOpUnreachableLast) {
      after_terminator = true;
    }
    i += length;
  }
  return out;
}

}  // namespace

// Repairs translated SPIR-V so MoltenVK's SPIRV-Cross accepts it, then runs the
// SPIRV-Tools optimizer to normalize the (now valid) CFG. Returns {} if nothing
// usable could be produced, in which case the caller uses the original binary.
static std::vector<uint32_t> NormalizeSpirvForMoltenVK(const uint8_t* data, size_t size_bytes) {
  if (size_bytes < sizeof(uint32_t) || (size_bytes % sizeof(uint32_t)) != 0) {
    return {};
  }
  const uint32_t* words = reinterpret_cast<const uint32_t*>(data);
  const size_t word_count = size_bytes / sizeof(uint32_t);

  std::vector<uint32_t> repaired = StripDeadCodeAfterTerminators(words, word_count);
  const uint32_t* src = repaired.empty() ? words : repaired.data();
  const size_t src_count = repaired.empty() ? word_count : repaired.size();

  spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_2);
  optimizer.SetMessageConsumer([](spv_message_level_t, const char*, const spv_position_t&,
                                  const char* message) {
    REXGPU_WARN("SPIR-V MoltenVK normalization: {}", message ? message : "");
  });
  optimizer.RegisterPass(spvtools::CreateDeadBranchElimPass())
      .RegisterPass(spvtools::CreateBlockMergePass())
      .RegisterPass(spvtools::CreateCFGCleanupPass())
      .RegisterPass(spvtools::CreateAggressiveDCEPass());
  std::vector<uint32_t> optimized;
  if (optimizer.Run(src, src_count, &optimized)) {
    return optimized;
  }
  // Optimization failed but the repair may still be valid and sufficient.
  return repaired;
}
#endif  // REX_PLATFORM_MAC

VulkanShader::VulkanTranslation::~VulkanTranslation() {
  if (shader_module_) {
    const ui::vulkan::VulkanDevice* const vulkan_device =
        static_cast<const VulkanShader&>(shader()).vulkan_device_;
    vulkan_device->functions().vkDestroyShaderModule(vulkan_device->device(), shader_module_,
                                                     nullptr);
  }
}

VkShaderModule VulkanShader::VulkanTranslation::GetOrCreateShaderModule() {
  if (!is_valid()) {
    return VK_NULL_HANDLE;
  }
  if (shader_module_ != VK_NULL_HANDLE) {
    return shader_module_;
  }
  const ui::vulkan::VulkanDevice* const vulkan_device =
      static_cast<const VulkanShader&>(shader()).vulkan_device_;
  VkShaderModuleCreateInfo shader_module_create_info;
  shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_module_create_info.pNext = nullptr;
  shader_module_create_info.flags = 0;
  shader_module_create_info.codeSize = translated_binary().size();
  shader_module_create_info.pCode = reinterpret_cast<const uint32_t*>(translated_binary().data());
#if REX_PLATFORM_MAC
  std::vector<uint32_t> normalized_spirv =
      NormalizeSpirvForMoltenVK(translated_binary().data(), translated_binary().size());
  if (!normalized_spirv.empty()) {
    shader_module_create_info.codeSize = normalized_spirv.size() * sizeof(uint32_t);
    shader_module_create_info.pCode = normalized_spirv.data();
  }
#endif  // REX_PLATFORM_MAC
  if (vulkan_device->functions().vkCreateShaderModule(vulkan_device->device(),
                                                      &shader_module_create_info, nullptr,
                                                      &shader_module_) != VK_SUCCESS) {
    REXGPU_ERROR(
        "VulkanShader::VulkanTranslation: Failed to create a Vulkan shader "
        "module for shader {:016X} modification {:016X}",
        shader().ucode_data_hash(), modification());
    MakeInvalid();
    return VK_NULL_HANDLE;
  }
  return shader_module_;
}

VulkanShader::VulkanShader(const ui::vulkan::VulkanDevice* const vulkan_device,
                           const xenos::ShaderType shader_type, const uint64_t ucode_data_hash,
                           const uint32_t* const ucode_dwords, const size_t ucode_dword_count,
                           const std::endian ucode_source_endian)
    : SpirvShader(shader_type, ucode_data_hash, ucode_dwords, ucode_dword_count,
                  ucode_source_endian),
      vulkan_device_(vulkan_device) {
  assert_not_null(vulkan_device);
}

Shader::Translation* VulkanShader::CreateTranslationInstance(uint64_t modification) {
  return new VulkanTranslation(*this, modification);
}

}  // namespace rex::graphics::vulkan
