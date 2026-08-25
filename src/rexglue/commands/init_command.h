/**
 * @file        rexglue/commands/init_command.h
 * @brief       Project initialization command interface
 *
 * @copyright   Copyright (c) 2026 Tom Clay
 * @license     BSD 3-Clause License
 */

#pragma once

#include "../cli_utils.h"

#include <cstdint>
#include <string>

#include <rex/result.h>

namespace CLI {
class App;
}

namespace rexglue::cli {

using rex::Result;

struct InitOptions {
  std::string project_name;
  std::string xex_path;
  std::string game_root;
  std::string project_root;
  bool scan_dlls = false;
  bool with_config = false;
  std::string template_dir;
  bool force = false;
};

struct InitModuleOptions {
  std::string manifest_path;  // Empty: discover in cwd.
  std::string dll_path;       // Single DLL to add; empty with scan.
  std::string guest_path;     // Empty: derive from the game root.
  bool scan = false;
  bool with_config = false;
};

struct InitAchievementsOptions {
  std::string manifest_path;
  std::string output_dir;
  // XLanguage ID to extract strings for; defaults to English (1) when unset.
  uint32_t language = 1;
};

Result<void> InitProject(const InitOptions& opts, const CliContext& ctx);
Result<void> InitModule(const InitModuleOptions& opts, const CliContext& ctx);
Result<void> InitAchievements(const InitAchievementsOptions& opts, const CliContext& ctx);

void RegisterInit(CLI::App& parent, const CliContext& ctx, DeferredAction& pending);

}  // namespace rexglue::cli
