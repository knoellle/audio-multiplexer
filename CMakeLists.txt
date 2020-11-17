cmake_minimum_required(VERSION 3.10)

project(AudioMultiplexer)

find_library(JACK_LIB jack)
find_library(SoundTouch soundtouch)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    src/main.cpp
    src/multiplexer.cpp
)

add_executable(audio-multiplexer ${SOURCES})
target_link_libraries(audio-multiplexer PRIVATE
    jack
    SoundTouch
)