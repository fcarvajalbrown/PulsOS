cmake_minimum_required(VERSION 3.21)
project(pulsos C CXX OBJC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- vendor paths ---
set(CIMGUI_DIR  ${CMAKE_SOURCE_DIR}/vendor/cimgui)
set(IMPLOT_DIR  ${CMAKE_SOURCE_DIR}/vendor/implot)

# --- find SDL2 (installed via Homebrew) ---
find_package(SDL2 REQUIRED)

# --- core sources (platform-agnostic) ---
set(CORE_SOURCES
    src/main.c
    src/core/poll.c
    src/ui/app.c
    src/ui/process_list.c
    src/ui/detail_panel.c
    src/ui/treemap.c
    src/gpu/metal_context.m
    src/ui/implot_wrapper.cpp
)

# --- platform backend selection ---
if(APPLE)
    list(APPEND CORE_SOURCES src/platform/proc_macos.c)
elseif(UNIX)
    list(APPEND CORE_SOURCES src/platform/proc_linux.c)
elseif(WIN32)
    list(APPEND CORE_SOURCES src/platform/proc_windows.c)
endif()

# --- cimgui (Dear ImGui C bindings) ---
set(CIMGUI_SOURCES
    ${CIMGUI_DIR}/cimgui.cpp
    ${CIMGUI_DIR}/imgui/imgui.cpp
    ${CIMGUI_DIR}/imgui/imgui_draw.cpp
    ${CIMGUI_DIR}/imgui/imgui_tables.cpp
    ${CIMGUI_DIR}/imgui/imgui_widgets.cpp
    ${CIMGUI_DIR}/imgui/backends/imgui_impl_sdl2.cpp
    ${CIMGUI_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)

# --- implot ---
set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

# --- executable ---
add_executable(pulsos
    ${CORE_SOURCES}
    ${CIMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

# --- include dirs ---
target_include_directories(pulsos PRIVATE
    src
    ${CIMGUI_DIR}
    ${CIMGUI_DIR}/imgui
    ${CIMGUI_DIR}/imgui/backends
    ${IMPLOT_DIR}
    ${SDL2_INCLUDE_DIRS}
)

# --- compile definitions ---
target_compile_definitions(pulsos PRIVATE
    CIMGUI_DEFINE_ENUMS_AND_STRUCTS  # expose ImGui enums and structs to C
    CIMGUI_USE_SDL2                  # unlock SDL2 backend in cimgui_impl.h
    CIMGUI_USE_OPENGL3               # unlock OpenGL3 backend in cimgui_impl.h
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM  # we manage OpenGL ourselves via SDL2
)

# --- platform-specific linking ---
if(APPLE)
    # Metal + system frameworks
    find_library(METAL_LIB         Metal)
    find_library(FOUNDATION_LIB    Foundation)
    find_library(QUARTZ_LIB        QuartzCore)
    find_library(COREFOUNDATION_LIB CoreFoundation)

    target_link_libraries(pulsos PRIVATE
        ${SDL2_LIBRARIES}
        ${METAL_LIB}
        ${FOUNDATION_LIB}
        ${QUARTZ_LIB}
        ${COREFOUNDATION_LIB}
        "-framework OpenGL"
    )

    # compile the Metal shader to a .metallib at build time
    set(METAL_SOURCE ${CMAKE_SOURCE_DIR}/src/gpu/treemap.metal)
    set(METAL_LIB_OUT ${CMAKE_BINARY_DIR}/treemap.metallib)

    add_custom_command(
        OUTPUT  ${METAL_LIB_OUT}
        COMMAND xcrun -sdk macosx metal -c ${METAL_SOURCE}
                      -o ${CMAKE_BINARY_DIR}/treemap.air
        COMMAND xcrun -sdk macosx metallib ${CMAKE_BINARY_DIR}/treemap.air
                      -o ${METAL_LIB_OUT}
        DEPENDS ${METAL_SOURCE}
        COMMENT "Compiling Metal shader"
    )
    add_custom_target(metal_shader DEPENDS ${METAL_LIB_OUT})
    add_dependencies(pulsos metal_shader)

    # tell the binary where to find the .metallib at runtime
    target_compile_definitions(pulsos PRIVATE
        METAL_LIB_PATH="${METAL_LIB_OUT}"
    )

elseif(UNIX)
    target_link_libraries(pulsos PRIVATE
        ${SDL2_LIBRARIES}
        GL
        m
    )

elseif(WIN32)
    target_link_libraries(pulsos PRIVATE
        ${SDL2_LIBRARIES}
        opengl32
        psapi       # GetProcessMemoryInfo
    )
endif()

# --- compiler warnings ---
if(MSVC)
    target_compile_options(pulsos PRIVATE /W4)
else()
    target_compile_options(pulsos PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- build type default ---
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()