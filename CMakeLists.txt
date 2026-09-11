cmake_minimum_required(VERSION 3.20)
project(RavenXD-Launcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
#  MSVC flags
# ============================================================
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
#  ImGui
# ============================================================
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
if (NOT EXISTS ${IMGUI_DIR}/imgui.cpp AND EXISTS ${CMAKE_SOURCE_DIR}/imgui/imgui.cpp)
    set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)
endif()

if (NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    message(STATUS "ImGui not found — cloning...")
    find_package(Git QUIET)
    if (Git_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1
                    https://github.com/ocornut/imgui.git ${IMGUI_DIR}
            RESULT_VARIABLE git_result)
        if (NOT git_result EQUAL 0)
            message(FATAL_ERROR "Failed to clone ImGui")
        endif()
    else()
        message(FATAL_ERROR "ImGui missing — clone manually to third_party/imgui")
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
    ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(imgui PRIVATE
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DEFINE_MATH_OPERATORS
)

# ============================================================
#  nlohmann/json (vcpkg)
# ============================================================
find_package(nlohmann_json CONFIG REQUIRED)
# target: nlohmann_json::nlohmann_json

# ============================================================
#  discord-rpc (vcpkg)
# ============================================================
find_package(discord-rpc CONFIG REQUIRED)
# target: discord-rpc

# ============================================================
#  Asset embedding
# ============================================================
set(ASSET_LOGO   ${CMAKE_SOURCE_DIR}/assets/logo.png)
set(ASSET_BG     ${CMAKE_SOURCE_DIR}/assets/bg.png)
set(ASSET_BANNER ${CMAKE_SOURCE_DIR}/assets/banner.png)
set(ASSETS_HDR   ${CMAKE_BINARY_DIR}/generated/embedded_assets.h)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)

if (EXISTS ${ASSET_LOGO} AND EXISTS ${CMAKE_SOURCE_DIR}/cmake/embed_assets.cmake)
    add_custom_command(
        OUTPUT  ${ASSETS_HDR}
        COMMAND ${CMAKE_COMMAND}
            -DLOGO=${ASSET_LOGO}
            -DBG=${ASSET_BG}
            -DBANNER=${ASSET_BANNER}
            -DOUT=${ASSETS_HDR}
            -P ${CMAKE_SOURCE_DIR}/cmake/embed_assets.cmake
        DEPENDS ${ASSET_LOGO} ${ASSET_BG} ${ASSET_BANNER}
                ${CMAKE_SOURCE_DIR}/cmake/embed_assets.cmake
        COMMENT "Embedding assets"
        VERBATIM)
else()
    file(WRITE ${ASSETS_HDR}
        "#pragma once\n"
        "static const unsigned char RAVENXD_LOGO_PNG[]   = {0,0,0,0};\n"
        "static const unsigned int  RAVENXD_LOGO_PNG_SIZE   = 4;\n"
        "static const unsigned char RAVENXD_BG_PNG[]     = {0,0,0,0};\n"
        "static const unsigned int  RAVENXD_BG_PNG_SIZE     = 4;\n"
        "static const unsigned char RAVENXD_BANNER_PNG[] = {0,0,0,0};\n"
        "static const unsigned int  RAVENXD_BANNER_PNG_SIZE = 4;\n")
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
    ${CMAKE_BINARY_DIR}/generated)

target_link_libraries(RavenXD-Launcher PRIVATE
    imgui
    nlohmann_json::nlohmann_json
    discord-rpc
    d3d11 dxgi d3dcompiler
    winhttp shlwapi shell32
    ole32 oleaut32
    user32 gdi32 advapi32
    ws2_32 version crypt32)

add_dependencies(RavenXD-Launcher embedded_assets)

# ============================================================
#  Copy fonts (runtime)
# ============================================================
set(FONT_DIR ${CMAKE_SOURCE_DIR}/assets/fonts)

if (EXISTS ${FONT_DIR})
    add_custom_command(TARGET RavenXD-Launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:RavenXD-Launcher>/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${FONT_DIR}
                $<TARGET_FILE_DIR:RavenXD-Launcher>/fonts
        COMMENT "Copying fonts")
endif()

# ============================================================
#  Copy assets (runtime)
# ============================================================
if (EXISTS ${ASSET_LOGO})
    add_custom_command(TARGET RavenXD-Launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:RavenXD-Launcher>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${ASSET_LOGO}
                $<TARGET_FILE_DIR:RavenXD-Launcher>/assets/
        COMMENT "Copying assets")
endif()

# ============================================================
#  Windows: DPI aware
# ============================================================
if (MSVC)
    set_target_properties(RavenXD-Launcher PROPERTIES VS_DPI_AWARE "PerMonitor")
endif()
