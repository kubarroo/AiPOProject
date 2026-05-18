cmake_minimum_required(VERSION 3.20)

project(AiPOProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_NAME AiPOProject)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.h"
)

add_executable(${APP_NAME} ${APP_SOURCES})

target_include_directories(${APP_NAME} PRIVATE app)

# SDL3
add_subdirectory(external/SDL)

# OpenCV
set(BUILD_LIST core,imgproc,imgcodecs,videoio,highgui CACHE STRING "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/opencv)

# Dear ImGui
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")

target_sources(${APP_NAME} PRIVATE
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
)

target_include_directories(${APP_NAME} PRIVATE
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

target_link_libraries(${APP_NAME} PRIVATE
    SDL3::SDL3
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_videoio
    opencv_highgui
)