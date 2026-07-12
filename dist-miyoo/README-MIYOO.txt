METAL MUTANT (Silmarils, 1991) — Miyoo Mini port (Allium CFW)
==============================================================

Runs the original, untouched game files through a re-implementation of
Silmarils' ALIS engine (https://github.com/maestun/alis) with local
additions: config file, translation layer, cheats, save states.

INSTALL
-------
1. Copy this MetalMutant.pak folder to Apps/ on your SD card.
2. Copy your original Metal Mutant game files (Atari ST or DOS version)
   into MetalMutant.pak/data/. The set must include MAIN.IO — that is
   the file the engine looks for; without it the game shows
   "NO GAME FILES FOUND" and exits.
   NO GAME DATA IS INCLUDED — bring your own copy.
3. Launch Metal Mutant from the Apps menu.

Everything is configured in metal.ini (data folder, sound, language,
cheats).

BUTTONS
-------
D-pad             move / navigate (joystick directions)
A                 fire / action (joystick button)
B                 Esc — pause, skip, back
Start             Return — confirm
Select            F1
Menu              quit back to Allium

Engine features added by this port:
L2 / R2           save state / load state (experimental)
R1                turbo (cheat, if enabled in metal.ini)
L1                slow motion (cheat, if enabled in metal.ini)
X                 infinite power on/off (cheat, needs one-time Y calibration)
Y                 calibrate infinite power (see below)

INFINITE POWER — ONE-TIME CALIBRATION
-------------------------------------
The engine has to find where the game stores your vital energy. Once
found it is remembered in cheat.dat and X just works from then on.

  1. Start a game, then press Y  ("CALIBRATION" appears on screen).
  2. Let an enemy hit you (energy bar must go down), press Y again.
  3. Repeat step 2 until the screen says "ENERGY LOCKED".
     (Usually 2-3 hits are enough. Press Y ONLY right after losing
     energy — if you press it without having taken damage in between,
     calibration resets and you start over.)
  4. Press X anytime to freeze your energy at its current level.

All cheat and save-state feedback is shown in-game (green text,
top-left corner). On-screen messages still name the desktop keys
(F5/F6/F11...) — the button mapping above applies.

Loading a save state (R2) does NOT require recalibrating. To
recalibrate (e.g. after a wrong lock), delete cheat.dat and repeat.

TRANSLATION
-----------
translations/english.txt is applied at load time by patching the
original French strings in memory — the game files on disk are never
modified. Comment out the "language=" line in metal.ini to play in
French.

NOTE ON COPY PROTECTION
-----------------------
The original game may occasionally ask for a word from the back of the
game box ("CONTROL TEST"). Typing it needs a keyboard, which the Miyoo
does not have — if it triggers, quit and restart, or load a save state
made after a passed check. Box scans: https://hol.abime.net/2456

CREDITS
-------
Game: Silmarils, 1991. Engine re-implementation: the ALIS project
(Olivier Huguenot, Vadim Kindl, MIT license). Not affiliated.
