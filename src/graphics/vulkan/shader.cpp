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
#include <string>
#include <vector>

#include <spirv-tools/libspirv.hpp>

#include <rex/assert.h>
#include <rex/cvar.h>
#include <rex/graphics/vulkan/shader.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/ui/vulkan/provider.h>

REXCVAR_DEFINE_BOOL(vulkan_validate_translated_spirv, false, "GPU/Vulkan",
                    "Run the SPIR-V validator on every translated shader before creating a shader "
                    "module, and log the diagnostic if it fails. Invalid SPIR-V is often silently "
                    "accepted by desktop drivers but rejected or miscompiled by MoltenVK.");

namespace rex::graphics::vulkan {

namespace {

// Purely diagnostic - the binary is handed to the driver either way, so a
// validator stricter than the driver can't break a working configuration.
void ValidateTranslatedSpirv(const uint8_t* data, size_t size_bytes, uint64_t ucode_data_hash,
                             uint64_t modification, bool uniform_buffer_standard_layout,
                             bool scalar_block_layout) {
  if (size_bytes < sizeof(uint32_t) || (size_bytes % sizeof(uint32_t)) != 0) {
    REXGPU_ERROR("Translated SPIR-V for shader {:016X} modification {:016X} has a size of {} bytes",
                 ucode_data_hash, modification, size_bytes);
    return;
  }
  spvtools::SpirvTools spirv_tools(SPV_ENV_VULKAN_1_2);
  std::string diagnostic;
  spirv_tools.SetMessageConsumer(
      [&diagnostic](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
        if (message) {
          if (!diagnostic.empty()) {
            diagnostic += "; ";
          }
          diagnostic += message;
        }
      });
  spvtools::ValidatorOptions validator_options;
  validator_options.SetUniformBufferStandardLayout(uniform_buffer_standard_layout);
  validator_options.SetScalarBlockLayout(scalar_block_layout);
  if (!spirv_tools.Validate(reinterpret_cast<const uint32_t*>(data),
                            size_bytes / sizeof(uint32_t), validator_options)) {
    REXGPU_ERROR("Invalid translated SPIR-V for shader {:016X} modification {:016X}: {}",
                 ucode_data_hash, modification, diagnostic);
  }
}

}  // namespace

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
  if (REXCVAR_GET(vulkan_validate_translated_spirv)) {
    const ui::vulkan::VulkanDevice::Properties& properties = vulkan_device->properties();
    ValidateTranslatedSpirv(translated_binary().data(), translated_binary().size(),
                            shader().ucode_data_hash(), modification(),
                            properties.uniformBufferStandardLayout, properties.scalarBlockLayout);
  }
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
