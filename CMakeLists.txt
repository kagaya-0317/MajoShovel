cmake_minimum_required(VERSION 3.22)
project(MajoShovel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MAJOSHOVEL_PREBUILT_SDL_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL3-prebuilt"
    CACHE PATH "Path to prebuilt SDL3 package")
option(MAJOSHOVEL_USE_PREBUILT_SDL "Use prebuilt SDL3 from external/SDL3-prebuilt" ON)
option(MAJOSHOVEL_VENDOR_SDL "Build SDL from external/SDL when prebuilt SDL3 is unavailable" OFF)

set(MAJOSHOVEL_PREBUILT_SDL_DLL "${MAJOSHOVEL_PREBUILT_SDL_DIR}/bin/SDL3.dll")
set(MAJOSHOVEL_PREBUILT_SDL_IMPLIB "${MAJOSHOVEL_PREBUILT_SDL_DIR}/lib/SDL3.lib")
set(MAJOSHOVEL_PREBUILT_SDL_INCLUDE "${MAJOSHOVEL_PREBUILT_SDL_DIR}/include")

if(MAJOSHOVEL_USE_PREBUILT_SDL
    AND EXISTS "${MAJOSHOVEL_PREBUILT_SDL_DLL}"
    AND EXISTS "${MAJOSHOVEL_PREBUILT_SDL_IMPLIB}"
    AND EXISTS "${MAJOSHOVEL_PREBUILT_SDL_INCLUDE}/SDL3/SDL.h")
    add_library(SDL3::SDL3 SHARED IMPORTED GLOBAL)
    set_target_properties(SDL3::SDL3 PROPERTIES
        IMPORTED_LOCATION "${MAJOSHOVEL_PREBUILT_SDL_DLL}"
        IMPORTED_IMPLIB "${MAJOSHOVEL_PREBUILT_SDL_IMPLIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${MAJOSHOVEL_PREBUILT_SDL_INCLUDE}"
    )
elseif(MAJOSHOVEL_VENDOR_SDL AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL/CMakeLists.txt")
    add_subdirectory(external/SDL)
else()
    find_package(SDL3 CONFIG REQUIRED)
endif()

add_executable(MajoShovel
    src/main.cpp
    src/engine/App.cpp
    src/engine/Time.cpp
    src/engine/Input.cpp
    src/engine/FileWatcher.cpp
    src/engine/Log.cpp
    src/engine/Ui.cpp
    src/engine/Renderer.cpp
    src/engine/Camera.cpp
    src/data/GoogleSheetSource.cpp
    src/data/EnemyCatalog.cpp
    src/data/ObjectCatalog.cpp
    src/data/RuntimeBalance.cpp
    src/game/Game.cpp
    src/game/DungeonLayout.cpp
    src/game/EntityStatus.cpp
    src/game/EffectDispatcher.cpp
    src/game/OrbitModifiers.cpp
    src/game/Player.cpp
    src/game/ProjectileSystem.cpp
    src/game/SpellRingSystem.cpp
    src/game/SpellRingItem.cpp
    src/game/DiggingSystem.cpp
    src/game/TileMap.cpp
    src/game/Enemy.cpp
    src/game/EnemySystem.cpp
    src/game/EncyclopediaSystem.cpp
    src/game/InventorySystem.cpp
    src/game/ItemModel.cpp
    src/game/WorldDropSystem.cpp
    src/game/EffectSystem.cpp
    src/game/Collision.cpp
    src/game/LevelSystem.cpp
    src/game/UpgradeSystem.cpp
    src/game/DebugOverlay.cpp
)

target_include_directories(MajoShovel PRIVATE src)
target_link_libraries(MajoShovel PRIVATE SDL3::SDL3)

if(WIN32)
    target_link_libraries(MajoShovel PRIVATE gdiplus winhttp)

    add_custom_command(TARGET MajoShovel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:MajoShovel>
    )
endif()

add_custom_command(TARGET MajoShovel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:MajoShovel>/data
)

add_custom_command(TARGET MajoShovel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:MajoShovel>/assets
)

if(MSVC)
    target_compile_options(MajoShovel PRIVATE /W4 /permissive- /utf-8 /MP)
else()
    target_compile_options(MajoShovel PRIVATE -Wall -Wextra -Wpedantic)
endif()
