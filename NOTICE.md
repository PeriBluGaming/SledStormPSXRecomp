# Notices: SledStormPSXRecomp

SledStormPSXRecomp is an unofficial, noncommercial fan project: a static
recompilation of the PlayStation game *Sled Storm* (PAL release, SLES-02194),
built with [psxrecomp](https://github.com/mstan/psxrecomp) and
[recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui).

This repository combines material of several origins under different terms.
This file states which terms apply to which files. It does not grant any rights
in the original game, and neither it nor any other notice in this repository
creates a right to redistribute game code.

Provenance below was determined from the Git history of this repository (the
scaffold commit `67423d1` and the first playable baseline `f3b7cb8`) and by
comparing files with the psxrecomp New Project Layout templates at the pinned
psxrecomp commit. Where ownership is inferred rather than documented, this file
says so.

## 1. Original project code: PolyForm Noncommercial 1.0.0

PeriBluGaming's own copyrightable contributions are Copyright (c) 2026
PeriBluGaming and licensed under the PolyForm Noncommercial License 1.0.0
(see `LICENSE`).

**Current state:** the project's original code is the optional native 16:9
widescreen mod and the tooling that applies its local framework patch. The
rest of the repository consists of psxrecomp scaffold files, code generated
from the original game, tool-generated metadata and documentation.
PeriBluGaming's own contributions are:

- `mods/src/widescreen.c`: the widescreen mod (16:9 presentation for
  single-player races and, optionally, frontend menus, and the track
  renderer's widened screen-edge cull)
- `mods/preloaded/packages/sledstorm.enhancement.widescreen/0.1.0/manifest.toml`:
  the widescreen mod's package manifest
- `scripts/apply_framework_patches.sh`, `cmake/framework_patches.cmake`,
  `patches/psxrecomp/README.md` and `patches/psxrecomp/BASE`: applying and
  checking the local psxrecomp patches (the patches themselves are covered in
  section 7)
- `NOTICE.md` (this file)
- The project-specific text of `README.md` (the file as a whole is of mixed
  origin; see section 2)
- PeriBluGaming's own changes, if any are protectable, in the mixed-origin
  files listed in section 2 and in `game.toml` (section 5)

`mods/src/widescreen.c` carries
`SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0`, and the manifest
states the same license. Original code added in the future will be listed in
this section.

The mod is written against psxrecomp's mod API and is compiled into a runtime
that is itself under psxrecomp's terms (section 7). Game addresses and
behaviour described in it are facts about the original game; no claim is made
to those facts.

## 2. Mixed-origin files: psxrecomp-derived with project changes

These files were generated from psxrecomp's New Project Layout templates
(`psxrecomp/tools/new_project_layout/templates/` and
`psxrecomp/docs/ci/templates/`). Their upstream portions are Copyright (c) 2026
Matthew Stanley, under psxrecomp's license (copy in
`docs/licenses/psxrecomp-LICENSE`; see section 6). PeriBluGaming cannot license
the upstream portions. Any protectable changes by PeriBluGaming are offered
under the PolyForm Noncommercial License 1.0.0. Each file as a whole remains
subject to psxrecomp's terms.

| Path | Origin |
|---|---|
| `README.md` | Scaffold template, rewritten with substantial project-specific text. The `retcomm-readme-*` blocks are maintained by upstream tooling. |
| `CMakeLists.txt` | Scaffold template with token substitution, plus the project's build entry for the widescreen mod |
| `codegen_setup.c`, `symbols.toml` | Scaffold template, token substitution only |
| `.github/workflows/release.yml` | psxrecomp release workflow template, token substitution only |
| `scripts/package_prebuilt_release.sh` | Scaffold template, token substitution only |
| `packaging/release.toml`, `packaging/linux/AppRun`, `packaging/linux/sledstorm.desktop`, `packaging/release/START_HERE.txt` | Scaffold templates, token substitution only |

Token substitution inserts names, identifiers and paths such as the game title,
the executable name and the boot executable name. No copyright is claimed in
those values.

## 3. Unmodified upstream files

These files are byte-identical to, or generated unchanged from, psxrecomp. They
are under psxrecomp's terms, Copyright (c) 2026 Matthew Stanley, unless stated
otherwise. PeriBluGaming claims no copyright in them.

- `codegen_setup.h`, `game_options.toml`, `.gitignore`, `VERSION`,
  `scripts/package_release.sh` (scaffold templates without substitutions)
- `tools/sync_symbols.py`
- `mods/preloaded/README.md` (scaffold text written by psxrecomp's
  `setup_project.sh`) and `mods/preloaded/packages/.gitkeep`
- `assets/psxrecomp.ico`, `assets/psxrecomp.png`, `assets/psxrecomp.svg`
  (default app icon from `psxrecomp/assets/`)
- `.github/raid-discord.png` (R.A.I.D. community branding, shipped by the
  psxrecomp scaffold)
- `docs/licenses/psxrecomp-LICENSE` (verbatim copy of `psxrecomp/LICENSE` at
  the pinned commit)

The icons and `raid-discord.png` carry no notice of their own. Their terms are
inferred from psxrecomp's repository-wide license.

## 4. Game code, data and assets: not licensed

*Sled Storm* (1999), developed by EA Canada and published by Electronic Arts,
and all of its code, data, audio, video and artwork belong to their respective
rights holders. PeriBluGaming does not own them, and the license in `LICENSE`
does not cover them.

- `generated/`: C code that psxrecomp produced by machine translation of the
  game's retail executable (`SLES_021.94`). It is derived from copyrighted game
  code. It also contains boilerplate emitted by psxrecomp's code generator,
  including the four mod hook calls the generator inserts for the instruction
  sites listed in `game.toml`; `generated/` is never edited by hand. No
  license to it is granted by this project. The same applies to the compiled
  game inside release executables.
- `docs/screenshots/`: screenshots of the game running in this project, used
  in `README.md`. They show copyrighted *Sled Storm* imagery and are not
  covered by this project's PolyForm Noncommercial license or by `LICENSE`.

**Publishing `generated/` is a separate copyright question.** Whether
machine-translated game code may be published or redistributed at all depends
on the rights of the game's rights holders and on applicable law. `LICENSE`,
this file and psxrecomp's license do not answer that question and do not
grant any permission for it.

Users are responsible for obtaining and using a lawful copy of the original
game. Disc images and retail BIOS dumps are not included in this repository
or in its releases.

## 5. Tool-generated metadata

The following files were produced by psxrecomp tooling or the project scaffold
and record facts about the game, the disc or the build. PeriBluGaming claims no
copyright in them.

- `game.toml` (written by `probe_disc.py` from the game disc)
- `catalog_identity.json`, `disc_probe.json` (disc and executable identity)
- `seeds/ghidra_funcs.txt` (function entry points scanned from the game
  executable)
- `psx_symbols.h` (generated from `symbols.toml` by `tools/sync_symbols.py`)
- `.recomp.json`, `.gitmodules`, `framework_pins.txt`

`probe_disc.py` is psxrecomp's vendored copy of a tool from retcomm-studio
(MIT). Its license applies to the tool, not to the facts it writes.

PeriBluGaming added the widescreen mod's settings to `game.toml` (the
`mod_instruction_sites` list and the `[widescreen]` table, with their
comments). The addresses and options record facts about the game and the
build; PeriBluGaming's own changes are offered under the PolyForm
Noncommercial License 1.0.0 to the extent they are protectable.

## 6. psxrecomp's license text

psxrecomp's `LICENSE` is titled "PolyForm Noncommercial License 1.0.0", but at
the pinned commit its text differs from the official text published by the
PolyForm Project. Compared with the official text:

- The sections *Distribution License*, *Notices*, *Changes and New Works
  License* and *Patent License* are absent.
- The *Copyright License* section is worded differently. It grants a license
  to do everything that would otherwise need the licensor's permission,
  expressly including distributing the software and making changes and new
  works, subject to the other terms.
- A closing paragraph states the licensor's intent to restrict uses where
  profit is derived from the software and names a contact for commercial
  licensing.

The terms that apply to psxrecomp and to the upstream portions of the files in
sections 2 and 3 are those in psxrecomp's actual license text, reproduced
unchanged in `docs/licenses/psxrecomp-LICENSE`. They are not the official
PolyForm text used in this project's `LICENSE`. Nothing in this file modifies
or interprets psxrecomp's terms with binding effect.

## 7. Submodules and third-party components

| Component | Terms | Where |
|---|---|---|
| [psxrecomp](https://github.com/mstan/psxrecomp), pinned at `c1f5cafd4102daa57b7342bec7c432aaca8981ce` | psxrecomp's license (see section 6), Copyright (c) 2026 Matthew Stanley | `psxrecomp/LICENSE`; copy in `docs/licenses/psxrecomp-LICENSE` |
| [recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui), pinned at `817c2985d371f8d19d70d9d1d9629425ed6cfd68` | MIT, Copyright (c) 2026 Matthew Stanley | `recomp-ui/LICENSE` |
| recomp-net and retcomm-rbengine (nested in psxrecomp under `lib/`) | MIT, Copyright (c) 2026 their contributors | `psxrecomp/lib/*/LICENSE` |
| [OpenBIOS](https://github.com/grumpycoders/pcsx-redux) by the PCSX-Redux authors, bundled with releases | MIT; the binary also links code from uC-sdk under permissive licenses | `psxrecomp/bios/OpenBIOS.LICENSE` |
| Other components bundled by psxrecomp (including libchdr, libwebp, xxHash, toml11, json11, stb_image, libjuice, TinyCC and the Python runtime of the overlay toolchain) | Their own licenses (permissive, plus MPL-2.0 for libjuice, LGPL-2.1 for TinyCC and the PSF license for Python) | `psxrecomp/THIRD_PARTY_ATTRIBUTION.md`, `psxrecomp/runtime/licenses/`, `psxrecomp/overlay_toolchain/` |
| Components bundled by recomp-ui (Dear ImGui; fonts and images under OFL-1.1 and CC BY-SA 4.0) | Their own licenses | `recomp-ui/src/third_party/`, `recomp-ui/assets/common/*/NOTICE.md` |

Source archives downloaded from GitHub do not contain submodule files. Fetch
them with `git submodule update --init --recursive`.

**Local psxrecomp patch.** Builds of this title use psxrecomp at the pinned
revision above plus the local patch
`patches/psxrecomp/0001-native-wide-reveal-edge-quads.patch`, applied by
`scripts/apply_framework_patches.sh` (see `patches/psxrecomp/README.md`). It
adds a host-side rendering API (`psx_mod_tag_reveal_edge_quad`) and changes
these psxrecomp files: `runtime/include/gpu.h`,
`runtime/include/mod_plugins.h`, `runtime/src/gpu.c`,
`runtime/src/mod_runtime.cpp`, `runtime/CMakeLists.txt`,
`docs/ADAPTIVE_SCREEN_MASKS.md` and three runtime tests; it adds
`runtime/include/ws_reveal_edge.h` and two runtime tests. The patch and the
patched files are modifications of psxrecomp by PeriBluGaming and remain under
psxrecomp's license (section 6), Copyright (c) 2026 Matthew Stanley; this
project claims no other terms for them. The submodule pin is not changed, and
the patch is intended for upstream submission.

The psxrecomp release packagers copy the upstream notices into every release
package: `licenses/` holds psxrecomp's license (`psxrecomp-LICENSE`), its
third-party attribution and notices, and recomp-ui's license.
`bios/OpenBIOS.LICENSE`, `assets/fonts/NOTICE.md` and `assets/img/NOTICE.md`
sit beside the files they cover. The executable contains the compiled game
(section 4), the psxrecomp runtime, recomp-ui and this project's widescreen
mod (section 1). Each part keeps its own terms.

This project's `LICENSE` and this `NOTICE.md` ship at the root of every
release package, through `packaging/release.toml` (official builder) and
`scripts/package_prebuilt_release.sh` (prebuilt CI path). Anyone
redistributing a release should keep both files.

## 8. Attribution

If you redistribute PeriBluGaming's code from this project, the PolyForm
Noncommercial License 1.0.0 requires that anyone who gets a copy from you also
gets a copy of its terms or their URL, and a copy of every `Required Notice:`
line in `LICENSE`. That is the full attribution requirement of this project's
license.

Upstream and third-party components carry their own notice requirements (for
example, MIT and BSD licenses require their copyright and license notices to be
kept). Keep those notices as their licenses require. A visible credit in a
README, store page or in the app is appreciated but not required.

## 9. Commercial use

No license for commercial use is granted. Commercial use of PeriBluGaming's own
contributions requires separate written permission from PeriBluGaming. Such
permission could cover only PeriBluGaming's own contributions. It cannot cover
psxrecomp (licensed by Matthew Stanley), other third-party components, or the
game.

## 10. AI-assisted development

Claude (Anthropic) was used to draft the documentation in this repository,
including `NOTICE.md`, the `README.md` text and the `Required Notice:` lines
in `LICENSE`, under PeriBluGaming's direction and review. Claude was also used
to analyse the game's executable and to write the widescreen mod
(`mods/src/widescreen.c`, its manifest and its `game.toml` and
`CMakeLists.txt` entries) under PeriBluGaming's direction, review and
playtesting. Copyright is claimed only to the extent the law recognises it.
The license applies to whatever protectable authorship these files contain.

The code in `generated/` was produced by psxrecomp's code generator from the
game's executable. It was not written by an AI assistant.

## 11. Trademarks and affiliation

This project is not affiliated with, endorsed, sponsored or approved by
Electronic Arts, EA Canada or Sony Interactive Entertainment. "Sled Storm",
"Electronic Arts" and "EA" may be trademarks of Electronic Arts Inc.
"PlayStation" is a trademark of Sony Interactive Entertainment Inc. All names
and trademarks belong to their respective owners.
