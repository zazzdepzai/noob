cmake_minimum_required(VERSION 3.20)
project(RavenXD-Launcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/utf-8 /MP /permissive- /W3)
    add_compile_definitions(
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# ============================================================
#  ImGui  (auto-clone nếu thiếu)
# ============================================================
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
if (NOT EXISTS ${IMGUI_DIR}/imgui.cpp AND EXISTS ${CMAKE_SOURCE_DIR}/imgui/imgui.cpp)
    set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)
endif()

if (NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    message(STATUS "ImGui not found — cloning from GitHub...")
    find_package(Git QUIET)
    if (Git_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1
                    https://github.com/ocornut/imgui.git
                    ${IMGUI_DIR}
            RESULT_VARIABLE git_result
        )
        if (NOT git_result EQUAL 0 OR NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
            message(FATAL_ERROR "Failed to clone ImGui")
        endif()
        message(STATUS "ImGui cloned to ${IMGUI_DIR}")
    else()
        message(FATAL_ERROR
            "ImGui not found and Git not available.\n"
            "Clone manually: git clone --depth 1 https://github.com/ocornut/imgui.git third_party/imgui")
    endif()
endif()

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_compile_definitions(imgui PRIVATE
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DEFINE_MATH_OPERATORS
)

# ============================================================
#  nlohmann/json
# ============================================================
find_package(nlohmann_json CONFIG QUIET)
if (nlohmann_json_FOUND)
    set(NLOHMANN_TARGET nlohmann_json::nlohmann_json)
elseif (EXISTS ${CMAKE_SOURCE_DIR}/third_party/json.hpp)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party)
    set(NLOHMANN_TARGET nlohmann_json)
else()
    message(FATAL_ERROR "nlohmann/json not found (vcpkg or third_party/json.hpp)")
endif()

# ============================================================
#  discord-rpc
# ============================================================
find_package(discord-rpc CONFIG QUIET)
if (discord-rpc_FOUND)
    set(DISCORD_TARGET discord-rpc)
else()
    set(DISCORD_TARGET "")
    message(STATUS "discord-rpc not found via CMake — check include/link paths manually")
endif()

# ============================================================
#  Asset embedding
# ============================================================
set(ASSET_LOGO ${CMAKE_SOURCE_DIR}/assets/logo.png)
set(ASSET_BG   ${CMAKE_SOURCE_DIR}/assets/bg.png)
set(ASSETS_HDR ${CMAKE_BINARY_DIR}/generated/embedded_assets.h)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)

if (EXISTS ${ASSET_LOGO} AND EXISTS ${ASSET_BG} AND EXISTS ${CMAKE_SOURCE_DIR}/cmake/embed_assets.cmake)
    add_custom_command(
        OUTPUT  ${ASSETS_HDR}
        COMMAND ${CMAKE_COMMAND}
            -DLOGO=${ASSET_LOGO}
            -DBG=${ASSET_BG}
            -DOUT=${ASSETS_HDR}
            -P ${CMAKE_SOURCE_DIR}/cmake/embed_assets.cmake
        DEPENDS ${ASSET_LOGO} ${ASSET_BG}
        COMMENT "Embedding assets"
        VERBATIM
    )
else()
    file(WRITE ${ASSETS_HDR}
        "#pragma once\n"
        "static const unsigned char RAVENXD_LOGO_PNG[] = {0,0,0,0};\n"
        "static const unsigned int  RAVENXD_LOGO_PNG_SIZE = 4;\n"
        "static const unsigned char RAVENXD_BG_PNG[] = {0,0,0,0};\n"
        "static const unsigned int  RAVENXD_BG_PNG_SIZE = 4;\n"
    )
endif()

add_custom_target(embedded_assets DEPENDS ${ASSETS_HDR})

# ============================================================
#  Executable
# ============================================================
set(SOURCES
    src/main.cpp
    src/Launcher.cpp
    src/Minecraft.cpp
    src/DownloadManager.cpp
    src/ModManager.cpp
    src/Settings.cpp
    src/AccountManager.cpp
    src/DiscordRPC.cpp
    src/UI/UI.cpp
)

add_executable(RavenXD-Launcher WIN32 ${SOURCES})

target_include_directories(RavenXD-Launcher PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/UI
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(RavenXD-Launcher PRIVATE
    imgui
    ${NLOHMANN_TARGET}
    d3d11
    dxgi
    d3dcompiler
    winhttp
    shlwapi
    shell32
    ole32
    oleaut32
    user32
    gdi32
    advapi32
    ws2_32
    version
    crypt32
)

if (DISCORD_TARGET)
    target_link_libraries(RavenXD-Launcher PRIVATE ${DISCORD_TARGET})
endif()

add_dependencies(RavenXD-Launcher embedded_assets)

# ============================================================
#  Runtime assets copy
# ============================================================
if (EXISTS ${ASSET_LOGO} AND EXISTS ${ASSET_BG})
    add_custom_command(TARGET RavenXD-Launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:RavenXD-Launcher>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${ASSET_LOGO} ${ASSET_BG}
                $<TARGET_FILE_DIR:RavenXD-Launcher>/assets/
        COMMENT "Copying runtime assets"
    )
endif()

if (MSVC)
    set_target_properties(RavenXD-Launcher PROPERTIES VS_DPI_AWARE "PerMonitor")
endif()
