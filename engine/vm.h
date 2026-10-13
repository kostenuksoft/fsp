#pragma once

#include <string.h>

#include "../common/fsp_types.h"
#include "gen/vm_cells.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VM_X87_EXCEPTION_MASKS 0x003Fu
#define VM_X87_ROUNDING_MASK   0x0C00u
#define VM_SSE_ROUNDING_MASK   0x6000u
#define VM_ROUNDING_SHIFT      3
#define VM_F80_BYTES           10

#define VM_I64_LIMIT 9223372036854775808.0L
#define VM_I32_LIMIT 2147483648.0L
#define VM_I16_LIMIT 32768.0L
#define VM_I64_INDEFINITE UINT64_C(0x8000000000000000)
#define VM_I32_INDEFINITE 0x80000000u
#define VM_I16_INDEFINITE 0x8000u

extern u8 *vm_mem;

typedef struct VmImport {
    u32 slot;
    const c8 *name;
} VmImport;

typedef struct VmSection {
    u32 va;
    u32 vsize;
    const u8 *data;
    u32 size;
} VmSection;

extern const VmImport vm_imports[];
extern const VmSection vm_sections[];

void vm_call_ind(CPU *c, u32 target);
void vm_import(CPU *c, u32 target);
void vm_trap(CPU *c, u32 where) __attribute__((noreturn));

void vm_note_end(CPU *c);
void vm_hook_malloc(CPU *c);
void vm_hook_free(CPU *c);
void vm_hook_sprintf(CPU *c);
void vm_hook_fclose(CPU *c);

typedef u16 __attribute__((may_alias, aligned(1))) vm_u16;
typedef u32 __attribute__((may_alias, aligned(1))) vm_u32;
typedef u64 __attribute__((may_alias, aligned(1))) vm_u64;

#define VM_INLINE static inline __attribute__((always_inline))
#define VM_BARRIER(x) __asm__ volatile("" : "+m"(x))

#ifdef VM_SHADOW
extern u8 *vm_shadow;
void vm_shadow_read(u32 address, u32 size, const c8 *function);

VM_INLINE u32 vm_shadow_checked(u32 address, u32 size, const c8 *function)
{
    vm_shadow_read(address, size, function);
    return address;
}

VM_INLINE u32 vm_shadow_marked(u32 address, u32 size)
{
    memset(vm_shadow + address, 1, size);
    return address;
}

#define VM_RA(a, n) vm_shadow_checked((u32)(a), n, __func__)
#define VM_WA(a, n) vm_shadow_marked((u32)(a), n)
#else
#define VM_RA(a, n) ((u32)(a))
#define VM_WA(a, n) ((u32)(a))
#endif

#define RD8(a)  (M[VM_RA(a, sizeof(u8))])
#define RD16(a) (*(const vm_u16 *)(M + VM_RA(a, sizeof(u16))))
#define RD32(a) (*(const vm_u32 *)(M + VM_RA(a, sizeof(u32))))
#define RD64(a) (*(const vm_u64 *)(M + VM_RA(a, sizeof(u64))))
#define WR8(a, v)  (M[VM_WA(a, sizeof(u8))] = (u8)(v))
#define WR16(a, v) (*(vm_u16 *)(M + VM_WA(a, sizeof(u16))) = (u16)(v))
#define WR32(a, v) (*(vm_u32 *)(M + VM_WA(a, sizeof(u32))) = (u32)(v))
#define WR64(a, v) (*(vm_u64 *)(M + VM_WA(a, sizeof(u64))) = (u64)(v))

#define VM_SHIFTS(N, U, S) \
    VM_INLINE U vm_shl##N(U a, u32 n) \
    { \
        return n < N ? (U)(a << n) : (U)0; \
    } \
    VM_INLINE U vm_shr##N(U a, u32 n) \
    { \
        return n < N ? (U)(a >> n) : (U)0; \
    } \
    VM_INLINE U vm_sar##N(U a, u32 n) \
    { \
        return (U)((S)a >> (n < N ? n : N - 1)); \
    }

VM_SHIFTS(8, u8, i8)
VM_SHIFTS(16, u16, i16)
VM_SHIFTS(32, u32, i32)
VM_SHIFTS(64, u64, i64)

#define VM_OVF(N, S) \
    VM_INLINE u8 vm_add_ovf##N(S a, S b) \
    { \
        S r; \
        return (u8)__builtin_add_overflow(a, b, &r); \
    } \
    VM_INLINE u8 vm_sub_ovf##N(S a, S b) \
    { \
        S r; \
        return (u8)__builtin_sub_overflow(a, b, &r); \
    }

VM_OVF(8, i8)
VM_OVF(16, i16)
VM_OVF(32, i32)
VM_OVF(64, i64)

#define VM_DIVS(N, U, S) \
    VM_INLINE U vm_div##N(U a, U b) \
    { \
        return b ? (U)(a / b) : (U)0; \
    } \
    VM_INLINE U vm_rem##N(U a, U b) \
    { \
        return b ? (U)(a % b) : (U)0; \
    } \
    VM_INLINE U vm_sdiv##N(U a, U b) \
    { \
        if (!b) { \
            return 0; \
        } \
        if ((S)b == -1) { \
            return (U)(0 - a); \
        } \
        return (U)((S)a / (S)b); \
    } \
    VM_INLINE U vm_srem##N(U a, U b) \
    { \
        if (!b || (S)b == -1) { \
            return 0; \
        } \
        return (U)((S)a % (S)b); \
    }

VM_DIVS(8, u8, i8)
VM_DIVS(16, u16, i16)
VM_DIVS(32, u32, i32)
VM_DIVS(64, u64, i64)

VM_INLINE f80 vm_f32(u32 bits)
{
    f32 value;

    memcpy(&value, &bits, sizeof(value));
    return (f80)value;
}

VM_INLINE f80 vm_f64(u64 bits)
{
    f64 value;

    memcpy(&value, &bits, sizeof(value));
    return (f80)value;
}

VM_INLINE u32 vm_u32f(f80 x)
{
    volatile f32 rounded = (f32)x;
    f32 value = rounded;
    u32 bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

VM_INLINE u64 vm_u64f(f80 x)
{
    volatile f64 rounded = (f64)x;
    f64 value = rounded;
    u64 bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

VM_INLINE f80 vm_ld80(const u8 *M, u32 address)
{
    f80 x = 0;

    memcpy(&x, M + address, VM_F80_BYTES);
    return x;
}

VM_INLINE void vm_st80(u8 *M, u32 address, f80 x)
{
    memcpy(M + address, &x, VM_F80_BYTES);
}

VM_INLINE f80 vm_frndint(f80 x)
{
    __asm__("frndint" : "+t"(x));
    return x;
}

VM_INLINE f80 vm_fsqrt(f80 x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

VM_INLINE f80 vm_fsin(f80 x)
{
    __asm__("fsin" : "+t"(x));
    return x;
}

VM_INLINE f80 vm_fcos(f80 x)
{
    __asm__("fcos" : "+t"(x));
    return x;
}

VM_INLINE f80 vm_f2xm1(f80 x)
{
    __asm__("f2xm1" : "+t"(x));
    return x;
}

VM_INLINE f80 vm_ftan(f80 x)
{
    __asm__("fptan\n\tfstp %%st(0)" : "+t"(x));
    return x;
}

VM_INLINE f80 vm_fyl2x(f80 st0, f80 st1)
{
    f80 r;

    __asm__("fyl2x" : "=t"(r) : "0"(st0), "u"(st1) : "st(1)");
    return r;
}

VM_INLINE f80 vm_fyl2xp1(f80 st0, f80 st1)
{
    f80 r;

    __asm__("fyl2xp1" : "=t"(r) : "0"(st0), "u"(st1) : "st(1)");
    return r;
}

VM_INLINE f80 vm_fpatan(f80 st0, f80 st1)
{
    f80 r;

    __asm__("fpatan" : "=t"(r) : "0"(st0), "u"(st1) : "st(1)");
    return r;
}

VM_INLINE f80 vm_fscale(f80 st0, f80 st1)
{
    f80 r;

    __asm__("fscale" : "=t"(r) : "0"(st0), "u"(st1));
    return r;
}

VM_INLINE f80 vm_fprem1(f80 st0, f80 st1)
{
    f80 r;

    __asm__("1:\tfprem1\n\tfnstsw %%ax\n\ttestb $4, %%ah\n\tjnz 1b" : "=t"(r) : "0"(st0), "u"(st1) : "ax", "cc");
    return r;
}

VM_INLINE f80 vm_fprem(f80 st0, f80 st1)
{
    f80 r;

    __asm__("1:\tfprem\n\tfnstsw %%ax\n\ttestb $4, %%ah\n\tjnz 1b" : "=t"(r) : "0"(st0), "u"(st1) : "ax", "cc");
    return r;
}

VM_INLINE f80 vm_fldpi(void)
{
    f80 r;

    __asm__("fldpi" : "=t"(r));
    return r;
}

VM_INLINE f80 vm_fldln2(void)
{
    f80 r;

    __asm__("fldln2" : "=t"(r));
    return r;
}

VM_INLINE f80 vm_fldl2e(void)
{
    f80 r;

    __asm__("fldl2e" : "=t"(r));
    return r;
}

VM_INLINE f80 vm_fldl2t(void)
{
    f80 r;

    __asm__("fldl2t" : "=t"(r));
    return r;
}

VM_INLINE f80 vm_fldlg2(void)
{
    f80 r;

    __asm__("fldlg2" : "=t"(r));
    return r;
}

VM_INLINE u32 vm_getmxcsr(void)
{
    volatile u32 mxcsr;

    __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
    return mxcsr;
}

VM_INLINE void vm_setmxcsr(u32 value)
{
    volatile u32 mxcsr = value;

    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
}

VM_INLINE u16 vm_getcw(void)
{
    volatile u16 control_word;

    __asm__ volatile("fnstcw %0" : "=m"(control_word));
    return control_word;
}

VM_INLINE void vm_setcw(u16 control_word)
{
    volatile u16 masked = (u16)(control_word | VM_X87_EXCEPTION_MASKS);
    volatile u32 mxcsr;

    __asm__ volatile("fldcw %0" : : "m"(masked));
    __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
    mxcsr = (mxcsr & ~VM_SSE_ROUNDING_MASK) | (((u32)control_word & VM_X87_ROUNDING_MASK) << VM_ROUNDING_SHIFT);
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
}

VM_INLINE u64 vm_ftoi64(f80 x)
{
    return (x >= -VM_I64_LIMIT && x < VM_I64_LIMIT) ? (u64)(i64)x : VM_I64_INDEFINITE;
}

VM_INLINE u32 vm_ftoi32(f80 x)
{
    return (x >= -VM_I32_LIMIT && x < VM_I32_LIMIT) ? (u32)(i32)x : VM_I32_INDEFINITE;
}

VM_INLINE u16 vm_ftoi16(f80 x)
{
    return (x >= -VM_I16_LIMIT && x < VM_I16_LIMIT) ? (u16)(i16)x : (u16)VM_I16_INDEFINITE;
}

#ifdef __cplusplus
}
#endif
