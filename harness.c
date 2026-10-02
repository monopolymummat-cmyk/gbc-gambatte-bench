/* harness.c — minimal headless libretro frontend for bounty #16517
 * independent frame-count reproduction on Gambatte.
 *
 * Loads a libretro core + ROM, runs frames headlessly, polls work RAM
 * (0xC000) for the bench protocol, prints raw frame counts + tokens.
 *
 * Build: cc -O2 -o harness harness.c -ldl
 * Usage: ./harness <core.so> <rom.gb> [max_frames]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#include <libretro.h>

static void *core_handle = NULL;
static unsigned frame_count = 0;
static int log_enabled = 1;

/* ---- libretro fn pointers ---- */
static void (*core_set_environment)(retro_environment_t);
static void (*core_set_video_refresh)(retro_video_refresh_t);
static void (*core_set_audio_sample)(retro_audio_sample_t);
static void (*core_set_audio_sample_batch)(retro_audio_sample_batch_t);
static void (*core_set_input_poll)(retro_input_poll_t);
static void (*core_set_input_state)(retro_input_state_t);
static void (*core_init)(void);
static void (*core_shutdown)(void);
static void (*core_run)(void);
static bool (*core_load_game)(const struct retro_game_info *);
static void *(*core_get_memory_data)(unsigned);
static size_t (*core_get_memory_size)(unsigned);

/* ---- callbacks ---- */
static void cb_video_refresh(const void *data, unsigned w, unsigned h, size_t pitch) {
    (void)data; (void)w; (void)h; (void)pitch;
}
static void cb_audio_sample(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t cb_audio_batch(const int16_t *data, size_t frames) { (void)data; return frames; }
static void cb_input_poll(void) {}
static int16_t cb_input_state(unsigned port, unsigned dev, unsigned idx, unsigned id) {
    (void)port; (void)dev; (void)idx; (void)id; return 0;
}
static void cb_log(enum retro_log_level level, const char *fmt, ...) {
    if (!log_enabled) return;
    (void)level; (void)fmt;
    /* silence core chatter; keep the run log clean */
}

static bool cb_environment(unsigned cmd, void *data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;
        if (*fmt == RETRO_PIXEL_FORMAT_RGB565) return true;
        *fmt = RETRO_PIXEL_FORMAT_0RGB1555;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
        const char **dir = (const char **)data;
        *dir = "/tmp";
        return true;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
        struct retro_log_callback *log = (struct retro_log_callback *)data;
        log->log = cb_log;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool *)data = true;
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
        /* accept core variable defaults silently */
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *var = (struct retro_variable *)data;
        var->value = NULL; /* force default */
        return false;
    }
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        *(bool *)data = false;
        return true;
    default:
        return false;
    }
}

static void die(const char *msg) { fprintf(stderr, "FATAL: %s\n", msg); exit(1); }

static void load_core(const char *path) {
    core_handle = dlopen(path, RTLD_NOW);
    if (!core_handle) die(dlerror());
    #define SYM(var, name) do { \
        *(void **)(&var) = dlsym(core_handle, name); \
        if (!var) { fprintf(stderr, "missing symbol %s\n", name); exit(1); } \
    } while (0)
    SYM(core_set_environment, "retro_set_environment");
    SYM(core_set_video_refresh, "retro_set_video_refresh");
    SYM(core_set_audio_sample, "retro_set_audio_sample");
    SYM(core_set_audio_sample_batch, "retro_set_audio_sample_batch");
    SYM(core_set_input_poll, "retro_set_input_poll");
    SYM(core_set_input_state, "retro_set_input_state");
    SYM(core_init, "retro_init");
    SYM(core_run, "retro_run");
    SYM(core_load_game, "retro_load_game");
    SYM(core_get_memory_data, "retro_get_memory_data");
    SYM(core_get_memory_size, "retro_get_memory_size");
    #undef SYM
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <core.so> <rom> [max_frames]\n", argv[0]); return 2; }
    const char *core_path = argv[1];
    const char *rom_path = argv[2];
    unsigned long max_frames = (argc > 3) ? strtoul(argv[3], NULL, 10) : 2000000UL;

    FILE *f = fopen(rom_path, "rb");
    if (!f) die("cannot open ROM");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *rom = malloc(sz);
    if (fread(rom, 1, sz, f) != (size_t)sz) die("short ROM read");
    fclose(f);

    load_core(core_path);
    core_set_environment(cb_environment);
    core_set_video_refresh(cb_video_refresh);
    core_set_audio_sample(cb_audio_sample);
    core_set_audio_sample_batch(cb_audio_batch);
    core_set_input_poll(cb_input_poll);
    core_set_input_state(cb_input_state);
    core_init();

    struct retro_game_info info = {0};
    info.path = rom_path;
    info.data = rom;
    info.size = sz;
    if (!core_load_game(&info)) die("core rejected ROM");

    uint8_t *wram = (uint8_t *)core_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
    size_t wram_size = core_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    if (!wram || wram_size < 0x1000) die("no WRAM exposed");
    fprintf(stderr, "[harness] WRAM: %zu bytes @ %p\n", wram_size, (void *)wram);
    fprintf(stderr, "[harness] ROM: %ld bytes\n", sz);

    /* CGB check: ROM header 0x143 */
    fprintf(stderr, "[harness] CGB flag (0x143): 0x%02x\n", rom[0x143]);

    unsigned magic_seen = 0, done_frame = 0;
    while ((unsigned long)frame_count < max_frames) {
        core_run();
        frame_count++;

        if (wram[0x00] == 0xB1) {
            if (!magic_seen) {
                magic_seen = frame_count;
                fprintf(stderr, "[harness] bench magic seen at frame %u\n", frame_count);
            }
            if (wram[0x01] == 0x2A) {
                done_frame = frame_count;
                break;
            }
        }
        if (frame_count % 10000 == 0) {
            fprintf(stderr, "[harness] frame %u magic=%u done=%u count=%u\n",
                    frame_count, wram[0x00], wram[0x01], wram[0x02]);
        }
    }

    printf("RESULT frames=%u magic_frame=%u done=%u token_count=%u\n",
           frame_count, magic_seen, done_frame, wram[0x02]);
    printf("TOKENS ");
    const uint16_t *toks = (const uint16_t *)(wram + 0x10);
    for (int i = 0; i < 16; i++) printf("%u ", toks[i]);
    printf("\n");
    return (done_frame ? 0 : 1);
}
