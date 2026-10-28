"""KairoST's third-party notices: the sources shared/tools/notices.py writes kairost/src/main/assets/THIRD_PARTY_NOTICES.txt from.
Run it again after an import."""
from notices import excerpt, text

OUT = 'kairost/src/main/assets/THIRD_PARTY_NOTICES.txt'

HEADER = '''KairoST third-party notices
These notices apply to the emulator code, system software and libraries in this package. KairoST
emulates the Atari ST with Hatari 2.6.1 and starts games with EmuTOS 1.4; KairoST is not
affiliated with the Hatari or EmuTOS teams, or with Atari.'''

HATARI_TERMS = '''Hatari, including its CPU core from WinUAE, its DSP and MMU code from ARAnyM and Previous and its
IDE code from QEMU, is distributed under the GNU General Public License, version 2 or (at your
option) any later version. Its authors are listed in doc/authors.txt of its source. A few of its
files carry other terms, given below. Its license statement and the GPL version 2 text follow.'''

EMUTOS_TERMS = '''EmuTOS is distributed under the GNU General Public License; the GPL version 2 text is above. It
runs inside the emulated ST as its system software. Its source is in third_party/emutos of
KairoST's source, and its authors are listed in doc/authors.txt there.'''

# (title, source, [text]) in the order shown.
SECTIONS = [
    ('Hatari 2.6.1', 'https://www.hatari-emu.org/, third_party/hatari',
     [HATARI_TERMS, excerpt('hatari/readme.txt', 'This program is free software', 'for Hatari and the licenses of the other code concerned.'),
      text('hatari/gpl.txt')]),
    ('Hatari: unzip.c and unzip.h by Gilles Vollant (zlib license)', 'third_party/hatari/src/includes/unzip.h',
     [excerpt('hatari/src/includes/unzip.h', 'Copyright (C) 1998 Gilles Vollant', 'This notice may not be removed')]),
    ('Hatari: cfgopts.c by Jeffry J. Brickley (public domain)', 'third_party/hatari/src/cfgopts.c',
     [excerpt('hatari/src/cfgopts.c', 'Like the original code from which this was taken', 'please do not charge him')]),
    ('Hatari: a.out.h by Guido Flohr (public domain)', 'third_party/hatari/src/debug/a.out.h',
     [excerpt('hatari/src/debug/a.out.h', 'a.out.h - Definitions and declarations', 'This file is in the public domain.')]),
    ('EmuTOS 1.4', 'https://emutos.sourceforge.io/, third_party/emutos and third_party/emutos-1024k',
     [EMUTOS_TERMS]),
    ('zlib 1.3.2', 'https://zlib.net/, third_party/zlib', [text('zlib/LICENSE')]),
    ('SDL2 2.32.10', 'https://www.libsdl.org/, third_party/sdl2', [text('sdl2/LICENSE.txt')]),
]
