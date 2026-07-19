#ifndef __SB_GLOBAL_H__
#define __SB_GLOBAL_H__

#ifdef DEBUG
#define PANIC(...) do { fprintf(stderr, "PANIC: " __VA_ARGS__); fprintf(stderr, "\n       at " __FILE__ ":%d\n", __LINE__); abort(); } while (0)
#define CHECK(...) do { fprintf(stderr, "BUGCHECK: " __VA_ARGS__); fprintf(stderr, "\n       at " __FILE__ ":%d\n", __LINE__); abort(); } while (0)
#define debug(...) printf(__VA_ARGS__)
#else
#define PANIC(...) do { fprintf(stderr, "PANIC: " __VA_ARGS__); fprintf(stderr, "\n"); abort(); } while (0)
#define CHECK(...) ((void)0)
#define debug(...) ((void)0)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;
typedef uint8_t flag;
typedef size_t usize;
typedef ptrdiff_t isize;

#define TRUE ((flag)1)
#define FALSE ((flag)0)

#endif
