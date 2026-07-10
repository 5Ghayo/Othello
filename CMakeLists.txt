cmake_minimum_required(VERSION 3.15)
project(Othello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try finding raylib via vcpkg or system package
find_package(raylib QUIET)

if(raylib_FOUND)
    message(STATUS "Using raylib: ${raylib_VERSION}")
else()
    # Fallback: look for raylib locally (manual install)
    set(RAYLIB_DIR ${CMAKE_SOURCE_DIR}/lib/raylib)
    if(EXISTS ${RAYLIB_DIR}/include/raylib.h)
        message(STATUS "Using local raylib at: ${RAYLIB_DIR}")
        include_directories(${RAYLIB_DIR}/include)
        link_directories(${RAYLIB_DIR}/lib)
    else()
        # Auto-download raylib by platform/compiler
        if(WIN32)
            if(MSVC)
                set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_msvc16.zip")
                set(RAYLIB_ARCHIVE "${CMAKE_BINARY_DIR}/raylib.zip")
            else()
                set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip")
                set(RAYLIB_ARCHIVE "${CMAKE_BINARY_DIR}/raylib.zip")
            endif()
        elseif(APPLE)
            set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_macos.tar.gz")
            set(RAYLIB_ARCHIVE "${CMAKE_BINARY_DIR}/raylib.tar.gz")
        endif()
        if(RAYLIB_URL)
            message(STATUS "Downloading raylib from ${RAYLIB_URL}")
            file(DOWNLOAD ${RAYLIB_URL} "${RAYLIB_ARCHIVE}" STATUS dl_status)
            if(dl_status EQUAL 0)
                file(ARCHIVE_EXTRACT INPUT "${RAYLIB_ARCHIVE}" DESTINATION "${CMAKE_BINARY_DIR}/raylib_extracted")
                file(GLOB RAYLIB_DIRS "${CMAKE_BINARY_DIR}/raylib_extracted/*")
                list(GET RAYLIB_DIRS 0 RAYLIB_DIR)
                include_directories(${RAYLIB_DIR}/include)
                link_directories(${RAYLIB_DIR}/lib)
            else()
                message(FATAL_ERROR "raylib download failed. Place raylib in lib/raylib/ or install via vcpkg.")
            endif()
        else()
            message(FATAL_ERROR "raylib not found. Install raylib via your package manager or place in lib/raylib/")
        endif()
    endif()
endif()

# Source files
set(SOURCES
    main.cpp
    board.cpp
    ai.cpp
    game.cpp
    replay.cpp
    network.cpp
)

add_executable(othello ${SOURCES})

if(WIN32)
    # Windows-specific: link multimedia and winsock libraries
    # raylib handles WinMain->main mapping internally, so WIN32_EXECUTABLE would work.
    # Console window kept for debug output during development.
    if(TARGET raylib::raylib)
        # vcpkg/system install: use imported target (auto-links GLFW etc.)
        target_link_libraries(othello PRIVATE raylib::raylib winmm ws2_32)
    else()
        # Auto-download or local: link against library file directly
        target_link_libraries(othello PRIVATE raylib winmm ws2_32)
    endif()
else()
    if(TARGET raylib::raylib)
        target_link_libraries(othello PRIVATE raylib::raylib)
    else()
        target_link_libraries(othello PRIVATE raylib)
    endif()
endif()

# MSVC: treat source files as UTF-8 (supports CJK in UTF-8 no-BOM source)
if(MSVC)
    target_compile_options(othello PRIVATE "/utf-8")
endif()


if(APPLE)
    set_target_properties(othello PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.codex.othello"
        MACOSX_BUNDLE_BUNDLE_NAME "黑白棋 Othello"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    )
    # Copy assets into macOS .app bundle Contents/Resources
    add_custom_command(TARGET othello POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_BUNDLE_CONTENT_DIR:othello>/Resources/assets"
        COMMENT "Copying assets to macOS bundle Resources"
    )
endif()
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# Post-build: copy assets alongside executable (handles MSVC multi-config builds like Release/)
add_custom_command(TARGET othello POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:othello>/assets"
    COMMENT "Copying assets to exe directory"
)

# When auto-downloading raylib (no vcpkg), copy DLL alongside exe if present
if(WIN32 AND NOT raylib_FOUND AND RAYLIB_DIR AND EXISTS "${RAYLIB_DIR}/lib/raylib.dll")
    add_custom_command(TARGET othello POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RAYLIB_DIR}/lib/raylib.dll"
            "$<TARGET_FILE_DIR:othello>"
        COMMENT "Copying raylib.dll to exe directory"
    )
endif()
