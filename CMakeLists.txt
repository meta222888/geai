cmake_minimum_required(VERSION 3.20)
project(Geai VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GEAI_ICON_B64 "${CMAKE_SOURCE_DIR}/assets/geai.ico.b64")
set(GEAI_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(GEAI_ICON_FILE "${GEAI_GENERATED_DIR}/geai.ico")
file(MAKE_DIRECTORY "${GEAI_GENERATED_DIR}")

if(EXISTS "${GEAI_ICON_B64}")
    execute_process(
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command "[IO.File]::WriteAllBytes('${GEAI_ICON_FILE}', [Convert]::FromBase64String((Get-Content '${GEAI_ICON_B64}' -Raw).Trim()))"
        RESULT_VARIABLE GEAI_ICON_RESULT
    )
    if(NOT GEAI_ICON_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to generate application icon from ${GEAI_ICON_B64}")
    endif()
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
