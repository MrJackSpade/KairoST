/*
 * KairoST: the function attributes Hatari's WinUAE CPU core names, in place of the
 * uae/attributes.h left out of the Hatari snapshot (docs/source-import.md).
 *
 * KairoST is free software under the GNU General Public License, version 2 or (at your option)
 * any later version. See LICENSE.md.
 */

#ifndef UAE_ATTRIBUTES_H
#define UAE_ATTRIBUTES_H

/* The C calling convention, where a compiler would otherwise pick another. */
#if defined(_WIN32)
#define uae_cdecl __cdecl
#elif defined(__GNUC__) && defined(__i386__)
#define uae_cdecl __attribute__((cdecl))
#else
#define uae_cdecl
#endif

/* Lets the compiler check printf-style arguments. */
#if defined(__GNUC__)
#define UAE_PRINTF_FORMAT(format, first) __attribute__((format(printf, format, first)))
#else
#define UAE_PRINTF_FORMAT(format, first)
#endif
#define UAE_WPRINTF_FORMAT(format, first)

#endif /* UAE_ATTRIBUTES_H */
