cmake_minimum_required(VERSION 3.18)
project(v4l2_mplane)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)

#set(CMAKE_C_COMPILER "/mnt/sdd1/code/gnu/android-ndk-r22b/android-toolchain_64/bin/aarch64-linux-android-gcc")
#set(CMAKE_CXX_COMPILER "/mnt/sdd1/code/gnu/android-ndk-r22b/android-toolchain_64/bin/aarch64-linux-android-g++")

include_directories(inc)
include_directories(inc/libyuv)

link_directories(lib)


add_executable(v4l2_mplane main.c)

add_executable(yuv_to_h264 yuv_to_h264.c)
target_link_libraries(yuv_to_h264 x264.so.164)

add_executable(libyuv_scale libyuv_scale.cpp)
target_link_libraries(libyuv_scale yuv)