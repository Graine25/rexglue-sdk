/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_D3D12_D3D12_GRAPHICS_SYSTEM_H_
#define XENIA_GPU_D3D12_D3D12_GRAPHICS_SYSTEM_H_

#include <memory>

#include <rex/graphics/command_processor.h>
#include <rex/graphics/d3d12/deferred_command_list.h>
#include <rex/graphics/graphics_system.h>
#include <rex/graphics/xe_compat.h>

namespace rex::graphics::d3d12 {

class D3D12GraphicsSystem : public GraphicsSystem {
 public:
  D3D12GraphicsSystem();
  ~D3D12GraphicsSystem() override;

  static bool IsAvailable();

  std::string name() const override;

  X_STATUS Setup(cpu::Processor* processor, kernel::KernelState* kernel_state,
                 ui::WindowedAppContext* app_context,
                 bool with_presentation) override;

 protected:
  std::unique_ptr<CommandProcessor> CreateCommandProcessor() override;
};

}  // namespace rex::graphics::d3d12

#endif  // XENIA_GPU_D3D12_D3D12_GRAPHICS_SYSTEM_H_
