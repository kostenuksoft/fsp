#include "vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <mmsystem.h>

#include "vm_rt.h"

#define ARENA_SIZE        0x100000000ull
#define ARENA_GUARD       0x10000ull
#define IMAGE_BASE        0x10000000u
#define IMAGE_END         0x10120000u
#define IMAGE_HEADER_SIZE 0x1000u
#define IMPORT_BASE       0xFE000000u

#define HEAP_BASE          0x20000000u
#define HEAP_LIMIT         0xF0000000u
#define HEAP_MAX_REQUEST   0xC0000000u
#define HEAP_HEADER        16u
#define HEAP_ALIGN         16u
#define HEAP_MAGIC         0x4C535048u
#define HEAP_MIN_PAYLOAD   16u
#define HEAP_SPLIT_PAYLOAD 32u
#define HEAP_COMMIT_STEP   0x100000u

#define STACK_SIZE      0x400000u
#define STACK_TOP_SLACK 0x100u
#define TEB_SIZE        0x1000u
#define TEB_SEH_CHAIN   0x00u
#define TEB_SEH_END     0xFFFFFFFFu
#define TEB_SELF        0x18u
#define RETURN_SENTINEL 0xDEADC0DEu
#define GUEST_THREAD_ID 1u

#define FPU_DEFAULT_CONTROL 0x137Fu
#define FPU_EMPTY_TAGS      0xFFFFu

#define HANDLE_BASE  0x00AB0000u
#define HANDLE_COUNT 64
#define MAX_IMPORTS  256

#define WSPRINTF_LIMIT    1024
#define SPRINTF_LIMIT     4096
#define LOG_LINE_SIZE     1024
#define MESSAGE_SIZE      512
#define FORMAT_SPEC_SIZE  32
#define FORMAT_FIELD_SIZE 512
#define LOG_ENV_NAME      "FLSPRO_LOG"

#define FAULT_STACK_SLACK   1024
#define FAULT_STACK_ALIGN   16
#define FAULT_RETURN_SLOT   8
#define SHADOW_REPORT_LIMIT 400

enum {
    SHADOW_UNINIT = 0,
    SHADOW_INIT = 1,
    SHADOW_REPORTED = 2
};

enum {
    LOCK_ABSENT = 0,
    LOCK_CREATING = 1,
    LOCK_READY = 2
};

enum {
    VM_FAILED = -1,
    VM_COLD = 0,
    VM_READY = 1
};

typedef struct HeapHeader {
    u32 size;
    u32 prev;
    u32 free;
    u32 magic;
} HeapHeader;

typedef struct FreeLinks {
    u32 next;
    u32 prev;
} FreeLinks;

typedef void (*ImportFn)(CPU *c);

typedef struct ImportDef {
    const c8 *name;
    ImportFn fn;
} ImportDef;

u8 *vm_mem;

static CPU g_cpu;
static CRITICAL_SECTION g_lock;
static volatile LONG g_lock_state;
static i32 g_state;
static void *g_jmp[5];
static bool g_trapped;
static bool g_dead;
static u16 g_host_cw;
static u32 g_host_mxcsr;
static i32 g_depth;
static u32 g_stack;
static c8 g_last_message[MESSAGE_SIZE];
static FILE *g_log;
static bool g_log_checked;
static DWORD g_vm_thread;
static PVOID g_fault_handler;

static u32 g_heap_end = HEAP_BASE;
static u32 g_heap_commit = HEAP_BASE;
static u32 g_heap_last;
static u32 g_free_head;

static void *g_handles[HANDLE_COUNT];
static ImportFn g_import_fn[MAX_IMPORTS];
static i32 g_import_count;

#define M vm_mem

#ifdef VM_SHADOW
u8 *vm_shadow;
static i32 g_shadow_reports;
static u32 shadow_block(u32 address);
#define SHADOW_SET(a, n, v) memset(vm_shadow + (a), (v), (n))
#else
#define SHADOW_SET(a, n, v) ((void)0)
#endif

void vm_log(const c8 *format, ...)
{
    c8 line[LOG_LINE_SIZE];
    va_list list;

    va_start(list, format);
    vsnprintf(line, sizeof(line), format, list);
    va_end(list);
    if (!g_log_checked) {
        const c8 *path = getenv(LOG_ENV_NAME);

        g_log_checked = true;
        if (path && *path) {
            g_log = fopen(path, "a");
        }
    }
    if (g_log) {
        fputs(line, g_log);
        fputc('\n', g_log);
        fflush(g_log);
    }
    OutputDebugStringA(line);
}

const c8 *vm_last_message(void)
{
    return g_last_message;
}

#ifdef VM_SHADOW
void vm_shadow_read(u32 address, u32 size, const c8 *function)
{
    u32 i;

    for (i = 0; i < size; ++i) {
        if (vm_shadow[address + i] != SHADOW_UNINIT) {
            continue;
        }
        if (g_shadow_reports < SHADOW_REPORT_LIMIT) {
            u32 block = shadow_block(address);

            ++g_shadow_reports;
            vm_log("uninit read %08x+%u in %s block=%08x off=%d", address, size, function, block,
                   (i32)(address - block));
        }
        vm_shadow[address + i] = SHADOW_REPORTED;
        return;
    }
}
#endif

static inline HeapHeader *heap_header(u32 block)
{
    return (HeapHeader *)(M + block);
}

static inline FreeLinks *free_links(u32 block)
{
    return (FreeLinks *)(M + block + HEAP_HEADER);
}

static void free_list_remove(u32 block)
{
    u32 next = free_links(block)->next;
    u32 prev = free_links(block)->prev;

    if (prev) {
        free_links(prev)->next = next;
    } else {
        g_free_head = next;
    }
    if (next) {
        free_links(next)->prev = prev;
    }
}

static void free_list_push(u32 block)
{
    free_links(block)->next = g_free_head;
    free_links(block)->prev = 0;
    if (g_free_head) {
        free_links(g_free_head)->prev = block;
    }
    g_free_head = block;
}

static u32 commit_ceiling(u32 address)
{
    return (address + HEAP_COMMIT_STEP - 1) & ~(HEAP_COMMIT_STEP - 1);
}

static bool heap_grow(u32 new_end)
{
    u32 top;

    if (new_end > HEAP_LIMIT || new_end < HEAP_BASE) {
        return false;
    }
    if (new_end <= g_heap_commit) {
        return true;
    }
    top = commit_ceiling(new_end);
    if (!VirtualAlloc(M + g_heap_commit, top - g_heap_commit, MEM_COMMIT, PAGE_READWRITE)) {
        return false;
    }
#ifdef VM_SHADOW
    VirtualAlloc(vm_shadow + g_heap_commit, top - g_heap_commit, MEM_COMMIT, PAGE_READWRITE);
#endif
    g_heap_commit = top;
    return true;
}

static void heap_split(u32 block, u32 need)
{
    u32 rest_block = block + need;
    u32 rest_size = heap_header(block)->size - need;

    heap_header(rest_block)->size = rest_size;
    heap_header(rest_block)->prev = need;
    heap_header(rest_block)->free = 1;
    heap_header(rest_block)->magic = HEAP_MAGIC;
    if (rest_block + rest_size < g_heap_end) {
        heap_header(rest_block + rest_size)->prev = rest_size;
    } else {
        g_heap_last = rest_size;
    }
    heap_header(block)->size = need;
    free_list_push(rest_block);
}

static u32 heap_take(u32 size)
{
    u32 need;
    u32 block;

    if (size > HEAP_MAX_REQUEST) {
        return 0;
    }
    need = (((size ? size : 1) + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1)) + HEAP_HEADER;
    if (need < HEAP_HEADER + HEAP_MIN_PAYLOAD) {
        need = HEAP_HEADER + HEAP_MIN_PAYLOAD;
    }
    for (block = g_free_head; block; block = free_links(block)->next) {
        if (heap_header(block)->size < need) {
            continue;
        }
        free_list_remove(block);
        if (heap_header(block)->size - need >= HEAP_HEADER + HEAP_SPLIT_PAYLOAD) {
            heap_split(block, need);
        }
        heap_header(block)->free = 0;
        return block + HEAP_HEADER;
    }
    block = g_heap_end;
    if ((u64)block + need > HEAP_LIMIT || !heap_grow(block + need)) {
        return 0;
    }
    heap_header(block)->size = need;
    heap_header(block)->prev = g_heap_last;
    heap_header(block)->free = 0;
    heap_header(block)->magic = HEAP_MAGIC;
    g_heap_end = block + need;
    g_heap_last = need;
    return block + HEAP_HEADER;
}

static u32 payload_size(u32 address)
{
    return heap_header(address - HEAP_HEADER)->size - HEAP_HEADER;
}

u32 vm_alloc(u32 size)
{
    u32 address = heap_take(size);

    if (address) {
        memset(M + address, 0, payload_size(address));
        SHADOW_SET(address, payload_size(address), SHADOW_UNINIT);
    }
    return address;
}

static bool heap_valid(u32 address)
{
    if (address < HEAP_BASE + HEAP_HEADER || address >= g_heap_end) {
        return false;
    }
    if (address & (HEAP_ALIGN - 1)) {
        return false;
    }
    return heap_header(address - HEAP_HEADER)->magic == HEAP_MAGIC && !heap_header(address - HEAP_HEADER)->free;
}

static void heap_release_tail(u32 block)
{
    u32 top;

    g_heap_end = block;
    g_heap_last = heap_header(block)->prev;
    top = commit_ceiling(g_heap_end);
    if (top < g_heap_commit) {
        VirtualFree(M + top, g_heap_commit - top, MEM_DECOMMIT);
        g_heap_commit = top;
    }
}

void vm_free(u32 address)
{
    u32 block;
    u32 next;

    if (!address) {
        return;
    }
    if (!heap_valid(address)) {
        vm_log("flspro: free of invalid pointer %08x ignored", address);
        return;
    }
    block = address - HEAP_HEADER;
    heap_header(block)->free = 1;
    next = block + heap_header(block)->size;
    if (next < g_heap_end && heap_header(next)->free) {
        free_list_remove(next);
        heap_header(block)->size += heap_header(next)->size;
    }
    if (heap_header(block)->prev) {
        u32 prev = block - heap_header(block)->prev;

        if (heap_header(prev)->free) {
            free_list_remove(prev);
            heap_header(prev)->size += heap_header(block)->size;
            block = prev;
        }
    }
    next = block + heap_header(block)->size;
    if (next < g_heap_end) {
        heap_header(next)->prev = heap_header(block)->size;
        free_list_push(block);
    } else {
        heap_release_tail(block);
    }
}

u32 vm_msize(u32 address)
{
    return heap_valid(address) ? payload_size(address) : 0;
}

static u32 heap_resize(u32 address, u32 size)
{
    u32 old_size;
    u32 moved;

    if (!address) {
        return vm_alloc(size);
    }
    old_size = vm_msize(address);
    if (old_size >= size) {
        return address;
    }
    moved = vm_alloc(size);
    if (moved) {
        memcpy(M + moved, M + address, old_size);
        vm_free(address);
    }
    return moved;
}

static bool address_ok(u32 address)
{
    if (address >= IMAGE_BASE + IMAGE_HEADER_SIZE && address < IMAGE_END) {
        return true;
    }
    return address >= HEAP_BASE && address < g_heap_end;
}

void *vm_ptr(u32 address)
{
    return address ? M + address : NULL;
}

static u32 handle_new(void *host_handle)
{
    u32 i;

    for (i = 1; i < HANDLE_COUNT; ++i) {
        if (!g_handles[i]) {
            g_handles[i] = host_handle;
            return HANDLE_BASE + i;
        }
    }
    return 0;
}

static void *handle_get(u32 guest_handle)
{
    u32 i = guest_handle - HANDLE_BASE;

    return i < HANDLE_COUNT ? g_handles[i] : NULL;
}

static void handle_delete(u32 guest_handle)
{
    u32 i = guest_handle - HANDLE_BASE;

    if (i < HANDLE_COUNT) {
        g_handles[i] = NULL;
    }
}

static void format_field(c8 *field, usize field_size, const c8 *spec, c8 conversion, u32 *args)
{
    field[0] = 0;
    switch (conversion) {
    case 's': {
        u32 text = *(vm_u32 *)(M + *args);

        *args += VM_GUEST_WORD;
        snprintf(field, field_size, spec, text && address_ok(text) ? (const c8 *)(M + text) : "(null)");
        break;
    }
    case 'c':
    case 'd':
    case 'i':
    case 'u':
    case 'x':
    case 'X':
    case 'o':
        snprintf(field, field_size, spec, *(vm_u32 *)(M + *args));
        *args += VM_GUEST_WORD;
        break;
    case 'f':
    case 'g':
    case 'e':
    case 'G':
    case 'E': {
        f64 value;

        memcpy(&value, M + *args, sizeof(value));
        *args += sizeof(value);
        snprintf(field, field_size, spec, value);
        break;
    }
    default:
        snprintf(field, field_size, "%s", spec);
        break;
    }
}

static i32 format_guest(c8 *dest, usize cap, const c8 *format, u32 args)
{
    c8 spec[FORMAT_SPEC_SIZE];
    c8 field[FORMAT_FIELD_SIZE];
    usize length = 0;

    if (!cap) {
        return 0;
    }
    while (*format && length + 1 < cap) {
        const c8 *start;
        usize count;

        if (*format != '%') {
            dest[length++] = *format++;
            continue;
        }
        start = format++;
        if (*format == '%') {
            dest[length++] = *format++;
            continue;
        }
        while (*format && strchr("-+ #0123456789.", *format)) {
            ++format;
        }
        while (*format == 'l' || *format == 'h') {
            ++format;
        }
        if (!*format) {
            break;
        }
        count = (usize)(format - start) + 1;
        if (count >= sizeof(spec)) {
            count = sizeof(spec) - 1;
        }
        memcpy(spec, start, count);
        spec[count] = 0;
        format_field(field, sizeof(field), spec, *format, &args);
        ++format;
        count = strlen(field);
        if (count > cap - 1 - length) {
            count = cap - 1 - length;
        }
        memcpy(dest + length, field, count);
        length += count;
    }
    dest[length] = 0;
    return (i32)length;
}

#define ARG(i) (*(vm_u32 *)(M + c->esp + VM_GUEST_WORD * (1 + (i))))
#define RET(n) \
    do { \
        c->esp += VM_GUEST_WORD * (1 + (n)); \
        return; \
    } while (0)
#define STR(a) ((c8 *)(M + (a)))

static void imp_zero_0(CPU *c)
{
    c->eax = 0;
    RET(0);
}

static void imp_zero_1(CPU *c)
{
    c->eax = 0;
    RET(1);
}

static void imp_zero_2(CPU *c)
{
    c->eax = 0;
    RET(2);
}

static void imp_zero_3(CPU *c)
{
    c->eax = 0;
    RET(3);
}

static void imp_zero_5(CPU *c)
{
    c->eax = 0;
    RET(5);
}

static void imp_one_1(CPU *c)
{
    c->eax = 1;
    RET(1);
}

static void imp_one_2(CPU *c)
{
    c->eax = 1;
    RET(2);
}

static u32 alloc_for_guest(u32 size, bool zero)
{
    u32 address = vm_alloc(size);

    if (address && zero) {
        memset(M + address, 0, size);
        SHADOW_SET(address, size, SHADOW_INIT);
    }
    return address;
}

static void imp_GlobalAlloc(CPU *c)
{
    u32 flags = ARG(0);
    u32 size = ARG(1);

    c->eax = alloc_for_guest(size, (flags & GMEM_ZEROINIT) != 0);
    RET(2);
}

static void imp_GlobalFree(CPU *c)
{
    vm_free(ARG(0));
    c->eax = 0;
    RET(1);
}

static void imp_GlobalLock(CPU *c)
{
    c->eax = ARG(0);
    RET(1);
}

static void imp_GlobalHandle(CPU *c)
{
    c->eax = ARG(0);
    RET(1);
}

static void imp_HeapAlloc(CPU *c)
{
    u32 flags = ARG(1);
    u32 size = ARG(2);

    c->eax = alloc_for_guest(size, (flags & HEAP_ZERO_MEMORY) != 0);
    RET(3);
}

static void imp_HeapFree(CPU *c)
{
    vm_free(ARG(2));
    c->eax = 1;
    RET(3);
}

static void imp_HeapReAlloc(CPU *c)
{
    c->eax = heap_resize(ARG(2), ARG(3));
    RET(4);
}

static void imp_IsBadCodePtr(CPU *c)
{
    c->eax = address_ok(ARG(0)) ? 0 : 1;
    RET(1);
}

static void imp_IsBadDataPtr(CPU *c)
{
    c->eax = address_ok(ARG(0)) ? 0 : 1;
    RET(2);
}

static void imp_Sleep(CPU *c)
{
    RET(1);
}

static void imp_timeGetTime(CPU *c)
{
    c->eax = timeGetTime();
    RET(0);
}

static void imp_GetCurrentThreadId(CPU *c)
{
    c->eax = GUEST_THREAD_ID;
    RET(0);
}

static void imp_OutputDebugStringA(CPU *c)
{
    vm_log("flspro: %s", STR(ARG(0)));
    RET(1);
}

static void imp_lstrcpyA(CPU *c)
{
    strcpy(STR(ARG(0)), STR(ARG(1)));
    SHADOW_SET(ARG(0), strlen(STR(ARG(0))) + 1, SHADOW_INIT);
    c->eax = ARG(0);
    RET(2);
}

static void imp_lstrcatA(CPU *c)
{
    strcat(STR(ARG(0)), STR(ARG(1)));
    SHADOW_SET(ARG(0), strlen(STR(ARG(0))) + 1, SHADOW_INIT);
    c->eax = ARG(0);
    RET(2);
}

static void imp_wsprintfA(CPU *c)
{
    c->eax = (u32)format_guest(STR(ARG(0)), WSPRINTF_LIMIT, STR(ARG(1)), c->esp + VM_GUEST_WORD * (1 + 2));
    SHADOW_SET(ARG(0), c->eax + 1, SHADOW_INIT);
    RET(0);
}

static void imp_MessageBoxA(CPU *c)
{
    const c8 *text = ARG(1) ? STR(ARG(1)) : "";

    snprintf(g_last_message, sizeof(g_last_message), "%s", text);
    vm_log("flspro message: %s", text);
    c->eax = IDOK;
    RET(4);
}

static void imp_LoadStringA(CPU *c)
{
    if (ARG(2) && ARG(3)) {
        STR(ARG(2))[0] = 0;
    }
    c->eax = 0;
    RET(4);
}

static void imp_GetPrivateProfileIntA(CPU *c)
{
    c->eax = ARG(2);
    RET(4);
}

static void imp_GetPrivateProfileStringA(CPU *c)
{
    u32 fallback = ARG(2);
    u32 buffer = ARG(3);
    u32 capacity = ARG(4);
    u32 length = 0;

    if (buffer && capacity) {
        if (fallback) {
            length = (u32)strlen(STR(fallback));
            if (length > capacity - 1) {
                length = capacity - 1;
            }
            memcpy(STR(buffer), STR(fallback), length);
        }
        STR(buffer)[length] = 0;
    }
    c->eax = length;
    RET(6);
}

static void imp_GetWindowsDirectoryA(CPU *c)
{
    c8 directory[MAX_PATH];
    u32 buffer = ARG(0);
    u32 capacity = ARG(1);
    UINT length = GetWindowsDirectoryA(directory, sizeof(directory));

    if (buffer && capacity > length) {
        memcpy(STR(buffer), directory, length + 1);
    }
    c->eax = length;
    RET(2);
}

static void imp_CreateFileA(CPU *c)
{
    HANDLE file = CreateFileA(STR(ARG(0)), ARG(1), ARG(2), NULL, ARG(4), ARG(5), NULL);

    c->eax = VM_CALL_FAILED;
    if (file != INVALID_HANDLE_VALUE) {
        u32 guest_handle = handle_new(file);

        if (guest_handle) {
            c->eax = guest_handle;
        } else {
            CloseHandle(file);
        }
    }
    RET(7);
}

static void imp_GetFileTime(CPU *c)
{
    HANDLE file = handle_get(ARG(0));

    c->eax = 0;
    if (file) {
        c->eax = (u32)GetFileTime(file, (FILETIME *)vm_ptr(ARG(1)), (FILETIME *)vm_ptr(ARG(2)),
                                  (FILETIME *)vm_ptr(ARG(3)));
    }
    RET(4);
}

static void imp_CloseHandle(CPU *c)
{
    HANDLE file = handle_get(ARG(0));

    if (file) {
        CloseHandle(file);
        handle_delete(ARG(0));
    }
    c->eax = file ? 1 : 0;
    RET(1);
}

static void imp_mmioOpenA(CPU *c)
{
    HMMIO file;

    if (ARG(1)) {
        vm_log("flspro: mmioOpenA with MMIOINFO not supported");
    }
    file = mmioOpenA(ARG(0) ? STR(ARG(0)) : NULL, NULL, ARG(2));
    c->eax = file ? handle_new(file) : 0;
    if (file && !c->eax) {
        mmioClose(file, 0);
    }
    RET(3);
}

static void imp_mmioClose(CPU *c)
{
    HMMIO file = handle_get(ARG(0));

    c->eax = file ? mmioClose(file, ARG(1)) : MMIOERR_INVALIDFILE;
    handle_delete(ARG(0));
    RET(2);
}

static void imp_mmioRead(CPU *c)
{
    HMMIO file = handle_get(ARG(0));
    u32 buffer = ARG(1);
    u32 size = ARG(2);

    if (!file || !address_ok(buffer) || (size && !address_ok(buffer + size - 1))) {
        c->eax = VM_CALL_FAILED;
    } else {
        c->eax = (u32)mmioRead(file, (HPSTR)(M + buffer), (LONG)size);
        SHADOW_SET(buffer, size, SHADOW_INIT);
    }
    RET(3);
}

static void imp_mmioSeek(CPU *c)
{
    HMMIO file = handle_get(ARG(0));

    c->eax = file ? (u32)mmioSeek(file, (LONG)ARG(1), (i32)ARG(2)) : VM_CALL_FAILED;
    RET(3);
}

static void imp_mmioDescend(CPU *c)
{
    HMMIO file = handle_get(ARG(0));

    c->eax = MMIOERR_INVALIDFILE;
    if (file) {
        c->eax = mmioDescend(file, (MMCKINFO *)(M + ARG(1)), (MMCKINFO *)vm_ptr(ARG(2)), ARG(3));
    }
    SHADOW_SET(ARG(1), sizeof(MMCKINFO), SHADOW_INIT);
    RET(4);
}

static void imp_mmioAscend(CPU *c)
{
    HMMIO file = handle_get(ARG(0));

    c->eax = file ? mmioAscend(file, (MMCKINFO *)(M + ARG(1)), ARG(2)) : MMIOERR_INVALIDFILE;
    RET(3);
}

static void imp_mmioStringToFOURCCA(CPU *c)
{
    c->eax = mmioStringToFOURCCA(STR(ARG(0)), ARG(1));
    RET(2);
}

static const ImportDef g_import_defs[] = {
    { "InitializeCriticalSection", imp_zero_1 },
    { "DeleteCriticalSection", imp_zero_1 },
    { "EnterCriticalSection", imp_zero_1 },
    { "LeaveCriticalSection", imp_zero_1 },
    { "GlobalAlloc", imp_GlobalAlloc },
    { "GlobalFree", imp_GlobalFree },
    { "GlobalLock", imp_GlobalLock },
    { "GlobalUnlock", imp_one_1 },
    { "GlobalHandle", imp_GlobalHandle },
    { "HeapAlloc", imp_HeapAlloc },
    { "HeapFree", imp_HeapFree },
    { "HeapReAlloc", imp_HeapReAlloc },
    { "IsBadCodePtr", imp_IsBadCodePtr },
    { "IsBadReadPtr", imp_IsBadDataPtr },
    { "IsBadWritePtr", imp_IsBadDataPtr },
    { "Sleep", imp_Sleep },
    { "timeGetTime", imp_timeGetTime },
    { "GetCurrentThreadId", imp_GetCurrentThreadId },
    { "OutputDebugStringA", imp_OutputDebugStringA },
    { "lstrcpyA", imp_lstrcpyA },
    { "lstrcatA", imp_lstrcatA },
    { "wsprintfA", imp_wsprintfA },
    { "MessageBoxA", imp_MessageBoxA },
    { "LoadStringA", imp_LoadStringA },
    { "GetPrivateProfileIntA", imp_GetPrivateProfileIntA },
    { "GetPrivateProfileStringA", imp_GetPrivateProfileStringA },
    { "GetWindowsDirectoryA", imp_GetWindowsDirectoryA },
    { "CreateFileA", imp_CreateFileA },
    { "GetFileTime", imp_GetFileTime },
    { "CloseHandle", imp_CloseHandle },
    { "mmioOpenA", imp_mmioOpenA },
    { "mmioClose", imp_mmioClose },
    { "mmioRead", imp_mmioRead },
    { "mmioSeek", imp_mmioSeek },
    { "mmioDescend", imp_mmioDescend },
    { "mmioAscend", imp_mmioAscend },
    { "mmioStringToFOURCCA", imp_mmioStringToFOURCCA },
    { "CreateDialogParamA", imp_zero_5 },
    { "DestroyWindow", imp_one_1 },
    { "UpdateWindow", imp_one_1 },
    { "InvalidateRect", imp_zero_3 },
    { "LoadCursorA", imp_zero_2 },
    { "SetCursor", imp_zero_1 },
    { "GetDesktopWindow", imp_zero_0 },
    { "GetLastError", imp_zero_0 },
    { "SetLastError", imp_zero_1 },
    { "GetDC", imp_zero_1 },
    { "ReleaseDC", imp_one_2 },
};

static ImportFn import_lookup(const c8 *name)
{
    usize i;

    for (i = 0; i < ARRAY_COUNT(g_import_defs); ++i) {
        if (!strcmp(g_import_defs[i].name, name)) {
            return g_import_defs[i].fn;
        }
    }
    return NULL;
}

void vm_import(CPU *c, u32 target)
{
    u32 index = target - IMPORT_BASE;

    if (target == VM_NOTE_END_THUNK) {
        vm_note_end(c);
        return;
    }
    if (index >= (u32)g_import_count) {
        vm_log("flspro: indirect call to unknown address %08x", target);
        vm_trap(c, target);
    }
    if (!g_import_fn[index]) {
        vm_log("flspro: unimplemented import %s", vm_imports[index].name);
        vm_trap(c, target);
    }
    g_import_fn[index](c);
}

void vm_trap(CPU *c, u32 where)
{
    vm_log("flspro: trap at %08x (esp=%08x)", where, c->esp);
    g_trapped = true;
    __builtin_longjmp(g_jmp, 1);
}

void vm_hook_malloc(CPU *c)
{
    c->eax = vm_alloc(ARG(0));
    RET(0);
}

void vm_hook_free(CPU *c)
{
    vm_free(ARG(0));
    RET(0);
}

void vm_hook_fclose(CPU *c)
{
    c->eax = 0;
    RET(0);
}

void vm_hook_sprintf(CPU *c)
{
    c->eax = (u32)format_guest(STR(ARG(0)), SPRINTF_LIMIT, STR(ARG(1)), c->esp + VM_GUEST_WORD * (1 + 2));
    SHADOW_SET(ARG(0), c->eax + 1, SHADOW_INIT);
    RET(0);
}

static void fault_resume(void)
{
    vm_log("flspro: fault inside the engine, engine stopped");
    g_trapped = true;
    g_dead = true;
    __builtin_longjmp(g_jmp, 1);
}

static bool fault_is_ours(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return true;
    default:
        return false;
    }
}

static LONG CALLBACK fault_filter(EXCEPTION_POINTERS *info)
{
    EXCEPTION_RECORD *record = info->ExceptionRecord;
    CONTEXT *context = info->ContextRecord;
    u64 guest_address = 0;

    if (!g_vm_thread || g_vm_thread != GetCurrentThreadId()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (!fault_is_ours(record->ExceptionCode)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (record->NumberParameters > 1) {
        guest_address = record->ExceptionInformation[1] - (ULONG_PTR)vm_mem;
    }
    vm_log("flspro: exception %08x at rip-base=%llx data=%llx", (u32)record->ExceptionCode,
           (u64)(context->Rip - (DWORD64)GetModuleHandleA(NULL)), guest_address);
    context->Rsp = ((context->Rsp - FAULT_STACK_SLACK) & ~(DWORD64)(FAULT_STACK_ALIGN - 1)) - FAULT_RETURN_SLOT;
    context->Rip = (DWORD64)(void *)fault_resume;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void bind_imports(void)
{
    i32 i;

    for (i = 0; vm_imports[i].slot && i < MAX_IMPORTS; ++i) {
        *(vm_u32 *)(M + vm_imports[i].slot) = IMPORT_BASE + (u32)i;
        g_import_fn[i] = import_lookup(vm_imports[i].name);
    }
    g_import_count = i;
}

static bool vm_init(void)
{
    const VmSection *section;
    u32 stack;
    u32 teb;

    vm_mem = VirtualAlloc(NULL, ARENA_SIZE + ARENA_GUARD, MEM_RESERVE, PAGE_NOACCESS);
    if (!vm_mem) {
        return false;
    }
#ifdef VM_SHADOW
    vm_shadow = VirtualAlloc(NULL, ARENA_SIZE + ARENA_GUARD, MEM_RESERVE, PAGE_NOACCESS);
    VirtualAlloc(vm_shadow + IMAGE_BASE, IMAGE_END - IMAGE_BASE, MEM_COMMIT, PAGE_READWRITE);
    memset(vm_shadow + IMAGE_BASE, SHADOW_INIT, IMAGE_END - IMAGE_BASE);
#endif
    if (!VirtualAlloc(M + IMAGE_BASE, IMAGE_END - IMAGE_BASE, MEM_COMMIT, PAGE_READWRITE)) {
        return false;
    }
    for (section = vm_sections; section->va; ++section) {
        memcpy(M + section->va, section->data, section->size);
    }
    bind_imports();

    stack = vm_alloc(STACK_SIZE);
    g_stack = stack;
    teb = vm_alloc(TEB_SIZE);
    if (!stack || !teb) {
        return false;
    }
    memset(M + teb, 0, TEB_SIZE);
    SHADOW_SET(teb, TEB_SIZE, SHADOW_INIT);
    SHADOW_SET(stack, STACK_SIZE, SHADOW_INIT);
    *(vm_u32 *)(M + teb + TEB_SEH_CHAIN) = TEB_SEH_END;
    *(vm_u32 *)(M + teb + TEB_SELF) = teb;

    memset(&g_cpu, 0, sizeof(g_cpu));
    g_cpu.esp = stack + STACK_SIZE - STACK_TOP_SLACK;
    g_cpu.fs = teb;
    g_cpu.cw = FPU_DEFAULT_CONTROL;
    g_cpu.fpu_10a4 = FPU_EMPTY_TAGS;
    g_fault_handler = AddVectoredExceptionHandler(1, fault_filter);
    return true;
}

void vm_lock(void)
{
    if (g_lock_state != LOCK_READY) {
        if (InterlockedCompareExchange(&g_lock_state, LOCK_CREATING, LOCK_ABSENT) == LOCK_ABSENT) {
            InitializeCriticalSection(&g_lock);
            InterlockedExchange(&g_lock_state, LOCK_READY);
        } else {
            while (g_lock_state != LOCK_READY) {
                Sleep(0);
            }
        }
    }
    EnterCriticalSection(&g_lock);
}

void vm_unlock(void)
{
    LeaveCriticalSection(&g_lock);
}

bool vm_enter(void)
{
    if (g_dead || g_state == VM_FAILED) {
        return false;
    }
    if (g_state == VM_COLD) {
        if (!vm_init()) {
            g_state = VM_FAILED;
            return false;
        }
        g_state = VM_READY;
    }
    if (g_depth == 0) {
        g_host_cw = vm_getcw();
        g_host_mxcsr = vm_getmxcsr();
        __asm__ volatile("fninit");
        vm_setcw(g_cpu.cw);
    }
    ++g_depth;
    return true;
}

void vm_leave(void)
{
    --g_depth;
    if (g_depth == 0) {
        __asm__ volatile("fninit");
        vm_setcw(g_host_cw);
        vm_setmxcsr(g_host_mxcsr);
    }
}

void vm_set_fpu_cw(u16 control_word)
{
    g_cpu.cw = control_word;
}

u32 vm_invoke(void (*fn)(CPU *), i32 arg_count, const u32 *args)
{
    CPU *c = &g_cpu;
    u32 saved = c->esp;
    u32 esp = saved;
    i32 i;

    for (i = arg_count - 1; i >= 0; --i) {
        esp -= VM_GUEST_WORD;
        *(vm_u32 *)(M + esp) = args[i];
    }
    esp -= VM_GUEST_WORD;
    *(vm_u32 *)(M + esp) = RETURN_SENTINEL;
    c->esp = esp;
    c->df = 0;
    SHADOW_SET(g_stack, esp - g_stack, SHADOW_UNINIT);
    SHADOW_SET(esp, saved - esp, SHADOW_INIT);
    g_trapped = false;
    if (__builtin_setjmp(g_jmp) == 0) {
        g_vm_thread = GetCurrentThreadId();
        fn(c);
    } else {
        c->eax = VM_CALL_FAILED;
    }
    g_vm_thread = 0;
    c->esp = saved;
    if (g_trapped) {
        vm_setcw(c->cw);
    }
    return c->eax;
}

bool vm_trapped(void)
{
    return g_trapped;
}

void vm_mark_init(u32 address, u32 size)
{
    (void)address;
    (void)size;
    SHADOW_SET(address, size, SHADOW_INIT);
}

#ifdef VM_SHADOW
static u32 shadow_block(u32 address)
{
    u32 block = HEAP_BASE;

    while (block < g_heap_end) {
        if (address >= block && address < block + heap_header(block)->size) {
            return block + HEAP_HEADER;
        }
        block += heap_header(block)->size;
    }
    return 0;
}
#endif

void vm_shutdown(void)
{
    if (g_fault_handler) {
        RemoveVectoredExceptionHandler(g_fault_handler);
        g_fault_handler = NULL;
    }
    g_dead = true;
    if (vm_mem) {
        VirtualFree(vm_mem, 0, MEM_RELEASE);
        vm_mem = NULL;
    }
}
