/**
 * @file        rexglue/commands/init_command.cpp
 * @brief       Project initialization command implementation
 *
 * @copyright   Copyright (c) 2026 Tom Clay
 * @license     BSD 3-Clause License
 */

#include "init_command.h"
#include "../ui/ui.h"
#include "template_utils.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <rex/codegen/manifest.h>
#include <rex/codegen/template_registry.h>
#include <rex/kernel/init.h>
#include <rex/logging.h>
#include <rex/result.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/util/xdbf_utils.h>
#include <rex/version.h>

#include <CLI/CLI.hpp>
#include <fmt/chrono.h>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

namespace fs = std::filesystem;

namespace rexglue::cli {

using rex::Err;
using rex::ErrorCategory;
using rex::Ok;

namespace {

std::string LowercaseAscii(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string ManifestPath(const fs::path& target, const fs::path& base) {
  std::error_code ec;
  fs::path rel = fs::relative(target, base, ec);
  if (ec || rel.empty()) {
    return LowercaseAscii(target.generic_string());
  }
  return LowercaseAscii(rel.generic_string());
}

std::string ModuleStem(const fs::path& xex) {
  std::string stem = LowercaseAscii(xex.stem().string());
  std::replace(stem.begin(), stem.end(), '.', '_');
  std::replace(stem.begin(), stem.end(), ' ', '_');
  return stem;
}

std::string IsoUtcStamp() {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  return fmt::format("{:%Y-%m-%d %H:%M:%S} UTC", fmt::gmtime(t));
}

fs::path ResolveDir(const std::string& raw, std::error_code& ec) {
  fs::path p = fs::absolute(raw, ec);
  if (ec)
    return p;
  fs::path canon = fs::weakly_canonical(p, ec);
  if (ec) {
    ec.clear();
    return p;
  }
  return canon;
}

std::string BinaryConfigRel(const std::string& stem) {
  return "config/" + stem + ".toml";
}

std::string TrimAscii(std::string_view s) {
  const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
  while (!s.empty() && is_space(static_cast<unsigned char>(s.front())))
    s.remove_prefix(1);
  while (!s.empty() && is_space(static_cast<unsigned char>(s.back())))
    s.remove_suffix(1);
  return std::string(s);
}

std::vector<std::string> SplitLines(const std::string& content) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= content.size()) {
    std::size_t nl = content.find('\n', start);
    if (nl == std::string::npos) {
      lines.push_back(content.substr(start));
      break;
    }
    lines.push_back(content.substr(start, nl - start));
    start = nl + 1;
  }
  return lines;
}

std::string JoinLines(const std::vector<std::string>& lines) {
  std::string out;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    out += lines[i];
    if (i + 1 < lines.size())
      out.push_back('\n');
  }
  return out;
}

// Quoted entries of a single-line TOML string array. The manifest only ever
// holds generated config paths here, so escapes are not a concern.
std::vector<std::string> ParseInlineStringArray(std::string_view value) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while ((i = value.find('"', i)) != std::string_view::npos) {
    std::size_t j = value.find('"', i + 1);
    if (j == std::string_view::npos)
      break;
    out.emplace_back(value.substr(i + 1, j - i - 1));
    i = j + 1;
  }
  return out;
}

std::string FormatIncludes(const std::vector<std::string>& entries) {
  std::string out = "[";
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (i > 0)
      out += ", ";
    out += "\"" + entries[i] + "\"";
  }
  out += "]";
  return out;
}

// Section header lines partition the manifest; a section runs until the next
// one. Returns [header_line, one_past_last_body_line).
std::pair<std::size_t, std::size_t> SectionRange(const std::vector<std::string>& lines,
                                                 std::size_t header_index) {
  std::size_t end = header_index + 1;
  while (end < lines.size() && !TrimAscii(lines[end]).starts_with("["))
    ++end;
  return {header_index, end};
}

// Add `entry` to the includes array between [begin, end), rewriting the
// existing key in place. Already-present entries are left alone, so repeating
// a command never duplicates a wiring. Returns true when a line changed.
bool EnsureIncludesInRange(std::vector<std::string>& lines, std::size_t begin, std::size_t end,
                           const std::string& entry) {
  for (std::size_t i = begin; i < end && i < lines.size(); ++i) {
    const std::string trimmed = TrimAscii(lines[i]);
    const std::size_t eq = trimmed.find('=');
    if (eq == std::string::npos || TrimAscii(trimmed.substr(0, eq)) != "includes")
      continue;
    const std::string value = TrimAscii(trimmed.substr(eq + 1));
    if (value.find(']') == std::string::npos) {
      REXLOG_WARN("Leaving multi-line includes array untouched; add '{}' by hand", entry);
      return false;
    }
    std::vector<std::string> entries = ParseInlineStringArray(value);
    if (std::find(entries.begin(), entries.end(), entry) != entries.end())
      return false;
    entries.push_back(entry);
    lines[i] = "includes = " + FormatIncludes(entries);
    return true;
  }
  lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(begin) + 1,
               "includes = " + FormatIncludes({entry}));
  return true;
}

// Wire config/<stem>.toml into the [entrypoint] section.
bool WireEntrypointConfig(std::string& content, const std::string& entry) {
  std::vector<std::string> lines = SplitLines(content);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (TrimAscii(lines[i]) != "[entrypoint]")
      continue;
    auto [begin, end] = SectionRange(lines, i);
    if (!EnsureIncludesInRange(lines, begin, end, entry))
      return false;
    content = JoinLines(lines);
    return true;
  }
  return false;
}

// Include resolution fails hard on a missing file, so a deleted config makes
// the manifest unloadable - including by the command whose job is to create
// configs. Restore blanks for the generated `config/` entries before loading.
// Includes outside `config/` are left alone so a genuine typo still errors.
Result<void> RestoreMissingConfigIncludes(const fs::path& manifest_path) {
  toml::table tbl;
  try {
    tbl = toml::parse_file(manifest_path.string());
  } catch (const toml::parse_error&) {
    return Ok();  // Loading reports the parse error with better context.
  }
  const fs::path root = manifest_path.parent_path();

  const auto restore = [&](const toml::table& section) -> Result<void> {
    const auto* includes = section["includes"].as_array();
    if (!includes)
      return Ok();
    for (const auto& item : *includes) {
      const auto rel = item.value_or<std::string>("");
      if (rel.empty() || !std::string_view{rel}.starts_with("config/"))
        continue;
      const fs::path abs = root / rel;
      if (fs::exists(abs))
        continue;
      std::error_code ec;
      fs::create_directories(abs.parent_path(), ec);
      if (ec) {
        return Err<void>(ErrorCategory::IO, fmt::format("Failed to create {}: {}",
                                                        abs.parent_path().string(), ec.message()));
      }
      if (!write_file_atomic(abs, ""))
        return Err<void>(ErrorCategory::IO, "Failed to write " + abs.string());
      REXLOG_INFO("Restored missing config {}", rel);
    }
    return Ok();
  };

  if (const auto* entrypoint = tbl["entrypoint"].as_table()) {
    if (auto r = restore(*entrypoint); !r)
      return r;
  }
  if (const auto* modules = tbl["modules"].as_array()) {
    for (const auto& mod : *modules) {
      if (const auto* modTbl = mod.as_table()) {
        if (auto r = restore(*modTbl); !r)
          return r;
      }
    }
  }
  return Ok();
}

// Wire config/<stem>.toml into the [[modules]] block whose file_path matches.
bool WireModuleConfig(std::string& content, const std::string& file_path,
                      const std::string& entry) {
  std::vector<std::string> lines = SplitLines(content);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (TrimAscii(lines[i]) != "[[modules]]")
      continue;
    auto [begin, end] = SectionRange(lines, i);
    bool matches = false;
    for (std::size_t j = begin; j < end; ++j) {
      const std::string trimmed = TrimAscii(lines[j]);
      const std::size_t eq = trimmed.find('=');
      if (eq == std::string::npos || TrimAscii(trimmed.substr(0, eq)) != "file_path")
        continue;
      auto values = ParseInlineStringArray(TrimAscii(trimmed.substr(eq + 1)));
      matches = !values.empty() && values.front() == file_path;
      break;
    }
    if (!matches)
      continue;
    if (!EnsureIncludesInRange(lines, begin, end, entry))
      return false;
    content = JoinLines(lines);
    return true;
  }
  return false;
}

// Create a blank config/<stem>.toml when absent. Existing files are never
// overwritten.
Result<void> EnsureBinaryConfig(const fs::path& project_root, const std::string& stem) {
  const fs::path abs = project_root / "config" / (stem + ".toml");
  if (fs::exists(abs))
    return Ok();
  std::error_code ec;
  fs::create_directories(abs.parent_path(), ec);
  if (ec) {
    return Err<void>(ErrorCategory::IO, fmt::format("Failed to create {}: {}",
                                                    abs.parent_path().string(), ec.message()));
  }
  if (!write_file_atomic(abs, ""))
    return Err<void>(ErrorCategory::IO, "Failed to write " + abs.string());
  REXLOG_DEBUG("  Created {}", BinaryConfigRel(stem));
  return Ok();
}

}  // namespace

Result<void> InitProject(const InitOptions& opts, const CliContext& ctx) {
  (void)ctx;

  if (opts.project_name.empty())
    return Err<void>(ErrorCategory::Config, "--project-name is required");

  std::string validation_error;
  if (!validate_app_name(opts.project_name, validation_error))
    return Err<void>(ErrorCategory::Config, validation_error);
  auto names = parse_app_name(opts.project_name);

  std::error_code ec;
  fs::path projectRoot =
      opts.project_root.empty() ? fs::current_path(ec) : ResolveDir(opts.project_root, ec);
  if (ec)
    return Err<void>(ErrorCategory::IO, "Failed to resolve project root: " + ec.message());

  const bool xex_defaulted = opts.xex_path.empty();
  fs::path xexAbs;
  if (xex_defaulted) {
    xexAbs = projectRoot / "assets" / "default.xex";
    fs::path canon = fs::weakly_canonical(xexAbs, ec);
    if (!ec)
      xexAbs = canon;
    ec.clear();
  } else {
    xexAbs = ResolveDir(opts.xex_path, ec);
    if (ec)
      return Err<void>(ErrorCategory::IO,
                       "Failed to resolve --xex-path '" + opts.xex_path + "': " + ec.message());
  }
  const bool xex_present = fs::exists(xexAbs);
  if (!xex_defaulted) {
    if (!xex_present)
      return Err<void>(ErrorCategory::IO, "Entrypoint XEX not found: " + xexAbs.string());
    if (!fs::is_regular_file(xexAbs))
      return Err<void>(ErrorCategory::IO, "--xex-path is not a regular file: " + xexAbs.string());
  } else if (xex_present && !fs::is_regular_file(xexAbs)) {
    return Err<void>(
        ErrorCategory::IO,
        "Default entrypoint path exists but is not a regular file: " + xexAbs.string());
  }

  std::string xexStem = xexAbs.stem().string();
  if (xexStem.empty())
    return Err<void>(ErrorCategory::Config, "--xex-path has no filename: " + opts.xex_path);

  fs::path gameRootAbs;
  if (opts.game_root.empty()) {
    gameRootAbs = xexAbs.parent_path();
  } else {
    gameRootAbs = ResolveDir(opts.game_root, ec);
    if (ec)
      return Err<void>(ErrorCategory::IO,
                       "Failed to resolve --game-root '" + opts.game_root + "': " + ec.message());
  }
  // The default game root (assets/) is created during scaffolding when the
  // defaulted entrypoint does not exist yet; an explicit --game-root must.
  const bool scaffold_game_root = xex_defaulted && !xex_present && opts.game_root.empty();
  if (!scaffold_game_root && (!fs::exists(gameRootAbs) || !fs::is_directory(gameRootAbs)))
    return Err<void>(ErrorCategory::IO, "--game-root is not a directory: " + gameRootAbs.string());

  fs::path xexRelToGame = fs::relative(xexAbs, gameRootAbs, ec);
  if (ec || xexRelToGame.empty() || *xexRelToGame.begin() == fs::path("..")) {
    return Err<void>(ErrorCategory::Config,
                     "Entrypoint XEX (" + xexAbs.string() + ") is not inside the game root (" +
                         gameRootAbs.string() + "); pass --xex-path for a custom layout");
  }

  std::string outDir = LowercaseAscii("generated/" + xexStem);
  std::string xexRelManifest = ManifestPath(xexAbs, projectRoot);
  std::string gameRootRelManifest = ManifestPath(gameRootAbs, projectRoot);

  // Binaries that receive a config/<stem>.toml when --with-config is set.
  // The includes strings go into the manifest; the files are written after
  // the overwrite gate below.
  std::vector<std::string> config_stems;
  auto includes_for = [&](const std::string& stem) -> std::string {
    if (!opts.with_config)
      return "[]";
    config_stems.push_back(stem);
    return "[\"" + BinaryConfigRel(stem) + "\"]";
  };
  std::string entrypointIncludes = includes_for(ModuleStem(xexAbs));

  nlohmann::json modulesJson = nlohmann::json::array();
  if (opts.scan_dlls && scaffold_game_root) {
    REXLOG_WARN("--scan-dll skipped: game root '{}' does not exist yet", gameRootAbs.string());
  } else if (opts.scan_dlls) {
    std::vector<fs::path> dllPaths;
    fs::recursive_directory_iterator it(gameRootAbs, fs::directory_options::skip_permission_denied,
                                        ec);
    fs::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
      if (!it->is_regular_file())
        continue;
      if (LowercaseAscii(it->path().extension().string()) != ".dll")
        continue;
      dllPaths.push_back(it->path());
    }
    std::sort(dllPaths.begin(), dllPaths.end());

    for (const auto& dllAbs : dllPaths) {
      fs::path relUnderGame = fs::relative(dllAbs, gameRootAbs, ec);
      if (ec || relUnderGame.empty()) {
        ec.clear();
        continue;
      }
      modulesJson.push_back({
          {"guest_path", rex::codegen::CanonicalizeModuleGuestPath(relUnderGame.generic_string(),
                                                                   names.snake_case)},
          {"file_path", ManifestPath(dllAbs, projectRoot)},
          {"out_directory_path", "generated/" + ModuleStem(dllAbs)},
          {"includes", includes_for(ModuleStem(dllAbs))},
      });
    }
  }

  rex::codegen::TemplateRegistry registry;
  if (!opts.template_dir.empty())
    registry.loadOverrides(opts.template_dir);

  nlohmann::json data = {
      {"names", names_to_json(names)},
      {"sdk_version", REXGLUE_VERSION_FLOOR},
      {"sdk_version_full", REXGLUE_VERSION_STRING},
      {"generated_on", IsoUtcStamp()},
      {"include_stamp", true},
      {"xex_path", xexRelManifest},
      {"game_root", gameRootRelManifest},
      {"out_directory_path", outDir},
      {"entrypoint_out_dir", outDir},
      {"entrypoint_includes", entrypointIncludes},
      {"modules", modulesJson},
  };
  std::string jsonStr = data.dump();

  std::vector<ui::KeyValueRow> header_rows;
  header_rows.push_back({"Project", names.snake_case});
  header_rows.push_back({"Root", projectRoot.string()});
  header_rows.push_back(
      {"Entrypoint", xex_present ? xexRelManifest : xexRelManifest + " (not found yet)"});
  header_rows.push_back({"Game root", gameRootRelManifest});
  if (opts.scan_dlls) {
    header_rows.push_back({"DLL modules", std::to_string(modulesJson.size())});
  }
  ui::KeyValueBlock("Initializing project:", header_rows);

  if (fs::exists(projectRoot) && !fs::is_directory(projectRoot)) {
    return Err<void>(ErrorCategory::IO,
                     "Path exists but is not a directory: " + projectRoot.string());
  }

  enum class RegeneratePolicy { AlwaysRegenerate, FirstInitOnly, RequiresForce };
  struct Render {
    std::string template_id;
    fs::path out;
    RegeneratePolicy policy;
  };
  std::string app_header = names.snake_case + "_app.h";
  std::string manifest_file = names.snake_case + "_manifest.toml";
  std::vector<Render> renders = {
      {"init/cmakelists", projectRoot / "CMakeLists.txt", RegeneratePolicy::RequiresForce},
      {"init/rexglue_cmake", projectRoot / "generated" / "rexglue.cmake",
       RegeneratePolicy::AlwaysRegenerate},
      {"init/main_cpp", projectRoot / "src" / "main.cpp", RegeneratePolicy::FirstInitOnly},
      {"init/app_header", projectRoot / "src" / app_header, RegeneratePolicy::FirstInitOnly},
      {"init/manifest_toml", projectRoot / manifest_file, RegeneratePolicy::RequiresForce},
      {"init/cmake_presets", projectRoot / "CMakePresets.json", RegeneratePolicy::RequiresForce},
  };

  if (!opts.force) {
    std::vector<std::string> blocked;
    for (const auto& r : renders) {
      if (r.policy == RegeneratePolicy::RequiresForce && fs::exists(r.out)) {
        blocked.push_back(fs::relative(r.out, projectRoot).generic_string());
      }
    }
    if (!blocked.empty()) {
      std::string msg = "Existing project files would be overwritten. Use --force to proceed:";
      for (const auto& path : blocked) {
        msg += "\n  - " + path;
      }
      return Err<void>(ErrorCategory::IO, msg);
    }
  }

  REXLOG_TRACE("Creating directory structure...");
  std::vector<fs::path> dirs = {projectRoot, projectRoot / "src", projectRoot / "generated"};
  if (scaffold_game_root)
    dirs.push_back(gameRootAbs);
  for (const auto& dir : dirs) {
    fs::create_directories(dir, ec);
    if (ec) {
      return Err<void>(ErrorCategory::IO,
                       fmt::format("Failed to create {}: {}", dir.string(), ec.message()));
    }
  }

  for (const auto& stem : config_stems) {
    if (auto cfg = EnsureBinaryConfig(projectRoot, stem); !cfg)
      return cfg;
  }

  REXLOG_TRACE("Generating project files...");
  for (const auto& r : renders) {
    if (r.policy == RegeneratePolicy::FirstInitOnly && fs::exists(r.out)) {
      REXLOG_DEBUG("  Skipped {} (preserving user content)",
                   fs::relative(r.out, projectRoot).generic_string());
      continue;
    }
    if (!write_file_atomic(r.out, registry.render(r.template_id, jsonStr))) {
      return Err<void>(ErrorCategory::IO, "Failed to write " + r.out.string());
    }
    REXLOG_DEBUG("  Created {}", fs::relative(r.out, projectRoot).generic_string());
  }
  if (xex_defaulted && !xex_present) {
    REXLOG_WARN("Entrypoint XEX set to default path '{}'. Insert XEX file before building.",
                xexRelManifest);
  }
  return Ok();
}

Result<void> InitModule(const InitModuleOptions& opts, const CliContext& ctx) {
  (void)ctx;

  // --with-config on its own is a valid request: it covers the entrypoint and
  // every module the manifest already lists, which is all a title with no DLLs
  // needs.
  const bool config_only = !opts.scan && opts.dll_path.empty();
  if (config_only && !opts.with_config)
    return Err<void>(ErrorCategory::Config,
                     "Pass a DLL path, --scan to discover them, or --with-config to create "
                     "configs for what the manifest already lists");
  if (opts.scan && !opts.dll_path.empty())
    return Err<void>(ErrorCategory::Config, "--scan does not take a DLL path");
  if (opts.scan && !opts.guest_path.empty())
    return Err<void>(ErrorCategory::Config, "--guest-path applies to a single DLL, not --scan");

  auto located = LocateManifest(opts.manifest_path);
  if (!located)
    return Err<void>(located.error());
  fs::path manifestPath = *located;

  if (opts.with_config) {
    if (auto restored = RestoreMissingConfigIncludes(manifestPath); !restored)
      return restored;
  }

  auto manifest = rex::codegen::ManifestConfig::Load(manifestPath);
  if (!manifest)
    return Err<void>(ErrorCategory::Config, "Failed to load manifest: " + manifestPath.string());
  const fs::path root = manifest->manifestDir;

  std::error_code ec;
  fs::path gameRootAbs =
      manifest->gameRoot
          ? fs::weakly_canonical(root / *manifest->gameRoot, ec)
          : fs::weakly_canonical(root / manifest->entrypoint.recompiler.filePath, ec).parent_path();
  ec.clear();

  std::vector<fs::path> dlls;
  if (opts.scan) {
    if (!fs::exists(gameRootAbs) || !fs::is_directory(gameRootAbs)) {
      return Err<void>(ErrorCategory::IO,
                       "Manifest game root is not a directory: " + gameRootAbs.string());
    }
    fs::recursive_directory_iterator it(gameRootAbs, fs::directory_options::skip_permission_denied,
                                        ec);
    fs::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
      if (!it->is_regular_file())
        continue;
      if (LowercaseAscii(it->path().extension().string()) != ".dll")
        continue;
      dlls.push_back(it->path());
    }
    std::sort(dlls.begin(), dlls.end());
    if (dlls.empty())
      REXLOG_INFO("No .dll files found under {}", gameRootAbs.string());
  } else if (!config_only) {
    fs::path dllAbs = fs::weakly_canonical(fs::absolute(opts.dll_path), ec);
    ec.clear();
    if (!fs::exists(dllAbs) || !fs::is_regular_file(dllAbs))
      return Err<void>(ErrorCategory::IO, "DLL not found: " + opts.dll_path);
    dlls.push_back(dllAbs);
  }

  toml::table manifestTbl;
  try {
    manifestTbl = toml::parse_file(manifestPath.string());
  } catch (const toml::parse_error& err) {
    return Err<void>(ErrorCategory::Config, fmt::format("Manifest parse error: {}", err.what()));
  }
  std::vector<std::pair<std::string, std::string>> existing;  // file_path, guest_path
  if (auto* modulesArr = manifestTbl["modules"].as_array()) {
    for (const auto& mod : *modulesArr) {
      if (auto* modTbl = mod.as_table()) {
        existing.emplace_back((*modTbl)["file_path"].value_or<std::string>(""),
                              (*modTbl)["guest_path"].value_or<std::string>(""));
      }
    }
  }

  std::string appended;
  // Owned stem/guest-path pairs; KeyValueRow keys are string_views, so the
  // display rows are built from this storage after the loop.
  std::vector<std::pair<std::string, std::string>> added;
  // Modules this run touched that the manifest already lists, so --with-config
  // can backfill a config for them without adding a second entry. A standalone
  // --with-config touches every listed module.
  std::vector<std::string> already_listed;
  if (config_only) {
    for (const auto& [file_path, guest_path] : existing) {
      if (!file_path.empty())
        already_listed.push_back(file_path);
    }
  }
  std::size_t skipped = 0;
  for (const auto& dllAbs : dlls) {
    fs::path relUnderGame = fs::relative(dllAbs, gameRootAbs, ec);
    const bool under_game_root =
        !ec && !relUnderGame.empty() && *relUnderGame.begin() != fs::path("..");
    ec.clear();

    std::string guestPath;
    if (!opts.guest_path.empty()) {
      guestPath = rex::codegen::CanonicalizeModuleGuestPath(opts.guest_path, manifest->projectName);
    } else if (under_game_root) {
      guestPath = rex::codegen::CanonicalizeModuleGuestPath(relUnderGame.generic_string(),
                                                            manifest->projectName);
    } else {
      return Err<void>(ErrorCategory::Config,
                       "DLL is outside the manifest game root (" + gameRootAbs.string() +
                           "); pass --guest-path explicitly: " + dllAbs.string());
    }

    std::string fileRel = ManifestPath(dllAbs, root);
    const auto match = std::find_if(existing.begin(), existing.end(), [&](const auto& e) {
      return e.first == fileRel || e.second == guestPath;
    });
    if (match != existing.end()) {
      ++skipped;
      already_listed.push_back(match->first);
      continue;
    }
    existing.emplace_back(fileRel, guestPath);

    std::string stem = ModuleStem(dllAbs);
    std::string includes = "[]";
    if (opts.with_config) {
      if (auto cfg = EnsureBinaryConfig(root, stem); !cfg)
        return cfg;
      includes = "[\"" + BinaryConfigRel(stem) + "\"]";
    }
    appended += fmt::format(
        "\n[[modules]]\nguest_path = \"{}\"\nfile_path = \"{}\"\nout_directory_path = "
        "\"generated/{}\"\nincludes = {}\n",
        guestPath, fileRel, stem, includes);
    added.emplace_back(std::move(stem), guestPath);
  }

  std::ifstream in(manifestPath);
  if (!in)
    return Err<void>(ErrorCategory::IO,
                     "Failed to open manifest for reading: " + manifestPath.string());
  std::stringstream ss;
  ss << in.rdbuf();
  std::string content = ss.str();
  in.close();

  bool wired = false;
  if (opts.with_config) {
    // The entrypoint gets a config too, so --with-config leaves every binary
    // in the manifest covered regardless of which command created it.
    const std::string entryStem = ModuleStem(manifest->entrypoint.recompiler.filePath);
    if (!entryStem.empty()) {
      if (auto cfg = EnsureBinaryConfig(root, entryStem); !cfg)
        return cfg;
      wired |= WireEntrypointConfig(content, BinaryConfigRel(entryStem));
    }
    for (const auto& fileRel : already_listed) {
      const std::string stem = ModuleStem(fileRel);
      if (auto cfg = EnsureBinaryConfig(root, stem); !cfg)
        return cfg;
      wired |= WireModuleConfig(content, fileRel, BinaryConfigRel(stem));
    }
  }

  if (appended.empty() && !wired) {
    if (config_only)
      REXLOG_INFO("Every binary in {} already has a config.", manifestPath.filename().string());
    else
      REXLOG_INFO("Manifest already lists {} module(s); nothing to do.", skipped);
    return Ok();
  }

  if (!appended.empty()) {
    if (!content.empty() && content.back() != '\n')
      content.push_back('\n');
    content += appended;
  }

  fs::path tmpPath = manifestPath;
  tmpPath += ".tmp";
  {
    std::ofstream out(tmpPath, std::ios::binary);
    if (!out)
      return Err<void>(ErrorCategory::IO,
                       "Failed to open manifest tmp for writing: " + tmpPath.string());
    out << content;
    if (!out.good()) {
      std::error_code ignore;
      fs::remove(tmpPath, ignore);
      return Err<void>(ErrorCategory::IO, "Failed while writing manifest tmp: " + tmpPath.string());
    }
  }
  fs::rename(tmpPath, manifestPath, ec);
  if (ec) {
    std::error_code ignore;
    fs::remove(tmpPath, ignore);
    return Err<void>(ErrorCategory::IO,
                     "Failed to rename manifest tmp into place: " + ec.message());
  }

  if (!added.empty()) {
    std::vector<ui::KeyValueRow> rows;
    rows.reserve(added.size());
    for (const auto& [stem, guest] : added) {
      rows.push_back({stem, guest});
    }
    ui::KeyValueBlock(
        fmt::format("Added {} module(s) to {}:", rows.size(), manifestPath.filename().string()),
        rows);
  }
  if (added.empty() && wired) {
    REXLOG_INFO("Wired configs into {}", manifestPath.filename().string());
  }
  if (skipped > 0) {
    REXLOG_INFO("{} module(s) already listed; skipped.", skipped);
  }
  return Ok();
}

Result<void> InitAchievements(const InitAchievementsOptions& opts, const CliContext& /*ctx*/) {
  std::error_code ec;
  auto located = LocateManifest(opts.manifest_path);
  if (!located)
    return Err<void>(located.error());
  fs::path manifestPath = *located;
  auto manifest = rex::codegen::ManifestConfig::Load(manifestPath);
  if (!manifest) {
    return Err<void>(ErrorCategory::Config, "Failed to load manifest: " + manifestPath.string());
  }
  if (manifest->entrypoint.recompiler.filePath.empty()) {
    return Err<void>(ErrorCategory::Config,
                     "Manifest has no [entrypoint] file_path: " + manifestPath.string());
  }

  // Manifest paths are relative to the manifest's directory.
  fs::path xexPath =
      fs::weakly_canonical(manifest->manifestDir / manifest->entrypoint.recompiler.filePath, ec);
  ec.clear();
  if (!fs::exists(xexPath)) {
    return Err<void>(ErrorCategory::IO,
                     fmt::format("Entrypoint XEX not found: {} (from manifest {})",
                                 xexPath.string(), manifestPath.string()));
  }
  xexPath = fs::canonical(xexPath, ec);

  fs::path outDir =
      opts.output_dir.empty() ? xexPath.parent_path() : fs::absolute(opts.output_dir, ec);
  fs::create_directories(outDir, ec);
  if (ec) {
    return Err<void>(ErrorCategory::IO, "Cannot create output directory: " + outDir.string());
  }

  // Build VFS path: game:\ maps to the manifest's game root (or the XEX's
  // parent directory when the manifest does not record one).
  fs::path gameRoot = manifest->gameRoot
                          ? fs::weakly_canonical(manifest->manifestDir / *manifest->gameRoot, ec)
                          : xexPath.parent_path();
  ec.clear();
  fs::path entryRel = fs::relative(xexPath, gameRoot, ec);
  if (ec || entryRel.empty() || *entryRel.begin() == fs::path("..")) {
    return Err<void>(ErrorCategory::Config,
                     fmt::format("Entrypoint XEX ({}) is not inside the manifest game root ({})",
                                 xexPath.string(), gameRoot.string()));
  }
  std::string entryRelStr = entryRel.generic_string();
  std::replace(entryRelStr.begin(), entryRelStr.end(), '/', '\\');
  auto entryVfsPath = "game:\\" + entryRelStr;

  auto runtime = std::make_unique<rex::Runtime>(gameRoot.string());
  auto rtStatus = runtime->Setup(rex::RuntimeConfig{
      .kernel_init = rex::kernel::InitializeKernel,
      .tool_mode = true,
  });
  if (!XSUCCEEDED(rtStatus)) {
    return Err<void>(ErrorCategory::IO, fmt::format("Runtime init failed: {:#x}", rtStatus));
  }

  rtStatus = runtime->LoadXexImage(entryVfsPath);
  if (!XSUCCEEDED(rtStatus)) {
    return Err<void>(ErrorCategory::IO,
                     fmt::format("Failed to load XEX '{}': {:#x}", xexPath.string(), rtStatus));
  }

  const rex::system::util::XdbfGameData db = runtime->kernel_state()->title_xdbf();
  if (!db.is_valid()) {
    return Err<void>(ErrorCategory::IO, "No XDBF resource found in XEX (title_id section absent)");
  }

  const rex::system::XLanguage lang =
      db.GetExistingLanguage(static_cast<rex::system::XLanguage>(opts.language));
  const auto achievements = db.GetAchievements();
  const uint32_t title_id = runtime->kernel_state()->title_id();

  auto escape_toml = [](const std::string& s) -> std::string {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
      unsigned char ch = static_cast<unsigned char>(c);
      if (c == '"')
        out += "\\\"";
      else if (c == '\\')
        out += "\\\\";
      else if (c == '\b')
        out += "\\b";
      else if (c == '\t')
        out += "\\t";
      else if (c == '\n')
        out += "\\n";
      else if (c == '\f')
        out += "\\f";
      else if (c == '\r')
        out += "\\r";
      else if (ch < 0x20 || ch == 0x7F)
        out += fmt::format("\\u{:04X}", ch);
      else
        out += c;
    }
    return out;
  };

  std::string content;
  content += fmt::format("# Achievements metadata \xe2\x80\x94 auto-generated by rexglue\n");
  content += fmt::format("# Source:    {}\n", xexPath.filename().string());
  content += fmt::format("# Title ID:  {:08X}\n", title_id);
  content += fmt::format("# Language:  {}\n", static_cast<uint32_t>(lang));
  content += fmt::format("# Generated: {}\n", IsoUtcStamp());
  content += fmt::format("# {} achievements\n\n", achievements.size());

  for (const auto& entry : achievements) {
    content += "[[achievements]]\n";
    content += fmt::format("id                    = {}\n", uint32_t(entry.id));
    content += fmt::format("label                 = \"{}\"\n",
                           escape_toml(db.GetStringTableEntry(lang, entry.label_id)));
    content += fmt::format("description           = \"{}\"\n",
                           escape_toml(db.GetStringTableEntry(lang, entry.description_id)));
    content += fmt::format("unachieved_description = \"{}\"\n",
                           escape_toml(db.GetStringTableEntry(lang, entry.unachieved_id)));
    content += fmt::format("gamerscore            = {}\n", uint32_t(entry.gamerscore));
    content += fmt::format("image_id              = {}\n", uint32_t(entry.image_id));
    content += fmt::format("icon_path             = \"icons/{}.png\"\n", uint32_t(entry.image_id));
    content += fmt::format("flags                 = {}\n\n", uint32_t(entry.flags));
  }

  fs::path outPath = outDir / "achievements.toml";
  {
    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
      return Err<void>(ErrorCategory::IO, "Failed to open for writing: " + outPath.string());
    }
    out << content;
    if (!out.good()) {
      return Err<void>(ErrorCategory::IO, "Write error: " + outPath.string());
    }
  }

  REXLOG_INFO("Wrote {} achievements to {}", achievements.size(), outPath.string());

  // ---- Icons ---------------------------------------------------------------
  fs::path iconsDir = outDir / "icons";
  fs::create_directories(iconsDir, ec);
  if (ec) {
    REXLOG_WARN("Could not create icons directory: {}", ec.message());
  } else {
    int icons_written = 0;
    int icons_missing = 0;
    std::vector<uint32_t> seen;

    for (const auto& entry : achievements) {
      uint32_t img_id = entry.image_id;
      if (std::find(seen.begin(), seen.end(), img_id) != seen.end())
        continue;
      seen.push_back(img_id);

      auto block =
          db.GetEntry(rex::system::util::XdbfSection::kImage, static_cast<uint64_t>(img_id));
      if (!block || !block.buffer || block.size == 0) {
        ++icons_missing;
        continue;
      }

      fs::path icon_path = iconsDir / fmt::format("{}.png", img_id);
      std::ofstream f(icon_path, std::ios::binary);
      if (!f) {
        REXLOG_WARN("Cannot write icon {}", icon_path.string());
        continue;
      }
      f.write(reinterpret_cast<const char*>(block.buffer),
              static_cast<std::streamsize>(block.size));
      if (f.good())
        ++icons_written;
    }

    if (icons_written > 0) {
      REXLOG_INFO("Wrote {} icon(s) to {}", icons_written, iconsDir.string());
    }
    if (icons_missing > 0) {
      REXLOG_WARN("{} achievement(s) had no icon in XDBF", icons_missing);
    }

    // Game title icon (not tied to any achievement).
    if (auto game_icon = db.icon(); game_icon && game_icon.size > 0) {
      fs::path game_icon_path = iconsDir / "title.png";
      std::ofstream f(game_icon_path, std::ios::binary);
      if (f) {
        f.write(reinterpret_cast<const char*>(game_icon.buffer),
                static_cast<std::streamsize>(game_icon.size));
      }
      if (f && f.good()) {
        REXLOG_INFO("Wrote game icon to {}", game_icon_path.string());
      } else {
        REXLOG_WARN("Cannot write game icon {}", game_icon_path.string());
      }
    } else {
      REXLOG_WARN("No game icon found in XDBF");
    }
  }

  return Ok();
}

namespace {

struct InitArgs {
  std::string project_name;
  std::string project_root;
  std::string xex_path;
  std::string game_root;
  std::string template_dir;
  bool scan_dll = false;
  bool with_config = false;
};

struct InitModuleArgs {
  std::string manifest_path;
  std::string dll_path;
  std::string guest_path;
  bool scan = false;
  bool with_config = false;
};

struct InitAchievementsArgs {
  std::string manifest_path;
  std::string output_dir;
  uint32_t language = static_cast<uint32_t>(rex::system::XLanguage::kEnglish);
};

}  // namespace

void RegisterInit(CLI::App& parent, const CliContext& ctx, DeferredAction& pending) {
  auto* init = parent.add_subcommand("init", "Initialize a new project")->fallthrough();
  auto args = std::make_shared<InitArgs>();
  init->add_option("--project-name", args->project_name,
                   "Project name (becomes [project].name in the manifest)")
      ->type_name("NAME");
  init->add_option("--xex-path", args->xex_path,
                   "Path to entrypoint XEX (default: assets/default.xex under the project root)")
      ->type_name("PATH");
  init->add_option("--game-root", args->game_root, "Game asset root for DLL guest-path derivation")
      ->type_name("PATH");
  init->add_option("--project-root", args->project_root,
                   "Where to create the project (defaults to current directory)")
      ->type_name("PATH");
  init->add_flag("--scan-dll", args->scan_dll,
                 "Scan --game-root for .dll files and add each as a [[modules]] entry");
  init->add_flag("--with-config", args->with_config,
                 "Create config/<binary>.toml for the entrypoint and each scanned module, "
                 "wired into the manifest's includes");
  init->add_option("--template-dir", args->template_dir, "Custom template directory for overrides")
      ->type_name("PATH");

  auto* mod = init->add_subcommand("module", "Add DLL modules to an existing project manifest")
                  ->fallthrough();
  auto modArgs = std::make_shared<InitModuleArgs>();
  mod->add_option("dll", modArgs->dll_path, "Path to the DLL XEX to add (omit with --scan)")
      ->type_name("PATH");
  mod->add_option("--manifest", modArgs->manifest_path,
                  "Project manifest TOML (auto-discovered in cwd if omitted)")
      ->type_name("PATH");
  mod->add_option("--guest-path", modArgs->guest_path,
                  "Guest path for XexLoadImage matching (derived from the game root when omitted)")
      ->type_name("PATH");
  mod->add_flag("--scan", modArgs->scan,
                "Discover .dll files under the manifest's game root and add missing entries");
  mod->add_flag("--with-config", modArgs->with_config,
                "Create config/<binary>.toml for the entrypoint and each module, wired into "
                "its includes; usable on its own to cover what the manifest already lists");

  init->callback([args, &ctx, &pending]() {
    if (pending)
      return;  // a subcommand already claimed the action
    pending = [args, &ctx]() -> Result<void> {
      InitOptions opts;
      opts.project_name = args->project_name;
      opts.project_root = args->project_root;
      opts.xex_path = args->xex_path;
      opts.game_root = args->game_root;
      opts.scan_dlls = args->scan_dll;
      opts.with_config = args->with_config;
      opts.template_dir = args->template_dir;
      opts.force = ctx.overwrite_existing;
      return InitProject(opts, ctx);
    };
  });
  mod->callback([modArgs, &ctx, &pending]() {
    pending = [modArgs, &ctx]() -> Result<void> {
      InitModuleOptions opts;
      opts.manifest_path = modArgs->manifest_path;
      opts.dll_path = modArgs->dll_path;
      opts.guest_path = modArgs->guest_path;
      opts.scan = modArgs->scan;
      opts.with_config = modArgs->with_config;
      return InitModule(opts, ctx);
    };
  });

  auto* ach = init->add_subcommand("achievements",
                                   "Extract achievement metadata from the project's entrypoint "
                                   "XEX to TOML")
                  ->fallthrough();
  auto achArgs = std::make_shared<InitAchievementsArgs>();
  ach->add_option("manifest", achArgs->manifest_path,
                  "Project manifest TOML whose [entrypoint] locates the XEX "
                  "(auto-discovered in cwd if omitted)");
  ach->add_option("output_dir", achArgs->output_dir,
                  "Output directory for achievements.toml (default: XEX directory)");
  ach->add_option("--language", achArgs->language,
                  "Language ID to extract strings for (1=English, 2=Japanese, 3=German, "
                  "4=French, 5=Spanish, 6=Italian, 7=Korean, 8=TChinese, 9=Portuguese, "
                  "10=SChinese, 11=Polish, 12=Russian; default: English)");
  ach->callback([achArgs, &ctx, &pending]() {
    pending = [achArgs, &ctx]() -> Result<void> {
      InitAchievementsOptions opts;
      opts.manifest_path = achArgs->manifest_path;
      opts.output_dir = achArgs->output_dir;
      opts.language = achArgs->language;
      return InitAchievements(opts, ctx);
    };
  });
}

}  // namespace rexglue::cli
