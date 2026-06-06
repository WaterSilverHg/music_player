# CMakeLists.txt: Music Player Client (Qt6 + FFmpeg)
cmake_minimum_required(VERSION 3.16)

project(MusicPlayerClient VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Qt6 配置 ----
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Network Concurrent)
qt_standard_project_setup()

# ---- FFmpeg (静态库) ----
set(VCPKG_ROOT "C:/cppsoft/vcpkg")
set(FFMPEG_ROOT "${VCPKG_ROOT}/packages/ffmpeg_x64-windows-static")
set(FFMPEG_FOUND FALSE)

if(EXISTS "${FFMPEG_ROOT}/include")
    set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
    
    # 根据构建类型选择库目录
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/debug/lib")
    else()
        set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
    endif()
    
    find_library(FFMPEG_AVFORMAT  avformat  PATHS "${FFMPEG_LIB_DIR}")
    find_library(FFMPEG_AVCODEC   avcodec   PATHS "${FFMPEG_LIB_DIR}")
    find_library(FFMPEG_AVUTIL    avutil    PATHS "${FFMPEG_LIB_DIR}")
    find_library(FFMPEG_SWRESAMPLE swresample PATHS "${FFMPEG_LIB_DIR}")
    
    if(FFMPEG_AVFORMAT AND FFMPEG_AVCODEC AND FFMPEG_AVUTIL AND FFMPEG_SWRESAMPLE)
        set(FFMPEG_FOUND TRUE)
        message(STATUS "Found FFmpeg (static): ${FFMPEG_INCLUDE_DIR}")
        message(STATUS "  Library dir: ${FFMPEG_LIB_DIR}")
    else()
        message(WARNING "FFmpeg libraries not found")
    endif()
else()
    message(WARNING "FFmpeg not found at ${FFMPEG_ROOT}")
endif()

# ---- 源文件 ----
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/apiclient.cpp
    src/lyricsclient.cpp
    src/audio_extractor.cpp
    src/config_manager.cpp
    src/lyrics_generator.cpp
    src/lyrics_parser.cpp
    # 移除未使用的 RTSP 相关文件
    # src/rtsp_player.cpp
)

set(HEADERS
    src/mainwindow.h
    src/apiclient.h
    src/lyricsclient.h
    src/audio_extractor.h
    src/config_manager.h
    src/lyrics_generator.h
    src/lyrics_parser.h
    # 移除未使用的 RTSP 相关文件
    # src/rtsp_player.h
    src/Playlist.h
    src/global.h
)

set(RESOURCES
    picture/music.qrc
)

set(FORMS
    src/mainwindow.ui
)

# ---- 可执行文件 ----
qt_add_executable(MusicPlayerClient
    MANUAL_FINALIZATION
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
    ${FORMS}
)

# ---- 包含目录 ----
target_include_directories(MusicPlayerClient PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

if(FFMPEG_FOUND)
    target_include_directories(MusicPlayerClient PRIVATE ${FFMPEG_INCLUDE_DIR})
    target_compile_definitions(MusicPlayerClient PRIVATE USE_FFMPEG)
endif()

# ---- 链接库 ----
target_link_libraries(MusicPlayerClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Network
    Qt6::Concurrent
)

if(FFMPEG_FOUND)
    target_link_libraries(MusicPlayerClient PRIVATE
        ${FFMPEG_AVFORMAT}
        ${FFMPEG_AVCODEC}
        ${FFMPEG_AVUTIL}
        ${FFMPEG_SWRESAMPLE}
    )
endif()

# ---- 平台特定库 ----
if(WIN32)
    target_link_libraries(MusicPlayerClient PRIVATE
        ws2_32 Secur32 Bcrypt Mfuuid strmiids
        mfplat mfuuid advapi32 ole32 bcrypt secur32 crypt32 gdi32 user32
    )
endif()

# ---- 输出目录 ----
set_target_properties(MusicPlayerClient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release
)

# ---- 编译选项 ----
if(MSVC)
    target_compile_options(MusicPlayerClient PRIVATE /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS)
    target_link_options(MusicPlayerClient PRIVATE /STACK:8388608)
endif()

# ---- Qt 部署 ----
qt_finalize_executable(MusicPlayerClient)

# ---- 复制资源文件 ----
add_custom_command(TARGET MusicPlayerClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:MusicPlayerClient>/resources"
    COMMENT "Copying resources..."
)

add_custom_command(TARGET MusicPlayerClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/bin/debug/config.ini"
        "$<TARGET_FILE_DIR:MusicPlayerClient>/config.ini"
    COMMENT "Copying config.ini..."
)

add_custom_command(TARGET MusicPlayerClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:MusicPlayerClient>/playlists"
    COMMENT "Creating playlists directory..."
)