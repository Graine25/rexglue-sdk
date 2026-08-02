/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <rex/logging.h>
#include <rex/graphics/trace_viewer.h>
#include <rex/graphics/vulkan/command_processor.h>
#include <rex/graphics/vulkan/graphics_system.h>
#include <rex/graphics/xe_compat.h>

namespace rex::graphics::vulkan {

class VulkanTraceViewer final : public TraceViewer {
 public:
  static std::unique_ptr<WindowedApp> Create(
      xe::ui::WindowedAppContext& app_context) {
    return std::unique_ptr<WindowedApp>(new VulkanTraceViewer(app_context));
  }

  std::unique_ptr<gpu::GraphicsSystem> CreateGraphicsSystem() override {
    return std::unique_ptr<gpu::GraphicsSystem>(new VulkanGraphicsSystem());
  }

  uintptr_t GetColorRenderTarget(
      uint32_t pitch, xenos::MsaaSamples samples, uint32_t base,
      xenos::ColorRenderTargetFormat format) override {
    // TODO(Triang3l): EDRAM viewer.
    return 0;
  }

  uintptr_t GetDepthRenderTarget(
      uint32_t pitch, xenos::MsaaSamples samples, uint32_t base,
      xenos::DepthRenderTargetFormat format) override {
    // TODO(Triang3l): EDRAM viewer.
    return 0;
  }

  uintptr_t GetTextureEntry(const TextureInfo& texture_info,
                            const SamplerInfo& sampler_info) override {
    // TODO(Triang3l): Textures, but from a fetch constant rather than
    // TextureInfo/SamplerInfo which are going away.
    return 0;
  }

 private:
  explicit VulkanTraceViewer(xe::ui::WindowedAppContext& app_context)
      : TraceViewer(app_context, "xenia-gpu-vulkan-trace-viewer") {}
};

}  // namespace rex::graphics::vulkan

XE_DEFINE_WINDOWED_APP(xenia_gpu_vulkan_trace_viewer,
                       rex::graphics::vulkan::VulkanTraceViewer::Create);
