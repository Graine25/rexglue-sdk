/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2016 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_NULL_NULL_COMMAND_PROCESSOR_H_
#define XENIA_GPU_NULL_NULL_COMMAND_PROCESSOR_H_

#include <rex/graphics/command_processor.h>
#include <rex/graphics/null/graphics_system.h>
#include <rex/graphics/xenos.h>
#include <rex/system/kernel_state.h>
#include <rex/graphics/xe_compat.h>

namespace rex::graphics::null {

class NullCommandProcessor : public CommandProcessor {
 public:
  NullCommandProcessor(NullGraphicsSystem* graphics_system,
                       kernel::KernelState* kernel_state);
  ~NullCommandProcessor();

  void TracePlaybackWroteMemory(uint32_t base_ptr, uint32_t length) override;

  void RestoreEdramSnapshot(const void* snapshot) override;

 private:
  bool SetupContext() override;
  void ShutdownContext() override;

  void IssueSwap(uint32_t frontbuffer_ptr, uint32_t frontbuffer_width,
                 uint32_t frontbuffer_height) override;

  Shader* LoadShader(xenos::ShaderType shader_type, uint32_t guest_address,
                     const uint32_t* host_address,
                     uint32_t dword_count) override;

  bool IssueDraw(xenos::PrimitiveType prim_type, uint32_t index_count,
                 IndexBufferInfo* index_buffer_info,
                 bool major_mode_explicit) override;
  bool IssueCopy() override;

  void InitializeTrace() override;
};

}  // namespace rex::graphics::null

#endif  // XENIA_GPU_NULL_NULL_COMMAND_PROCESSOR_H_
