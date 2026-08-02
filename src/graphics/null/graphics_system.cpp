/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2016 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <rex/graphics/null/graphics_system.h>

#include <rex/graphics/null//null_command_processor.h>
#if !XE_PLATFORM_MAC
#include <rex/ui/vulkan/provider.h>
#endif
#include <rex/system/xtypes.h>
#include <rex/graphics/xe_compat.h>

namespace rex::graphics::null {

NullGraphicsSystem::NullGraphicsSystem() {}

NullGraphicsSystem::~NullGraphicsSystem() {}

X_STATUS NullGraphicsSystem::Setup(cpu::Processor* processor,
                                   kernel::KernelState* kernel_state,
                                   ui::WindowedAppContext* app_context,
                                   bool with_presentation) {
#if XE_PLATFORM_MAC
  provider_ = nullptr;
#else
  // This is a null graphics system, but we still setup vulkan because UI needs
  // it through us :|
  provider_ = xe::ui::vulkan::VulkanProvider::Create(false, with_presentation);
#endif
  return GraphicsSystem::Setup(processor, kernel_state, app_context,
                               with_presentation);
}

std::unique_ptr<CommandProcessor> NullGraphicsSystem::CreateCommandProcessor() {
  return std::unique_ptr<CommandProcessor>(
      new NullCommandProcessor(this, kernel_state_));
}

}  // namespace rex::graphics::null
