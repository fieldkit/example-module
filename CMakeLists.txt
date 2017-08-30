project(ARDUINO_PROJECTS)
cmake_minimum_required(VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE on)

include($ENV{HOME}/conservify/arduino-cmake-minimal/cmake/ArduinoMinimal.cmake)

add_subdirectory(src)
