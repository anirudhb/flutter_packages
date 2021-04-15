cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME "video_player_windows")
project(${PROJECT_NAME} LANGUAGES CXX)

include(FindFFmpeg.cmake)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/")
if(WIN32)
  set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} C:\\Users\\anirudh\\Downloads\\ffmpeg-4.4-full_build-shared\\lib)
  set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} C:\\Users\\anirudh\\Downloads\\ffmpeg-4.4-full_build-shared\\include)
endif()
# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} /WX-)
find_package(FFmpeg REQUIRED COMPONENTS AVFORMAT AVCODEC AVFILTER AVUTIL SWSCALE SWRESAMPLE POSTPROC)
# find_package(FFmpeg REQUIRED)

message(INFO ${FFMPEG_INCLUDE_DIRS})
message(INFO ${FFMPEG_LIBRARIES})

string(REPLACE " /WX" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# include(DownloadProject.cmake)
# download_project(PROJ           libao
#                  GIT_REPOSITORY https://github.com/aarcangeli/libao-cmake
#                  GIT_TAG        master)
# add_subdirectory(${libao_SOURCE_DIR} ${libao_BINARY_DIR})
# download_project(PROJ           AvCpp
#                  GIT_REPOSITORY https://github.com/h4tr3d/avcpp.git
#                  GIT_TAG        master)
# add_subdirectory(${AvCpp_SOURCE_DIR} ${AvCpp_BINARY_DIR})
add_subdirectory(libao-cmake)

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
  "C:/Users/anirudh/Downloads/ffmpeg-4.4-full_build-shared/bin/avcodec-58.dll"
  "C:/Users/anirudh/Downloads/ffmpeg-4.4-full_build-shared/bin/avfilter-7.dll"
  "C:/Users/anirudh/Downloads/ffmpeg-4.4-full_build-shared/bin/avformat-58.dll"
  "C:/Users/anirudh/Downloads/ffmpeg-4.4-full_build-shared/bin/avutil-56.dll"
  "C:/Users/anirudh/Downloads/ffmpeg-4.4-full_build-shared/bin/postproc-55.dll"
  "C:/Users/anirudh/Downloads/ffmpeg-4.4-full_build-shared/bin/swresample-3.dll"
  "C:/Users/anirudh/Downloads/ffmpeg-4.4-full_build-shared/bin/swscale-5.dll"
  ${FFMPEG_LIBRARIES}
  PARENT_SCOPE
)
