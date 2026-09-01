/**
 * @file        system/interfaces/input.h
 * @brief       Abstract input system interface for dependency injection
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#pragma once

#include <functional>

#include <rex/input/input.h>
#include <rex/system/xtypes.h>

namespace rex::ui {
class Window;
}

namespace rex::system {

/// Everything the host calls on an input system. The implementation lives in
/// a runtime-loaded plugin, so this is an ABI boundary: adding or reordering
/// methods requires bumping kInputPluginAbiVersion.
class IInputSystem {
 public:
  virtual ~IInputSystem() = default;
  virtual X_STATUS Setup() = 0;
  virtual void Shutdown() = 0;

  /// Called once the window exists, which is after Setup.
  virtual void AttachWindow(rex::ui::Window* window) = 0;
  /// Returns false while an overlay owns the pointer, silencing the drivers.
  virtual void SetActiveCallback(std::function<bool()> callback) = 0;

  virtual X_RESULT GetCapabilities(uint32_t user_index, uint32_t flags,
                                   rex::input::X_INPUT_CAPABILITIES* out_caps) = 0;
  virtual X_RESULT GetState(uint32_t user_index, rex::input::X_INPUT_STATE* out_state) = 0;
  virtual X_RESULT SetState(uint32_t user_index, rex::input::X_INPUT_VIBRATION* vibration) = 0;
  virtual X_RESULT GetKeystroke(uint32_t user_index, uint32_t flags,
                                rex::input::X_INPUT_KEYSTROKE* out_keystroke) = 0;

  virtual void AddUIInputBlocker() = 0;
  virtual void RemoveUIInputBlocker() = 0;
};

}  // namespace rex::system
