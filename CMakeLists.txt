cmake_minimum_required(VERSION 3.22)
project(MajoShovel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MAJOSHOVEL_VENDOR_SDL "Use vendored SDL from external/SDL if present" ON)

if(MAJOSHOVEL_VENDOR_SDL AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL/CMakeLists.txt")
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
    src/engine/Renderer.cpp
    src/engine/Camera.cpp
    src/data/RuntimeBalance.cpp
    src/game/Game.cpp
    src/game/Player.cpp
    src/game/OrbitSystem.cpp
    src/game/OrbitItem.cpp
    src/game/DiggingSystem.cpp
    src/game/TileMap.cpp
    src/game/Enemy.cpp
    src/game/EnemySystem.cpp
    src/game/Collision.cpp
    src/game/LevelSystem.cpp
    src/game/UpgradeSystem.cpp
    src/game/DebugOverlay.cpp
)

target_include_directories(MajoShovel PRIVATE src)
target_link_libraries(MajoShovel PRIVATE SDL3::SDL3)

if(WIN32)
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

if(MSVC)
    target_compile_options(MajoShovel PRIVATE /W4 /permissive-)
else()
    target_compile_options(MajoShovel PRIVATE -Wall -Wextra -Wpedantic)
endif()
