cmake_minimum_required(VERSION 3.21)
project(jecho)

set(TARGET_FILE "jecho")

aux_source_directory(./src DIR_SRCS)
add_executable(${TARGET_FILE} ${DIR_SRCS})