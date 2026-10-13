#pragma once

#include "../common/fsp_types.h"
#include "gen/vm_cells.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VM_NOTE_END_THUNK 0xFD000000u
#define VM_CALL_FAILED    0xFFFFFFFFu
#define VM_GUEST_WORD     4u

#define VM_ARG_COUNT(...) ((i32)(sizeof((u32[]){ __VA_ARGS__ }) / sizeof(u32)))
#define VM_CALL(fn, ...)  vm_invoke(fn, VM_ARG_COUNT(__VA_ARGS__), (const u32[]){ __VA_ARGS__ })

void vm_lock(void);
void vm_unlock(void);
bool vm_enter(void);
void vm_leave(void);
void vm_shutdown(void);
u32 vm_invoke(void (*fn)(CPU *), i32 arg_count, const u32 *args);
bool vm_trapped(void);

u32 vm_alloc(u32 size);
void vm_free(u32 address);
u32 vm_msize(u32 address);
void *vm_ptr(u32 address);
void vm_mark_init(u32 address, u32 size);

void vm_set_fpu_cw(u16 control_word);
void vm_log(const c8 *format, ...);
const c8 *vm_last_message(void);

#ifdef __cplusplus
}
#endif
