//
// mmx_miyoo.h — Miyoo Mini (Allium CFW) platform layer for the Metal Mutant
// port: /dev/fb0 video output, raw evdev button input, SDL keyboard-state shim.
// Local addition, not part of upstream ALIS. Compiled only when
// ALIS_MIYOO_ALLIUM is defined.
//

#pragma once

#ifdef ALIS_MIYOO_ALLIUM

#if defined(_MSC_VER)
# include "SDL.h"
#elif __has_include(<SDL.h>)
# include <SDL.h>
#else
# include <SDL2/SDL.h>
#endif

#include "config.h"

// Opens /dev/fb0 and the /dev/input/event* devices. Returns 0 on success
// (input devices are best-effort; only the framebuffer is required).
int  miyoo_init(void);
void miyoo_deinit(void);

// Reads pending evdev events, updates the keyboard-state shim and pushes
// matching SDL_KEYDOWN/SDL_KEYUP events so the engine's existing event
// handling (hotkeys, `button`) keeps working. Call once per poll cycle.
void miyoo_poll_input(void);

// Drop-in replacements for SDL_GetKeyboardState / SDL_GetModState: with no
// SDL video driver on the device, SDL's own keyboard state never updates.
const Uint8 *miyoo_keyboard_state(int *numkeys);
SDL_Keymod   miyoo_mod_state(void);

// Atari-style joystick bits composed from held buttons:
// 0x01 up, 0x02 down, 0x04 left, 0x08 right, 0x80 fire (A).
u8   miyoo_joystick_bits(void);

// Draws the mmx OSD line into the frame, then scales/rotates the ARGB8888
// frame onto /dev/fb0 (the Miyoo panel is 180-degree rotated).
void miyoo_present(u32 *pixels, int w, int h);

// Draws a full-screen error on the panel and holds it for a few seconds —
// launcher logs are invisible on device. Usable before/without miyoo_init.
void miyoo_fatal_message(const char *line1, const char *line2);

// Audio pump: /dev/dsp via Allium's libpadsp OSS shim, driven by a thread
// that runs the engine's SDL-style audio callback (SDL's own dsp backend
// misses the shim because it opens through open64). With no device the
// callback still runs so the music engine keeps advancing, just silently.
// miyoo_audio_open returns the mixing rate; call it before the engine
// derives timing from the rate, and miyoo_audio_run after the mixer
// state (PSG/OPL) is initialized.
int  miyoo_audio_open(int freq);
void miyoo_audio_run(int samples, int muted);
void miyoo_audio_lock(void);
void miyoo_audio_unlock(void);
int  miyoo_audio_playing(void);

#endif // ALIS_MIYOO_ALLIUM
