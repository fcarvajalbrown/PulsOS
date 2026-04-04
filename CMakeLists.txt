cmake_minimum_required(VERSION 3.21)
project(pulsos C CXX OBJC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CIMGUI_DIR  ${CMAKE_SOURCE_DIR}/vendor/cimgui)
set(IMPLOT_DIR  ${CMAKE_SOURCE_DIR}/vendor/implot)
set(IMGUI_DIR   ${CIMGUI_DIR}/imgui)

find_package(SDL2 REQUIRED)

# --- core sources (pure C) ---
set(CORE_SOURCES
    src/main.c
    src/core/poll.c
    src/ui/app.c
    src/ui/detail_panel.c
    src/ui/process_list.c
    src/ui/treemap.c
    src/core/fsm.c
)

# --- UI shim sources (thin C++ ImGui layer) ---
set(UI_SHIM_SOURCES
    src/ui/app_ui.cpp
    src/ui/detail_panel_ui.cpp
    src/ui/process_list_ui.cpp
    src/ui/treemap_ui.cpp
    src/ui/implot_wrapper.cpp
    src/gpu/metal_context.m
)

if(APPLE)
    list(APPEND CORE_SOURCES src/platform/proc_macos.c)
elseif(UNIX)
    list(APPEND CORE_SOURCES src/platform/proc_linux.c)
elseif(WIN32)
    list(APPEND CORE_SOURCES src/platform/proc_windows.c)
endif()

# --- imgui core + backends ---
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

add_library(imgui_lib STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_lib PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${SDL2_INCLUDE_DIRS}
    /opt/homebrew/include
)
target_compile_definitions(imgui_lib PRIVATE GL_SILENCE_DEPRECATION)

# --- Metal shader ---
set(METAL_SRC  ${CMAKE_SOURCE_DIR}/src/gpu/treemap.metal)
set(METAL_AIR  ${CMAKE_BINARY_DIR}/treemap.air)
set(METAL_LIB  ${CMAKE_BINARY_DIR}/treemap.metallib)

add_custom_command(OUTPUT ${METAL_AIR}
    COMMAND xcrun metal -c ${METAL_SRC} -o ${METAL_AIR}
    DEPENDS ${METAL_SRC})

add_custom_command(OUTPUT ${METAL_LIB}
    COMMAND xcrun metallib ${METAL_AIR} -o ${METAL_LIB}
    DEPENDS ${METAL_AIR})

add_custom_target(metal_shader DEPENDS ${METAL_LIB})

# --- executable ---
add_executable(pulsos ${CORE_SOURCES} ${UI_SHIM_SOURCES})
add_dependencies(pulsos metal_shader)

target_include_directories(pulsos PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${SDL2_INCLUDE_DIRS}
    /opt/homebrew/include
)

target_compile_definitions(pulsos PRIVATE
    GL_SILENCE_DEPRECATION
)
target_compile_options(pulsos PRIVATE
    -DMETAL_LIB_PATH="${CMAKE_BINARY_DIR}/treemap.metallib"
)

target_link_libraries(pulsos PRIVATE
    imgui_lib
    ${SDL2_LIBRARIES}
    "-framework OpenGL"
    "-framework Metal"
    "-framework Foundation"
    "-framework QuartzCore"
)