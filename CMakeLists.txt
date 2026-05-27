cmake_minimum_required(VERSION 3.20)

project(AiPOProject LANGUAGES CXX)

set(APP_NAME AiPOProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sources
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/*.h"
)

add_executable(${APP_NAME} ${APP_SOURCES})

target_include_directories(${APP_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/app"
)

# SDL3
add_subdirectory(external/SDL)

# OpenCV
set(BUILD_LIST core,imgproc,imgcodecs,videoio,highgui CACHE STRING "" FORCE)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)

set(WITH_IPP OFF CACHE BOOL "" FORCE)
set(WITH_WEBP OFF CACHE BOOL "" FORCE)
set(BUILD_WEBP OFF CACHE BOOL "" FORCE)

set(CPU_BASELINE "SSE3" CACHE STRING "" FORCE)
set(CPU_DISPATCH "" CACHE STRING "" FORCE)
set(CV_DISABLE_OPTIMIZATION ON CACHE BOOL "" FORCE)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/external/opencv"
    "${CMAKE_CURRENT_BINARY_DIR}/external/opencv"
)

# OpenMP
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    get_filename_component(CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(LLVM_ROOT "${CLANG_BIN_DIR}" DIRECTORY)

    find_library(OpenMP_libomp_LIBRARY
        NAMES libomp omp
        PATHS
            "${LLVM_ROOT}/lib"
            "${LLVM_ROOT}/bin"
        REQUIRED
    )

    set(OpenMP_CXX_FLAGS "-fopenmp")
    set(OpenMP_CXX_LIB_NAMES "libomp")
endif()

find_package(OpenMP REQUIRED)

# Dear ImGui
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")

target_sources(${APP_NAME}
    PRIVATE
        "${IMGUI_DIR}/imgui.cpp"
        "${IMGUI_DIR}/imgui_draw.cpp"
        "${IMGUI_DIR}/imgui_tables.cpp"
        "${IMGUI_DIR}/imgui_widgets.cpp"

        "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
)

target_include_directories(${APP_NAME}
    PRIVATE
        "${IMGUI_DIR}"
        "${IMGUI_DIR}/backends"

        "${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/core/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/imgproc/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/imgcodecs/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/videoio/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/opencv/modules/highgui/include"

        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(${APP_NAME}
    PRIVATE
        SDL3::SDL3

        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        opencv_videoio
        opencv_highgui

        OpenMP::OpenMP_CXX
)