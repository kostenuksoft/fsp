#include "flspro.h"

#include <string.h>

#include "vm.h"
#include "vm_rt.h"

enum {
    TMP_SIZE = 4096,
    NAME_PAD = 16,
    SCRATCH_MAIN = 0,
    SCRATCH_REVERB = 1,
    SCRATCH_CHORUS = 2,
    SCRATCH_COUNT = 3,
    SCRATCH_PAD = 64,
    FRAME_BYTES = FLS_CHANNELS * sizeof(f32),
    NOTE_END_ARG_COUNT = 3,
    NOTE_END_TAG_ARG = 2,
    RENDER_ALL_VOICES = -1,
    RENDER_INTERLEAVED = 1
};

static FlsNoteEndFn g_note_end;
static u32 g_scratch[SCRATCH_COUNT];
static u32 g_scratch_frames;
static u32 g_tmp;

static bool enter(void)
{
    vm_lock();
    if (!vm_enter()) {
        vm_unlock();
        return false;
    }
    if (!g_tmp) {
        g_tmp = vm_alloc(TMP_SIZE);
        vm_mark_init(g_tmp, TMP_SIZE);
    }
    return true;
}

static void leave(void)
{
    vm_leave();
    vm_unlock();
}

static u32 float_bits(f32 value)
{
    u32 bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static i32 copy_name(u32 address, c8 *out, i32 cap)
{
    const c8 *name;
    i32 length;

    if (cap > 0) {
        out[0] = 0;
    }
    if (!address || address == VM_CALL_FAILED || cap <= 0) {
        return 0;
    }
    name = (const c8 *)vm_ptr(address);
    length = (i32)strnlen(name, (usize)cap - 1);
    memcpy(out, name, (usize)length);
    out[length] = 0;
    return length;
}

void vm_note_end(CPU *c)
{
    u32 tag = *(vm_u32 *)(vm_mem + c->esp + VM_GUEST_WORD * (1 + NOTE_END_TAG_ARG));

    c->esp += VM_GUEST_WORD * (1 + NOTE_END_ARG_COUNT);
    if (g_note_end) {
        g_note_end(tag);
    }
}

i32 fls_init(u32 sample_rate, i32 *instance)
{
    u32 result;

    if (!enter()) {
        return FLS_ERROR;
    }
    *(vm_u32 *)vm_ptr(g_tmp) = 0;
    result = VM_CALL(FLS_InitSynthInstance, g_tmp, sample_rate);
    *instance = (i32)*(vm_u32 *)vm_ptr(g_tmp);
    if (vm_trapped()) {
        result = VM_CALL_FAILED;
    }
    leave();
    return (i32)result;
}

void fls_free(i32 instance)
{
    if (!enter()) {
        return;
    }
    VM_CALL(FLS_FreeSynthInstance, (u32)instance);
    leave();
}

void fls_clear_soundfont(i32 instance)
{
    if (!enter()) {
        return;
    }
    VM_CALL(FLS_LUClearSoundFont, (u32)instance, 0);
    leave();
}

i32 fls_load_soundfont(i32 instance, const c8 *path)
{
    usize size = strlen(path) + 1;
    u32 result;
    u32 name;

    if (!enter()) {
        return FLS_ERROR;
    }
    name = vm_alloc((u32)size + NAME_PAD);
    if (!name) {
        leave();
        return FLS_ERROR;
    }
    memcpy(vm_ptr(name), path, size);
    vm_mark_init(name, (u32)size);
    result = VM_CALL(FLS_LULoadSoundFont, (u32)instance, 0, 0, name, 0);
    if (vm_trapped()) {
        result = VM_CALL_FAILED;
    }
    vm_free(name);
    leave();
    return (i32)result;
}

i32 fls_bank_name(i32 instance, c8 *out, i32 cap)
{
    i32 length;

    if (!enter()) {
        return 0;
    }
    length = copy_name(VM_CALL(FLS_GetBankName, (u32)instance, 0), out, cap);
    leave();
    return length;
}

i32 fls_patch_name(i32 instance, i32 patch, i32 bank, c8 *out, i32 cap)
{
    i32 length;

    if (!enter()) {
        return 0;
    }
    length = copy_name(VM_CALL(FLS_GetInstrPatchName, (u32)instance, 0, (u32)patch, (u32)bank), out, cap);
    leave();
    return length;
}

i32 fls_sub_banks_filled(i32 instance, u8 filled[FLS_SUB_BANKS])
{
    u32 result;

    memset(filled, 0, FLS_SUB_BANKS);
    if (!enter()) {
        return 0;
    }
    memset(vm_ptr(g_tmp), 0, FLS_SUB_BANKS);
    result = VM_CALL(FLS_GetSubBanksFilled, (u32)instance, 0, g_tmp);
    memcpy(filled, vm_ptr(g_tmp), FLS_SUB_BANKS);
    leave();
    return (i32)result;
}

void fls_set_use_reverb(i32 instance, bool on)
{
    if (!enter()) {
        return;
    }
    VM_CALL(FLS_SetUseReverb, (u32)instance, on ? 1 : 0);
    leave();
}

void fls_set_use_chorus(i32 instance, bool on)
{
    if (!enter()) {
        return;
    }
    VM_CALL(FLS_SetUseChorus, (u32)instance, on ? 1 : 0);
    leave();
}

i32 fls_set_attenuation(i32 instance, i32 value)
{
    u32 result;

    if (!enter()) {
        return 0;
    }
    result = VM_CALL(FLS_SetAttenuation, (u32)instance, (u32)value);
    leave();
    return (i32)result;
}

i32 fls_set_polyphony(i32 instance, i32 voices)
{
    u32 result;

    if (!enter()) {
        return 0;
    }
    result = VM_CALL(FLS_SetPolyphony, (u32)instance, (u32)voices);
    leave();
    return (i32)result;
}

void fls_short_message(i32 instance, u32 message)
{
    if (!enter()) {
        return;
    }
    VM_CALL(FLS_QueueShortMessage, (u32)instance, message, 0);
    leave();
}

void fls_set_nrpn(i32 instance, i32 channel, i32 nrpn, i32 value)
{
    if (!enter()) {
        return;
    }
    VM_CALL(FLS_SetNRPN, (u32)instance, (u32)channel, (u32)nrpn, (u32)value);
    leave();
}

i32 fls_get_nrpn(i32 instance, i32 channel, i32 nrpn)
{
    u32 result;

    if (!enter()) {
        return FLS_NRPN_UNSET;
    }
    result = VM_CALL(FLS_GetNRPN, (u32)instance, (u32)channel, (u32)nrpn);
    leave();
    return (i32)result;
}

u32 fls_levels_new(const FlsLevels *init)
{
    u32 levels;

    if (!enter()) {
        return 0;
    }
    levels = vm_alloc(sizeof(FlsLevels));
    if (levels) {
        vm_mark_init(levels, sizeof(FlsLevels));
        if (init) {
            memcpy(vm_ptr(levels), init, sizeof(FlsLevels));
        } else {
            memset(vm_ptr(levels), 0, sizeof(FlsLevels));
        }
    }
    leave();
    return levels;
}

void fls_levels_set(u32 levels, const FlsLevels *value)
{
    if (!levels) {
        return;
    }
    vm_lock();
    memcpy(vm_ptr(levels), value, sizeof(FlsLevels));
    vm_unlock();
}

void fls_levels_delete(u32 levels)
{
    if (!levels || !enter()) {
        return;
    }
    vm_free(levels);
    leave();
}

void fls_set_note_end_callback(FlsNoteEndFn fn)
{
    g_note_end = fn;
}

i32 fls_note_on(i32 instance, i32 channel, i32 bank, i32 patch, i32 key, i32 velocity,
                u32 levels, u32 tag, i32 quality)
{
    u32 result;

    if (!enter()) {
        return FLS_ERROR;
    }
    result = VM_CALL(FLS_AllocateNote, (u32)instance, (u32)channel, 0, (u32)bank, (u32)patch, (u32)key,
                     (u32)velocity, levels, 0, VM_NOTE_END_THUNK, tag, 0, (u32)quality);
    if (vm_trapped()) {
        result = VM_CALL_FAILED;
    }
    leave();
    return (i32)result;
}

void fls_note_off(i32 instance, i32 note, bool kill)
{
    if (!enter()) {
        return;
    }
    VM_CALL(FLS_FreeNote, (u32)instance, (u32)note, 0, kill ? 1 : 0);
    leave();
}

static bool scratch_reserve(u32 frames)
{
    i32 i;

    if (frames <= g_scratch_frames) {
        return true;
    }
    g_scratch_frames = frames;
    for (i = 0; i < SCRATCH_COUNT; ++i) {
        vm_free(g_scratch[i]);
        g_scratch[i] = vm_alloc(frames * FRAME_BYTES + SCRATCH_PAD);
        if (!g_scratch[i]) {
            g_scratch_frames = 0;
        }
    }
    return g_scratch_frames != 0;
}

static void mix_add(f32 *dest, const f32 *source, i32 samples)
{
    i32 i;

    for (i = 0; i < samples; ++i) {
        dest[i] += source[i];
    }
}

i32 fls_render(i32 instance, f32 *dest, i32 frames, f32 *reverb, f32 *chorus,
               f32 reverb_level, f32 chorus_level)
{
    u32 bytes;
    i32 samples;
    i32 result;
    i32 i;

    if (frames <= 0) {
        return FLS_OK;
    }
    if (!enter()) {
        return FLS_ERROR;
    }
    if (!scratch_reserve((u32)frames)) {
        leave();
        return FLS_ERROR;
    }

    bytes = (u32)frames * FRAME_BYTES;
    samples = frames * FLS_CHANNELS;
    for (i = 0; i < SCRATCH_COUNT; ++i) {
        memset(vm_ptr(g_scratch[i]), 0, bytes);
        vm_mark_init(g_scratch[i], bytes);
    }

    VM_CALL(FLS_RenderVoices, (u32)instance, (u32)RENDER_ALL_VOICES, g_scratch[SCRATCH_MAIN], (u32)frames,
            RENDER_INTERLEAVED, reverb ? g_scratch[SCRATCH_REVERB] : 0, chorus ? g_scratch[SCRATCH_CHORUS] : 0,
            float_bits(reverb_level), float_bits(chorus_level));

    result = vm_trapped() ? FLS_ERROR : FLS_OK;
    if (result == FLS_OK) {
        mix_add(dest, (const f32 *)vm_ptr(g_scratch[SCRATCH_MAIN]), samples);
        if (reverb) {
            mix_add(reverb, (const f32 *)vm_ptr(g_scratch[SCRATCH_REVERB]), samples);
        }
        if (chorus) {
            mix_add(chorus, (const f32 *)vm_ptr(g_scratch[SCRATCH_CHORUS]), samples);
        }
    }
    leave();
    return result;
}

void fls_set_fpu_control(u16 control_word)
{
    vm_lock();
    if (vm_enter()) {
        vm_leave();
        vm_set_fpu_cw(control_word);
    }
    vm_unlock();
}

const c8 *fls_last_message(void)
{
    return vm_last_message();
}

void *fls_debug_ptr(u32 address)
{
    return vm_ptr(address);
}

void fls_shutdown(void)
{
    vm_shutdown();
}
