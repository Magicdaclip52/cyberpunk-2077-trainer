cmake_minimum_required(VERSION 3.24)

project(CP2077Trainer LANGUAGES CXX RC)

option(ERTRAINER_DIST "Сборка для раздачи: без лога на диск" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.9
    GIT_SHALLOW    TRUE
)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(imgui json)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
if(MSVC)
    target_compile_options(imgui PRIVATE /W3 /utf-8)
endif()

add_executable(CP2077Trainer WIN32
    resources/CP2077Trainer.rc
    src/main.cpp
    src/core/Logger.cpp
    src/core/CrashDebug.cpp
    src/core/Config.cpp
    src/core/EmbeddedResources.cpp
    src/memory/ProcessMemory.cpp
    src/memory/SignatureScanner.cpp
    src/memory/PointerChain.cpp
    src/memory/BytePatch.cpp
    src/memory/CodeCave.cpp
    src/signatures/PatternTable.cpp
    src/hooks/GameHooks.cpp
    src/game/GameVersion.cpp
    src/game/GameAttach.cpp
    src/features/FeatureCatalog.cpp
    src/features/FeatureManager.cpp
    src/ui/Hotkeys.cpp
    src/ui/UiRenderer.cpp
    src/ui/TextureLoader.cpp
    src/worker/WorkerThread.cpp
    src/app/Application.cpp
)

target_include_directories(CP2077Trainer PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/resources
)
target_compile_definitions(CP2077Trainer PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
    NTDDI_VERSION=0x0A000000
)
target_link_libraries(CP2077Trainer PRIVATE
    imgui
    nlohmann_json::nlohmann_json
    d3d11
    dxgi
    psapi
    version
    shell32
    ole32
    dwmapi
    windowscodecs
    dbghelp
)
if(ERTRAINER_DIST)
    target_compile_definitions(CP2077Trainer PRIVATE ERTRAINER_DIST)
endif()
if(MSVC)
    target_compile_options(CP2077Trainer PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    target_link_options(CP2077Trainer PRIVATE
        /SUBSYSTEM:WINDOWS,10.0
        /MANIFEST:NO
    )
endif()

set_target_properties(CP2077Trainer PROPERTIES
    OUTPUT_NAME "CP2077Trainer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
)
