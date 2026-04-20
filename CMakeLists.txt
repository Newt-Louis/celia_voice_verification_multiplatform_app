cmake_minimum_required(VERSION 3.20)
project("Voice Embedded Verification" VERSION 1.0.0 LANGUAGES CXX)
set(TARGET_NAME Voice_Embedded_Verification)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    find_program(NPM_CMD_EXECUTABLE NAMES npm.cmd REQUIRED)
else()
    find_program(NPM_CMD_EXECUTABLE NAMES npm REQUIRED)
endif()

file(GLOB_RECURSE SOURCES
        "src-cpp/app/*.cpp"
        "src-cpp/app/*.h"
        "src-cpp/audio/*.cpp"
        "src-cpp/audio/*.h"
        "src-cpp/main.cpp"
)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/utf-8)
    set(PLATFORM_LIBS
            advapi32
            dwmapi
            ole32
            shell32
            version
            shlwapi
            user32
            windowscodecs
            ws2_32
    )
elseif(ANDROID)
    find_library(log-lib log)
    find_library(android-lib android)
    set(PLATFORM_LIBS
            ${log-lib}
            ${android-lib}
            OpenSLES
    )
elseif(APPLE)
    find_library(COCOA_LIB Cocoa)
    find_library(WEBKIT_LIB WebKit)
    find_library(COREAUDIO_LIB CoreAudio)
    set(PLATFORM_LIBS
            ${COCOA_LIB}
            ${WEBKIT_LIB}
            ${COREAUDIO_LIB}
    )

    if(IOS)
        add_definitions(-DTARGET_OS_IPHONE)
    endif()
endif()

set(SHERPA_ONNX_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sherpa-onnx/sherpa-onnx-v1.12.39-win-x64-shared-MD-Release-no-tts"
        CACHE PATH "sherpa-onnx prebuilt runtime root")
set(SHERPA_ONNX_INCLUDE_DIR "${SHERPA_ONNX_ROOT}/include")
set(SHERPA_ONNX_LIB_DIR "${SHERPA_ONNX_ROOT}/lib")
set(SHERPA_ONNX_LIBS
        "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-cxx-api.lib"
        "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-c-api.lib"
        "${SHERPA_ONNX_LIB_DIR}/onnxruntime.lib"
)
set(SHERPA_ONNX_DLLS
        "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-cxx-api.dll"
        "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-c-api.dll"
        "${SHERPA_ONNX_LIB_DIR}/onnxruntime.dll"
        "${SHERPA_ONNX_LIB_DIR}/onnxruntime_providers_shared.dll"
)

foreach(SHERPA_FILE IN LISTS SHERPA_ONNX_LIBS SHERPA_ONNX_DLLS)
    if(NOT EXISTS "${SHERPA_FILE}")
        message(FATAL_ERROR "Missing sherpa-onnx runtime file: ${SHERPA_FILE}")
    endif()
endforeach()

add_custom_target(frontend_build
        COMMAND ${NPM_CMD_EXECUTABLE} --prefix "${CMAKE_CURRENT_SOURCE_DIR}/frontend" run build
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building Vue frontend"
        VERBATIM
)

add_executable(${TARGET_NAME} ${SOURCES})
add_dependencies(${TARGET_NAME} frontend_build)

target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src-cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src-cpp/includes
        ${SHERPA_ONNX_INCLUDE_DIR}
)

target_link_libraries(${TARGET_NAME} PRIVATE
        ${SHERPA_ONNX_LIBS}
        ${PLATFORM_LIBS}
)

set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "Voice Embedded Verification"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET_NAME}>/frontend"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/frontend/dist"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>/frontend/dist"
        COMMENT "Copying frontend dist next to executable"
        VERBATIM
)

foreach(SHERPA_DLL IN LISTS SHERPA_ONNX_DLLS)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SHERPA_DLL}"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
            COMMENT "Copying ${SHERPA_DLL}"
            VERBATIM
    )
endforeach()

set(CELIA_PACKAGE_LABEL "[WINDOWS]_TEST_V1.0.0" CACHE STRING "Build artifact folder name under builds/")
set(CELIA_PACKAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/builds/${CELIA_PACKAGE_LABEL}" CACHE PATH "Final package output directory")
set(CELIA_MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/models" CACHE PATH "Runtime model directory")
set(CELIA_SHERPA_MODEL_NAME "sherpa-onnx-streaming-zipformer-ar_en_id_ja_ru_th_vi_zh-2025-02-10")
set(CELIA_SHERPA_MODEL_DIR "${CELIA_MODELS_DIR}/${CELIA_SHERPA_MODEL_NAME}")
set(CELIA_ECAPA_MODEL "${CELIA_MODELS_DIR}/v2_int8_fast/ecapa_int8_dynamic.onnx")

set(CELIA_PACKAGE_RUNTIME_COMMANDS "")
foreach(SHERPA_DLL IN LISTS SHERPA_ONNX_DLLS)
    list(APPEND CELIA_PACKAGE_RUNTIME_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SHERPA_DLL}" "${CELIA_PACKAGE_DIR}/app/"
    )
endforeach()

set(CELIA_PACKAGE_MODEL_COMMANDS "")
if(EXISTS "${CELIA_SHERPA_MODEL_DIR}")
    list(APPEND CELIA_PACKAGE_MODEL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CELIA_SHERPA_MODEL_DIR}" "${CELIA_PACKAGE_DIR}/app/models/${CELIA_SHERPA_MODEL_NAME}"
    )
endif()
if(EXISTS "${CELIA_ECAPA_MODEL}")
    list(APPEND CELIA_PACKAGE_MODEL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CELIA_PACKAGE_DIR}/app/models/v2_int8_fast"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CELIA_ECAPA_MODEL}" "${CELIA_PACKAGE_DIR}/app/models/v2_int8_fast/"
    )
endif()

add_custom_target(package_app
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CELIA_PACKAGE_DIR}/app"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CELIA_PACKAGE_DIR}/distribute"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CELIA_PACKAGE_DIR}/app"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CELIA_PACKAGE_DIR}/distribute"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${TARGET_NAME}>" "${CELIA_PACKAGE_DIR}/app/"
        ${CELIA_PACKAGE_RUNTIME_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CELIA_PACKAGE_DIR}/app/frontend"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/frontend/dist" "${CELIA_PACKAGE_DIR}/app/frontend/dist"
        ${CELIA_PACKAGE_MODEL_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "${CELIA_PACKAGE_DIR}/README.md"
        COMMAND ${CMAKE_COMMAND} -E chdir "${CELIA_PACKAGE_DIR}" ${CMAKE_COMMAND} -E tar cf "distribute/Voice Embedded Verification.zip" --format=zip app
        DEPENDS ${TARGET_NAME} frontend_build
        COMMENT "Packaging app into ${CELIA_PACKAGE_DIR}"
        VERBATIM
)
