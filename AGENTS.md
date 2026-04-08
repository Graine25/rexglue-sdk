# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the SDK modules: `core`, `filesystem`, `ui`, `input`, `audio`, `graphics`, `system`, `kernel`, `codegen`, and the `rexglue` CLI. Public headers live in `include/rex/`. Tests are split between `tests/unit/` for host-side Catch2 cases and `tests/ppc/` for PowerPC assembly fixtures in `tests/ppc/asm/`, but we will not need to focus on that. Build helpers live in `cmake/`, codegen templates in `resources/templates/`, vendored dependencies in `thirdparty/`, PPC binutils in `tools/binutils/`, and generated artifacts in `out/`.

## Build, Test, and Development Commands
Use CMake presets from the repo root; swap in the preset for your host platform such as `linux-amd64`, `mac-arm64`, or `win-amd64`.

- `cmake --preset mac-arm64` configures the multi-config build tree in `out/build/mac-arm64/`.
- `cmake --build --preset mac-arm64-debug --target rexglue` builds the CLI and writes runtime outputs under `out/mac-arm64/Debug/`.
- `cmake --build out/build/mac-arm64 --config Debug --target install` installs the SDK into `out/install/mac-arm64/`.
- `cmake --preset mac-arm64 -DREXGLUE_BUILD_TESTS=ON` enables test targets.
- `cmake --build out/build/mac-arm64 --config Debug --target unit_tests ppc_tests` builds both test suites.
- `ctest --test-dir out/build/mac-arm64 -C Debug --output-on-failure` runs discovered Catch2 tests.

## Initializing and Testing projects
Users will most likely try and test projects like "Iruka" and similar executables. There is a process to actually start developing and testing those projects out
- The ReXGlue SDK must have the executable linked to the `REX_SDK` environment variable
- `rexglue init --app_name <name> --app_root <folder>` to initalize a project
- A user will have to manually place the assets inside of an assets folder located in the repo. Stop the conversation once you get to this part.
- `rexglue codegen <name>_config.toml` change name to whatever your project is titled, and run codegen to see if the game can be ran.
- `cmake --preset mac-<platform>-<mode>` to preset.
- `cmake --build mac-<platform>-<mode>` to build.
- `./<name> <path/to/executable/assets>` to run.
- Chances are that the executable will not be able to run on first try, so it is best to test using LLDB/in debug mode or other debuggers beforehand.

## Coding Style & Naming Conventions
Follow `.editorconfig`, `.clang-format`, and `.clang-tidy`. Use 2-space indentation, LF line endings, and keep lines near the 100-column limit. Preserve the existing include grouping; `SortIncludes` is disabled. New types, functions, and enums should use `CamelCase`; variables and namespaces use `lower_case`; private members end with `_`; constants use a `k` prefix. Source files generally follow `snake_case.cpp` and `snake_case.h`.

## Mac specific Content
Since we are working on a Mac port of ReXGlue, there are a lot of parts unaccounted. Mac users will be forced to run LLVM-Clang instead of plain Apple Clang so that the general compilation parts are accounted for on all platforms (Linux/Windows run LLVM-Clang as well). Additionally, there will not be refactoring how the runtime or graphics system works for just the Mac Platform. If drastic changes are needed, a new file named `foomac.cpp` will be made rather than drastic changes to support one operating system. The goal is tidyness, not speed.

## Testing Guidelines
Testing uses Catch2. Add host logic tests under `tests/unit/` with names ending in `_test.cpp`. Add PPC instruction coverage as `.s` files under `tests/ppc/asm/`; the build generates corresponding Catch2 cases through `rexglue recompile-tests`. Keep tests deterministic and run the affected suite plus `ctest` before opening a PR.

## Commit & Pull Request Guidelines
Recent history follows Conventional Commits with optional scopes, for example `feat: macOS Scaffolding`, `fix(audio): resolve AudioSystem shutdown deadlock`, or `fix(gpu/xma): make unknown register messages debug`. Keep commits focused and imperative. PRs should explain the change, note the preset/configuration tested, link related issues or discussions, and include screenshots when UI or debug-overlay behavior changes. Avoid editing `thirdparty/` unless you are intentionally updating vendored code.
