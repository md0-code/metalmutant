//
// mmx.h — Metal Mutant extras: INI config, translation layer, cheat state.
// Local addition, not part of upstream ALIS.
//

#pragma once

#define MMX_PATH_MAX 1024

// Human-readable names of the port's hotkeys, used in player-facing
// messages (OSD, console). The desktop build uses the F-keys; the Miyoo
// build names the buttons they are mapped to (see mmx_miyoo.c key_map).
#ifdef ALIS_MIYOO_ALLIUM
# define MMX_KEY_CALIBRATE  "Y"
# define MMX_KEY_POWER      "X"
# define MMX_KEY_TURBO      "R1"
# define MMX_KEY_SLOWMO     "L1"
# define MMX_KEY_SAVE       "L2"
# define MMX_KEY_LOAD       "R2"
#else
# define MMX_KEY_CALIBRATE  "F5"
# define MMX_KEY_POWER      "F6"
# define MMX_KEY_TURBO      "F8"
# define MMX_KEY_SLOWMO     "F7"
# define MMX_KEY_SAVE       "F11"
# define MMX_KEY_LOAD       "F12"
#endif

typedef struct {
    char data_path[MMX_PATH_MAX];   // resolved absolute data folder
    char language[MMX_PATH_MAX];    // resolved translation file, empty = off
    int  fullscreen;
    int  scale;                     // 0 = engine default
    int  mute;
    int  cheats;                    // enables F7/F8 speed cheats
} sMMXConfig;

extern sMMXConfig mmx_cfg;
extern int mmx_turbo;
extern int mmx_slowmo;

// Reads <exe_dir>/metal.ini if present; always fills mmx_cfg with defaults
// (data_path = <exe_dir>/data). exe_dir must end with a path separator.
void mmx_load_config(const char *exe_dir);

// Loads "ORIGINAL=TRANSLATION" pairs from mmx_cfg.language. Returns pair count.
int mmx_translate_load(void);

// In-place patches every known original string found in a script buffer.
// Same-length replacement: shorter is space-padded, longer is truncated.
void mmx_translate_apply(unsigned char *buf, int size);

// --- infinite power cheat -------------------------------------------------
// One-time calibration: F5 snapshots VM memory; on each further F5 press only
// cells that strictly decreased (damage taken in between) stay candidates.
// When few enough remain they are locked and persisted to <exe_dir>cheat.dat.
// F6 then freezes the locked bytes every frame.

extern int mmx_power;                       // infinite power toggle state

void mmx_cheat_mark(unsigned char *mem, unsigned int size);   // F5
void mmx_cheat_toggle_power(unsigned char *mem);              // F6
void mmx_cheat_frame(unsigned char *mem);                     // every frame

// --- on-screen display ------------------------------------------------------
// Sets the OSD line (also echoed to the console). The renderer polls
// mmx_osd_current() once per drawn frame; it returns NULL once expired.

void        mmx_osd(const char *fmt, ...);
const char *mmx_osd_current(void);
