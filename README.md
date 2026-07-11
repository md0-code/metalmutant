# Metal Mutant — modern Windows port

Runs **Metal Mutant** (Silmarils, 1991) natively on modern Windows using the
open-source [ALIS engine re-implementation](https://github.com/maestun/alis),
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

## How the port is layered (and how to track upstream ALIS)

| Layer | Contents | Update cost |
|---|---|---|
| `engine/` | `mmx.c/h` + font — fully self-contained module (config, translation, trainer, OSD) | copied in, never conflicts |
| `patches/` | small hooks into 5 upstream files + MSVC build fixes | may need rebasing |
| `dist/` | `metal.ini`, player README, `translations/english.txt` | independent |
| `UPSTREAM` | pinned upstream repo + commit | bump to move forward |

To move to a newer ALIS version:

1. Edit `UPSTREAM` with the new commit SHA.
2. Run `.\scripts\setup.ps1 -Clean`. Patches apply with `git am --3way`, so
   clean or trivially-conflicting updates just work.
3. If a patch fails: in `build\alis`, resolve the conflict, `git am --continue`,
   then regenerate the series with `git format-patch -2 -o ..\..\patches` and
   commit the refreshed patches together with the `UPSTREAM` bump.

The hooks are deliberately tiny (one `#include` + one call in `script.c`, a
config block in `main.c`, hotkey cases and an OSD call in `sys_sdl2.c`) to
keep rebases painless.

## Credits & license

- **Game**: © 1991 [Silmarils](https://en.wikipedia.org/wiki/Silmarils_(company)). Not affiliated.
- **Engine**: [maestun/alis](https://github.com/maestun/alis) by Olivier
  Huguenot & Vadim Kindl, MIT license.
- **Port additions** (this repo): MIT license — see [LICENSE](LICENSE).
