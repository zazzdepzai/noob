cmake_minimum_required(VERSION 3.20)
project(RavenXD-Launcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
#  MSVC compile flags  (NOMINMAX fixes std::min/max clash)
# ============================================================
if (MSVC)
    add_compile_options(/utf-8 /MP /permissive- /W3)
    add_compile_definitions(
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY         ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# ============================================================
#  Third-party: ImGui
# ============================================================
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
if (NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)
endif()

if (NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    message(FATAL_ERROR "ImGui not found. Place it in third_party/imgui/ or imgui/")
endif()

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_compile_definitions(imgui PRIVATE
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DEFINE_MATH_OPERATORS
)

# ============================================================
#  Third-party: nlohmann/json (header-only)
# ============================================================
find_package(nlohmann_json CONFIG QUIET)

if (NOT nlohmann_json_FOUND)
    # Fallback 1: third_party/json/single_include
    if (EXISTS ${CMAKE_SOURCE_DIR}/third_party/json/single_include/nlohmann/json.hpp)
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE
            ${CMAKE_SOURCE_DIR}/third_party/json/single_include)
        message(STATUS "nlohmann_json: using bundled single-header")
    # Fallback 2: third_party/json.hpp
    elseif (EXISTS ${CMAKE_SOURCE_DIR}/third_party/json.hpp)
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE
            ${CMAKE_SOURCE_DIR}/third_party)
        message(STATUS "nlohmann_json: using third_party/json.hpp")
    else()
        message(FATAL_ERROR
            "nlohmann/json not found.\n"
            "Install via vcpkg (vcpkg install nlohmann-json) or place\n"
            "json.hpp in third_party/")
    endif()
endif()

# ============================================================
#  Asset embedding  (PNG → C byte array)
# ============================================================
set(ASSET_LOGO ${CMAKE_SOURCE_DIR}/assets/logo.png)
set(ASSET_BG   ${CMAKE_SOURCE_DIR}/assets/bg.png)
set(ASSETS_HDR ${CMAKE_BINARY_DIR}/generated/embedded_assets.h)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)

if (EXISTS ${ASSET_LOGO} AND EXISTS ${ASSET_BG})
    add_custom_command(
        OUTPUT  ${ASSETS_HDR}
        COMMAND ${CMAKE_COMMAND}
            -DLOGO=${ASSET_LOGO}
            -DBG=${ASSET_BG}
            -DOUT=${ASSETS_HDR}
            -P ${CMAKE_SOURCE_DIR}/cmake/embed_assets.cmake
        DEPENDS ${ASSET_LOGO} ${ASSET_BG}
        COMMENT "Embedding assets → embedded_assets.h"
        VERBATIM
    )
else()
    # Tạo header rỗng nếu không có asset
    file(WRITE ${ASSETS_HDR}
        "#pragma once\n"
        "// Auto-generated — no assets found\n"
        "static const unsigned char RAVENXD_LOGO_PNG[] = {0,0,0,0};\n"
        "static const unsigned int  RAVENXD_LOGO_PNG_SIZE = 4;\n"
        "static const unsigned char RAVENXD_BG_PNG[] = {0,0,0,0};\n"
        "static const unsigned int  RAVENXD_BG_PNG_SIZE = 4;\n"
    )
endif()

add_custom_target(embedded_assets DEPENDS ${ASSETS_HDR})

# ============================================================
#  Main executable
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
    nlohmann_json
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

add_dependencies(RavenXD-Launcher embedded_assets)

# ============================================================
#  Copy assets next to exe (runtime)
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

# ============================================================
#  Windows: DPI aware + manifest
# ============================================================
if (MSVC)
    set_target_properties(RavenXD-Launcher PROPERTIES
        VS_DPI_AWARE "PerMonitor"
    )
endif()

# ============================================================
#  Copy dependencies (DLLs) nếu dùng vcpkg dynamic
# ============================================================
if (WIN32)
    find_program(VCPKG_CMD vcpkg)
    if (VCPKG_CMD AND DEFINED VCPKG_TARGET_TRIPLET)
        add_custom_command(TARGET RavenXD-Launcher POST_BUILD
            COMMAND ${VCPKG_CMD} export
                    --raw --triplet ${VCPKG_TARGET_TRIPLET}
                    --output-dir $<TARGET_FILE_DIR:RavenXD-Launcher>/vcpkg_export
            COMMENT "Exporting vcpkg DLLs (best-effort)"
        )
    endif()
endif()
