#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef long double f80;
typedef size_t usize;
typedef intptr_t iptr;
typedef uintptr_t uptr;
typedef char c8;

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
