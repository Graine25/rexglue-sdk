/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <rex/console_app_main.h>
#include <rex/logging.h>
#include <rex/graphics/trace_dump.h>
#include <rex/graphics/vulkan/command_processor.h>
#include <rex/graphics/vulkan/graphics_system.h>
#include <rex/ui/vulkan/provider.h>
#include <rex/graphics/xe_compat.h>

namespace rex::graphics::vulkan {

using namespace rex::graphics::xenos;

class VulkanTraceDump : public TraceDump {
 public:
  std::unique_ptr<gpu::GraphicsSystem> CreateGraphicsSystem() override {
    return std::unique_ptr<gpu::GraphicsSystem>(new VulkanGraphicsSystem());
  }

  void BeginHostCapture() override {
    const ui::RenderDocAPI* const renderdoc_api =
        static_cast<const ui::vulkan::VulkanProvider*>(
            graphics_system_->provider())
            ->vulkan_instance()
            ->renderdoc_api();
    if (renderdoc_api && !renderdoc_api->api_1_0_0()->IsFrameCapturing()) {
      renderdoc_api->api_1_0_0()->StartFrameCapture(nullptr, nullptr);
    }
  }

  void EndHostCapture() override {
    const ui::RenderDocAPI* const renderdoc_api =
        static_cast<const ui::vulkan::VulkanProvider*>(
            graphics_system_->provider())
            ->vulkan_instance()
            ->renderdoc_api();
    if (renderdoc_api && renderdoc_api->api_1_0_0()->IsFrameCapturing()) {
      renderdoc_api->api_1_0_0()->EndFrameCapture(nullptr, nullptr);
    }
  }
};

int trace_dump_main(const std::vector<std::string>& args) {
  VulkanTraceDump trace_dump;
  return trace_dump.Main(args);
}

}  // namespace rex::graphics::vulkan

XE_DEFINE_CONSOLE_APP("xenia-gpu-vulkan-trace-dump",
                      rex::graphics::vulkan::trace_dump_main, "some.trace",
                      "target_trace_file");
