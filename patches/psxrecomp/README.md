# Local PSXRecomp patches

Patches against the pinned `psxrecomp` submodule
([psxrecomp](https://github.com/mstan/psxrecomp), Copyright (c) 2026 Matthew
Stanley, under its own license; see `psxrecomp/LICENSE` and
`docs/licenses/psxrecomp-LICENSE`). The patched files stay under that license.
Each patch is meant to become a standalone upstream pull request.

Builds of this title need them: `mods/src/widescreen.c` calls the API patch
0001 adds, and CMake refuses to configure without it.

## Layout

- `BASE`: the full psxrecomp revision every patch is made against. It must
  match the submodule gitlink (`git ls-files --stage psxrecomp`).
- `NNNN-name.patch`: applied in file-name order, as plain `git diff` output
  against `BASE` and against the result of every earlier patch.

## Apply

From the repository root, after `git submodule update --init --recursive`:

```sh
bash scripts/apply_framework_patches.sh
```

The script:

- refuses any psxrecomp revision other than `BASE`;
- builds the expected patched content outside the repository and compares it
  file by file with the framework working tree (line-ending neutral);
- applies the whole series when psxrecomp is the clean base revision;
- does nothing when psxrecomp already carries exactly the series;
- stops, changing nothing, on anything else: a partly applied series, edits
  inside or outside the patched files, extra untracked files, staged changes,
  modified nested submodules, or a patch that no longer applies;
- logs the SHA-256 of every patch.

Only the submodule's working tree changes. Its index and HEAD and the
superproject's gitlink stay as they are; never commit inside `psxrecomp/` for
a local patch. `--check` verifies without writing (exit 0: applied, 2: clean
base, 1: anything else). Release CI runs the script right after checkout. The
shared release builder (`scripts/package_release.sh`) needs `--allow-dirty`
for the patched framework files; see the top-level README.

Run it with bash 3.2 or newer: macOS `/bin/bash`, Linux, Git Bash or MSYS2 on
Windows.

## Remove

```sh
git -C psxrecomp apply -R ../patches/psxrecomp/0001-native-wide-reveal-edge-quads.patch
git -C psxrecomp status --short        # must print nothing
```

With several patches, reverse them from the highest number down.

## Changing the pin

When the psxrecomp gitlink moves, the script stops until the patches are
revalidated: rebase them onto the new revision (or drop the ones merged
upstream), regenerate them with `git diff` and update `BASE`.

## 0001-native-wide-reveal-edge-quads.patch

- **Base revision:** `c1f5cafd4102daa57b7342bec7c432aaca8981ce`
- **Adds:** `psx_mod_tag_reveal_edge_quad(primitive, edges)`, a host-side,
  packet-guarded tag that grows an axis-aligned flat quad (GP0 0x28-0x2B), or
  a textured quad whose four UVs name one texel (0x2C-0x2F), from a left or
  right boundary that already reaches the canonical display edge to the
  visible native-wide edge. Only the missing band outside the quad is drawn,
  in the packet's ordering-table position with its colour, CLUT, texture
  page and blend. Inert at native 4:3; guest packets are never written.
- **Touches:** `runtime/include/{ws_reveal_edge.h,gpu.h,mod_plugins.h}`,
  `runtime/src/{gpu.c,mod_runtime.cpp}`, `runtime/CMakeLists.txt`,
  `docs/ADAPTIVE_SCREEN_MASKS.md`, two new tests and one link stub in each of
  three existing C++ tests.
- **Tests:** `ws_reveal_edge_test`, `ws_reveal_edge_exec_test`.

### Run the new tests without a full build

From `psxrecomp/runtime` (macOS shown; use `-Wl,--gc-sections` on Linux):

```sh
cc -std=c11 -I include tests/test_ws_reveal_edge.c -o /tmp/ws_reveal_edge_test && /tmp/ws_reveal_edge_test
cc -std=c11 -I include -I src -ffunction-sections -fdata-sections -Wl,-dead_strip \
   tests/test_ws_reveal_edge_exec.c src/psx_video_timing.c src/gpu_timeline.c \
   -o /tmp/ws_reveal_edge_exec_test -lm && /tmp/ws_reveal_edge_exec_test
```
