cmake_minimum_required(VERSION 3.20)

project(RSDKv4WiiU LANGUAGES C CXX)

set(PACKAGED_GAME "0" CACHE STRING "Packaged game id: 0, 1, or 2")
set(BASE_PATH "" CACHE STRING "Runtime base path")
option(FORCE_CASE_INSENSITIVE "Force case-insensitive file opens" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(PkgConfig REQUIRED)
pkg_check_modules(VORBIS REQUIRED IMPORTED_TARGET vorbisfile vorbis)

set(SDL_DIR "${CMAKE_SOURCE_DIR}/dependencies/all/SDL")

set(RSDK_SOURCES
    dependencies/all/tinyxml2/tinyxml2.cpp
    RSDKv4/Animation.cpp
    RSDKv4/Audio.cpp
    RSDKv4/Collision.cpp
    RSDKv4/Debug.cpp
    RSDKv4/Drawing.cpp
    RSDKv4/Ini.cpp
    RSDKv4/Input.cpp
    RSDKv4/main.cpp
    RSDKv4/Math.cpp
    RSDKv4/ModAPI.cpp
    RSDKv4/Networking.cpp
    RSDKv4/Object.cpp
    RSDKv4/Palette.cpp
    RSDKv4/Reader.cpp
    RSDKv4/Renderer.cpp
    RSDKv4/RetroEngine.cpp
    RSDKv4/Scene.cpp
    RSDKv4/Scene3D.cpp
    RSDKv4/Script.cpp
    RSDKv4/Sprite.cpp
    RSDKv4/String.cpp
    RSDKv4/Text.cpp
    RSDKv4/Userdata.cpp
    RSDKv4/NativeObjects/AboutScreen.cpp
    RSDKv4/NativeObjects/AchievementDisplay.cpp
    RSDKv4/NativeObjects/AchievementsButton.cpp
    RSDKv4/NativeObjects/AchievementsMenu.cpp
    RSDKv4/NativeObjects/BackButton.cpp
    RSDKv4/NativeObjects/CWSplash.cpp
    RSDKv4/NativeObjects/CreditText.cpp
    RSDKv4/NativeObjects/DialogPanel.cpp
    RSDKv4/NativeObjects/FadeScreen.cpp
    RSDKv4/NativeObjects/InstructionsScreen.cpp
    RSDKv4/NativeObjects/LeaderboardsButton.cpp
    RSDKv4/NativeObjects/MenuBG.cpp
    RSDKv4/NativeObjects/MenuControl.cpp
    RSDKv4/NativeObjects/ModInfoButton.cpp
    RSDKv4/NativeObjects/ModsButton.cpp
    RSDKv4/NativeObjects/ModsMenu.cpp
    RSDKv4/NativeObjects/MultiplayerButton.cpp
    RSDKv4/NativeObjects/MultiplayerScreen.cpp
    RSDKv4/NativeObjects/OptionsButton.cpp
    RSDKv4/NativeObjects/OptionsMenu.cpp
    RSDKv4/NativeObjects/PauseMenu.cpp
    RSDKv4/NativeObjects/PlayerSelectScreen.cpp
    RSDKv4/NativeObjects/PushButton.cpp
    RSDKv4/NativeObjects/RecordsScreen.cpp
    RSDKv4/NativeObjects/RetroGameLoop.cpp
    RSDKv4/NativeObjects/SaveSelect.cpp
    RSDKv4/NativeObjects/SegaIDButton.cpp
    RSDKv4/NativeObjects/SegaSplash.cpp
    RSDKv4/NativeObjects/SettingsScreen.cpp
    RSDKv4/NativeObjects/StaffCredits.cpp
    RSDKv4/NativeObjects/StartGameButton.cpp
    RSDKv4/NativeObjects/SubMenuButton.cpp
    RSDKv4/NativeObjects/TextLabel.cpp
    RSDKv4/NativeObjects/TimeAttack.cpp
    RSDKv4/NativeObjects/TimeAttackButton.cpp
    RSDKv4/NativeObjects/TitleScreen.cpp
    RSDKv4/NativeObjects/VirtualDPad.cpp
    RSDKv4/NativeObjects/VirtualDPadM.cpp
    RSDKv4/NativeObjects/ZoneButton.cpp
    RSDKv4/Platform/WiiUProc.cpp
    RSDKv4/Platform/WiiUForeground.cpp
    RSDKv4/Platform/WiiUDRC.cpp
)

if(FORCE_CASE_INSENSITIVE)
    list(APPEND RSDK_SOURCES RSDKv4/fcaseopen.c)
endif()

file(GLOB SDL_SOURCES CONFIGURE_DEPENDS
    "${SDL_DIR}/src/*.c"
    "${SDL_DIR}/src/atomic/*.c"
    "${SDL_DIR}/src/audio/*.c"
    "${SDL_DIR}/src/audio/wiiu/*.c"
    "${SDL_DIR}/src/cpuinfo/*.c"
    "${SDL_DIR}/src/events/*.c"
    "${SDL_DIR}/src/file/*.c"
    "${SDL_DIR}/src/haptic/*.c"
    "${SDL_DIR}/src/haptic/dummy/*.c"
    "${SDL_DIR}/src/joystick/*.c"
    "${SDL_DIR}/src/joystick/wiiu/*.c"
    "${SDL_DIR}/src/joystick/dummy/*.c"
    "${SDL_DIR}/src/loadso/dummy/*.c"
    "${SDL_DIR}/src/power/*.c"
    "${SDL_DIR}/src/filesystem/dummy/*.c"
    "${SDL_DIR}/src/render/*.c"
    "${SDL_DIR}/src/render/wiiu/*.c"
    "${SDL_DIR}/src/render/software/*.c"
    "${SDL_DIR}/src/stdlib/*.c"
    "${SDL_DIR}/src/sensor/*.c"
    "${SDL_DIR}/src/thread/*.c"
    "${SDL_DIR}/src/thread/wiiu/*.c"
    "${SDL_DIR}/src/timer/*.c"
    "${SDL_DIR}/src/timer/wiiu/*.c"
    "${SDL_DIR}/src/timer/dummy/*.c"
    "${SDL_DIR}/src/video/*.c"
    "${SDL_DIR}/src/video/wiiu/*.c"
    "${SDL_DIR}/src/video/yuv2rgb/*.c"
    "${SDL_DIR}/src/video/dummy/*.c"
)

add_library(SDL2_wiiu STATIC ${SDL_SOURCES})
target_include_directories(SDL2_wiiu PRIVATE
    "${SDL_DIR}/include"
    "${WUT_ROOT}/include"
    "${DEVKITPRO}/portlibs/wiiu/include"
    "${DEVKITPRO}/portlibs/ppc/include"
)
target_compile_definitions(SDL2_wiiu PRIVATE __WIIU__ __WUT__)
target_compile_options(SDL2_wiiu PRIVATE -O2 -g -ffunction-sections -fdata-sections)

add_executable(RSDKv4 ${RSDK_SOURCES})
target_include_directories(RSDKv4 PRIVATE
    RSDKv4
    RSDKv4/NativeObjects
    dependencies/all/asio/include
    dependencies/all/stb-image
    dependencies/all/tinyxml2
    "${SDL_DIR}/include"
)
target_compile_definitions(RSDKv4 PRIVATE
    __WIIU__
    __WUT__
    PACKAGED_GAME=${PACKAGED_GAME}
    BASE_PATH="${BASE_PATH}"
    STBI_NO_THREAD_LOCALS
    RETRO_WIIU_CF_AROMA
)
if(FORCE_CASE_INSENSITIVE)
    target_compile_definitions(RSDKv4 PRIVATE FORCE_CASE_INSENSITIVE)
endif()
target_compile_options(RSDKv4 PRIVATE -O2 -g -ffunction-sections -fdata-sections)
target_link_options(RSDKv4 PRIVATE "-Wl,-T,${CMAKE_SOURCE_DIR}/RSDKv4/Platform/WiiUHeapAnchor.ld")
target_link_libraries(RSDKv4 PRIVATE SDL2_wiiu PkgConfig::VORBIS)

wut_create_rpx(RSDKv4)
get_target_property(RSDKV4_RPX RSDKv4 WUT_RPL)
add_custom_command(TARGET RSDKv4 POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/bin"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${RSDKV4_RPX}" "${CMAKE_SOURCE_DIR}/bin/RSDKv4.rpx"
    VERBATIM
)
