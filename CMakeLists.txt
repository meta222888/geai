cmake_minimum_required(VERSION 3.20)
project(Geai VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Geai WIN32
    src/main.cpp
    src/resource.rc
)

target_compile_definitions(Geai PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_compile_options(Geai PRIVATE /FIwindowsx.h)
target_link_libraries(Geai PRIVATE winhttp comctl32)
