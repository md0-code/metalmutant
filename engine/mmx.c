//
// mmx.c — Metal Mutant extras: INI config, translation layer, cheat state.
// Local addition, not part of upstream ALIS. Plain C, stdio only.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmx.h"

#ifndef _MSC_VER
#define _strdup strdup
#endif

sMMXConfig mmx_cfg;
int mmx_turbo = 0;
int mmx_slowmo = 0;
int mmx_power = 0;

static char exe_dir_saved[MMX_PATH_MAX];

#define MMX_MAX_PAIRS 1024

typedef struct {
    char *orig;
    char *repl;
    int   olen;
    int   rlen;
} sMMXPair;

static sMMXPair pairs[MMX_MAX_PAIRS];
static int      pair_count = 0;

static void trim_eol(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r'))
        s[--n] = 0;
}

// resolve a possibly-relative path against exe_dir (which ends in a separator)
static void resolve(char *out, size_t outsz, const char *exe_dir, const char *val) {
    int absolute = (val[0] == '/' || val[0] == '\\' ||
                    (val[0] != 0 && val[1] == ':'));
    if (absolute)
        snprintf(out, outsz, "%s", val);
    else
        snprintf(out, outsz, "%s%s", exe_dir, val);
}

static void mmx_cheat_addrs_load(void);

void mmx_load_config(const char *exe_dir) {

    memset(&mmx_cfg, 0, sizeof(mmx_cfg));
    snprintf(exe_dir_saved, sizeof(exe_dir_saved), "%s", exe_dir);
    resolve(mmx_cfg.data_path, sizeof(mmx_cfg.data_path), exe_dir, "data");
    mmx_cheat_addrs_load();

    char ini_path[MMX_PATH_MAX];
    snprintf(ini_path, sizeof(ini_path), "%smetal.ini", exe_dir);

    FILE *fp = fopen(ini_path, "r");
    if (fp == NULL)
        return;

    printf("Config: %s\n", ini_path);

    char line[MMX_PATH_MAX];
    while (fgets(line, sizeof(line), fp)) {
        trim_eol(line);
        if (line[0] == '#' || line[0] == ';' || line[0] == '[' || line[0] == 0)
            continue;

        char *eq = strchr(line, '=');
        if (eq == NULL)
            continue;
        *eq = 0;
        const char *key = line;
        const char *val = eq + 1;

        if      (strcmp(key, "data") == 0 && val[0])
            resolve(mmx_cfg.data_path, sizeof(mmx_cfg.data_path), exe_dir, val);
        else if (strcmp(key, "language") == 0 && val[0])
            resolve(mmx_cfg.language, sizeof(mmx_cfg.language), exe_dir, val);
        else if (strcmp(key, "fullscreen") == 0)
            mmx_cfg.fullscreen = atoi(val);
        else if (strcmp(key, "scale") == 0)
            mmx_cfg.scale = atoi(val);
        else if (strcmp(key, "mute") == 0)
            mmx_cfg.mute = atoi(val);
        else if (strcmp(key, "cheats") == 0)
            mmx_cfg.cheats = atoi(val);
    }
    fclose(fp);
}

// longest originals first, so prefixes of longer strings can't shadow them
static int pair_cmp(const void *a, const void *b) {
    return ((const sMMXPair *)b)->olen - ((const sMMXPair *)a)->olen;
}

int mmx_translate_load(void) {

    pair_count = 0;
    if (mmx_cfg.language[0] == 0)
        return 0;

    FILE *fp = fopen(mmx_cfg.language, "r");
    if (fp == NULL) {
        printf("Translation file not found: %s\n", mmx_cfg.language);
        return 0;
    }

    // format: ORIGINAL|TRANSLATION — '|' because game text itself uses '='
    char line[2048];
    while (fgets(line, sizeof(line), fp) && pair_count < MMX_MAX_PAIRS) {
        trim_eol(line);
        if (line[0] == '#' || line[0] == 0)
            continue;

        char *eq = strchr(line, '|');
        if (eq == NULL || eq == line)
            continue;
        *eq = 0;

        sMMXPair *p = &pairs[pair_count];
        p->orig = _strdup(line);
        p->repl = _strdup(eq + 1);
        p->olen = (int)strlen(p->orig);
        p->rlen = (int)strlen(p->repl);
        if (p->rlen > p->olen) {
            printf("Translation truncated to %d chars: '%s'\n", p->olen, p->repl);
            p->repl[p->olen] = 0;
            p->rlen = p->olen;
        }
        pair_count++;
    }
    fclose(fp);

    qsort(pairs, pair_count, sizeof(sMMXPair), pair_cmp);
    printf("Translation: %d strings loaded from %s\n", pair_count, mmx_cfg.language);
    return pair_count;
}

// ===========================================================================
// On-screen display
// ===========================================================================

#define MMX_OSD_FRAMES 240   // ~4 seconds at 60 fps

static char osd_text[160];
static int  osd_frames = 0;

void mmx_osd(const char *fmt, ...) {

    va_list args;
    va_start(args, fmt);
    vsnprintf(osd_text, sizeof(osd_text), fmt, args);
    va_end(args);
    osd_frames = MMX_OSD_FRAMES;

    printf("%s\n", osd_text);
}

const char *mmx_osd_current(void) {

    if (osd_frames <= 0)
        return NULL;
    osd_frames--;
    return osd_text;
}

// ===========================================================================
// Infinite power cheat — built-in memory-diff trainer
// ===========================================================================

#define MMX_MAX_LOCKED  2     // energy + at most a display copy
#define MMX_MAX_CHANGES 100   // frame-to-frame changes before a cell is ruled out
#define MMX_SCAN_BLOCK  4096

static unsigned char *scan_prev = NULL;   // VM memory at last F5 press
static unsigned char *scan_cand = NULL;   // 1 = still a candidate cell
static unsigned char *scan_last = NULL;   // VM memory at last frame
static unsigned char *scan_chg  = NULL;   // per-cell change counter
static unsigned int   scan_size = 0;
static int            scan_marks = 0;

static unsigned int   locked_addr[MMX_MAX_LOCKED];
static unsigned char  locked_val[MMX_MAX_LOCKED];
static int            locked_count = 0;

static void cheat_dat_path(char *out, size_t outsz) {
    snprintf(out, outsz, "%scheat.dat", exe_dir_saved);
}

static void mmx_cheat_addrs_load(void) {

    char path[MMX_PATH_MAX];
    cheat_dat_path(path, sizeof(path));

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return;

    locked_count = 0;
    unsigned int addr;
    while (locked_count < MMX_MAX_LOCKED && fscanf(fp, "%x", &addr) == 1)
        locked_addr[locked_count++] = addr;
    fclose(fp);

    if (locked_count > 0)
        printf("Cheat: energy address(es) loaded from cheat.dat — " MMX_KEY_POWER " ready\n");
}

static void mmx_cheat_addrs_save(void) {

    char path[MMX_PATH_MAX];
    cheat_dat_path(path, sizeof(path));

    FILE *fp = fopen(path, "w");
    if (fp == NULL)
        return;
    for (int i = 0; i < locked_count; i++)
        fprintf(fp, "%x\n", locked_addr[i]);
    fclose(fp);
}

static void scan_reset(void) {
    free(scan_prev);  scan_prev = NULL;
    free(scan_cand);  scan_cand = NULL;
    free(scan_last);  scan_last = NULL;
    free(scan_chg);   scan_chg  = NULL;
    scan_marks = 0;
}

// Runs every frame while calibrating: cells that keep changing on their own
// (timers, animation counters, video memory) are not the energy variable —
// energy only moves when you are hit. Rule out the fidgety ones.
static void scan_frequency_filter(unsigned char *mem) {

    for (unsigned int o = 0; o < scan_size; o += MMX_SCAN_BLOCK) {
        unsigned int n = scan_size - o < MMX_SCAN_BLOCK ? scan_size - o : MMX_SCAN_BLOCK;
        if (memcmp(mem + o, scan_last + o, n) == 0)
            continue;

        for (unsigned int a = o; a < o + n; a++) {
            if (mem[a] != scan_last[a] && scan_cand[a] && ++scan_chg[a] > MMX_MAX_CHANGES)
                scan_cand[a] = 0;
        }
        memcpy(scan_last + o, mem + o, n);
    }
}

void mmx_cheat_mark(unsigned char *mem, unsigned int size) {

    if (scan_prev == NULL) {
        scan_prev = (unsigned char *)malloc(size);
        scan_cand = (unsigned char *)malloc(size);
        scan_last = (unsigned char *)malloc(size);
        scan_chg  = (unsigned char *)calloc(size, 1);
        if (scan_prev == NULL || scan_cand == NULL || scan_last == NULL || scan_chg == NULL) {
            scan_reset();
            return;
        }
        memcpy(scan_prev, mem, size);
        memcpy(scan_last, mem, size);
        memset(scan_cand, 1, size);
        scan_size = size;
        scan_marks = 1;
        mmx_osd("CALIBRATION: TAKE A HIT, PRESS " MMX_KEY_CALIBRATE " AGAIN");
        return;
    }

    unsigned int survivors = 0, first = 0;
    for (unsigned int a = 0; a < scan_size; a++) {
        if (scan_cand[a] && mem[a] < scan_prev[a]) {
            if (survivors == 0)
                first = a;
            survivors++;
        }
        else {
            scan_cand[a] = 0;
        }
    }
    memcpy(scan_prev, mem, scan_size);
    scan_marks++;

    if (survivors == 0) {
        mmx_osd("NO MATCH: CALIBRATION RESET, PRESS " MMX_KEY_CALIBRATE);
        scan_reset();
        return;
    }

    if (scan_marks >= 3 && survivors <= MMX_MAX_LOCKED) {
        locked_count = 0;
        for (unsigned int a = first; a < scan_size && locked_count < MMX_MAX_LOCKED; a++)
            if (scan_cand[a])
                locked_addr[locked_count++] = a;
        mmx_cheat_addrs_save();
        mmx_osd("ENERGY LOCKED: " MMX_KEY_POWER " = INFINITE POWER");
        printf("[cheat] locked bytes:");
        for (int i = 0; i < locked_count; i++)
            printf(" 0x%x", locked_addr[i]);
        printf("\n");
        scan_reset();
        return;
    }

    mmx_osd("%u CANDIDATES: MORE DAMAGE, THEN " MMX_KEY_CALIBRATE, survivors);
}

void mmx_cheat_toggle_power(unsigned char *mem) {

    if (locked_count == 0) {
        mmx_osd("NOT CALIBRATED: " MMX_KEY_CALIBRATE ", TAKE A HIT, " MMX_KEY_CALIBRATE " AGAIN");
        return;
    }

    mmx_power ^= 1;
    if (mmx_power)
        for (int i = 0; i < locked_count; i++)
            locked_val[i] = mem[locked_addr[i]];   // freeze at current level

    mmx_osd("INFINITE POWER %s", mmx_power ? "ON" : "OFF");
}

void mmx_cheat_frame(unsigned char *mem) {

    if (scan_prev != NULL)
        scan_frequency_filter(mem);

    if (!mmx_power)
        return;
    for (int i = 0; i < locked_count; i++)
        mem[locked_addr[i]] = locked_val[i];
}

void mmx_translate_apply(unsigned char *buf, int size) {

    if (pair_count == 0 || buf == NULL || size <= 0)
        return;

    for (int i = 0; i < pair_count; i++) {
        sMMXPair *p = &pairs[i];
        int limit = size - p->olen;
        for (int at = 0; at <= limit; at++) {
            if (buf[at] != (unsigned char)p->orig[0] ||
                memcmp(buf + at, p->orig, p->olen) != 0)
                continue;
            memcpy(buf + at, p->repl, p->rlen);
            if (p->rlen < p->olen)
                memset(buf + at + p->rlen, ' ', p->olen - p->rlen);
            at += p->olen - 1;
        }
    }
}
