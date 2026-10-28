# Licensing and credits

[← KairoST](../README.md)

KairoST first-party code is [GPL-2.0-or-later](../LICENSE.md). The pinned [Kairo frontend](https://github.com/MrJackSpade/Kairo) is also GPL-2.0-or-later. Third-party components keep their own notices.

## Hatari

The Atari ST emulation comes from [Hatari](https://www.hatari-emu.org/) 2.6.1. Its `readme.txt` licenses it under the GNU General Public License, version 2 or (at your option) any later version, with an exception for combining it with LGPL libraries and the IPF support library (which KairoST does not build). The GPL version 2 text is in `third_party/hatari/gpl.txt`; contributors are listed in `doc/authors.txt`.

Hatari carries code from other projects under the same terms: its CPU core from WinUAE, DSP code from ARAnyM, MMU code from WinUAE and Previous, and IDE code from QEMU. Debian's copyright record for Hatari reads the whole tree as GPL-2+, with these files under other terms:

| Files | License |
| --- | --- |
| `src/includes/unzip.h`, `src/unzip.c` | zlib license (Gilles Vollant's minizip) |
| `src/cfgopts.c` | Public domain (Jeffry J. Brickley) |
| `src/debug/a.out.h` | Public domain (Guido Flohr) |
| `doc/toc.js` | MIT (not built) |

Three small FS-UAE headers in the CPU core (`src/cpu/uae/types.h`, `attributes.h`, `vm.h`) say "GNU General Public License version 2" without "or later". KairoST leaves them out of its snapshot and builds its own headers in their place (`core/kairo/uae/`; see [source import](source-import.md)), so every file it builds is GPL-2.0-or-later or more permissive.

`cfgopts.c` is dedicated to the public domain; its author's "please do not charge him" is a request, not a condition of the dedication.

### The shader runtime

Since Hatari is GPL-2.0-or-later, KairoST links the shared RetroArch Slang shader runtime, which uses librashader under its GPL-3.0-only option and includes Rust crates licensed only under Apache-2.0, and the combined app is distributed under GPLv3 terms, as the other Kairo apps are.

### KairoST's build of Hatari

KairoST builds Hatari's emulation code with its own replacements for the parts that run the program, pace it, play its sound and ask the user questions (`core/kairo/`, GPL-2.0-or-later). It builds none of Hatari's tools, its macOS or Windows interfaces, or its optional PNG, readline, PortMidi, IPF, X11, udev or Capstone support.

## EmuTOS

`third_party/emutos-1024k/` holds the [EmuTOS](https://emutos.sourceforge.io/) 1.4 release image the app ships, and `third_party/emutos/` its source. EmuTOS is a free TOS replacement under the GNU General Public License (`doc/license.txt`), built on the GEM sources Caldera released under the GPL. It contains no Atari TOS code. It runs inside the emulated ST as the machine's system software; it is not linked into the app.

## Notices in the app

`shared/tools/notices.py .` writes the app's `THIRD_PARTY_NOTICES.txt` (About → Licenses) from `tools/notices_sources.py`, which lists the license files and source notices above: Hatari's license statement and GPL text, the notices of the files under other terms, EmuTOS, zlib and SDL2. `tools/notices.py --check` fails when the file is out of date.

## Libraries Hatari needs

| Library | Location | License |
| --- | --- | --- |
| zlib 1.3.2 | `third_party/zlib/` | zlib license (`LICENSE`) |
| SDL2 2.32.10 | `third_party/sdl2/` | zlib license (`LICENSE.txt`) |

Both permit combination with GPL code.

## Shared frontend

<!-- kairo:shared-frontend-license -->
The shared RetroArch Slang shader runtime uses librashader under its GPL-3.0-only option, so a shader-enabled KairoST build is distributed under GPLv3 terms using first-party "or later" permission. See [shared licensing](../shared/docs/licensing.md).
<!-- /kairo:shared-frontend-license -->

## Firmware and games

Apart from EmuTOS, KairoST does not include Atari ST system firmware (TOS), operating systems, or games. Users supply any TOS images and games they need; the app copies an imported TOS image to its private storage, one for each model.
