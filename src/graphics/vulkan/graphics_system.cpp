/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <rex/graphics/vulkan/graphics_system.h>

#include <rex/graphics/vulkan/command_processor.h>
#include <rex/ui/vulkan/provider.h>
#include <rex/system/xtypes.h>
#include <rex/graphics/xe_compat.h>

namespace rex::graphics::vulkan {

VulkanGraphicsSystem::VulkanGraphicsSystem() {}

VulkanGraphicsSystem::~VulkanGraphicsSystem() {}

std::string VulkanGraphicsSystem::name() const {
  auto vulkan_command_processor =
      static_cast<VulkanCommandProcessor*>(command_processor());
  if (vulkan_command_processor != nullptr) {
    return vulkan_command_processor->GetWindowTitleText();
  }
  return "Vulkan";
}

X_STATUS VulkanGraphicsSystem::Setup(cpu::Processor* processor,
                                     kernel::KernelState* kernel_state,
                                     ui::WindowedAppContext* app_context,
                                     bool with_presentation) {
  provider_ = xe::ui::vulkan::VulkanProvider::Create(true, with_presentation);
  return GraphicsSystem::Setup(processor, kernel_state, app_context,
                               with_presentation);
}

std::unique_ptr<CommandProcessor>
VulkanGraphicsSystem::CreateCommandProcessor() {
  return std::make_unique<VulkanCommandProcessor>(this, kernel_state_);
}

}  // namespace rex::graphics::vulkan
