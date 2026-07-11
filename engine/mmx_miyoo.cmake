# mmx_miyoo.cmake — Miyoo Mini (Allium CFW) build support for the Metal
# Mutant port. Local addition, not part of upstream ALIS; included by the
# top-level CMakeLists when ALIS_MIYOO_ALLIUM is ON.
#
# The device has no usable system SDL2, so build one statically from source:
# no video backend works on the Miyoo (frames go straight to /dev/fb0, see
# mmx_miyoo.c), audio goes through the OSS /dev/dsp shim that Allium's
# libpadsp.so preload provides.

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

# audio: OSS only (via the padsp /dev/dsp shim)
set(SDL_OSS ON CACHE BOOL "" FORCE)
set(SDL_ALSA OFF CACHE BOOL "" FORCE)
set(SDL_JACK OFF CACHE BOOL "" FORCE)
set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_ESD OFF CACHE BOOL "" FORCE)
set(SDL_NAS OFF CACHE BOOL "" FORCE)
set(SDL_DISKAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_LIBSAMPLERATE OFF CACHE BOOL "" FORCE)

# no SDL video/input backends on device
set(SDL_DUMMYVIDEO ON CACHE BOOL "" FORCE)
set(SDL_X11 OFF CACHE BOOL "" FORCE)
set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
set(SDL_KMSDRM OFF CACHE BOOL "" FORCE)
set(SDL_RPI OFF CACHE BOOL "" FORCE)
set(SDL_VIVANTE OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
set(SDL_DBUS OFF CACHE BOOL "" FORCE)
set(SDL_IBUS OFF CACHE BOOL "" FORCE)
set(SDL_LIBUDEV OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL2)

# stand in for the find_package(SDL2) results the top-level CMakeLists expects
set(SDL2_INCLUDE_DIRS "")
set(SDL2_LIBRARIES SDL2::SDL2-static dl pthread)

set(ALIS_MIYOO_SOURCES ${CMAKE_CURRENT_LIST_DIR}/mmx_miyoo.c)

add_definitions(-DALIS_MIYOO_ALLIUM)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")
