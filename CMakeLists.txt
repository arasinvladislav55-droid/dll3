cmake_minimum_required(VERSION 3.20)
project(visuals_overlay LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Статическая CRT: DLL не требует установленного VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
# SOURCE_SUBDIR указывает на несуществующую папку: нужны только исходники, их CMake не запускаем
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9 GIT_SHALLOW TRUE SOURCE_SUBDIR _none)
FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3 GIT_SHALLOW TRUE SOURCE_SUBDIR _none)
FetchContent_MakeAvailable(imgui minhook)

# ImGui с OpenGL3-бэкендом (Minecraft Java рисует через OpenGL, а не DirectX)
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui_lib PUBLIC NOMINMAX _CRT_SECURE_NO_WARNINGS)

add_library(minhook_lib STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c)
target_include_directories(minhook_lib PUBLIC ${minhook_SOURCE_DIR}/include)

add_library(visuals SHARED src/visuals_overlay.cpp)
target_compile_options(visuals PRIVATE /utf-8 /W3)
target_link_libraries(visuals PRIVATE imgui_lib minhook_lib opengl32)
set_target_properties(visuals PROPERTIES OUTPUT_NAME "visuals")
