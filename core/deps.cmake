# The libraries Hatari needs, built from the pinned snapshots in third_party/: zlib and SDL2.
# Hatari uses SDL2 for its surfaces, key codes, timing and threads, and draws into the window
# surface of SDL's dummy video driver (core/kairo/main.c); KairoST supplies sound and input
# itself.

get_filename_component(THIRD_PARTY "${CMAKE_CURRENT_LIST_DIR}/../third_party" ABSOLUTE)

# zlib
set(ZLIB_DIR "${THIRD_PARTY}/zlib")
add_library(kairo_zlib STATIC
    "${ZLIB_DIR}/adler32.c" "${ZLIB_DIR}/compress.c" "${ZLIB_DIR}/crc32.c" "${ZLIB_DIR}/deflate.c"
    "${ZLIB_DIR}/gzclose.c" "${ZLIB_DIR}/gzlib.c" "${ZLIB_DIR}/gzread.c" "${ZLIB_DIR}/gzwrite.c"
    "${ZLIB_DIR}/infback.c" "${ZLIB_DIR}/inffast.c" "${ZLIB_DIR}/inflate.c" "${ZLIB_DIR}/inftrees.c"
    "${ZLIB_DIR}/trees.c" "${ZLIB_DIR}/uncompr.c" "${ZLIB_DIR}/zutil.c")
target_include_directories(kairo_zlib PUBLIC "${ZLIB_DIR}")
if(NOT WIN32)
    target_compile_definitions(kairo_zlib PRIVATE HAVE_UNISTD_H)
endif()

# SDL2, statically, with video (the dummy driver only, on Android) and the subsystems Hatari's
# code calls without starting them. No audio, joystick or other device drivers: Hatari starts
# none of them in KairoST. On Android, SDL is built without its bridge to its own Java classes
# (SDL_KAIRO_NO_JAVA; see the sdl-no-java change in tools/local_patches.py): the Kairo frontend
# owns the app's Java side and JNI_OnLoad.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
foreach(subsystem AUDIO RENDER JOYSTICK HAPTIC HIDAPI POWER SENSOR LOCALE MISC)
    set(SDL_${subsystem} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(subsystem VIDEO ATOMIC EVENTS THREADS TIMERS FILE LOADSO CPUINFO FILESYSTEM DUMMYVIDEO)
    set(SDL_${subsystem} ON CACHE BOOL "" FORCE)
endforeach()
# Hatari's window is the dummy driver's in-memory surface; no display, GL or Vulkan is needed.
foreach(feature OFFSCREEN OPENGL OPENGLES VULKAN X11 WAYLAND KMSDRM RPI VIVANTE DIRECTX WASAPI RENDER_D3D)
    set(SDL_${feature} OFF CACHE BOOL "" FORCE)
endforeach()
set(SDL_KAIRO_NO_JAVA ${ANDROID})
add_subdirectory("${THIRD_PARTY}/sdl2" sdl2 EXCLUDE_FROM_ALL)
if(ANDROID)
    target_compile_definitions(SDL2-static PRIVATE SDL_KAIRO_NO_JAVA)
endif()
