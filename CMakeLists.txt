cmake_minimum_required(VERSION 3.20)
project(FlyTracer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# LTO for Release builds (modern CMake way)
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
if(IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Find Vulkan
find_package(Vulkan REQUIRED)

# =============================================================================
# External Dependencies (grouped for parallel downloads)
# =============================================================================
include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.10
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)

# Configure SDL before fetching
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

# Fetch all dependencies
FetchContent_MakeAvailable(SDL3 tinyobjloader imgui)

# =============================================================================
# Initialize git submodules if not present
# =============================================================================
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    # Check if FlyFish submodule is initialized
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/FlyFish/CMakeLists.txt")
        message(STATUS "Initializing FlyFish submodule...")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive external/FlyFish
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            RESULT_VARIABLE GIT_SUBMOD_RESULT
        )
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}")
        endif()
    endif()
endif()

# Verify FlyFish exists
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/FlyFish/CMakeLists.txt")
    message(FATAL_ERROR "FlyFish submodule not found. Please run: git submodule update --init --recursive")
endif()

# Add FlyFish library (local)
add_subdirectory(external/FlyFish)

# =============================================================================
# ImGui library with backends
# =============================================================================
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3 Vulkan::Vulkan)

# =============================================================================
# FlyTracerEngine - Static library
# =============================================================================
add_library(FlyTracerEngine STATIC
    engine/src/Application.cpp
    engine/src/VulkanRenderer.cpp
    engine/src/GameScene.cpp
    engine/src/Mesh.cpp
    engine/src/Raytracer.cpp
)

target_include_directories(FlyTracerEngine
    PUBLIC  ${CMAKE_SOURCE_DIR}/engine/include
    PRIVATE ${Vulkan_INCLUDE_DIRS}
    SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/external/FlyFish/src
)

target_link_libraries(FlyTracerEngine
    PUBLIC  SDL3::SDL3 Vulkan::Vulkan FlyFish tinyobjloader imgui
)

# Platform definitions
target_compile_definitions(FlyTracerEngine PUBLIC
    $<$<PLATFORM_ID:Darwin>:VK_USE_PLATFORM_MACOS_MVK>
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
)

# Release optimizations (target-specific)
target_compile_options(FlyTracerEngine PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang,AppleClang>,$<CONFIG:Release>>:-O3 -march=native>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang,AppleClang>,$<CONFIG:Debug>>:-g -O0>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /Ob2>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Zi /Od>
)

set_target_properties(FlyTracerEngine PROPERTIES
    VERSION ${PROJECT_VERSION}
    OUTPUT_NAME "flytracer_engine"
)

# =============================================================================
# FlyTracer - Game executable
# =============================================================================
add_executable(${PROJECT_NAME}
    game/src/main.cpp
    game/src/TestBoxScene.cpp
    game/src/DebugDemoScene.cpp
    game/src/MyCoolScene.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/game/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE FlyTracerEngine)

# =============================================================================
# Shader compilation
# =============================================================================
find_program(GLSLC glslc HINTS
    $ENV{VULKAN_SDK}/bin
    $ENV{VULKAN_SDK}/macOS/bin
    $ENV{VK_SDK_PATH}/Bin
)

if(GLSLC)
    message(STATUS "Found glslc: ${GLSLC}")

    file(GLOB_RECURSE SHADER_SOURCES
        "${CMAKE_SOURCE_DIR}/engine/shaders/*.comp"
        "${CMAKE_SOURCE_DIR}/engine/shaders/*.vert"
        "${CMAKE_SOURCE_DIR}/engine/shaders/*.frag"
    )
    file(GLOB_RECURSE SHADER_INCLUDES "${CMAKE_SOURCE_DIR}/engine/shaders/*.glsl")

    set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SHADER_OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)

        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${GLSLC} -I${CMAKE_SOURCE_DIR}/engine/shaders ${SHADER} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER} ${SHADER_INCLUDES}
            COMMENT "Compiling ${SHADER_NAME}"
            VERBATIM
        )
        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    endforeach()

    if(SHADER_OUTPUTS)
        add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS}
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${SHADER_OUTPUT_DIR}
                ${CMAKE_BINARY_DIR}/bin/shaders
        )
        add_dependencies(${PROJECT_NAME} Shaders)
    endif()
else()
    message(WARNING "glslc not found - shaders will not be compiled")
endif()

# =============================================================================
# Resources (proper dependency tracking)
# =============================================================================
add_custom_target(CopyResources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/game/resources
        ${CMAKE_BINARY_DIR}/resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/game/resources
        ${CMAKE_BINARY_DIR}/bin/resources
    COMMENT "Copying game resources"
)
add_dependencies(${PROJECT_NAME} CopyResources)


# =============================================================================
# Install rules
# =============================================================================
install(TARGETS FlyTracerEngine
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/engine/include/
    DESTINATION include/flytracer
    FILES_MATCHING PATTERN "*.h"
)
install(FILES ${CMAKE_SOURCE_DIR}/engine/include/FlyTracer.h
    DESTINATION include
)
install(DIRECTORY ${CMAKE_BINARY_DIR}/shaders/
    DESTINATION share/flytracer/shaders
    FILES_MATCHING PATTERN "*.spv"
)