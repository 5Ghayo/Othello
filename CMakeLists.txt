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
        # Auto-download raylib on Windows
        if(WIN32)
            message(STATUS "Downloading raylib...")
            set(RAYLIB_URL "https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip")
            file(DOWNLOAD ${RAYLIB_URL} "${CMAKE_BINARY_DIR}/raylib.zip" STATUS dl_status)
            if(dl_status EQUAL 0)
                file(ARCHIVE_EXTRACT INPUT "${CMAKE_BINARY_DIR}/raylib.zip" DESTINATION "${CMAKE_BINARY_DIR}/raylib_extracted")
                file(GLOB RAYLIB_DIRS "${CMAKE_BINARY_DIR}/raylib_extracted/*")
                list(GET RAYLIB_DIRS 0 RAYLIB_DIR)
                include_directories(${RAYLIB_DIR}/include)
                link_directories(${RAYLIB_DIR}/lib)
            else()
                message(FATAL_ERROR "raylib not found. Place raylib in lib/raylib/ or install via vcpkg.")
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
    # Windows-specific: hide console window, link ws2_32
    # WIN32_EXECUTABLE would hide the console but needs WinMain entry point
    # Our code uses standard main(), so we skip this and accept a console window.
    target_link_libraries(othello PRIVATE raylib winmm ws2_32)
else()
    target_link_libraries(othello PRIVATE raylib)
endif()

if(APPLE)
    set_target_properties(othello PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.codex.othello"
        MACOSX_BUNDLE_BUNDLE_NAME "黑白棋 Othello"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    )
endif()

# Copy assets to build directory
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
