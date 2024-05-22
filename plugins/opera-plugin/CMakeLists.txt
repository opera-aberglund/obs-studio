cmake_minimum_required(VERSION 3.22...3.25)

add_library(opera-plugin MODULE)
add_library(OBS::opera-plugin ALIAS opera-plugin)

target_sources(opera-plugin PRIVATE color-source.c)

target_link_libraries(opera-plugin PRIVATE OBS::libobs)

# cmake-format: off
set_target_properties_obs(opera-plugin PROPERTIES FOLDER plugins PREFIX "")
# cmake-format: on
