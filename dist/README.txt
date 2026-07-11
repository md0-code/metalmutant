METAL MUTANT (Silmarils, 1991) — modern Windows port
=====================================================

Runs the original, untouched game files through a re-implementation of
Silmarils' ALIS engine (https://github.com/maestun/alis) with local
additions: config file, translation layer, cheat hotkeys.

RUN
---
Double-click MetalMutant.exe. Everything is configured in metal.ini
(data folder, fullscreen, window scale, sound, language, cheats).

KEYS
----
In-game controls are the original ones — see the INFO option in the
main menu or the original manual. Engine keys added by this port:

Alt+Enter         toggle fullscreen
F11 / F12         save state / load state (experimental)
F8                turbo (cheat, if enabled in metal.ini)
F7                slow motion (cheat, if enabled in metal.ini)
F6                infinite power on/off (cheat, needs one-time F5 calibration)
F5                calibrate infinite power (see below)
Pause             quit

INFINITE POWER — ONE-TIME CALIBRATION
-------------------------------------
The engine has to find where the game stores your vital energy. Once
found it is remembered in cheat.dat and F6 just works from then on.

  1. Start a game, then press F5  ("CALIBRATION" appears on screen).
  2. Let an enemy hit you (energy bar must go down), press F5 again.
  3. Repeat step 2 until the screen says "ENERGY LOCKED".
     (Usually 2-3 hits are enough. Press F5 ONLY right after losing
     energy — if you press it without having taken damage in between,
     calibration resets and you start over.)
  4. Press F6 anytime to freeze your energy at its current level.

All cheat and save-state feedback is shown in-game (green text,
top-left corner) and mirrored to the console window.

Loading a save state (F12) does NOT require recalibrating — the
locked address stays valid across save states and across sessions.

To recalibrate (e.g. after a wrong lock), delete cheat.dat and repeat.

TRANSLATION
-----------
translations\english.txt is applied at load time by patching the
original French strings in memory — the game files on disk are never
modified. Edit the file to improve wording, or comment out the
"language=" line in metal.ini to play in French. Rule: a translated
line may not be longer than the original (it will be truncated).

NOTE ON COPY PROTECTION
-----------------------
The original game may occasionally ask for a word from the back of the
game box ("CONTROL TEST"). Box scans can be found on the Hall of Light
page for Metal Mutant (https://hol.abime.net/2456) or on abandonware
archive sites.

CREDITS
-------
Game: Silmarils, 1991. Engine re-implementation: the ALIS project
(Olivier Huguenot, Vadim Kindl, MIT license). Not affiliated.
