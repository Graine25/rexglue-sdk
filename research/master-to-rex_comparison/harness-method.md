# canary-sync

Tooling to port Xenia **Canary** (`canary_experimental`) GPU changes into
ReXGlue's `src/graphics`, repeatably.

ReXGlue's `src/graphics` is Xenia **master** `src/xenia/gpu`, forked at
`dfa1b3fae174def5204ca6e9cd2d47a92c83a156` and put through a consistent
transform (namespace `xe::gpu` -> `rex::graphics`, include paths rewritten,
files reorganized into `pipeline/{shader,texture,render_target}`, `util/`,
`format/`, backends in `d3d12/` `vulkan/`, wrapped behind the runtime-loaded
`IGraphicsSystem` plugin ABI). Canary is the same base plus years of divergent
GPU work. Porting it = **replay `Canary - base` on top of the ReXGlue transform**.

## How it works

For each Xenia gpu file we build three normalized versions and 3-way merge:

```
BASE   = xenia @ base   -> rexform -> clang-format   # ReXGlue-flavored ancestor
OURS   = ReXGlue current file        -> clang-format
THEIRS = xenia @ canary -> rexform -> clang-format   # ReXGlue-flavored canary
```

`rexform.py` only normalizes *context* (includes + namespace), not symbol-level
namespace moves (e.g. `xe::load_and_swap` -> `rex::memory::load_and_swap`). It
doesn't need to: those lines are identical in BASE and THEIRS, so
`git merge-file` keeps ReXGlue's version ("ours wins when base == theirs").
`clang-format` (run on all three with the repo `.clang-format`) removes
formatting noise so hunks align. What's left as a conflict is a genuine
seam: a spot where a ReXGlue change -- usually an earlier piecemeal Canary
cherry-pick -- overlaps a later Canary change.

## Usage

```bash
export XENIA_REPO=/path/to/xenia-repo     # git dir with refs `base` and `canary`
export CLANG_FORMAT=/path/to/clang-format # v14+; `pip install clang-format` works
export REX_ROOT="$PWD"

# one-time: set up the xenia repo (treeless, two commits is enough)
git init xenia-delta && cd xenia-delta
git remote add master https://github.com/xenia-project/xenia.git
git remote add canary https://github.com/xenia-canary/xenia-canary.git
git fetch --depth=1 --filter=blob:none master \
    dfa1b3fae174def5204ca6e9cd2d47a92c83a156:refs/heads/base
git fetch --depth=1 --filter=blob:none canary canary_experimental:refs/heads/canary
cd ..

python3 tools/canary-sync/sync.py report              # worksheet + stage all merges
python3 tools/canary-sync/sync.py file spirv_shader_translator_rb.cc   # one file
```

Merged output is staged under `REX_ROOT/.rexsync/merged/` (gitignored) --
**never the live tree**. Review there, resolve conflicts, then copy a subsystem
in and verify with a build + GPU trace diff before committing.

## Files

- `rexform.py` -- the mechanical transform + the xenia<->rex path/include maps.
- `sync.py`    -- the 3-way merge driver and worksheet reporter.
- `DRIFT.md`   -- the sync ledger: base/canary SHAs, per-subsystem status, plan.

## Landing checklist (per subsystem)

1. `sync.py report`, review staged files for that subsystem.
2. Resolve conflicts (see DRIFT.md for the seam taxonomy).
3. Reconcile earlier cherry-picks: for each `<<<<<<<`, decide keep-ours /
   take-canary / merge -- "already have it, different shape" is common.
4. Copy resolved files into `src/graphics/`, wire any new files into CMake.
5. Build both backends.
6. Trace-diff a few titles (trace_reader/player/writer) against pre-port goldens.
7. Commit; update DRIFT.md with the canary SHA reached.
