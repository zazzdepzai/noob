cmake_minimum_required(VERSION 3.20)
project(RavenXD-Launcher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT → single exe
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Cho phép các dep cũ chạy được với CMake 4.x
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

include(FetchContent)

# ==================== ImGui ====================
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.5
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)

# ==================== nlohmann/json (từ vcpkg) ====================
find_package(nlohmann_json CONFIG REQUIRED)

# ==================== discord-rpc (tìm thủ công) ====================
set(_VCPKG_INSTALLED "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static")

find_path(DISCORD_RPC_INCLUDE_DIR
    NAMES discord_rpc.h
    PATHS
        "${_VCPKG_INSTALLED}/include"
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
        "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include"
    NO_DEFAULT_PATH)

find_library(DISCORD_RPC_LIBRARY
    NAMES discord-rpc
    PATHS
        "${_VCPKG_INSTALLED}/lib"
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
        "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib"
    NO_DEFAULT_PATH)

if(NOT DISCORD_RPC_INCLUDE_DIR)
    message(FATAL_ERROR "discord_rpc.h not found — check vcpkg install")
endif()
if(NOT DISCORD_RPC_LIBRARY)
    message(FATAL_ERROR "discord-rpc.lib not found — check vcpkg install")
endif()

message(STATUS "Discord RPC include: ${DISCORD_RPC_INCLUDE_DIR}")
message(STATUS "Discord RPC library: ${DISCORD_RPC_LIBRARY}")

# ==================== Embed assets ====================
# --- Logo ravenxd.png ---
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/ravenxd.png")
    file(READ "${CMAKE_SOURCE_DIR}/assets/ravenxd.png" RAVENXD_LOGO_HEX HEX)
    string(REGEX REPLACE "(..)" "0x\\1," RAVENXD_LOGO_BYTES "${RAVENXD_LOGO_HEX}")
else()
    set(RAVENXD_LOGO_BYTES "0x00")
endif()

file(WRITE "${CMAKE_BINARY_DIR}/embedded_assets.h"
"#pragma once
static const unsigned char RAVENXD_LOGO_PNG[] = { ${RAVENXD_LOGO_BYTES} };
static const unsigned int  RAVENXD_LOGO_PNG_SIZE = sizeof(RAVENXD_LOGO_PNG);
")

# --- Background background.png ---
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/background.png")
    file(READ "${CMAKE_SOURCE_DIR}/assets/background.png" BG_HEX HEX)
    string(REGEX REPLACE "(..)" "0x\\1," BG_BYTES "${BG_HEX}")
else()
    set(BG_BYTES "0x00")
endif()

file(APPEND "${CMAKE_BINARY_DIR}/embedded_assets.h"
"static const unsigned char RAVENXD_BG_PNG[] = { ${BG_BYTES} };
static const unsigned int  RAVENXD_BG_PNG_SIZE = sizeof(RAVENXD_BG_PNG);
")

# ==================== Executable ====================
add_executable(RavenXD-Launcher WIN32
    src/main.cpp
    src/Launcher.cpp
    src/DownloadManager.cpp
    src/Minecraft.cpp
    src/ModManager.cpp
    src/Settings.cpp
    src/AccountManager.cpp
    src/DiscordRPC.cpp
    src/UI/UI.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(RavenXD-Launcher PRIVATE
    src
    ${CMAKE_BINARY_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${DISCORD_RPC_INCLUDE_DIR}
)

target_link_libraries(RavenXD-Launcher PRIVATE
    nlohmann_json::nlohmann_json
    ${DISCORD_RPC_LIBRARY}
    d3d11 dxgi d3dcompiler dwmapi winhttp shlwapi version ole32 comctl32 shell32
    windowscodecs
)

set_target_properties(RavenXD-Launcher PROPERTIES
    OUTPUT_NAME "RavenXD-Launcher"
    VS_DPI_AWARE "PerMonitor"
)
