#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../engine/flspro.h"

#define ENGINE_DATA_BASE    0x1001C000u
#define ENGINE_DATA_SIZE    0xF5000u
#define ENGINE_VOICES_SLOT  0x100BB538u
#define ENGINE_VOICE_COUNT  0x8Au
#define ENGINE_VOICE_SIZE   0x248u

enum {
    SLOT_COUNT = 512,
    LINE_SIZE = 1024,
    COMMAND_SIZE = 32,
    NAME_SIZE = 256,
    DEFAULT_SAMPLE_RATE = 44100,
    DEFAULT_CONTROL_WORD = 0x27F,
    HEX_BASE = 16,
    FRAME_BYTES = FLS_CHANNELS * sizeof(f32),
    ARG_SCRIPT = 1,
    ARG_AUDIO = 2,
    ARG_LOG = 3,
    ARG_COUNT = 4,
    USAGE_ERROR = 2
};

typedef void (*CommandFn)(const c8 *arg);

typedef struct Command {
    const c8 *name;
    CommandFn fn;
} Command;

static FILE *g_out;
static FILE *g_log;
static i32 g_instance = -1;
static i32 g_control_word = DEFAULT_CONTROL_WORD;
static i32 g_sample_rate = DEFAULT_SAMPLE_RATE;
static i32 g_note[SLOT_COUNT];
static u32 g_levels[SLOT_COUNT];
static f32 *g_dest;
static f32 *g_reverb;
static f32 *g_chorus;
static i32 g_capacity;
static f64 g_seconds;
static i64 g_frames_total;
static LARGE_INTEGER g_frequency;

static bool slot_valid(i32 slot)
{
    return slot >= 0 && slot < SLOT_COUNT;
}

static void note_end(u32 tag)
{
    u32 slot = tag - 1;

    fprintf(g_log, "end %u\n", slot);
    if (slot < SLOT_COUNT) {
        g_note[slot] = 0;
    }
}

static void cmd_sample_rate(const c8 *arg)
{
    g_sample_rate = atoi(arg);
}

static void cmd_control_word(const c8 *arg)
{
    g_control_word = (i32)strtol(arg, NULL, HEX_BASE);
}

static void cmd_init(const c8 *arg)
{
    (void)arg;
    fls_set_fpu_control((u16)g_control_word);
    fprintf(g_log, "init=%d\n", fls_init((u32)g_sample_rate, &g_instance));
}

static void cmd_effects(const c8 *arg)
{
    i32 reverb = 0;
    i32 chorus = 0;

    sscanf(arg, "%d %d", &reverb, &chorus);
    fls_set_use_reverb(g_instance, reverb != 0);
    fls_set_use_chorus(g_instance, chorus != 0);
}

static void cmd_attenuation(const c8 *arg)
{
    fprintf(g_log, "atten=%d\n", fls_set_attenuation(g_instance, atoi(arg)));
}

static void cmd_polyphony(const c8 *arg)
{
    fprintf(g_log, "poly=%d\n", fls_set_polyphony(g_instance, atoi(arg)));
}

static void cmd_load(const c8 *arg)
{
    fls_clear_soundfont(g_instance);
    fprintf(g_log, "load=%d\n", fls_load_soundfont(g_instance, arg));
}

static void cmd_names(const c8 *arg)
{
    c8 name[NAME_SIZE];
    i32 patch = 0;
    i32 bank = 0;

    sscanf(arg, "%d %d", &patch, &bank);
    fls_bank_name(g_instance, name, sizeof(name));
    fprintf(g_log, "bank=%s\n", name);
    fls_patch_name(g_instance, patch, bank, name, sizeof(name));
    fprintf(g_log, "patch %d %d=%s\n", patch, bank, name);
}

static void cmd_note_on(const c8 *arg)
{
    FlsLevels levels;
    i32 slot = 0;
    i32 channel = 0;
    i32 bank = 0;
    i32 patch = 0;
    i32 key = 0;
    i32 velocity = 0;
    i32 quality = 0;

    sscanf(arg, "%d %d %d %d %d %d %d %f %d %f %f %d", &slot, &channel, &bank, &patch, &key, &velocity, &levels.pan,
           &levels.vol, &levels.pitch, &levels.fcut, &levels.fres, &quality);
    if (!slot_valid(slot)) {
        return;
    }
    if (!g_levels[slot]) {
        g_levels[slot] = fls_levels_new(&levels);
    } else {
        fls_levels_set(g_levels[slot], &levels);
    }
    g_note[slot] = fls_note_on(g_instance, channel, bank, patch, key, velocity, g_levels[slot], (u32)slot + 1, quality);
    fprintf(g_log, "on %d=%d\n", slot, g_note[slot] > 0 ? 1 : g_note[slot]);
}

static void cmd_levels(const c8 *arg)
{
    FlsLevels levels;
    i32 slot = 0;

    sscanf(arg, "%d %d %f %d %f %f", &slot, &levels.pan, &levels.vol, &levels.pitch, &levels.fcut, &levels.fres);
    if (slot_valid(slot) && g_levels[slot]) {
        fls_levels_set(g_levels[slot], &levels);
    }
}

static void cmd_note_off(const c8 *arg)
{
    i32 slot = 0;
    i32 kill = 0;

    sscanf(arg, "%d %d", &slot, &kill);
    if (slot_valid(slot) && g_note[slot] > 0) {
        fls_note_off(g_instance, g_note[slot], kill != 0);
    }
}

static void cmd_message(const c8 *arg)
{
    fls_short_message(g_instance, (u32)strtoul(arg, NULL, HEX_BASE));
}

static void cmd_nrpn(const c8 *arg)
{
    i32 channel = 0;
    i32 number = 0;
    i32 value = 0;

    sscanf(arg, "%d %d %d", &channel, &number, &value);
    fls_set_nrpn(g_instance, channel, number, value);
}

static void reserve_buffers(i32 frames)
{
    if (frames <= g_capacity) {
        return;
    }
    g_capacity = frames;
    g_dest = realloc(g_dest, (usize)g_capacity * FRAME_BYTES);
    g_reverb = realloc(g_reverb, (usize)g_capacity * FRAME_BYTES);
    g_chorus = realloc(g_chorus, (usize)g_capacity * FRAME_BYTES);
}

static void render_block(i32 frames, bool sends, f32 reverb_level, f32 chorus_level)
{
    LARGE_INTEGER started;
    LARGE_INTEGER finished;
    usize bytes = (usize)frames * FRAME_BYTES;
    i32 result;

    memset(g_dest, 0, bytes);
    memset(g_reverb, 0, bytes);
    memset(g_chorus, 0, bytes);
    QueryPerformanceCounter(&started);
    result = fls_render(g_instance, g_dest, frames, sends ? g_reverb : NULL, sends ? g_chorus : NULL, reverb_level,
                        chorus_level);
    QueryPerformanceCounter(&finished);
    if (result != FLS_OK) {
        fprintf(g_log, "render failed\n");
    }
    g_seconds += (f64)(finished.QuadPart - started.QuadPart) / (f64)g_frequency.QuadPart;
    g_frames_total += frames;
    fwrite(g_dest, FRAME_BYTES, (usize)frames, g_out);
    if (sends) {
        fwrite(g_reverb, FRAME_BYTES, (usize)frames, g_out);
        fwrite(g_chorus, FRAME_BYTES, (usize)frames, g_out);
    }
}

static void cmd_render(const c8 *arg)
{
    i32 blocks = 0;
    i32 frames = 0;
    i32 sends = 0;
    f32 reverb_level = 0;
    f32 chorus_level = 0;
    i32 block;

    sscanf(arg, "%d %d %d %f %f", &blocks, &frames, &sends, &reverb_level, &chorus_level);
    reserve_buffers(frames);
    for (block = 0; block < blocks; ++block) {
        render_block(frames, sends != 0, reverb_level, chorus_level);
    }
}

static void dump_memory(const c8 *path, u32 address, u32 size)
{
    FILE *file = fopen(path, "wb");

    if (!file) {
        return;
    }
    fwrite(fls_debug_ptr(address), 1, size, file);
    fclose(file);
}

static void cmd_dump_voices(const c8 *arg)
{
    u32 voices = *(u32 *)fls_debug_ptr(ENGINE_VOICES_SLOT);

    dump_memory(arg, voices, ENGINE_VOICE_COUNT * ENGINE_VOICE_SIZE);
}

static void cmd_dump_data(const c8 *arg)
{
    dump_memory(arg, ENGINE_DATA_BASE, ENGINE_DATA_SIZE);
}

static void cmd_free(const c8 *arg)
{
    (void)arg;
    fls_free(g_instance);
}

static const Command g_commands[] = {
    { "sr", cmd_sample_rate },
    { "cw", cmd_control_word },
    { "init", cmd_init },
    { "fx", cmd_effects },
    { "atten", cmd_attenuation },
    { "poly", cmd_polyphony },
    { "load", cmd_load },
    { "names", cmd_names },
    { "on", cmd_note_on },
    { "lv", cmd_levels },
    { "off", cmd_note_off },
    { "msg", cmd_message },
    { "nrpn", cmd_nrpn },
    { "render", cmd_render },
    { "dumpv", cmd_dump_voices },
    { "dump", cmd_dump_data },
    { "free", cmd_free },
};

static void strip_line_end(c8 *line)
{
    usize length = strlen(line);

    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        --length;
        line[length] = 0;
    }
}

static void run_line(c8 *line)
{
    c8 name[COMMAND_SIZE];
    const c8 *arg;
    usize i;

    strip_line_end(line);
    if (sscanf(line, "%31s", name) != 1) {
        return;
    }
    arg = line + strlen(name);
    while (*arg == ' ') {
        ++arg;
    }
    for (i = 0; i < ARRAY_COUNT(g_commands); ++i) {
        if (!strcmp(name, g_commands[i].name)) {
            g_commands[i].fn(arg);
            return;
        }
    }
}

static void print_summary(void)
{
    f64 audio_seconds = (f64)g_frames_total / g_sample_rate;
    f64 speed = g_seconds > 0 ? audio_seconds / g_seconds : 0.0;

    fprintf(stderr, "rendered %.2f s of audio in %.3f s (%.1fx realtime)\n", audio_seconds, g_seconds, speed);
}

int main(int argc, char **argv)
{
    c8 line[LINE_SIZE];
    FILE *script;

    if (argc < ARG_COUNT) {
        fprintf(stderr, "usage: script_test <script> <audio out> <log out>\n");
        return USAGE_ERROR;
    }
    script = fopen(argv[ARG_SCRIPT], "r");
    g_out = fopen(argv[ARG_AUDIO], "wb");
    g_log = fopen(argv[ARG_LOG], "wb");
    if (!script || !g_out || !g_log) {
        return USAGE_ERROR;
    }
    fls_set_note_end_callback(note_end);
    QueryPerformanceFrequency(&g_frequency);
    while (fgets(line, sizeof(line), script)) {
        run_line(line);
    }
    fclose(script);
    fclose(g_out);
    fclose(g_log);
    print_summary();
    return 0;
}
