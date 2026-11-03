# The libraries Hatari needs: zlib from the shared frontend, and SDL2 built from the pinned snapshot
# in third_party/.
# Hatari uses SDL2 for its surfaces, key codes, timing and threads, and draws into the window
# surface of SDL's dummy video driver (core/kairo/main.c); KairoST supplies sound and input
# itself.

get_filename_component(THIRD_PARTY "${CMAKE_CURRENT_LIST_DIR}/../third_party" ABSOLUTE)

# zlib, from the shared frontend (kairo_zlib).
include("${CMAKE_CURRENT_LIST_DIR}/../shared/cmake/KairoLibraries.cmake")

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
