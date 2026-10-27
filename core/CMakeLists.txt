# The Atari ST machine on Hatari (third_party/hatari), driven one frame at a time, for the Android
# app and for host tests.
cmake_minimum_required(VERSION 3.22.1)
project(kairost_core LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# The emulator runs in real time only optimized, so the core is optimized in debug builds too.
add_compile_options(-O3)

# The device build of the core uses a profile of the device playing games (KairoST#9,
# tools/perf/pgo-train.ps1, core/pgo). The profile describes this exact code as the device compiles
# it, so a training build (KAIROST_PGO_GENERATE), the host or another compiler turns it off with
# -DKAIRO_PGO=OFF (the app's gradle property pgo=false).
if(ANDROID)
    set(KAIROST_PGO_DEFAULT ON)
else()
    set(KAIROST_PGO_DEFAULT OFF)
endif()
option(KAIRO_PGO "Build the core with its recorded profile (core/pgo)" ${KAIROST_PGO_DEFAULT})
option(KAIROST_PGO_GENERATE "Build the core to record a profile (tools/perf/pgo-train.ps1)" OFF)
# Until tools/perf/pgo-train.ps1 first records core/pgo, there is no profile to build with.
if(KAIRO_PGO AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/pgo/profile.json")
    if(KAIROST_PGO_GENERATE)
        message(FATAL_ERROR "The recorded profile is for the ordinary core: add -DKAIRO_PGO=OFF to a KAIROST_PGO_GENERATE build")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../shared/cmake/KairoPgo.cmake")
    kairo_use_profile("${CMAKE_CURRENT_LIST_DIR}/pgo/arm64.proftext" "${CMAKE_CURRENT_LIST_DIR}/pgo/profile.json"
        "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
if(KAIROST_PGO_GENERATE)
    add_compile_options(-fprofile-generate)
    add_link_options(-fprofile-generate)
endif()

include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckStructHasMember)
include("${CMAKE_CURRENT_LIST_DIR}/deps.cmake")

set(HATARI "${THIRD_PARTY}/hatari/src")

# Hatari's config.h, from its own template and the checks its CMakeLists.txt makes. Features
# KairoST does not build (PNG screenshots, readline, PortMidi, IPF disks, X11, udev, Capstone)
# stay off; so does tracing, which Hatari notes costs speed.
check_include_files(byteswap.h HAVE_BYTESWAP_H)
check_include_files(termios.h HAVE_TERMIOS_H)
check_include_files(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(sys/times.h HAVE_SYS_TIMES_H)
check_include_files(utime.h HAVE_UTIME_H)
check_include_files(sys/utime.h HAVE_SYS_UTIME_H)
check_symbol_exists(bswap_16 "byteswap.h" HAVE_BSWAP_16)
check_symbol_exists(bswap_32 "byteswap.h" HAVE_BSWAP_32)
check_symbol_exists(cfmakeraw "termios.h" HAVE_CFMAKERAW)
check_symbol_exists(tcsetattr "termios.h" HAVE_TCSETATTR)
check_symbol_exists(setenv "stdlib.h" HAVE_SETENV)
check_symbol_exists(select "sys/select.h" HAVE_SELECT)
check_symbol_exists(gettimeofday "sys/time.h" HAVE_GETTIMEOFDAY)
check_symbol_exists(nanosleep "time.h" HAVE_NANOSLEEP)
check_symbol_exists(alphasort "dirent.h" HAVE_ALPHASORT)
check_symbol_exists(scandir "dirent.h" HAVE_SCANDIR)
check_symbol_exists(statvfs "sys/statvfs.h" HAVE_STATVFS)
check_symbol_exists(fseeko "stdio.h" HAVE_FSEEKO)
check_symbol_exists(ftello "stdio.h" HAVE_FTELLO)
check_symbol_exists(flock "sys/file.h" HAVE_FLOCK)
check_struct_has_member("struct dirent" d_type dirent.h HAVE_DIRENT_D_TYPE)
set(HAVE_LIBZ 1)
set(HAVE_ZLIB_H 1)
set(ENABLE_DSP_EMU 1)
set(BIN2DATADIR ".")
configure_file("${THIRD_PARTY}/hatari/cmake/config-cmake.h" "${CMAKE_CURRENT_BINARY_DIR}/hatari-config/config.h")

# Hatari's sources, as its CMakeLists.txt files list them, without main.c, audio.c and the halt
# dialog, which KairoST replaces (kairo/), and without the tools, the macOS and Windows GUIs and
# the AVI recorder's PNG path, which KairoST does not build.
set(HATARI_SOURCES
    acia.c avi_record.c bios.c blitter.c cart.c cfgopts.c
    clocks_timings.c configuration.c options.c change.c control.c
    cycInt.c cycles.c dialog.c dmaSnd.c fdc.c file.c floppy.c
    floppy_ipf.c floppy_stx.c gemdos.c hdc.c ide.c ikbd.c
    ioMem.c ioMemTabST.c ioMemTabSTE.c ioMemTabTT.c ioMemTabFalcon.c joy.c
    keymap.c m68000.c midi.c memorySnapShot.c mfp.c nf_scsidrv.c
    ncr5380.c paths.c psg.c printer.c resolution.c rs232.c reset.c rtc.c
    scandir.c scc.c scu_vme.c stMemory.c screen.c screenConvert.c screenSnapShot.c
    shortcut.c sound.c spec512.c statusbar.c str.c tos.c utils.c
    vdi.c inffile.c video.c wavFormat.c xbios.c ymFormat.c lilo.c unzip.c
    # The disk image library Hatari shares with its hmsa tool.
    createBlankImage.c dim.c msa.c st.c zip.c
    debug/log.c debug/debugui.c debug/breakcond.c debug/debugcpu.c debug/debugInfo.c
    debug/debugdsp.c debug/evaluate.c debug/history.c debug/symbols.c debug/vars.c
    debug/profile.c debug/profilecpu.c debug/profiledsp.c debug/natfeats.c debug/console.c
    debug/68kDisass.c
    falcon/crossbar.c falcon/microphone.c falcon/nvram.c falcon/videl.c falcon/dsp.c
    falcon/dsp_core.c falcon/dsp_cpu.c falcon/dsp_disasm.c
    gui-sdl/dlgAbout.c gui-sdl/dlgAlert.c gui-sdl/dlgCpu.c gui-sdl/dlgDevice.c
    gui-sdl/dlgFileSelect.c gui-sdl/dlgFloppy.c gui-sdl/dlgHardDisk.c gui-sdl/dlgJoystick.c
    gui-sdl/dlgKeyboard.c gui-sdl/dlgMain.c gui-sdl/dlgMemory.c gui-sdl/dlgNewDisk.c
    gui-sdl/dlgRom.c gui-sdl/dlgScreen.c gui-sdl/dlgSound.c gui-sdl/dlgSystem.c gui-sdl/sdlgui.c)
if(WIN32)
    # Hatari's Windows console, for the host tests.
    list(APPEND HATARI_SOURCES gui-win/opencon.c)
endif()
list(TRANSFORM HATARI_SOURCES PREPEND "${HATARI}/")

# The WinUAE CPU core, with the instruction handlers tools/generate_cpu.py generates.
set(CPU "${HATARI}/cpu")
set(CPU_SOURCES
    cpummu.c cpummu030.c debug.c disasm.c newcpu_common.c newcpu.c readcpu.c writelog.c fpp.c
    fpp_native.c fpp_softfloat.c softfloat/softfloat.c softfloat/softfloat_decimal.c
    softfloat/softfloat_fpsp.c machdep/m68k.c custom.c events.c memory.c hatari-glue.c)
list(TRANSFORM CPU_SOURCES PREPEND "${CPU}/")
set(GENERATED_CPU "${CMAKE_CURRENT_LIST_DIR}/generated/cpu")
set(GENERATED_CPU_SOURCES
    cpudefs.c cpustbl.c cpuemu_0.c cpuemu_11.c cpuemu_13.c cpuemu_20.c cpuemu_21.c cpuemu_22.c
    cpuemu_23.c cpuemu_24.c cpuemu_31.c cpuemu_32.c cpuemu_33.c cpuemu_34.c cpuemu_35.c
    cpuemu_40.c cpuemu_50.c)
list(TRANSFORM GENERATED_CPU_SOURCES PREPEND "${GENERATED_CPU}/")

# Hatari, as a library of its own: each machine loads a fresh copy of it (hatari_library.h), so
# Hatari's process-wide variables start from their initial values for every machine. Its only
# exported symbol is KairoSt_Api (kairo/kairo_st.h); SDL and zlib are built into it.
add_library(kairost_hatari SHARED ${HATARI_SOURCES} ${CPU_SOURCES} ${GENERATED_CPU_SOURCES}
    kairo/main.c kairo/audio.c kairo/dlgHalt.c kairo/alert_hooks.c)
set_target_properties(kairost_hatari PROPERTIES C_STANDARD 99 C_EXTENSIONS ON C_VISIBILITY_PRESET hidden
    PREFIX "lib")
set_target_properties(kairo_zlib SDL2-static PROPERTIES POSITION_INDEPENDENT_CODE ON)
# KairoST's replacements come first: kairo/uae stands in for the uae/ headers left out of the
# snapshot. config.h is the one generated above. Alerts go to KairoST's hooks (kairo/alert_hooks.c).
target_include_directories(kairost_hatari PRIVATE "${CMAKE_CURRENT_LIST_DIR}/kairo"
    "${CMAKE_CURRENT_BINARY_DIR}/hatari-config" "${HATARI}/includes" "${HATARI}/debug" "${HATARI}/falcon"
    "${HATARI}/gui-sdl" "${HATARI}" "${CPU}" "${CPU}/softfloat" "${GENERATED_CPU}")
target_compile_definitions(kairost_hatari PRIVATE ALERT_HOOKS=1 CONFDIR="." _FILE_OFFSET_BITS=64)
# Hatari's flags for its CPU core (src/cpu/CMakeLists.txt): it relies on wrapping signed
# arithmetic. char is signed, as on the x86 hosts Hatari is mostly developed on (ARM's is
# unsigned).
target_compile_options(kairost_hatari PRIVATE -fwrapv -fsigned-char -Wno-write-strings
    -Wno-unused-variable -Wno-unused-function -Wno-unused-label -Wno-sign-compare
    -Wno-missing-braces -Wno-unused-but-set-variable)
target_link_libraries(kairost_hatari PRIVATE kairo_zlib SDL2-static)
if(ANDROID)
    # ThinLTO across Hatari's files, and scheduling for the Cortex-A55 cores of the handhelds it runs
    # on (tuning only: code still runs on any ARMv8-A core). RG DS, profile-guided build: ThinLTO
    # +9-15%, the A55 tuning a few percent more (KairoST#10, #12).
    target_compile_options(kairost_hatari PRIVATE -flto=thin -mtune=cortex-a55)
    target_link_options(kairost_hatari PRIVATE -flto=thin)
    target_link_libraries(kairost_hatari PRIVATE log)
    target_link_options(kairost_hatari PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/kairo/hatari.map")
    set_target_properties(kairost_hatari PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/kairo/hatari.map")
elseif(WIN32)
    # The DLL carries its own runtime, as boot_test does.
    target_link_options(kairost_hatari PRIVATE -static)
endif()

# The frame-driven machine (st_machine.h), which loads the Hatari library by its file name.
add_library(kairost_core STATIC st_machine.cpp hatari_library.cpp)
target_include_directories(kairost_core PUBLIC "${CMAKE_CURRENT_LIST_DIR}" PRIVATE "${CMAKE_CURRENT_LIST_DIR}/kairo"
    "${CMAKE_CURRENT_LIST_DIR}/../shared/frontend/src/main/cpp")
# The app has the shared state paths in its machine library; these executables compile them.
set(KAIRO_STATE_PATHS "${CMAKE_CURRENT_LIST_DIR}/../shared/frontend/src/main/cpp/kairo/state_paths.cpp")
target_compile_definitions(kairost_core PRIVATE KAIROST_HATARI_FILE="$<TARGET_FILE_NAME:kairost_hatari>")
add_dependencies(kairost_core kairost_hatari)
if(ANDROID)
    target_link_libraries(kairost_core PUBLIC dl log)
endif()

# The measurement tool (test/game_profile.cpp) builds for the host and, with KAIROST_TOOLS=ON and
# the NDK, for the device, beside the Hatari library it loads.
option(KAIROST_TOOLS "Build the measurement tool on Android" OFF)
if(NOT ANDROID OR KAIROST_TOOLS)
    add_executable(game_profile test/game_profile.cpp "${KAIRO_STATE_PATHS}")
    target_link_libraries(game_profile PRIVATE kairost_core)
    if(WIN32)
        target_link_options(game_profile PRIVATE -static)
    endif()
endif()

if(NOT ANDROID)
    add_executable(boot_test test/boot_test.cpp "${KAIRO_STATE_PATHS}")
    target_link_libraries(boot_test PRIVATE kairost_core)
    if(WIN32)
        target_link_options(boot_test PRIVATE -static)
    endif()
endif()
