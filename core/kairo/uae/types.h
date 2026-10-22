/*
 * KairoST: the integer, address and string types Hatari's WinUAE CPU core names, in place of the
 * uae/types.h left out of the Hatari snapshot (docs/source-import.md).
 *
 * KairoST is free software under the GNU General Public License, version 2 or (at your option)
 * any later version. See LICENSE.md.
 */

#ifndef UAE_TYPES_H
#define UAE_TYPES_H

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

typedef int8_t uae_s8;
typedef uint8_t uae_u8;
typedef int16_t uae_s16;
typedef uint16_t uae_u16;
typedef int32_t uae_s32;
typedef uint32_t uae_u32;
/* long long on every host, so the core's %lld and %llx formats fit. */
#ifndef uae_s64
typedef long long int uae_s64;
#endif
#ifndef uae_u64
typedef unsigned long long int uae_u64;
#endif

/* A 68000 bus address, 24 or 32 bits wide. */
typedef uae_u32 uaecptr;

/* The core's strings are 8-bit (WinUAE's TCHAR builds aside). */
typedef char uae_char;
#ifdef _WIN32
#include <tchar.h>
#else
typedef char TCHAR;
#endif
#ifndef _T
#define _T(x) x
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* Event times, in CPU cycles. */
typedef signed long long evt_t;

#endif /* UAE_TYPES_H */
