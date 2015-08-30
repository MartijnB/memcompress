#ifndef _SHARED_PLATFORM_H
#define _SHARED_PLATFORM_H

#include <stdint.h>

// Use internally always an uint64 value for pointers, even on 32 bit platforms
typedef uint64_t ptr_t;

#define UINT32_MAX_VALUE ~0u
#define UINT64_MAX_VALUE ~0ULL

#define IS_32BIT() (sizeof(void*) == 4)
#define IS_64BIT() (sizeof(void*) == 8)

#define IS_VALID_32BIT_POINTER(p) (p <= ~0u)
#define IS_VALID_POINTER(p) (IS_64BIT() || IS_VALID_32BIT_POINTER(p))

// Convert a pointer (most likely stored as uint64) to a void* without warnings
#define TO_N_PTR(p) ((void*) (uintptr_t) p)

#endif /* _SHARED_PLATFORM_H */