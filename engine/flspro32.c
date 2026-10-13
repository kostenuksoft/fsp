#include "flspro.h"

#include <stdlib.h>
#include <string.h>

#include <windows.h>

#define ENGINE_DLL "FLSPro.dll"

enum {
    RENDER_ALL_VOICES = -1,
    RENDER_INTERLEAVED = 1
};

typedef i32 (__stdcall *InitFn)(i32 *, u32);
typedef i32 (__stdcall *FreeFn)(i32);
typedef i32 (__stdcall *LoadFn)(i32, i32, i32, const c8 *, i32);
typedef i32 (__stdcall *SettingFn)(i32, i32);
typedef const c8 *(__stdcall *BankNameFn)(i32, i32);
typedef const c8 *(__stdcall *PatchNameFn)(i32, i32, i32, i32);
typedef i32 (__stdcall *SubBanksFn)(i32, i32, u8 *);
typedef i32 (__stdcall *NoteOnFn)(i32, i32, i32, i32, i32, i32, i32, void *, i32, void *, i32, i32, i32);
typedef i32 (__stdcall *NoteOffFn)(i32, i32, i32, i32);
typedef i32 (__stdcall *RenderFn)(i32, i32, f32 *, i32, i32, f32 *, f32 *, f32, f32);
typedef i32 (__stdcall *MessageFn)(i32, i32, i32);
typedef i32 (__stdcall *GetNrpnFn)(i32, i32, i32);
typedef i32 (__stdcall *SetNrpnFn)(i32, i32, i32, i32);

typedef struct Binding {
    const c8 *name;
    FARPROC *slot;
    bool required;
} Binding;

static HMODULE g_dll;
static bool g_tried;
static FlsNoteEndFn g_note_end;

static InitFn g_init;
static FreeFn g_free;
static LoadFn g_load;
static SettingFn g_clear;
static SettingFn g_use_reverb;
static SettingFn g_use_chorus;
static SettingFn g_attenuation;
static SettingFn g_polyphony;
static BankNameFn g_bank_name;
static PatchNameFn g_patch_name;
static SubBanksFn g_sub_banks;
static NoteOnFn g_note_on;
static NoteOffFn g_note_off;
static RenderFn g_render;
static MessageFn g_short_message;
static GetNrpnFn g_get_nrpn;
static SetNrpnFn g_set_nrpn;

static const Binding g_bindings[] = {
    { "InitSynthInstance", (FARPROC *)&g_init, true },
    { "FreeSynthInstance", (FARPROC *)&g_free, true },
    { "LULoadSoundFont", (FARPROC *)&g_load, true },
    { "LUClearSoundFont", (FARPROC *)&g_clear, true },
    { "GetInstrPatchName", (FARPROC *)&g_patch_name, true },
    { "GetSubBanksFilled", (FARPROC *)&g_sub_banks, false },
    { "GetBankName", (FARPROC *)&g_bank_name, true },
    { "SetUseReverb", (FARPROC *)&g_use_reverb, true },
    { "SetUseChorus", (FARPROC *)&g_use_chorus, true },
    { "SetAttenuation", (FARPROC *)&g_attenuation, true },
    { "SetPolyphony", (FARPROC *)&g_polyphony, true },
    { "AllocateNote", (FARPROC *)&g_note_on, true },
    { "RenderVoices", (FARPROC *)&g_render, true },
    { "FreeNote", (FARPROC *)&g_note_off, true },
    { "QueueShortMessage", (FARPROC *)&g_short_message, true },
    { "GetNRPN", (FARPROC *)&g_get_nrpn, false },
    { "SetNRPN", (FARPROC *)&g_set_nrpn, true },
};

static bool engine_path(c8 *path, usize cap)
{
    HMODULE self = NULL;
    c8 *slash;
    DWORD length;

    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)&engine_path, &self)) {
        return false;
    }
    length = GetModuleFileNameA(self, path, (DWORD)cap);
    if (!length || length >= cap) {
        return false;
    }
    slash = strrchr(path, '\\');
    if (slash) {
        slash[1] = 0;
    }
    if (strlen(path) + sizeof(ENGINE_DLL) > cap) {
        return false;
    }
    strcat(path, ENGINE_DLL);
    return true;
}

static bool bind_all(HMODULE dll)
{
    usize i;

    for (i = 0; i < ARRAY_COUNT(g_bindings); ++i) {
        *g_bindings[i].slot = GetProcAddress(dll, g_bindings[i].name);
        if (!*g_bindings[i].slot && g_bindings[i].required) {
            return false;
        }
    }
    return true;
}

static bool ready(void)
{
    c8 path[MAX_PATH];

    if (g_dll) {
        return true;
    }
    if (g_tried) {
        return false;
    }
    g_tried = true;
    if (!engine_path(path, sizeof(path))) {
        return false;
    }
    g_dll = LoadLibraryA(path);
    if (!g_dll) {
        return false;
    }
    if (!bind_all(g_dll)) {
        FreeLibrary(g_dll);
        g_dll = NULL;
        return false;
    }
    return true;
}

static void __stdcall note_end(i32 unused_a, i32 unused_b, i32 tag)
{
    (void)unused_a;
    (void)unused_b;
    if (g_note_end) {
        g_note_end((u32)tag);
    }
}

static i32 copy_name(const c8 *name, c8 *out, i32 cap)
{
    i32 length = 0;

    if (cap > 0) {
        out[0] = 0;
    }
    if (!name || cap <= 0) {
        return 0;
    }
    while (length < cap - 1 && name[length]) {
        out[length] = name[length];
        ++length;
    }
    out[length] = 0;
    return length;
}

i32 fls_init(u32 sample_rate, i32 *instance)
{
    if (!ready()) {
        return FLS_ERROR;
    }
    return g_init(instance, sample_rate);
}

void fls_free(i32 instance)
{
    if (ready()) {
        g_free(instance);
    }
}

void fls_clear_soundfont(i32 instance)
{
    if (ready()) {
        g_clear(instance, 0);
    }
}

i32 fls_load_soundfont(i32 instance, const c8 *path)
{
    if (!ready()) {
        return FLS_ERROR;
    }
    return g_load(instance, 0, 0, path, 0);
}

i32 fls_bank_name(i32 instance, c8 *out, i32 cap)
{
    return copy_name(ready() ? g_bank_name(instance, 0) : NULL, out, cap);
}

i32 fls_patch_name(i32 instance, i32 patch, i32 bank, c8 *out, i32 cap)
{
    return copy_name(ready() ? g_patch_name(instance, 0, patch, bank) : NULL, out, cap);
}

i32 fls_sub_banks_filled(i32 instance, u8 filled[FLS_SUB_BANKS])
{
    memset(filled, 0, FLS_SUB_BANKS);
    if (!ready() || !g_sub_banks) {
        return 0;
    }
    return g_sub_banks(instance, 0, filled);
}

void fls_set_use_reverb(i32 instance, bool on)
{
    if (ready()) {
        g_use_reverb(instance, on ? 1 : 0);
    }
}

void fls_set_use_chorus(i32 instance, bool on)
{
    if (ready()) {
        g_use_chorus(instance, on ? 1 : 0);
    }
}

i32 fls_set_attenuation(i32 instance, i32 value)
{
    if (!ready()) {
        return 0;
    }
    return g_attenuation(instance, value);
}

i32 fls_set_polyphony(i32 instance, i32 voices)
{
    if (!ready()) {
        return 0;
    }
    return g_polyphony(instance, voices);
}

void fls_short_message(i32 instance, u32 message)
{
    if (ready()) {
        g_short_message(instance, (i32)message, 0);
    }
}

void fls_set_nrpn(i32 instance, i32 channel, i32 nrpn, i32 value)
{
    if (ready()) {
        g_set_nrpn(instance, channel, nrpn, value);
    }
}

i32 fls_get_nrpn(i32 instance, i32 channel, i32 nrpn)
{
    if (!ready() || !g_get_nrpn) {
        return FLS_NRPN_UNSET;
    }
    return g_get_nrpn(instance, channel, nrpn);
}

u32 fls_levels_new(const FlsLevels *init)
{
    FlsLevels *levels = (FlsLevels *)calloc(1, sizeof(FlsLevels));

    if (levels && init) {
        *levels = *init;
    }
    return (u32)(uptr)levels;
}

void fls_levels_set(u32 levels, const FlsLevels *value)
{
    if (levels) {
        *(FlsLevels *)(uptr)levels = *value;
    }
}

void fls_levels_delete(u32 levels)
{
    free((void *)(uptr)levels);
}

void fls_set_note_end_callback(FlsNoteEndFn fn)
{
    g_note_end = fn;
}

i32 fls_note_on(i32 instance, i32 channel, i32 bank, i32 patch, i32 key, i32 velocity,
                u32 levels, u32 tag, i32 quality)
{
    if (!ready()) {
        return FLS_ERROR;
    }
    return g_note_on(instance, channel, 0, bank, patch, key, velocity, (void *)(uptr)levels, 0, (void *)note_end,
                     (i32)tag, 0, quality);
}

void fls_note_off(i32 instance, i32 note, bool kill)
{
    if (ready()) {
        g_note_off(instance, note, 0, kill ? 1 : 0);
    }
}

i32 fls_render(i32 instance, f32 *dest, i32 frames, f32 *reverb, f32 *chorus,
               f32 reverb_level, f32 chorus_level)
{
    if (!ready() || frames <= 0) {
        return FLS_ERROR;
    }
    g_render(instance, RENDER_ALL_VOICES, dest, frames, RENDER_INTERLEAVED, reverb, chorus, reverb_level, chorus_level);
    return FLS_OK;
}

void fls_set_fpu_control(u16 control_word)
{
    (void)control_word;
}

const c8 *fls_last_message(void)
{
    return "";
}

void *fls_debug_ptr(u32 address)
{
    return (void *)(uptr)address;
}

void fls_shutdown(void)
{
}
