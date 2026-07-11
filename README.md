# Metal Mutant — modern Windows & Miyoo Mini port

Runs **Metal Mutant** (Silmarils, 1991) natively on modern Windows and on the
Miyoo Mini (Allium CFW) using the open-source
[ALIS engine re-implementation](https://github.com/maestun/alis),
plus quality-of-life additions:

- **Config file** (`metal.ini`): data folder, fullscreen, window scale, sound, cheats
- **English fan translation** — applied in memory at load time, original game
  files are never modified (the game shipped in French)
- **Cheats**: turbo (F8), slow motion (F7), infinite power (F6, with a one-time
  self-calibrating memory trainer on F5)
- **Save states** (F11/F12), Alt+Enter fullscreen toggle, on-screen messages

## You need your own copy of the game

**No game data is included.** Metal Mutant is © Silmarils; the `.IO` game
files are not distributed here and must come from your own copy. The English
translation file necessarily quotes the original French strings it replaces;
that text remains © Silmarils.

## Build

Requirements: Windows, [CMake](https://cmake.org), Visual Studio 2022
(Build Tools suffice), git. Then:

```powershell
.\scripts\setup.ps1 -GameData "D:\path\to\your\metal_mutant_files"
```

The script clones the pinned upstream engine, applies the patch series, drops
in the `engine/` module, downloads SDL2, builds, and assembles a ready-to-run
`build\MetalMutant\` folder. Without `-GameData`, copy your game files into
`build\MetalMutant\data\` manually afterwards.

## Build for the Miyoo Mini (Allium CFW)

Requirements: Linux, CMake, git, and the local Miyoo cross toolchain checked
out at `../toolchain` (see its `install-local.sh`). Then:

```bash
./build-miyoo-allium.sh
```

Same flow as the Windows script (clone pinned engine, drop in modules, apply
patches) but cross-compiles a static-SDL2 ARM binary and assembles
`dist/miyoo-allium/MetalMutant.pak`. Copy the `.pak` folder to `Apps/` on the
SD card and your game files into its `data/` folder. On device the game draws
straight to `/dev/fb0`, reads the buttons from `/dev/input/event*`, and plays
sound through Allium's OSS shim; the button layout is in
[dist-miyoo/README-MIYOO.txt](dist-miyoo/README-MIYOO.txt).

## How the port is layered (and how to track upstream ALIS)

| Layer | Contents | Update cost |
|---|---|---|
| `engine/` | `mmx.c/h` + font — fully self-contained module (config, translation, trainer, OSD); `mmx_miyoo.c/h/cmake` — Miyoo platform module (fb0 video, evdev input, static SDL2) | copied in, never conflicts |
| `patches/` | small hooks into 5 upstream files + MSVC build fixes + Miyoo hooks | may need rebasing |
| `dist/` | `metal.ini`, player README, `translations/english.txt` | independent |
| `dist-miyoo/` | Miyoo `metal.ini`, `launch.sh`, player README | independent |
| `UPSTREAM` | pinned upstream repo + commit | bump to move forward |

To move to a newer ALIS version:

1. Edit `UPSTREAM` with the new commit SHA.
2. Run `.\scripts\setup.ps1 -Clean` (or `./build-miyoo-allium.sh --clean`).
   Patches apply with `git am --3way`, so clean or trivially-conflicting
   updates just work.
3. If a patch fails: in `build\alis`, resolve the conflict, `git am --continue`,
   then regenerate the series with `git format-patch -3 -o ..\..\patches` and
   commit the refreshed patches together with the `UPSTREAM` bump.

The hooks are deliberately tiny (one `#include` + one call in `script.c`, a
config block in `main.c`, hotkey cases and an OSD call in `sys_sdl2.c`) to
keep rebases painless.

## Credits & license

- **Game**: © 1991 [Silmarils](https://en.wikipedia.org/wiki/Silmarils_(company)). Not affiliated.
- **Engine**: [maestun/alis](https://github.com/maestun/alis) by Olivier
  Huguenot & Vadim Kindl, MIT license.
- **Port additions** (this repo): MIT license — see [LICENSE](LICENSE).
