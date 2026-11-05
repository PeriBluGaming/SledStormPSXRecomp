# SledStorm Recompiled

<!-- retcomm-readme-metrics -->
[![GitHub downloads (all assets, all releases)](https://img.shields.io/github/downloads/PeriBluGaming/SledStormPSXRecomp/total)](https://github.com/PeriBluGaming/SledStormPSXRecomp/releases)
[![GitHub downloads (latest release)](https://img.shields.io/github/downloads/PeriBluGaming/SledStormPSXRecomp/latest/total)](https://github.com/PeriBluGaming/SledStormPSXRecomp/releases/latest)
[![GitHub release](https://img.shields.io/github/v/release/PeriBluGaming/SledStormPSXRecomp)](https://github.com/PeriBluGaming/SledStormPSXRecomp/releases/latest)
<!-- /retcomm-readme-metrics -->

SledStormPSXRecomp is an unofficial, noncommercial static recompilation of the
PlayStation game **Sled Storm**, built on
[psxrecomp](https://github.com/mstan/psxrecomp) and
[recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui). It runs the game
as a native executable. You need your own legally obtained copy of the game to
play.

## About

*Sled Storm* is a snowmobile racing game for the PlayStation, developed by EA
Canada and published by Electronic Arts in 1999. This project targets the PAL
release:

| | |
|---|---|
| Serial | SLES-02194 |
| Boot executable | `SLES_021.94` |
| Region | Europe (PAL) |
| Languages on disc | English, French, German, Spanish |

psxrecomp translates the game's MIPS executable into C (`generated/`), which
is compiled together with the psxrecomp runtime into a native executable. The
runtime boots the game on the bundled OpenBIOS, so no retail BIOS is needed.

The project was scaffolded with psxrecomp's New Project Layout. See
`psxrecomp/docs/GAME_PROJECT_SETUP.md` for the full flow.

## Current status

This is an early playable build.

- The game starts, and controls, driving physics, graphics, tracks and audio
  work in the tests done so far. A complete race has been finished.
- The project's first own mod is an optional native 16:9 widescreen mode (see
  [Widescreen (16:9)](#widescreen-169)). It is experimental and disabled by
  default. An optional edge-aligned HUD is part of it, also experimental and
  off by default.
- Testing has been limited to the maintainer's own setup. No broader
  compatibility or platform testing has been done yet.
- The seed list in `seeds/ghidra_funcs.txt` is the first-pass scan of the boot
  executable and is expected to grow.

Expect bugs.

## Widescreen (16:9)

The optional **16:9 Widescreen** mod (`sledstorm.enhancement.widescreen`)
renders single-player races natively in 16:9 instead of stretching the 4:3
picture:

- Native 16:9 rendering for supported single-player races.
- A wider view of the track: track geometry and scenery that the original 4:3
  view cuts off at the sides are drawn.
- Menus, loading screens and split-screen races keep the original 4:3
  presentation.
- With the mod disabled, the game keeps its original 4:3 rendering.
- Disabled by default.

To enable it, open the launcher's **Mods** page, switch on **16:9 Widescreen**
(group *Visual*) and start the game. Switch it off the same way to return to
the original 4:3 presentation.

By default the HUD keeps its original 4:3 layout in the centre of the screen.
The mod's optional **Edge-aligned HUD** setting (`edge-hud`, default `false`)
moves the race HUD panels, the speedometer and the track map with its racer
markers to the edges of the 16:9 screen. Centred messages such as the
countdown, Wrong Way and stunt scores stay in the centre. The setting can be
switched during a race. So far the mod has been tested in single races, the
attract demo, pause and results screens and two-player split screen, and the
edge-aligned HUD on three tracks, on the maintainer's setup; other race modes
and tracks may still show issues.
Gameplay, physics and controls are not changed.

## Requirements

To play:

- Your own legally obtained copy of *Sled Storm* (PAL, SLES-02194) as a
  `.cue`/`.bin` disc image. The expected data-track digests are recorded in
  `game.toml`.
- No retail BIOS is required; OpenBIOS is bundled.

To build from source (see `psxrecomp/docs/BUILDING.md` for details):

- Git, to fetch the submodules
- A C/C++ toolchain: Apple Clang (Xcode command-line tools) on macOS, GCC or
  Clang on Linux, MSYS2 MinGW-w64 or MSVC on Windows
- CMake 3.20 or newer; on macOS and Linux also Ninja and pkg-config
- Python 3 (3.11 or newer, or `tomli`, for `tools/sync_symbols.py`)
- The game disc, only if you want to regenerate `generated/`

## Building from source

`generated/` (the recompiled game C) **is committed**, so a build does not
need the disc:

```bash
git clone --recursive https://github.com/PeriBluGaming/SledStormPSXRecomp.git
cd SledStormPSXRecomp
# or, in an existing clone:
git submodule update --init --recursive
bash scripts/apply_framework_patches.sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target psx-runtime
```

The build needs the local framework patches in `patches/psxrecomp/` (see its
README). `scripts/apply_framework_patches.sh` checks that `psxrecomp` is at the
pinned revision, applies the patches to the submodule's working tree and does
nothing when they are already applied; it stops with an explanation if the
framework checkout is at another revision or carries other changes. CMake
refuses to configure without the patches. Run the script again after every
`git submodule update` or `git -C psxrecomp checkout -- .`; on Windows, run it
from Git Bash or the MSYS2 shell.

To regenerate the game C from your own disc (needed when the seeds or the
framework pin change):

```bash
./psxrecomp/tools/ci/build_emitters.sh
python3 psxrecomp/psxrecomp_cli.py generate \
  --config game.toml --project-root . --disc disc/<your>.cue
git add generated && git commit -m "Regenerate game C"
```

Disc images under `disc/` are gitignored and must never be committed. Retail
BIOS dumps are not redistributed, and no C derived from one may be committed;
releases run on the bundled MIT-licensed OpenBIOS.

Default app icon: `assets/psxrecomp.ico` (and `.png` / `.svg`), the
Retro-themed controller mark from `psxrecomp/assets/`. Windows builds embed it
via `APP_ICON`. Optional box art under `launcher_assets/img/` may come from
[libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails)
(`Named_Boxarts`); see `BOXART_SOURCE.txt` when present.

### Release builds

Official optimized Windows/Linux releases use the maintained shared builder
with the title disc available. Commit the desired `VERSION`, source and
framework pins, then run:

```bash
bash scripts/apply_framework_patches.sh
scripts/package_release.sh --channel public --allow-dirty
# Use --channel playtest for a non-public package; --platform selects Windows/Linux.
```

The shared builder refuses a source tree with tracked changes, and the applied
framework patches are exactly such changes in `psxrecomp/`. `--allow-dirty`
accepts them and records the changed paths in the release manifest; check
that those are only the files listed in `patches/psxrecomp/README.md`.

On Windows PowerShell, apply the patches first (from Git Bash or MSYS2) and use
`psxrecomp/tools/package_title_release.ps1 --channel public --allow-dirty`. The generated
`packaging/release.toml` and player instructions define the title payload.
The shared builder owns regeneration, qualified build defaults, fresh private
PGO boot training on supported native toolchains, staging and verification.
Boot training is a build step, not gameplay qualification or a performance
claim. See `psxrecomp/docs/RELEASING_A_TITLE.md`.

The separate *Release builds* CI workflow (tag `vX.Y.Z` or dispatch) builds
committed `generated/` C without a disc on Linux, Windows and macOS. It applies
the framework patches right after checkout, uses
`scripts/package_prebuilt_release.sh` and attaches compiled-game ZIPs. That
explicit prebuilt path does not train selective PGO and is not the official
optimized-release command. The common builder does not add macOS support.

Studio's current Export action still uses the old prebuilt argument contract;
use the shared command above for optimized releases until Studio adopts the
recipe/channel interface. Its packager detection and `packaging/` warning also
need that adoption; the generated recipe is maintained configuration, not a
forked packager.

### Symbols

Progressive map: `symbols.toml` → `python3 tools/sync_symbols.py` →
`psx_symbols.h` (`PSX_FN_*`). See `psxrecomp/docs/SYMBOLS.md`.

### Framework pins

Submodule gitlinks (`psxrecomp`, optional `recomp-ui`, nested `recomp-net`)
are authoritative. `framework_pins.txt` is an optional scaffold snapshot;
release CI logs SHAs with `record_pins.sh` but builds whatever the gitlinks
resolve to. Bump submodules deliberately — do not float on `main`/`master`
in release CI.

<!-- retcomm-readme-launcher -->
## Retro Launcher

You can run this title **standalone** (download the release zip, point it at
your disc, play), or manage installs, updates, and disc/BIOS wiring with
**[Retro Launcher](https://github.com/RetroPortingToolKit/Retro-Launcher)** —
the Retro Compilation Manager hub for self-compiling recomps.

[Downloads](https://github.com/RetroPortingToolKit/Retro-Launcher/releases) ·
[Full README & features](https://github.com/RetroPortingToolKit/Retro-Launcher#readme)

<p align="center">
  <img src="https://raw.githubusercontent.com/RetroPortingToolKit/Retro-Launcher/main/docs/screenshots/hub-and-game-launcher.png" alt="Retro hub with a background build, next to a title’s recomp-ui launcher" width="720">
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/RetroPortingToolKit/Retro-Launcher/main/docs/screenshots/queue-and-background-build.png" alt="Background cmake build with titles queued" width="720">
</p>

Retro checks for updates, installs the prebuilt release zips, and automates
BIOS/ROM/save plumbing so you are not stuck repeating each game’s first run by hand.
<!-- /retcomm-readme-launcher -->

## Credits

- **[PeriBluGaming](https://github.com/PeriBluGaming)**: author and maintainer
  of SledStormPSXRecomp (project setup, recompilation, the 16:9 widescreen
  mod, testing and documentation).
- **Matthew Stanley**: [psxrecomp](https://github.com/mstan/psxrecomp) and
  [recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui), which this
  project is built on, including the scaffold templates most project files
  were generated from.
- **The PCSX-Redux authors**: [OpenBIOS](https://github.com/grumpycoders/pcsx-redux),
  bundled with releases (MIT).
- The authors of the further components listed in psxrecomp's and recomp-ui's
  third-party notices, which ship in every release under `licenses/`.
- *Sled Storm* was developed by EA Canada and published by Electronic Arts. The
  game belongs to its respective rights holders.

## License

SledStormPSXRecomp's own contributions are © 2026 PeriBluGaming and licensed
under the [PolyForm Noncommercial License 1.0.0](LICENSE). You may use, modify
and share them for noncommercial purposes, provided the license terms and its
`Required Notice` lines are kept. No license for commercial use is granted.

The project's own code is currently the widescreen mod: `mods/src/widescreen.c`
and its manifest under `mods/preloaded/packages/`. Beyond that, the license
covers the project's documentation and PeriBluGaming's own changes to
otherwise generated or template-derived files. Most files were generated from
psxrecomp templates or by psxrecomp tooling and keep their upstream terms.
[NOTICE.md](NOTICE.md) lists the terms for every part of the repository.

This license does **not** cover:

- **The game.** `generated/` and the compiled game in release builds are
  machine-translated from the original game's code, which belongs to its
  rights holders. No license to it is granted here. Whether that code may be
  published or redistributed is a separate copyright question that this
  project's license does not answer.
- **[psxrecomp](https://github.com/mstan/psxrecomp)** (© Matthew Stanley,
  under its own license text, copied in
  [docs/licenses/psxrecomp-LICENSE](docs/licenses/psxrecomp-LICENSE)),
  including the upstream portions of the project files generated from its
  templates.
- **[recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui)** (MIT,
  © Matthew Stanley) and all other bundled third-party components.

## Disclaimer

SledStormPSXRecomp is an unofficial fan project. It is not affiliated with,
endorsed, sponsored or approved by Electronic Arts, EA Canada or Sony
Interactive Entertainment. All names and trademarks belong to their respective
owners. You need your own legally obtained copy of the game to play. No game
disc images or retail BIOS dumps are included.

<!-- retcomm-readme-raid -->
---

<p align="center">
  <sub><b>R.A.I.D. — Retro AI Development</b> · a Discord for AI-assisted retro reverse-engineering, decomp &amp; recomp</sub>
</p>

<p align="center">
  <a href="https://discord.gg/Ad9BwSzctP"><img src=".github/raid-discord.png" alt="Join the Retro AI Development (R.A.I.D.) Discord" width="200"></a>
</p>
<!-- /retcomm-readme-raid -->
