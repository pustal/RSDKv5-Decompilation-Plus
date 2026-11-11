# (Plus) Wii U, with devkitPro's wut and its wiiu-sdl2 port.
#
# Configure from Sonic-Mania-Decompilation-Plus (the game is linked into the
# engine, as on the Wii):
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/WiiU.cmake -DPLATFORM=NintendoWiiU -DGAME_STATIC=ON
# The result is build/dependencies/RSDKv5/RSDKv5U.wuhb (and the .rpx), for
# sd:/wiiu/apps/. The data goes in sd:/RSDKv5/ (WIIU_DATA_PATH, RetroEngine.hpp).
#
# After Clownacy's (github.com/Clownacy/Sonic-Mania-Decompilation-Wii-U) and
# Radfordhound's (github.com/Radfordhound/RSDKv5-Decompilation) Wii U ports.

find_package(PkgConfig REQUIRED)

add_executable(RetroEngine ${RETRO_FILES})

set(RETRO_SUBSYSTEM "SDL2" CACHE STRING "The subsystem to use")
if(NOT RETRO_SUBSYSTEM STREQUAL "SDL2")
    message(FATAL_ERROR "RETRO_SUBSYSTEM must be SDL2 on the Wii U")
endif()

if(NOT GAME_STATIC)
    message(FATAL_ERROR "GAME_STATIC must be on: the Wii U cannot load the game as a library")
endif()

# devkitPro's ppc-libogg and ppc-libtheora, else built from source.
pkg_check_modules(OGG ogg)
if(NOT OGG_FOUND)
    set(COMPILE_OGG TRUE)
    message(NOTICE "libogg not found, attempting to build from source")
else()
    target_link_libraries(RetroEngine ${OGG_STATIC_LIBRARIES})
    target_link_directories(RetroEngine PRIVATE ${OGG_STATIC_LIBRARY_DIRS})
    target_compile_options(RetroEngine PRIVATE ${OGG_STATIC_CFLAGS})
endif()

pkg_check_modules(THEORA theora theoradec)
if(NOT THEORA_FOUND)
    set(COMPILE_THEORA TRUE)
    message(NOTICE "libtheora not found, attempting to build from source")
else()
    target_link_libraries(RetroEngine ${THEORA_STATIC_LIBRARIES})
    target_link_directories(RetroEngine PRIVATE ${THEORA_STATIC_LIBRARY_DIRS})
    target_compile_options(RetroEngine PRIVATE ${THEORA_STATIC_CFLAGS})
endif()

pkg_check_modules(SDL2 sdl2 REQUIRED)
target_link_libraries(RetroEngine ${SDL2_STATIC_LIBRARIES})
target_link_directories(RetroEngine PRIVATE ${SDL2_STATIC_LIBRARY_DIRS})
target_link_options(RetroEngine PRIVATE ${SDL2_STATIC_LDLIBS_OTHER})
target_compile_options(RetroEngine PRIVATE ${SDL2_STATIC_CFLAGS})

set(GAME_INCLUDE_EDITOR OFF CACHE BOOL "Disable unused editor code" FORCE)

# (Plus) Decoding Sonic Origins' 1080p VP9 videos on the Wii U's CPU would take
# hours: no video converter (packs converted elsewhere play).
set(PLUS_VIDEO_CONVERTER OFF CACHE BOOL "Plus: Sonic Origins video converter (not on the Wii U)" FORCE)

set(PLATFORM WiiU)

wut_create_rpx(RetroEngine)
wut_create_wuhb(RetroEngine
    NAME      "Sonic Mania (RSDKv5 Plus)"
    SHORTNAME "Sonic Mania"
    AUTHOR    "RSDKModding, Clownacy, Eduardo Martins"
    ICON      "${CMAKE_CURRENT_SOURCE_DIR}/platforms/wiiu/icon.png"
    TVSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/platforms/wiiu/tv.png"
    DRCSPLASH "${CMAKE_CURRENT_SOURCE_DIR}/platforms/wiiu/drc.png"
)
