cmake_minimum_required(VERSION 3.22.1)

project(kai_console_bridge LANGUAGES CXX)

add_library(kai_console_bridge SHARED kai_console_jni.cpp)

target_compile_features(kai_console_bridge PRIVATE cxx_std_20)

find_library(log-lib log)

target_link_libraries(kai_console_bridge PRIVATE ${log-lib})
