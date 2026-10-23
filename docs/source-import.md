# Source import policy

[← KairoST](../README.md)

This repository is independent. Its only Git remote is `origin`, pointing to this project's repository. The first-party Kairo frontend is pinned at `shared/` as a submodule. Do not add an `upstream` remote, fork relationship, other submodule, subtree sync, or automated upstream merge. Third-party source is copied in as pinned snapshots under `third_party/`.

## Hatari snapshot

`third_party/hatari/` holds [Hatari](https://www.hatari-emu.org/) 2.6.1.

| | |
| --- | --- |
| Release | `v2.6.1`, tag commit `6a8e26d69fb32d61efd398dc1ad9a77122400034` (2025-08-15) |
| Archive | <https://codeload.github.com/hatari/hatari/tar.gz/refs/tags/v2.6.1> |
| Archive SHA-256 | `75f09c8deddb511b8c1574b84c507743097db9abc6edbb078f942efa8677547e` |
| Imported | 2026-10-22 |

The archive was copied whole except for:

- `tests/`: Hatari's test suite, which includes prebuilt Atari test programs.
- `ci/` and `.gitlab-ci.yml`: Hatari's CI scripts.
- `src/cpu/uae/types.h`, `src/cpu/uae/attributes.h` and `src/cpu/uae/vm.h`: three small headers from FS-UAE whose notices say "GNU General Public License version 2" without "or later". KairoST's own headers in `core/kairo/uae/` define the same type and attribute names (`vm.h` declares nothing: Hatari's build uses none of its functions), so no file under a version-2-only notice is built. `tools/generate_cpu.py` builds Hatari's code generators with them too, and their output is unchanged.
- The nested `.gitignore` files, removed so every copied file is tracked.

Hatari generates its CPU core's instruction handlers at build time with two host tools (`build68k` and `gencpu`). `tools/generate_cpu.py` runs them and keeps their output in `core/generated/cpu/`, so no build needs a host compiler; `--check` regenerates the files and compares.

## EmuTOS

| | |
| --- | --- |
| Source (`third_party/emutos/`) | `VERSION_1_4`, tag commit `978e37569bff95841e42675d11fcc6799aad8483` (2025-06-07), <https://codeload.github.com/emutos/emutos/tar.gz/refs/tags/VERSION_1_4>, SHA-256 `654691678cd7617652dc30276642ddf2868658a4e4c3d68e8cc423975eaa95c3` |
| ROM image (`third_party/emutos-1024k/`) | The EmuTOS team's 1.4 release build `emutos-1024k-1.4.zip`, <https://sourceforge.net/projects/emutos/files/emutos/1.4/>, SHA-256 `dc9fbef6455a24ee8955cccd565588c718ba675fd54bc5a749003ac4bbd7f7e1` |
| Imported | 2026-10-22 |

The app ships `etos1024k.img`, the EmuTOS build for Hatari with every model and language. The source snapshot is the source of that build; it was copied whole except for `.github/` and the nested `.gitignore` files. The release archive was copied whole.

## Libraries Hatari needs

Hatari links zlib and SDL2. Each is a pinned release copied into `third_party/`, built by `core/deps.cmake`.

| Library | Release | Archive | Archive SHA-256 | Released |
| --- | --- | --- | --- | --- |
| zlib (`zlib/`) | 1.3.2 | GitHub release `v1.3.2` of `madler/zlib`, `zlib-1.3.2.tar.gz` | `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16` | 2026-02-17 |
| SDL2 (`sdl2/`) | 2.32.10 | GitHub release `release-2.32.10` of `libsdl-org/SDL`, `SDL2-2.32.10.tar.gz` | `5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165` | 2025-09-01 |

Both were imported on 2026-10-22, from the same archives as KairoMSX's (the hashes match). Each archive was copied whole except for files no KairoST build uses:

- zlib: `amiga/`, `contrib/`, `doc/`, `msdos/`, `os400/`, `qnx/`, `test/`, `watcom/` and `win32/`.
- SDL2: the IDE projects (`VisualC*/`, `Xcode*/`), `acinclude/`, `android-project*/`, `docs/`, `mingw/`, `test/`, `visualtest/` and `wayland-protocols/`.
- Nested Git files (`.gitignore`, `.git-hash` and similar), so every copied file is tracked.

## Local changes

`tools/local_patches.py` makes these changes to the snapshots and checks they are in place (`--check`). Each replaces text that occurs exactly once.

| Change | File | What and why |
| --- | --- | --- |
| `hatari-fdc-motor` | `hatari/src/fdc.c`, `includes/fdc.h` | Adds `FDC_MotorIsOn()`, the WD1772's motor-on status bit, which Hatari keeps to itself. KairoST runs a game's boot at full speed until its first floppy load is over: the motor has run and stopped. |
| `hatari-restart` | `hatari/src/cpu/memory.c`, `hatari/src/debug/symbols.c`, `hatari/src/inffile.c`, `includes/inffile.h` | Clears state Hatari's teardown leaves set: `IOmemory`, which points into the ROM memory `memory_uninit` frees; the debugger's symbol lists, which `Symbols_FreeAll` frees; and the autostart program and resolution of the virtual `DESKTOP.INF`, which nothing clears (`INF_Reset()`). Hatari tears down only when its process exits; KairoST runs one machine after another in its process, and the next machine read them, or would have started the previous game's program. |
| `hatari-snapshot-failed` | `hatari/src/memorySnapShot.c`, `includes/memorySnapShot.h` | Adds `MemorySnapShot_Failed()`, whether the last save or restore failed, which Hatari otherwise keeps to itself. Hatari does both at the end of a CPU instruction, after the request returns. |
| `sdl-android-sources` | `sdl2/CMakeLists.txt` | Leaves SDL's bridge to its own Java classes (`src/core/android`) out of Android builds with `SDL_KAIRO_NO_JAVA`, and adds the NDK's `cpufeatures` only to 32-bit ARM builds, the only ones whose code calls it. |
| `sdl-android-video` | `sdl2/CMakeLists.txt` | Without SDL's Java side, Android builds have only SDL's dummy video driver, whose window surface Hatari draws into. |
| `sdl-no-java` | `sdl2/src/SDL.c`, `stdlib/SDL_getenv.c`, `file/SDL_rwops.c`, `thread/pthread/SDL_systhread.c`, `video/SDL_video.c`, `joystick/SDL_gamecontroller.c` | With `SDL_KAIRO_NO_JAVA`, the rest of SDL takes its non-Android paths where it would call that bridge, as in KairoMSX. In a Kairo app the frontend owns the Java side and `JNI_OnLoad`. |

The rest of KairoST's replacements need no change to Hatari. `core/CMakeLists.txt` builds Hatari without its `main.c`, `audio.c` and halt dialog (`gui-sdl/dlgHalt.c`) and compiles KairoST's files in their place (`core/kairo/`): Hatari runs on its own thread and hands each frame to the host, the host takes the mixed sound, and a CPU halt ends the machine with an error. Hatari runs headless on SDL's dummy video driver, which its own `screen.c` supports. A game that is a program runs as Hatari runs one given on its command line: its folder becomes the GEMDOS hard disk C and TOS starts it from the virtual `DESKTOP.INF`; a floppy that does not boot and holds a single program starts it the same way from drive A. Alert boxes go through Hatari's `ALERT_HOOKS`, the hooks its macOS build uses, to `core/kairo/alert_hooks.c`.
