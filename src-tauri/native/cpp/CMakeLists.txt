cmake_minimum_required(VERSION 3.20)

project(celia_audio_core LANGUAGES CXX)

add_library(celia_audio_core STATIC
    celia_audio_core.cpp
    celia_audio_core.h
)

target_compile_features(celia_audio_core PUBLIC cxx_std_17)
target_include_directories(celia_audio_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
