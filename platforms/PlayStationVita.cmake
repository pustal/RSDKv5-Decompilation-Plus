# (Plus) PlayStation Vita, with VitaSDK and its SDL2 port.
#
# Configure from Sonic-Mania-Decompilation-Plus (the game is linked into the
# engine, as on the Wii and the Wii U):
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DPLATFORM=PlayStationVita -DGAME_STATIC=ON
# The result is build/dependencies/RSDKv5/RSDKv5U.vpk, to install with VitaShell.
# The data goes in ux0:data/RSDKv5/ (VITA_DATA_PATH, RetroEngine.hpp).
#
# After SonicMastr's Vita port (github.com/SonicMastr/Sonic-Mania-Vita and its
# RSDKv5-Decompilation fork).

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
find_package(PkgConfig REQUIRED)

add_executable(RetroEngine ${RETRO_FILES})

set(RETRO_SUBSYSTEM "SDL2" CACHE STRING "The subsystem to use")
if(NOT RETRO_SUBSYSTEM STREQUAL "SDL2")
    message(FATAL_ERROR "RETRO_SUBSYSTEM must be SDL2 on the Vita")
endif()

if(NOT GAME_STATIC)
    message(FATAL_ERROR "GAME_STATIC must be on: this port links the game into the engine")
endif()

# VitaSDK's libogg and libtheora, else built from source.
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
target_link_libraries(RetroEngine ${SDL2_STATIC_LIBRARIES} pthread)
target_link_directories(RetroEngine PRIVATE ${SDL2_STATIC_LIBRARY_DIRS})
target_compile_options(RetroEngine PRIVATE ${SDL2_STATIC_CFLAGS})

set(GAME_INCLUDE_EDITOR OFF CACHE BOOL "Disable unused editor code" FORCE)

# (Plus) Decoding Sonic Origins' 1080p VP9 videos on the Vita would take hours:
# no video converter (packs converted elsewhere play).
set(PLUS_VIDEO_CONVERTER OFF CACHE BOOL "Plus: Sonic Origins video converter (not on the Vita)" FORCE)

set(PLATFORM Vita)

# vita_create_self() reads the executable under the target's name.
set(RETRO_OUTPUT_NAME "RetroEngine" CACHE STRING "The exported name of the executable." FORCE)

set(VITA_TITLE_ID "RSDKPLUS5")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1 -d ATTRIBUTE2=12")

# The app's version as the Vita writes it: "1.6.0" -> "01.60".
string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+).*$" "\\1;\\2;\\3" VITA_VERSION_PARTS "${DECOMP_VERSION}")
list(GET VITA_VERSION_PARTS 0 VITA_VERSION_MAJOR)
list(GET VITA_VERSION_PARTS 1 VITA_VERSION_MINOR)
list(GET VITA_VERSION_PARTS 2 VITA_VERSION_PATCH)
if(VITA_VERSION_MAJOR LESS 10)
    set(VITA_VERSION_MAJOR "0${VITA_VERSION_MAJOR}")
endif()
set(VITA_APP_VERSION "${VITA_VERSION_MAJOR}.${VITA_VERSION_MINOR}${VITA_VERSION_PATCH}")

vita_create_self(eboot.bin RetroEngine UNSAFE)
vita_create_vpk(${RETRO_NAME}.vpk ${VITA_TITLE_ID} eboot.bin
    VERSION "${VITA_APP_VERSION}"
    NAME    "Sonic Mania (RSDKv5 Plus)"
    FILE    ${CMAKE_CURRENT_SOURCE_DIR}/platforms/vita/sce_sys sce_sys
)
