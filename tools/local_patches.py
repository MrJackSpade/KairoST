#!/usr/bin/env python3
"""Applies KairoST's local changes to the third-party snapshots in third_party/.

Usage: local_patches.py [--check]

Each change replaces text that must occur exactly once in its file. A change already applied is
skipped, so the script can run on a fresh import or on the committed tree; --check fails unless
every change is applied. docs/source-import.md describes each change.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'third_party'

# Hatari saves and restores the machine at the end of a CPU instruction, after the request
# (MemorySnapShot_Capture/Restore), and keeps whether it failed to itself. KairoST asks for it.
HATARI_SNAPSHOT_FAILED = [
    ('hatari/src/includes/memorySnapShot.h',
     'extern void MemorySnapShot_Restore_Do(void);\n',
     'extern void MemorySnapShot_Restore_Do(void);\n'
     'extern bool MemorySnapShot_Failed(void); /* KairoST: whether the last save or restore failed */\n'),
    ('hatari/src/memorySnapShot.c',
     'void MemorySnapShot_Restore_Do(void)\n{\n',
     '/* KairoST: whether the last save or restore failed. */\n'
     'bool MemorySnapShot_Failed(void)\n{\n\treturn bCaptureError;\n}\n\n'
     'void MemorySnapShot_Restore_Do(void)\n{\n'),
]

# EmuTOS 1.4 (the 1024k image KairoST ships) holds its welcome screen for 3 seconds on a first boot
# (INITINFO_DURATION, bios/initinfo.c), before it boots the floppy. With Hatari's fast boot (KairoST's
# Skip memory check), the hold is skipped: both places initinfo() computes its end time
# (addi.l #600,d4: 3 s of 200 Hz ticks) add 0 instead, as INITINFO_DURATION 0 would build it. Only an
# image with exactly those instructions there is patched, as Hatari checks its own TOS patches.
HATARI_EMUTOS_WELCOME = [
    ('hatari/src/tos.c',
     '\nstatic void TOS_FixRom(uint32_t *logopatch_addr)\n',
     '\n/* KairoST: EmuTOS 1.4 1024k\'s welcome screen hold, skipped with fast boot (tools/local_patches.py). */\n'
     'static void TOS_FixEmuTosWelcome(void)\n{\n'
     '\tstatic const uint32_t holds[] = { 0xE0D810, 0xE0D8E8 };\t/* addi.l #600,d4 */\n'
     '\tint i;\n\n'
     '\tfor (i = 0; i < 2; i++)\n'
     '\t\tif (STMemory_ReadWord(holds[i]) != 0x0684 || STMemory_ReadLong(holds[i] + 2) != 600)\n'
     '\t\t\treturn;\t/* another EmuTOS build */\n'
     '\tfor (i = 0; i < 2; i++)\n'
     '\t\tmemset(&RomMem[holds[i] + 2], 0, 4);\n'
     '\tLog_Printf(LOG_DEBUG, "Skipped the EmuTOS welcome screen hold.\\n");\n}\n\n'
     'static void TOS_FixRom(uint32_t *logopatch_addr)\n'),
    ('hatari/src/tos.c',
     '\t\tLog_Printf(LOG_DEBUG, "Skipped TOS patches.\\n");\n\t}\n',
     '\t\tLog_Printf(LOG_DEBUG, "Skipped TOS patches.\\n");\n\t}\n'
     '\tif (bIsEmuTOS && !bRamTosImage && ConfigureParams.System.bFastBoot)\n'
     '\t\tTOS_FixEmuTosWelcome();\t/* KairoST */\n'),
]

# The dummy video driver's window surface, which Hatari draws into (KairoST runs headless), in the
# byte order the frame host shows (R, G, B, A in memory: SDL_PIXELFORMAT_ABGR8888) instead of XRGB:
# Hatari maps its colours through the surface's format, so the picture is the same, and the machine
# hands it on without converting every pixel of every frame (KairoST#23).
SDL_DUMMY_RGBA = [
    ('sdl2/src/video/dummy/SDL_nullframebuffer.c',
     '    const Uint32 surface_format = SDL_PIXELFORMAT_RGB888;\n',
     '    const Uint32 surface_format = SDL_PIXELFORMAT_ABGR8888; /* KairoST: R, G, B, A bytes, as the frame host shows them */\n'),
]

# A track an STX image records with no data (an unformatted track some copy protections leave)
# has size 0, so one revolution took 0 cycles and the index pulse setup divided by it: a crash on
# x86, a wrong pulse time on ARM. The drive still spins it in one revolution, as a missing track.
HATARI_STX_EMPTY_TRACK = [
    ('hatari/src/floppy_stx.c',
     '\tif ( pStxTrack == NULL )\n'
     '\t\tTrackSize =  FDC_TRACK_BYTES_STANDARD;\t\t\t/* Use a standard track length is track is not available */\n',
     '\tif ( pStxTrack == NULL || ( pStxTrack->pTrackImageData ? pStxTrack->TrackImageSize : pStxTrack->MFMSize ) == 0 )\n'
     '\t\tTrackSize =  FDC_TRACK_BYTES_STANDARD;\t\t\t/* Use a standard track length is track is not available */'
     '\t/* KairoST: or empty */\n'),
]

# The WD1772's motor-on status bit: set when a command starts and cleared after the drive has
# idled for nine revolutions. KairoST runs a game's boot at full speed until its first floppy
# load is over (kairo/main.c), and reads it each frame; Hatari keeps it to itself.
HATARI_FDC_MOTOR = [
    ('hatari/src/includes/fdc.h',
     'extern void\tFDC_Drive_Set_BusyLed ( uint8_t SR );\n',
     'extern void\tFDC_Drive_Set_BusyLed ( uint8_t SR );\n'
     'extern bool\tFDC_MotorIsOn ( void );\t/* KairoST: the motor-on status bit */\n'),
    ('hatari/src/fdc.c',
     'void\tFDC_Drive_Set_BusyLed ( uint8_t STR )\n{\n',
     '/* KairoST: whether the motor-on status bit is set. */\n'
     'bool\tFDC_MotorIsOn ( void )\n{\n\treturn ( FDC.STR & FDC_STR_BIT_MOTOR_ON ) != 0;\n}\n\n\n'
     'void\tFDC_Drive_Set_BusyLed ( uint8_t STR )\n{\n'),
]

# SDL's Android sources: its bridge to its own Java classes, left out with SDL_KAIRO_NO_JAVA (see
# SDL_NO_JAVA), and the NDK's cpufeatures, which SDL only calls for 32-bit ARM's NEON check
# (src/cpuinfo/SDL_cpuinfo.c), so arm64 builds leave it out.
SDL_ANDROID_SOURCES = [
    ('sdl2/CMakeLists.txt',
     '''  list(APPEND SOURCE_FILES ${ANDROID_CORE_SOURCES} ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)
  set_property(SOURCE "${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c" APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-declaration-after-statement")
''',
     '''  if(NOT SDL_KAIRO_NO_JAVA) # KairoST: no bridge to SDL's Java classes
    list(APPEND SOURCE_FILES ${ANDROID_CORE_SOURCES})
  endif()
  # KairoST: only 32-bit ARM's NEON check uses cpufeatures (SDL_cpuinfo.c).
  if(SDL_CPU_ARM32)
    list(APPEND SOURCE_FILES ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)
    set_property(SOURCE "${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c" APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-declaration-after-statement")
  endif()
'''),
    ('sdl2/CMakeLists.txt',
     '''if(ANDROID)
  target_include_directories(sdl-build-options INTERFACE "${ANDROID_NDK}/sources/android/cpufeatures")
endif()''',
     '''if(ANDROID AND SDL_CPU_ARM32) # KairoST: see the cpufeatures sources above
  target_include_directories(sdl-build-options INTERFACE "${ANDROID_NDK}/sources/android/cpufeatures")
endif()'''),
]

# Hatari draws into the window surface of SDL's dummy video driver (core/kairo/main.c). SDL's
# Android video driver needs SDL's Java side; without it (SDL_KAIRO_NO_JAVA) an Android build has
# the dummy driver only, as other platforms without a display do.
SDL_ANDROID_VIDEO = [
    ('sdl2/CMakeLists.txt',
     '''  if(SDL_VIDEO)
    set(SDL_VIDEO_DRIVER_ANDROID 1)
''',
     '''  if(SDL_VIDEO AND NOT SDL_KAIRO_NO_JAVA) # KairoST: the dummy video driver only
    set(SDL_VIDEO_DRIVER_ANDROID 1)
'''),
]

# SDL's Android port is a bridge to its own Java classes (org.libsdl.app.SDLActivity), with its own
# JNI_OnLoad. In KairoST the Kairo frontend owns the Java side and the library's JNI_OnLoad. With
# SDL_KAIRO_NO_JAVA (core/deps.cmake) SDL is built without that bridge (src/core/android), and the
# few calls into it from the rest of SDL take their non-Android paths: files open with stdio,
# threads and getenv do not attach to Java, and there is no game controller mapping file in app
# storage, minimize-on-focus-loss setting or tablet check.
SDL_NO_JAVA = [
    ('sdl2/src/stdlib/SDL_getenv.c',
     '#if defined(__ANDROID__)\n    /* Make sure variables from the application manifest are available */\n',
     '#if defined(__ANDROID__) && !defined(SDL_KAIRO_NO_JAVA) /* KairoST: no SDL Java side */\n'
     '    /* Make sure variables from the application manifest are available */\n'),
    ('sdl2/src/file/SDL_rwops.c',
     '    }\n#if defined(__ANDROID__)\n#ifdef HAVE_STDIO_H\n    /* Try to open the file on the filesystem first */\n',
     '    }\n#if defined(__ANDROID__) && !defined(SDL_KAIRO_NO_JAVA) /* KairoST: stdio only */\n'
     '#ifdef HAVE_STDIO_H\n    /* Try to open the file on the filesystem first */\n'),
    ('sdl2/src/thread/pthread/SDL_systhread.c',
     'static void *RunThread(void *data)\n{\n#ifdef __ANDROID__\n',
     'static void *RunThread(void *data)\n{\n#if defined(__ANDROID__) && !defined(SDL_KAIRO_NO_JAVA) /* KairoST */\n'),
    ('sdl2/src/video/SDL_video.c',
     '#ifdef __ANDROID__\n    {\n        extern SDL_bool Android_JNI_ShouldMinimizeOnFocusLoss(void);\n',
     '#if defined(__ANDROID__) && !defined(SDL_KAIRO_NO_JAVA) /* KairoST */\n'
     '    {\n        extern SDL_bool Android_JNI_ShouldMinimizeOnFocusLoss(void);\n'),
    ('sdl2/src/SDL.c',
     '#if defined(__ANDROID__)\n    extern SDL_bool SDL_IsAndroidTablet(void);\n',
     '#if defined(__ANDROID__) && !defined(SDL_KAIRO_NO_JAVA) /* KairoST */\n    extern SDL_bool SDL_IsAndroidTablet(void);\n'),
    ('sdl2/src/joystick/SDL_gamecontroller.c',
     '#if defined(__ANDROID__)\n#include "SDL_system.h"\n#endif\n',
     '#if defined(__ANDROID__)\n#include "SDL_system.h"\n#endif\n'
     '#ifdef SDL_KAIRO_NO_JAVA /* KairoST: the API level from the NDK instead of SDL\'s Java side */\n'
     '#include <android/api-level.h>\n'
     '#define SDL_GetAndroidSDKVersion() android_get_device_api_level()\n'
     '#endif\n'),
    ('sdl2/src/joystick/SDL_gamecontroller.c',
     '#if defined(__ANDROID__)\n    return SDL_snprintf(path, size, "%s/controller_map.txt", SDL_AndroidGetInternalStoragePath()) < size;\n',
     '#if defined(__ANDROID__) && !defined(SDL_KAIRO_NO_JAVA) /* KairoST */\n'
     '    return SDL_snprintf(path, size, "%s/controller_map.txt", SDL_AndroidGetInternalStoragePath()) < size;\n'),
]

PATCHES = {
    'hatari-emutos-welcome': HATARI_EMUTOS_WELCOME,
    'hatari-fdc-motor': HATARI_FDC_MOTOR,
    'hatari-snapshot-failed': HATARI_SNAPSHOT_FAILED,
    'hatari-stx-empty-track': HATARI_STX_EMPTY_TRACK,
    'sdl-android-sources': SDL_ANDROID_SOURCES,
    'sdl-android-video': SDL_ANDROID_VIDEO,
    'sdl-dummy-rgba': SDL_DUMMY_RGBA,
    'sdl-no-java': SDL_NO_JAVA,
}


def main():
    check = sys.argv[1:] == ['--check']
    missing = []
    for name, changes in PATCHES.items():
        for rel, old, new in changes:
            path = ROOT / rel
            with open(path, encoding='utf-8', newline='') as f:
                text = f.read()
            if text.count(new) == 1:
                continue
            if check:
                missing.append(f'{name}: {rel}')
                continue
            if text.count(old) != 1:
                raise SystemExit(f'patch {name}: {rel} does not contain the text to replace exactly once')
            with open(path, 'w', encoding='utf-8', newline='') as f:
                f.write(text.replace(old, new))
            print(f'applied {name} to {rel}')
    if missing:
        raise SystemExit('not applied: ' + ', '.join(missing))


if __name__ == '__main__':
    main()
