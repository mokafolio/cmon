#ifndef CMON_CMON_BASE_H
#define CMON_CMON_BASE_H

// DEBUG
#if !defined(NDEBUG)
#define CMON_DEBUG
#else
#undef CMON_DEBUG
#endif

#ifndef CMON_MALLOC
#define CMON_MALLOC(_bc) malloc(_bc)
#define CMON_REALLOC(_ptr, _bc) realloc(_ptr, _bc)
#define CMON_FREE(_ptr) free(_ptr)
#endif

#ifndef CMON_API
#define CMON_API __attribute__((visibility("default")))
#define CMON_LOCAL __attribute__((visibility("hidden")))
#endif

#define CMON_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define CMON_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CMON_ABS(x) ((x)<0 ? -(x) : (x))

// #ifndef CMON_MAX_IDENT_LEN
// #define CMON_MAX_IDENT_LEN 128
// #endif

// #ifndef CMON_TMP_STR_BUFFER_SIZE
// #define CMON_TMP_STR_BUFFER_SIZE 4096
// #endif

#define CMON_UNUSED(x) (void)(x)

/* these are based on the linux kernel */
#define OFFSET_OF(type, member) ((size_t) & ((type *)0)->member)
#define CONTAINER_OF(ptr, type, member)                                                            \
    ({                                                                                             \
        const __typeof__(((type *)0)->member) * __mptr = (ptr);                                    \
        (type *)((char *)__mptr - OFFSET_OF(type, member));                                        \
    })


#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || defined(__BIG_ENDIAN__) ||            \
    defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || defined(_MIBSEB) ||    \
    defined(__MIBSEB) || defined(__MIBSEB__) ||                                                    \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define CMON_BIG_ENDIAN
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || defined(__LITTLE_ENDIAN__) ||    \
    defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) ||    \
    defined(__MIPSEL) || defined(__MIPSEL__) ||                                                    \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define CMON_LITTLE_ENDIAN
#else
#error "I don't know what architecture this is!"
#endif

#define CMON_PATH_MAX 4096
#define CMON_FILENAME_MAX 256
#define CMON_EXT_MAX 32
#define CMON_ERR_MSG_MAX 4096
// //@TODO: Remove all of these that are not used anymore
// //@TODO: start using this for fixed size arrays
// //how many *mut[]mut****mut[] type prefixes dow we want to support?
// #define CMON_TYPE_PREFIX_MAX 128
// //max types per tuple
// #define CMON_TUPLE_MAX 128
// //max params per function signature
// #define CMON_PARAMS_MAX 128

//@TODO: Maybe only include assert here and the rest only where its needed?
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


// #include <setjmp.h>
// #include <stdarg.h>

typedef enum
{
    cmon_false = (unsigned char)0,
    cmon_true = (unsigned char)1
} cmon_bool;

typedef struct
{
    const char * begin;
    const char * end;
} cmon_str_view;

typedef size_t cmon_idx;

#endif //CMON_CMON_BASE_H
