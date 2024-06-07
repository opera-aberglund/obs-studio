cmake_minimum_required(VERSION 3.22...3.25)

legacy_check()

add_library(sink-source MODULE)
add_library(OBS::sink-source ALIAS sink-source)

target_sources(sink-source PRIVATE sink-source.cpp sink-file-util.cpp sink-image-util.cpp sink-socket.cpp)

target_link_libraries(sink-source PRIVATE OBS::libobs $<$<PLATFORM_ID:Windows>:OBS::w32-pthreads> FFmpeg::avformat FFmpeg::avcodec FFmpeg::avutil FFmpeg::swscale datachannel-static)

if (OS_WINDOWS)
    configure_file(cmake/windows/obs-module.rc.in sink-source.rc)
    target_sources(sink-source PRIVATE sink-source.rc)
endif ()

target_include_directories(sink-source PRIVATE ${FFmpeg_INCLUDE_DIRS})

# cmake-format: off
set_target_properties_obs(sink-source PROPERTIES FOLDER plugins PREFIX "")
# cmake-format: on

add_subdirectory(libdatachannel)
