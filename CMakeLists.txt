cmake_minimum_required(VERSION 3.20)

project(MySDL3App LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output executable
set(APP_NAME MySDL3App)

# Collect app sources
file(GLOB_RECURSE APP_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.hpp"
)

add_executable(${APP_NAME} ${APP_SOURCES})

target_include_directories(${APP_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/app"
)

# SDL3
find_package(SDL3 REQUIRED CONFIG)

# OpenCV
find_package(OpenCV REQUIRED)

target_link_libraries(${APP_NAME} PRIVATE
    SDL3::SDL3
    ${OpenCV_LIBS}
)

target_include_directories(${APP_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

# Dear ImGui
# Expected layout:
# external/imgui/
#   imgui.cpp
#   imgui_draw.cpp
#   imgui_tables.cpp
#   imgui_widgets.cpp
#   backends/imgui_impl_sdl3.cpp
#   backends/imgui_impl_sdlrenderer3.cpp OR imgui_impl_sdlgpu3.cpp/opengl3/etc.

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")

target_sources(${APP_NAME} PRIVATE
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"

    # Pick ONE renderer backend:
    "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
)

target_include_directories(${APP_NAME} PRIVATE
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

# Windows: copy SDL3 DLL after build if using shared SDL3
if(WIN32)
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:${APP_NAME}>
    )
endif()