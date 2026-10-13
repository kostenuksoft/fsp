#pragma once

#include "../common/fsp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FLS_OK = 0,
    FLS_ERROR = -1,
    FLS_CHANNELS = 2,
    FLS_SUB_BANKS = 128,
    FLS_NRPN_UNSET = 0x8000
};

typedef struct FlsLevels {
    i32 pan;
    f32 vol;
    i32 pitch;
    f32 fcut;
    f32 fres;
} FlsLevels;

typedef void (*FlsNoteEndFn)(u32 tag);

i32 fls_init(u32 sample_rate, i32 *instance);
void fls_free(i32 instance);
i32 fls_load_soundfont(i32 instance, const c8 *path);
void fls_clear_soundfont(i32 instance);
i32 fls_bank_name(i32 instance, c8 *out, i32 cap);
i32 fls_patch_name(i32 instance, i32 patch, i32 bank, c8 *out, i32 cap);
i32 fls_sub_banks_filled(i32 instance, u8 filled[FLS_SUB_BANKS]);
void fls_set_use_reverb(i32 instance, bool on);
void fls_set_use_chorus(i32 instance, bool on);
i32 fls_set_attenuation(i32 instance, i32 value);
i32 fls_set_polyphony(i32 instance, i32 voices);
void fls_short_message(i32 instance, u32 message);
void fls_set_nrpn(i32 instance, i32 channel, i32 nrpn, i32 value);
i32 fls_get_nrpn(i32 instance, i32 channel, i32 nrpn);

u32 fls_levels_new(const FlsLevels *init);
void fls_levels_set(u32 levels, const FlsLevels *value);
void fls_levels_delete(u32 levels);

i32 fls_note_on(i32 instance, i32 channel, i32 bank, i32 patch, i32 key, i32 velocity,
                u32 levels, u32 tag, i32 quality);
void fls_note_off(i32 instance, i32 note, bool kill);
void fls_set_note_end_callback(FlsNoteEndFn fn);

i32 fls_render(i32 instance, f32 *dest, i32 frames, f32 *reverb, f32 *chorus,
               f32 reverb_level, f32 chorus_level);

void fls_set_fpu_control(u16 control_word);
const c8 *fls_last_message(void);
void fls_shutdown(void);
void *fls_debug_ptr(u32 address);

#ifdef __cplusplus
}
#endif
