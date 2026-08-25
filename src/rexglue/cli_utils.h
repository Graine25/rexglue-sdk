/**
 * @file        rexglue/cli_utils.h
 * @brief       Shared types and helpers for rexglue CLI subcommands
 *
 * @copyright   Copyright (c) 2026 Tom Clay
 * @license     BSD 3-Clause License
 */

#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <rex/codegen/manifest.h>
#include <rex/result.h>

#include <fmt/format.h>

namespace rexglue::cli {

struct CliContext {
  bool verbose = false;
  bool overwrite_existing = false;
  bool generate_despite_errors = false;
};

using DeferredAction = std::function<rex::Result<void>()>;

/// Find the project manifest in the current directory: a unique *_manifest.toml
/// wins, then a unique remaining .toml. Errors when none or several qualify.
inline rex::Result<std::string> DiscoverManifestInCwd() {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path cwd = fs::current_path(ec);
  if (ec) {
    return rex::Err<std::string>(rex::ErrorCategory::IO,
                                 fmt::format("Cannot read current directory: {}", ec.message()));
  }

  std::vector<fs::path> manifests;
  std::vector<fs::path> other_tomls;
  for (const auto& entry : fs::directory_iterator(cwd, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file())
      continue;
    if (entry.path().extension() != ".toml")
      continue;
    auto stem = entry.path().stem().string();
    if (stem.size() >= 9 && std::string_view{stem}.substr(stem.size() - 9) == "_manifest") {
      manifests.push_back(entry.path());
    } else {
      other_tomls.push_back(entry.path());
    }
  }

  if (manifests.size() == 1)
    return rex::Ok(manifests.front().string());
  if (manifests.size() > 1) {
    return rex::Err<std::string>(
        rex::ErrorCategory::Config,
        fmt::format("Multiple *_manifest.toml files in {}; pass one explicitly", cwd.string()));
  }
  if (other_tomls.size() == 1)
    return rex::Ok(other_tomls.front().string());
  if (other_tomls.size() > 1) {
    return rex::Err<std::string>(
        rex::ErrorCategory::Config,
        fmt::format("Multiple .toml files in {}; pass the manifest explicitly", cwd.string()));
  }
  return rex::Err<std::string>(rex::ErrorCategory::Config,
                               fmt::format("No manifest .toml found in {}", cwd.string()));
}

/// Resolve a manifest argument: an explicit non-empty path is validated to
/// exist and parse as a manifest; an empty one falls back to cwd discovery.
inline rex::Result<std::string> LocateManifest(const std::string& explicit_path) {
  namespace fs = std::filesystem;
  if (explicit_path.empty()) {
    return DiscoverManifestInCwd();
  }
  std::error_code ec;
  fs::path abs = fs::absolute(explicit_path, ec);
  if (ec || !fs::exists(abs) || !fs::is_regular_file(abs)) {
    return rex::Err<std::string>(rex::ErrorCategory::IO, "Manifest not found: " + explicit_path);
  }
  if (!rex::codegen::ManifestConfig::IsManifest(abs)) {
    return rex::Err<std::string>(
        rex::ErrorCategory::Config,
        "Not a project manifest (missing [project] section): " + abs.string());
  }
  return rex::Ok(abs.string());
}

}  // namespace rexglue::cli
