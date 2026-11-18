add_library(RetroEngine SHARED ${RETRO_FILES} dependencies/android/androidHelpers.cpp dependencies/android/androidTransfer.cpp)

# (Plus) The transfer page (dependencies/android/transfer): SonicWebMania's server, on
# cpp-httplib, with its page built in. C++17 (std::filesystem), so a library of its own.
set(PLUS_TRANSFER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/android/transfer)
set(PLUS_TRANSFER_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/plus_transfer)
add_custom_command(
    OUTPUT ${PLUS_TRANSFER_GENERATED}/WebPage.generated.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUS_TRANSFER_GENERATED}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${PLUS_TRANSFER_DIR}/index.html -DOUTPUT=${PLUS_TRANSFER_GENERATED}/WebPage.generated.h
            -DVARIABLE=kWebPageHtml -P ${PLUS_TRANSFER_DIR}/EmbedTextFile.cmake
    DEPENDS ${PLUS_TRANSFER_DIR}/index.html ${PLUS_TRANSFER_DIR}/EmbedTextFile.cmake
    COMMENT "Embedding the transfer page"
)
add_library(plus_transfer STATIC
    ${PLUS_TRANSFER_DIR}/AndroidTransfer.cpp
    ${PLUS_TRANSFER_DIR}/IniEdit.cpp
    ${PLUS_TRANSFER_DIR}/Md5.cpp
    ${PLUS_TRANSFER_DIR}/ModLibrary.cpp
    ${PLUS_TRANSFER_DIR}/NetInfo.cpp
    ${PLUS_TRANSFER_DIR}/PlusFiles.cpp
    ${PLUS_TRANSFER_DIR}/RetroGames.cpp
    ${PLUS_TRANSFER_DIR}/RsdkPack.cpp
    ${PLUS_TRANSFER_DIR}/SettingsChannel.cpp
    ${PLUS_TRANSFER_DIR}/Storage.cpp
    ${PLUS_TRANSFER_DIR}/TransferServer.cpp
    ${PLUS_TRANSFER_DIR}/UsbDrives.cpp
    ${PLUS_TRANSFER_GENERATED}/WebPage.generated.h
)
target_compile_features(plus_transfer PRIVATE cxx_std_17)
target_include_directories(plus_transfer PRIVATE ${PLUS_TRANSFER_GENERATED} ${PLUS_TRANSFER_DIR})
target_compile_definitions(plus_transfer PRIVATE TRANSFER_APP_VERSION="${DECOMP_VERSION}")
target_link_libraries(plus_transfer log)
target_link_libraries(RetroEngine plus_transfer)

set(DEP_PATH android)
set(RETRO_SUBSYSTEM "OGL")
set(RETRO_OUTPUT_NAME "RetroEngine")

set(COMPILE_OGG TRUE)
set(COMPILE_THEORA TRUE)
set(OGG_FLAGS -ffast-math -fsigned-char -O2 -fPIC -DPIC -DBYTE_ORDER=LITTLE_ENDIAN -D_ARM_ASSEM_ -w)
set(THEORA_FLAGS -ffast-math -fsigned-char -O2 -fPIC -DPIC -DBYTE_ORDER=LITTLE_ENDIAN -D_ARM_ASSEM_ -w)

find_package(games-controller REQUIRED CONFIG)
find_package(game-activity REQUIRED CONFIG)
find_package(games-frame-pacing REQUIRED CONFIG)
find_package(oboe REQUIRED CONFIG)

target_link_libraries(RetroEngine
    android
    EGL
    GLESv3
    log
    z
    jnigraphics
    games-controller::paddleboat_static
    game-activity::game-activity
    games-frame-pacing::swappy_static
    oboe::oboe
    OpenSLES
)

target_link_options(RetroEngine PRIVATE -u GameActivity_onCreate)