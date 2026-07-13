//
// mmx_miyoo.c — Miyoo Mini (Allium CFW) platform layer for the Metal Mutant
// port. Local addition, not part of upstream ALIS.
//
// Video: no SDL video backend works on the device, so the converted ARGB8888
//   frame is scaled (nearest-neighbour, 1.2 pixel aspect like the desktop
//   build) and written straight to /dev/fb0. The panel is mounted upside
//   down, so the 180-degree rotation is baked into the scaling LUTs.
// Input: the Miyoo's buttons arrive as keyboard keycodes on
//   /dev/input/event*. They are translated to the SDL keys the engine
//   already understands (arrows, Space, Esc, Return, and the port's
//   F5-F8/F11/F12 hotkeys) and mirrored into a local keyboard-state array
//   that replaces SDL_GetKeyboardState.
//

#ifdef ALIS_MIYOO_ALLIUM

// for readdir64: 32-bit readdir EOVERFLOWs on 64-bit-inode filesystems. Do
// NOT build this file with _FILE_OFFSET_BITS=64 instead — that would turn
// open() into open64(), which Allium's libpadsp audio shim does not hook.
#define _LARGEFILE64_SOURCE 1

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "alis.h"
#include "image.h"
#include "mmx.h"
#include "mmx_font.h"
#include "mmx_miyoo.h"
#include "video.h"

// ============================================================================
#pragma mark - Case-insensitive paths
// ============================================================================

// The engine lowercases whole paths before fopen (sys_fopen, the csload
// opcode) — fine on Windows, fatal on the device where /mnt/SDCARD/... is
// case-sensitive: every game resource open fails and the VM idles on a
// black screen. Fix the basename case in place against the real directory
// listing; the directory part comes from config/SDL_GetBasePath and is
// already correct. Returns path (unchanged if no match — e.g. files about
// to be created).
char *miyoo_path_ci(char *path)
{
    FILE *probe = fopen(path, "rb");
    if (probe) {
        fclose(probe);
        return path;
    }

    char *slash = strrchr(path, '/');
    if (slash == NULL)
        slash = strrchr(path, '\\');
    if (slash == NULL || slash[1] == 0)
        return path;

    char dir[1024];
    size_t dlen = (size_t)(slash - path);
    if (dlen == 0 || dlen >= sizeof(dir))
        return path;
    memcpy(dir, path, dlen);
    dir[dlen] = 0;

    DIR *d = opendir(dir);
    if (d == NULL)
        return path;

    struct dirent64 *ent;
    while ((ent = readdir64(d)) != NULL) {
        if (strcasecmp(ent->d_name, slash + 1) == 0) {
            strcpy(slash + 1, ent->d_name);   // same length, case-only change
            break;
        }
    }
    closedir(d);
    return path;
}

// ============================================================================
#pragma mark - Input
// ============================================================================

#define MIYOO_MAX_INPUTS 8

static int   input_fds[MIYOO_MAX_INPUTS];
static int   input_fd_count = 0;
static Uint8 key_state[SDL_NUM_SCANCODES];
static u8    joy_bits = 0;

typedef struct {
    int          code;      // Linux keycode emitted by the button
    SDL_Scancode scancode;
    SDL_Keycode  sym;
    u8           joy;       // Atari joystick bit, 0 = none
} sMiyooKey;

// Physical button -> engine key. Verified device keycodes: A=57 B=29 X=42
// Y=56 L1=18 R1=20 L2=15 R2=14 Start=28 Select=97 Menu=1 (see README-MIYOO).
static const sMiyooKey key_map[] = {
    { KEY_UP,        SDL_SCANCODE_UP,     SDLK_UP,     0x01 },
    { KEY_DOWN,      SDL_SCANCODE_DOWN,   SDLK_DOWN,   0x02 },
    { KEY_LEFT,      SDL_SCANCODE_LEFT,   SDLK_LEFT,   0x04 },
    { KEY_RIGHT,     SDL_SCANCODE_RIGHT,  SDLK_RIGHT,  0x08 },
    { KEY_SPACE,     SDL_SCANCODE_SPACE,  SDLK_SPACE,  0x80 }, // A: fire
    { KEY_LEFTCTRL,  SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, 0 },    // B: pause/back
    { KEY_ENTER,     SDL_SCANCODE_RETURN, SDLK_RETURN, 0 },    // Start
    { KEY_RIGHTCTRL, SDL_SCANCODE_F1,     SDLK_F1,     0 },    // Select
    { KEY_LEFTSHIFT, SDL_SCANCODE_F6,     SDLK_F6,     0 },    // X: infinite power
    { KEY_LEFTALT,   SDL_SCANCODE_F5,     SDLK_F5,     0 },    // Y: calibrate
    { 18,            SDL_SCANCODE_F7,     SDLK_F7,     0 },    // L1: slow motion
    { 20,            SDL_SCANCODE_F8,     SDLK_F8,     0 },    // R1: turbo
    { KEY_TAB,       SDL_SCANCODE_F11,    SDLK_F11,    0 },    // L2: save state
    { KEY_BACKSPACE, SDL_SCANCODE_F12,    SDLK_F12,    0 },    // R2: load state
    { KEY_ESC,       SDL_SCANCODE_PAUSE,  SDLK_PAUSE,  0 },    // Menu: quit
};

static const sMiyooKey *miyoo_lookup_key(int code)
{
    for (size_t i = 0; i < sizeof(key_map) / sizeof(key_map[0]); i++)
        if (key_map[i].code == code)
            return &key_map[i];
    return NULL;
}

static void miyoo_input_open(void)
{
    input_fd_count = 0;
    for (int i = 0; i < MIYOO_MAX_INPUTS; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        input_fds[input_fd_count++] = fd;
    }

    if (input_fd_count == 0)
        fprintf(stderr, "   No /dev/input/event* devices found\n");
}

static void miyoo_input_close(void)
{
    for (int i = 0; i < input_fd_count; i++)
        close(input_fds[i]);
    input_fd_count = 0;
}

void miyoo_poll_input(void)
{
    for (int i = 0; i < input_fd_count; i++) {
        for (;;) {
            struct input_event ev;
            ssize_t bytes = read(input_fds[i], &ev, sizeof(ev));
            if (bytes != (ssize_t)sizeof(ev)) {
                if (bytes < 0 && errno == EINTR)
                    continue;
                break;
            }

            if (ev.type != EV_KEY || ev.value > 1)  // value 2 = autorepeat
                continue;

            const sMiyooKey *key = miyoo_lookup_key(ev.code);
            if (key == NULL)
                continue;

            key_state[key->scancode] = (Uint8)ev.value;
            if (key->joy) {
                if (ev.value) joy_bits |= key->joy;
                else          joy_bits &= (u8)~key->joy;
            }

            SDL_Event sdl_ev;
            memset(&sdl_ev, 0, sizeof(sdl_ev));
            sdl_ev.type = ev.value ? SDL_KEYDOWN : SDL_KEYUP;
            sdl_ev.key.state = ev.value ? SDL_PRESSED : SDL_RELEASED;
            sdl_ev.key.keysym.scancode = key->scancode;
            sdl_ev.key.keysym.sym = key->sym;
            sdl_ev.key.keysym.mod = KMOD_NONE;
            SDL_PushEvent(&sdl_ev);
        }
    }
}

const Uint8 *miyoo_keyboard_state(int *numkeys)
{
    if (numkeys)
        *numkeys = SDL_NUM_SCANCODES;
    return key_state;
}

SDL_Keymod miyoo_mod_state(void)
{
    return KMOD_NONE;
}

u8 miyoo_joystick_bits(void)
{
    return joy_bits;
}

// ============================================================================
#pragma mark - Audio pump
// ============================================================================
//
// SDL's OSS backend opens /dev/dsp through the LFS open64() entry point,
// which Allium's libpadsp.so preload does not intercept — so SDL audio
// reports "no such device" even though OSS-over-padsp works (the fuse port
// proves it with a plain open()). Drive /dev/dsp ourselves from a pump
// thread instead. If the device still cannot be opened the pump keeps
// calling the engine's audio callback and discards the output: the ALIS
// music engine advances from that callback, so game logic must never
// depend on a real audio device existing.

#define MIYOO_OSS_DEVICE            "/dev/dsp"
#define MIYOO_OSS_AFMT_S16_LE       0x00000010
#define MIYOO_OSS_SNDCTL_DSP_SPEED       _IOWR('P', 2, int)
#define MIYOO_OSS_SNDCTL_DSP_SETFMT      _IOWR('P', 5, int)
#define MIYOO_OSS_SNDCTL_DSP_CHANNELS    _IOWR('P', 6, int)
#define MIYOO_OSS_SNDCTL_DSP_SETFRAGMENT _IOWR('P', 10, int)
// eight 4096-byte fragments: room to absorb scheduling jitter
#define MIYOO_OSS_FRAGMENT          ((8 << 16) | 12)

extern void sys_audio_callback(void *userdata, u8 *stream, s32 len);

static int          dsp_fd = -1;
static int          audio_freq = 22050;
static SDL_mutex   *audio_mutex = NULL;
static SDL_Thread  *audio_thread = NULL;
static volatile int audio_running = 0;
static int          audio_muted = 0;
static u8          *audio_chunk = NULL;
static int          audio_chunk_bytes = 0;

// Opens /dev/dsp (best effort) and returns the sample rate the engine
// should mix at. Call before the engine derives timing from the rate.
int miyoo_audio_open(int freq)
{
    const char *env = getenv("MMX_SOUND_FREQ");
    if (env && atoi(env) > 0)
        freq = atoi(env);

    audio_freq = freq;

    dsp_fd = open(MIYOO_OSS_DEVICE, O_WRONLY);
    if (dsp_fd < 0) {
        fprintf(stderr, "   Could not open %s: %s — running silent\n",
                MIYOO_OSS_DEVICE, strerror(errno));
        return audio_freq;
    }

    int fragment = MIYOO_OSS_FRAGMENT;
    int format = MIYOO_OSS_AFMT_S16_LE;
    int channels = 1;
    int speed = freq;
    ioctl(dsp_fd, MIYOO_OSS_SNDCTL_DSP_SETFRAGMENT, &fragment);
    if (ioctl(dsp_fd, MIYOO_OSS_SNDCTL_DSP_SETFMT, &format) < 0 ||
        format != MIYOO_OSS_AFMT_S16_LE ||
        ioctl(dsp_fd, MIYOO_OSS_SNDCTL_DSP_CHANNELS, &channels) < 0 ||
        channels != 1) {
        fprintf(stderr, "   %s rejected s16/mono — running silent\n", MIYOO_OSS_DEVICE);
        close(dsp_fd);
        dsp_fd = -1;
        return audio_freq;
    }
    if (ioctl(dsp_fd, MIYOO_OSS_SNDCTL_DSP_SPEED, &speed) == 0 && speed > 0)
        audio_freq = speed;

    printf("  Audio: %s, %d Hz, s16 mono\n", MIYOO_OSS_DEVICE, audio_freq);
    return audio_freq;
}

static int miyoo_audio_thread_fn(void *unused)
{
    (void)unused;
    while (audio_running) {
        SDL_LockMutex(audio_mutex);
        sys_audio_callback(NULL, audio_chunk, audio_chunk_bytes);
        SDL_UnlockMutex(audio_mutex);

        if (dsp_fd >= 0) {
            if (audio_muted)
                memset(audio_chunk, 0, audio_chunk_bytes);
            // blocking write against the fragment queue paces the loop
            int off = 0;
            while (off < audio_chunk_bytes) {
                ssize_t n = write(dsp_fd, audio_chunk + off, audio_chunk_bytes - off);
                if (n < 0) {
                    if (errno == EINTR)
                        continue;
                    fprintf(stderr, "   %s write failed: %s — running silent\n",
                            MIYOO_OSS_DEVICE, strerror(errno));
                    close(dsp_fd);
                    dsp_fd = -1;
                    break;
                }
                off += (int)n;
            }
        }
        else {
            // no device: keep the callback (and the music engine) ticking
            usleep((useconds_t)((1000000LL * (audio_chunk_bytes / 2)) / audio_freq));
        }
    }
    return 0;
}

// Starts the pump thread; the engine's mixer state (PSG etc.) must be
// initialized first, since the callback runs immediately.
void miyoo_audio_run(int samples, int muted)
{
    if (audio_running)
        return;

    audio_muted = muted;
    audio_chunk_bytes = samples * 2;
    audio_chunk = calloc(1, (size_t)audio_chunk_bytes);
    audio_mutex = SDL_CreateMutex();
    if (audio_chunk == NULL || audio_mutex == NULL)
        return;

    audio_running = 1;
    audio_thread = SDL_CreateThread(miyoo_audio_thread_fn, "mmx_audio", NULL);
    if (audio_thread == NULL) {
        audio_running = 0;
        fprintf(stderr, "   Could not start audio thread: %s\n", SDL_GetError());
    }
}

static void miyoo_audio_stop(void)
{
    if (audio_running) {
        audio_running = 0;
        SDL_WaitThread(audio_thread, NULL);
        audio_thread = NULL;
    }
    if (audio_mutex) {
        SDL_DestroyMutex(audio_mutex);
        audio_mutex = NULL;
    }
    free(audio_chunk);
    audio_chunk = NULL;
    if (dsp_fd >= 0) {
        close(dsp_fd);
        dsp_fd = -1;
    }
}

void miyoo_audio_lock(void)
{
    if (audio_mutex)
        SDL_LockMutex(audio_mutex);
}

void miyoo_audio_unlock(void)
{
    if (audio_mutex)
        SDL_UnlockMutex(audio_mutex);
}

int miyoo_audio_playing(void)
{
    return audio_running;
}

// ============================================================================
#pragma mark - Framebuffer video
// ============================================================================

#define MIYOO_PRESENT_MIN_MS 16   // cap fb writes at ~60 Hz (poll runs at 120)

static int                       fb_fd = -1;
static struct fb_var_screeninfo  fb_var;
static struct fb_fix_screeninfo  fb_fix;
static unsigned char            *fb_mem = NULL;
static size_t                    fb_mem_size = 0;
static int                       fb_bpp = 4;      // bytes per pixel
static int                       fb_direct32 = 0; // raw u32 copy is valid

static int  *lut_src_x = NULL;   // dest column -> source column (rotated)
static int  *lut_src_y = NULL;   // dest row -> source row (rotated)
static int   lut_src_w = 0, lut_src_h = 0;
static int   dest_w = 0, dest_h = 0, dest_off_x = 0, dest_off_y = 0;
static Uint32 last_present_ticks = 0;

// The desktop build displays the 320x200 frame with a 1.2 vertical stretch
// (Atari ST non-square pixels): 320x240 logical, i.e. 4:3 - a perfect fit
// for the 640x480 panel.
#define MIYOO_PIXEL_ASPECT 1.2f

static int miyoo_fb_open(void)
{
    fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fb_fd < 0) {
        fprintf(stderr, "   Could not open /dev/fb0: %s\n", strerror(errno));
        return 1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_var) < 0 ||
        ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
        fprintf(stderr, "   Could not query /dev/fb0: %s\n", strerror(errno));
        close(fb_fd);
        fb_fd = -1;
        return 1;
    }

    if (fb_var.bits_per_pixel != 16 && fb_var.bits_per_pixel != 32) {
        fprintf(stderr, "   Unsupported /dev/fb0 depth: %u\n", fb_var.bits_per_pixel);
        close(fb_fd);
        fb_fd = -1;
        return 1;
    }

    fb_bpp = (int)(fb_var.bits_per_pixel + 7) / 8;
    if (!fb_fix.line_length)
        fb_fix.line_length = fb_var.xres * fb_bpp;

    // source is ARGB8888 u32; a raw copy is valid for the common
    // 32bpp BGRA-in-memory layout the Miyoo panel uses
    fb_direct32 = fb_bpp == 4 &&
                  fb_var.red.offset == 16 && fb_var.red.length == 8 &&
                  fb_var.green.offset == 8 && fb_var.green.length == 8 &&
                  fb_var.blue.offset == 0 && fb_var.blue.length == 8;

    fb_mem_size = (size_t)fb_fix.line_length *
                  (fb_var.yres_virtual ? fb_var.yres_virtual : fb_var.yres);
    if (fb_fix.smem_len > fb_mem_size)
        fb_mem_size = fb_fix.smem_len;

    fb_mem = mmap(NULL, fb_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        fprintf(stderr, "   Could not mmap /dev/fb0: %s\n", strerror(errno));
        fb_mem = NULL;
        close(fb_fd);
        fb_fd = -1;
        return 1;
    }

    printf("  Framebuffer: %ux%u (virtual %ux%u), %ubpp, stride %u%s\n",
           fb_var.xres, fb_var.yres, fb_var.xres_virtual, fb_var.yres_virtual,
           fb_var.bits_per_pixel, fb_fix.line_length,
           fb_direct32 ? ", direct copy" : "");
    return 0;
}

static void miyoo_fb_close(void)
{
    if (fb_mem) {
        munmap(fb_mem, fb_mem_size);
        fb_mem = NULL;
        fb_mem_size = 0;
    }
    if (fb_fd >= 0) {
        close(fb_fd);
        fb_fd = -1;
    }
    free(lut_src_x); lut_src_x = NULL;
    free(lut_src_y); lut_src_y = NULL;
    lut_src_w = lut_src_h = 0;
}

// Rebuilds the nearest-neighbour scaling LUTs (with the 180-degree panel
// rotation baked in) when the source geometry changes. Returns 0 on success.
static int miyoo_fb_update_cache(int sw, int sh)
{
    if (sw == lut_src_w && sh == lut_src_h && lut_src_x && lut_src_y)
        return 0;

    int fw = (int)fb_var.xres;
    int fh = (int)fb_var.yres;

    float scale_w = fw / (float)sw;
    float scale_h = fh / ((float)sh * MIYOO_PIXEL_ASPECT);
    float scale = scale_w < scale_h ? scale_w : scale_h;

    dest_w = (int)(sw * scale);
    dest_h = (int)(sh * MIYOO_PIXEL_ASPECT * scale);
    if (dest_w < 1) dest_w = 1;
    if (dest_h < 1) dest_h = 1;
    if (dest_w > fw) dest_w = fw;
    if (dest_h > fh) dest_h = fh;
    dest_off_x = (fw - dest_w) / 2;
    dest_off_y = (fh - dest_h) / 2;

    int *nx = realloc(lut_src_x, (size_t)dest_w * sizeof(int));
    int *ny = realloc(lut_src_y, (size_t)dest_h * sizeof(int));
    if (nx) lut_src_x = nx;
    if (ny) lut_src_y = ny;
    if (!nx || !ny) {
        free(lut_src_x); lut_src_x = NULL;
        free(lut_src_y); lut_src_y = NULL;
        lut_src_w = lut_src_h = 0;
        return 1;
    }

    for (int i = 0; i < dest_w; i++) {
        int sx = sw - 1 - (int)(i * sw / (float)dest_w);
        if (sx < 0) sx = 0;
        if (sx >= sw) sx = sw - 1;
        lut_src_x[i] = sx;
    }
    for (int i = 0; i < dest_h; i++) {
        int sy = sh - 1 - (int)(i * sh / (float)dest_h);
        if (sy < 0) sy = 0;
        if (sy >= sh) sy = sh - 1;
        lut_src_y[i] = sy;
    }

    lut_src_w = sw;
    lut_src_h = sh;

    // letterbox margins stay black for this geometry: clear the fb once here
    memset(fb_mem, 0, fb_mem_size);
    return 0;
}

static Uint32 miyoo_fb_component(Uint8 value, struct fb_bitfield field)
{
    Uint32 scaled;
    if (!field.length)
        return 0;
    if (field.length >= 8)
        scaled = (Uint32)value << (field.length - 8);
    else
        scaled = (Uint32)value >> (8 - field.length);
    return scaled << field.offset;
}

static Uint32 miyoo_fb_pack(u32 argb)
{
    return miyoo_fb_component((argb >> 16) & 0xff, fb_var.red) |
           miyoo_fb_component((argb >> 8) & 0xff, fb_var.green) |
           miyoo_fb_component(argb & 0xff, fb_var.blue) |
           miyoo_fb_component(0xff, fb_var.transp);
}

static void miyoo_draw_text(u32 *pixels, int w, int px, int py, u32 color, const char *text)
{
    for (int i = 0; text[i] && px + (i + 1) * 8 <= w; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c > 126)
            c = ' ';
        const unsigned char *glyph = mmx_font8x8[c - 32];
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                if (glyph[y] & (1 << x))
                    pixels[(py + y) * w + px + i * 8 + x] = color;
    }
}

// Draws the mmx OSD line (cheat/state messages) into the source frame,
// top-left, black box + HUD-green text, before the frame is scaled out.
static void miyoo_osd_blit(u32 *pixels, int w, int h)
{
    const char *text = mmx_osd_current();
    if (text == NULL)
        return;

    int len = (int)strlen(text);
    int max_chars = (w - 8) / 8;
    if (len > max_chars)
        len = max_chars;
    if (len <= 0 || h < 16)
        return;

    char clipped[64];   // max_chars is 39 on the 320px frame
    snprintf(clipped, (size_t)len + 1, "%s", text);

    int margin = 4;
    for (int y = margin - 2; y < margin + 10; y++)
        for (int x = margin - 2; x < margin + len * 8 + 2; x++)
            pixels[y * w + x] = 0xff000000;

    miyoo_draw_text(pixels, w, margin, margin, 0xff50ff64, clipped);
}

void miyoo_present(u32 *pixels, int w, int h)
{
    if (fb_fd < 0 || fb_mem == NULL || pixels == NULL || w <= 0 || h <= 0)
        return;

    // sys_poll_event renders at 120 Hz; writing the panel that often just
    // burns CPU the VM could use, so cap the actual fb writes
    Uint32 now = SDL_GetTicks();
    if (last_present_ticks && (now - last_present_ticks) < MIYOO_PRESENT_MIN_MS)
        return;
    last_present_ticks = now;

    // heartbeat for remote debugging: is the VM clock ticking, is the game
    // actually drawing non-black pixels, is audio flowing? Second line
    // separates "no pixel data composed" from "palette left all black".
    static Uint32 last_diag = 0;
    if (now - last_diag >= 5000) {
        last_diag = now;
        int probed = 0, nonblack = 0;
        for (int i = w * 16; i < w * h; i += 7, probed++)
            if ((pixels[i] & 0x00ffffff) != 0)
                nonblack++;
        printf("[miyoo] state=%d timeclock=%u vtiming=%u nonblack=%d/%d audio=%s@%d\n",
               (int)alis.state, (unsigned)alis.timeclock, (unsigned)image.vtiming,
               nonblack, probed, dsp_fd >= 0 ? "dsp" : "null", audio_freq);

        int bufnz = 0, bufprobed = 0, palnz = 0;
        if (host.pixelbuf.data) {
            for (int i = 0; i < host.pixelbuf.w * host.pixelbuf.h; i += 7, bufprobed++)
                if (host.pixelbuf.data[i])
                    bufnz++;
        }
        if (host.pixelbuf.palette)
            for (int i = 0; i < 256 * 4; i++)
                if (host.pixelbuf.palette[i])
                    palnz++;
        printf("[miyoo] bufnz=%d/%d palnz=%d/1024 palc=%d film=%d,%d fswitch=%d\n",
               bufnz, bufprobed, palnz, (int)image.palc,
               (int)bfilm.type, (int)bfilm.playing, (int)alis.fswitch);
    }

    miyoo_osd_blit(pixels, w, h);

    if (miyoo_fb_update_cache(w, h))
        return;

    unsigned char *fb_base = fb_mem + (size_t)fb_var.yoffset * fb_fix.line_length;

    for (int dy = 0; dy < dest_h; dy++) {
        const u32 *srow = pixels + (size_t)lut_src_y[dy] * w;
        unsigned char *drow_bytes = fb_base + (size_t)(dest_off_y + dy) * fb_fix.line_length;

        if (fb_direct32) {
            u32 *drow = (u32 *)drow_bytes + dest_off_x;
            for (int dx = 0; dx < dest_w; dx++)
                drow[dx] = srow[lut_src_x[dx]];
        }
        else if (fb_bpp == 4) {
            u32 *drow = (u32 *)drow_bytes + dest_off_x;
            for (int dx = 0; dx < dest_w; dx++)
                drow[dx] = miyoo_fb_pack(srow[lut_src_x[dx]]);
        }
        else {
            u16 *drow = (u16 *)drow_bytes + dest_off_x;
            for (int dx = 0; dx < dest_w; dx++)
                drow[dx] = (u16)miyoo_fb_pack(srow[lut_src_x[dx]]);
        }
    }
}

// ============================================================================
#pragma mark - Init / deinit
// ============================================================================

int miyoo_init(void)
{
    memset(key_state, 0, sizeof(key_state));
    joy_bits = 0;

    if (miyoo_fb_open() != 0)
        return 1;

    miyoo_input_open();
    return 0;
}

void miyoo_deinit(void)
{
    miyoo_audio_stop();
    miyoo_input_close();
    miyoo_fb_close();
}

// Full-screen error shown before exiting: launcher logs are invisible on
// device, so startup failures (typically missing game data) must be drawn
// on the panel. Usable before/without miyoo_init.
void miyoo_fatal_message(const char *line1, const char *line2)
{
    int opened_here = fb_fd < 0;
    if (opened_here && miyoo_fb_open() != 0)
        return;

    enum { W = 320, H = 200 };
    u32 *buf = calloc((size_t)W * H, sizeof(u32));
    if (buf) {
        miyoo_draw_text(buf, W, 8, 80, 0xffff5050, "METAL MUTANT");
        if (line1)
            miyoo_draw_text(buf, W, 8, 100, 0xffffffff, line1);
        if (line2)
            miyoo_draw_text(buf, W, 8, 112, 0xffffffff, line2);
        miyoo_present(buf, W, H);
        free(buf);
        sleep(6);
    }

    if (opened_here)
        miyoo_fb_close();
}

#endif // ALIS_MIYOO_ALLIUM
