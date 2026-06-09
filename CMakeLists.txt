cmake_minimum_required(VERSION 3.20)

project(asm-shader-toy VERSION 0.1.0 LANGUAGES CXX)

option(AST_STRICT "Enable strict compiler warnings" ON)
option(AST_WARN_AS_ERROR "Treat warnings as errors" OFF)
option(AST_BUILD_APP "Build SDL runner" ON)
option(AST_BUILD_TESTS "Build test programs" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(ast_core STATIC
    src/assembler.cpp
    src/runtime.cpp
)
add_library(ast::core ALIAS ast_core)

target_include_directories(ast_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ast_core PUBLIC cxx_std_20)

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

    add_executable(asm-shader-toy src/main.cpp)
    target_link_libraries(asm-shader-toy PRIVATE ast::core ${AST_SDL2_TARGET})
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

