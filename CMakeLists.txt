cmake_minimum_required(VERSION 3.20)
project(Geai VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GEAI_ICON_B64 "${CMAKE_SOURCE_DIR}/assets/geai.ico.b64")
set(GEAI_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(GEAI_ICON_FILE "${GEAI_GENERATED_DIR}/geai.ico")
file(MAKE_DIRECTORY "${GEAI_GENERATED_DIR}")
if(EXISTS "${GEAI_ICON_B64}")
    file(READ "${GEAI_ICON_B64}" GEAI_ICON_BASE64)
    string(STRIP "${GEAI_ICON_BASE64}" GEAI_ICON_BASE64)
    string(BASE64_DECODE "${GEAI_ICON_BASE64}" GEAI_ICON_BYTES)
    file(WRITE "${GEAI_ICON_FILE}" "${GEAI_ICON_BYTES}")
endif()

configure_file(src/resource.rc ${GEAI_GENERATED_DIR}/resource.rc @ONLY)

add_executable(Geai WIN32
    src/main.cpp
    ${GEAI_GENERATED_DIR}/resource.rc
)

target_compile_definitions(Geai PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    GEAI_VERSION=L"v1.0.0"
)
target_compile_options(Geai PRIVATE /FIwindowsx.h)
target_link_libraries(Geai PRIVATE winhttp comctl32)
