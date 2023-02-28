cmake_minimum_required(VERSION 3.21)
project(jecho)

set(TARGET_FILE "jecho")

include_directories(. /usr/local/include)

aux_source_directory(./src DIR_SRCS)

find_package(uriparser 0.9.6 CONFIG REQUIRED char)

add_executable(${TARGET_FILE} ${DIR_SRCS})

target_link_libraries(jecho PUBLIC uriparser::uriparser)