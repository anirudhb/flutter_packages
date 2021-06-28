cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME "video_player_windows")
project(${PROJECT_NAME} LANGUAGES CXX)

include(FindFFmpeg.cmake)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/")
set(FFMPEG_HOME "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg_libs/ffmpeg-4.4-full_build-shared")
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${FFMPEG_HOME}/lib")
set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${FFMPEG_HOME}/include")

file(DOWNLOAD https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-full-shared.7z "${FFMPEG_HOME}.7z"
  SHOW_PROGRESS
  EXPECTED_HASH SHA256=4aaf80f0ee78a9eb5a1396742fd51d1624a705bfa7ce263e9b440b8be7db5650
)
file(ARCHIVE_EXTRACT INPUT "${FFMPEG_HOME}.7z"
  DESTINATION "${FFMPEG_HOME}/.."
  PATTERNS "ffmpeg-*/*"
)

find_package(FFmpeg REQUIRED COMPONENTS AVFORMAT AVCODEC AVFILTER AVUTIL SWSCALE SWRESAMPLE POSTPROC)

execute_process(COMMAND git submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(libao-cmake)
set_target_properties(libao PROPERTIES COMPILE_OPTIONS "/w")

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "video_player_windows_plugin")

add_library(${PLUGIN_NAME} SHARED
  "video_player_windows_plugin.cpp"
  "video_player_texture.cpp"
  "video_player_stream_handler.cpp"
  "messages.cpp"
  "util.cpp"
)
apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
get_target_property(LIBAO_INCLUDES libao INCLUDE_DIRECTORIES)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE ${FFMPEG_INCLUDE_DIRS} ${LIBAO_INCLUDES})
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin libao ${FFMPEG_LIBRARIES})

# List of absolute paths to libraries that should be bundled with the plugin
set(video_player_windows_bundled_libraries
  "${FFMPEG_HOME}/bin/avcodec-58.dll"
  "${FFMPEG_HOME}/bin/avfilter-7.dll"
  "${FFMPEG_HOME}/bin/avformat-58.dll"
  "${FFMPEG_HOME}/bin/avutil-56.dll"
  "${FFMPEG_HOME}/bin/postproc-55.dll"
  "${FFMPEG_HOME}/bin/swresample-3.dll"
  "${FFMPEG_HOME}/bin/swscale-5.dll"
  ${FFMPEG_LIBRARIES}
  PARENT_SCOPE
)
