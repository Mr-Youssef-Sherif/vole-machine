cmake_minimum_required(VERSION 3.20)
project(VoleMachineSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# GLFW provides the window and OpenGL context.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw)

# Dear ImGui is compiled from the locally downloaded repository.
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui-master")

if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR "Dear ImGui not found at ${IMGUI_DIR}")
endif()

add_library(dear_imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(dear_imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(dear_imgui PUBLIC glfw)

# OpenGL
find_package(OpenGL REQUIRED)

# Simulator backend
add_library(vole_backend STATIC
    vole_machine.cpp
    vole_machine.h
)

target_include_directories(vole_backend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# UI application
add_executable(vole_imgui
    ui_client.cpp
)

target_link_libraries(vole_imgui PRIVATE
    vole_backend
    dear_imgui
    glfw
    OpenGL::GL
)

# Native Windows Open File / Save File dialogs.
if(WIN32)
    target_link_libraries(vole_imgui PRIVATE comdlg32)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(vole_backend PRIVATE /W4)
    target_compile_options(vole_imgui PRIVATE /W4)
else()
    target_compile_options(vole_backend PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(vole_imgui PRIVATE -Wall -Wextra -Wpedantic)
endif()
