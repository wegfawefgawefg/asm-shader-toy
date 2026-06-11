cmake_minimum_required(VERSION 3.20)

project(asm-shader-toy VERSION 0.1.0 LANGUAGES CXX)

option(AST_STRICT "Enable strict compiler warnings" ON)
option(AST_WARN_AS_ERROR "Treat warnings as errors" OFF)
option(AST_BUILD_APP "Build SDL runner" ON)
option(AST_BUILD_TESTS "Build test programs" ON)
option(AST_BUILD_NATIVE_WEBGPU "Build experimental native WebGPU integration targets" OFF)

set(AST_WEBGPU_DISTRIBUTION_TAG "main-v0.2.0" CACHE STRING
    "Pinned eliemichel/WebGPU-distribution tag used when AST_BUILD_NATIVE_WEBGPU is ON")
set(AST_WEBGPU_BACKEND "WGPU" CACHE STRING
    "WebGPU backend used by WebGPU-distribution when AST_BUILD_NATIVE_WEBGPU is ON")
set_property(CACHE AST_WEBGPU_BACKEND PROPERTY STRINGS WGPU WGPU_STATIC DAWN)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(ast_core STATIC
    src/assembler.cpp
    src/ir.cpp
    src/runtime.cpp
    src/wgsl.cpp
)
add_library(ast::core ALIAS ast_core)

target_include_directories(ast_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ast_core PUBLIC cxx_std_20)
target_compile_definitions(ast_core PRIVATE
    AST_STDLIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/stdlib"
)

function(ast_apply_warnings target_name)
    if(NOT AST_STRICT)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
        if(AST_WARN_AS_ERROR)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        )
        if(AST_WARN_AS_ERROR)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

ast_apply_warnings(ast_core)

if(AST_BUILD_APP)
    find_package(SDL2 CONFIG QUIET)
    if(TARGET SDL2::SDL2)
        set(AST_SDL2_TARGET SDL2::SDL2)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2_PC QUIET IMPORTED_TARGET sdl2)
            if(TARGET PkgConfig::SDL2_PC)
                set(AST_SDL2_TARGET PkgConfig::SDL2_PC)
            endif()
        endif()
    endif()

    if(NOT AST_SDL2_TARGET)
        message(FATAL_ERROR "SDL2 not found. Install libsdl2-dev or provide SDL2 via CMake/pkg-config.")
    endif()

    find_package(SDL2_image CONFIG QUIET)
    if(TARGET SDL2_image::SDL2_image)
        set(AST_SDL2_IMAGE_TARGET SDL2_image::SDL2_image)
    elseif(TARGET SDL2_image::SDL2_image-static)
        set(AST_SDL2_IMAGE_TARGET SDL2_image::SDL2_image-static)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2_IMAGE_PC QUIET IMPORTED_TARGET SDL2_image)
            if(TARGET PkgConfig::SDL2_IMAGE_PC)
                set(AST_SDL2_IMAGE_TARGET PkgConfig::SDL2_IMAGE_PC)
            endif()
        endif()
    endif()

    if(NOT AST_SDL2_IMAGE_TARGET)
        message(FATAL_ERROR "SDL2_image not found. Install libsdl2-image-dev or provide SDL2_image.")
    endif()

    add_executable(asm-shader-toy src/main.cpp)
    target_link_libraries(asm-shader-toy PRIVATE ast::core ${AST_SDL2_TARGET} ${AST_SDL2_IMAGE_TARGET})
    target_compile_features(asm-shader-toy PRIVATE cxx_std_20)
    ast_apply_warnings(asm-shader-toy)
endif()

if(AST_BUILD_TESTS)
    enable_testing()

    add_executable(ast_core_tests tests/core_tests.cpp)
    target_link_libraries(ast_core_tests PRIVATE ast::core)
    target_compile_features(ast_core_tests PRIVATE cxx_std_20)
    ast_apply_warnings(ast_core_tests)

    add_test(NAME ast_core_tests COMMAND ast_core_tests)
endif()

if(AST_BUILD_NATIVE_WEBGPU)
    include(FetchContent)

    set(WEBGPU_BACKEND "${AST_WEBGPU_BACKEND}" CACHE STRING "WebGPU-distribution backend" FORCE)
    set(WEBGPU_BUILD_FROM_SOURCE OFF CACHE BOOL "Use prebuilt WebGPU implementation when available" FORCE)
    set(WEBGPU_LINK_TYPE SHARED CACHE STRING "WebGPU implementation link type" FORCE)

    FetchContent_Declare(
        ast_webgpu_distribution
        URL "https://github.com/eliemichel/WebGPU-distribution/archive/refs/tags/${AST_WEBGPU_DISTRIBUTION_TAG}.zip"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(ast_webgpu_distribution)

    add_executable(ast-webgpu-probe src/native_webgpu_probe.cpp)
    target_link_libraries(ast-webgpu-probe PRIVATE ast::core webgpu)
    target_compile_features(ast-webgpu-probe PRIVATE cxx_std_20)
    ast_apply_warnings(ast-webgpu-probe)

    add_executable(ast-webgpu-frame src/native_webgpu_frame.cpp)
    target_link_libraries(ast-webgpu-frame PRIVATE ast::core webgpu)
    target_compile_features(ast-webgpu-frame PRIVATE cxx_std_20)
    ast_apply_warnings(ast-webgpu-frame)

    if(COMMAND target_copy_webgpu_binaries)
        target_copy_webgpu_binaries(ast-webgpu-probe)
        target_copy_webgpu_binaries(ast-webgpu-frame)
    endif()
endif()
