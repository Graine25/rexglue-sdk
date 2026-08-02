/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <rex/logging.h>
#include <rex/graphics/d3d12/command_processor.h>
#include <rex/graphics/d3d12/graphics_system.h>
#include <rex/graphics/trace_viewer.h>
#include <rex/graphics/xe_compat.h>

namespace rex::graphics::d3d12 {

class D3D12TraceViewer final : public TraceViewer {
 public:
  static std::unique_ptr<WindowedApp> Create(
      xe::ui::WindowedAppContext& app_context) {
    return std::unique_ptr<WindowedApp>(new D3D12TraceViewer(app_context));
  }

  std::unique_ptr<gpu::GraphicsSystem> CreateGraphicsSystem() override {
    return std::unique_ptr<gpu::GraphicsSystem>(new D3D12GraphicsSystem());
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
  explicit D3D12TraceViewer(xe::ui::WindowedAppContext& app_context)
      : TraceViewer(app_context, "xenia-gpu-d3d12-trace-viewer") {}
};

}  // namespace rex::graphics::d3d12

XE_DEFINE_WINDOWED_APP(xenia_gpu_d3d12_trace_viewer,
                       rex::graphics::d3d12::D3D12TraceViewer::Create);
