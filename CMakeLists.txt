cmake_minimum_required(VERSION 3.20)
project(UniversalVisuals LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT: the DLL runs without a VC++ Redistributable install.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# SOURCE_SUBDIR points at a non-existent folder: we only need the sources and
# add them to our own targets instead of running upstream CMake projects.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _none)
FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _none)
FetchContent_MakeAvailable(imgui minhook)

# ---------------------------------------------------------------------------
# Dear ImGui + Win32/OpenGL3 backends
# ---------------------------------------------------------------------------
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui_lib PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
target_link_libraries(imgui_lib PUBLIC opengl32 dwmapi)

# ---------------------------------------------------------------------------
# MinHook
# ---------------------------------------------------------------------------
add_library(minhook_lib STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c)
target_include_directories(minhook_lib PUBLIC ${minhook_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# UniversalVisuals
# ---------------------------------------------------------------------------
add_library(UniversalVisuals SHARED
    src/dllmain.cpp
    src/core/bridge.cpp
    src/core/config.cpp
    src/core/hook.cpp
    src/core/overlay.cpp
    src/gfx/gui.cpp
    src/gfx/hud.cpp
    src/gfx/renderer.cpp
    src/gfx/theme.cpp
    src/gfx/visuals.cpp
    src/gfx/widgets.cpp
    src/util/log.cpp)

target_include_directories(UniversalVisuals PRIVATE src)
target_compile_options(UniversalVisuals PRIVATE /utf-8 /W3 /MP)
target_link_libraries(UniversalVisuals PRIVATE imgui_lib minhook_lib opengl32 gdi32 user32 psapi)

set_target_properties(UniversalVisuals PROPERTIES
    OUTPUT_NAME "UniversalVisuals"
    PREFIX "")

# Copy the logger-friendly font hint file next to the DLL on demand:
# drop Inter-*.ttf into the output folder to use it, otherwise Segoe UI is used.
message(STATUS "UniversalVisuals: build the Release configuration to produce UniversalVisuals.dll")
