cmake_minimum_required(VERSION 3.20)
project("Voice Embedded Verification" VERSION 1.0.0 LANGUAGES CXX)
set(TARGET_NAME Voice_Embedded_Verification)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/includes)

file(GLOB_RECURSE SOURCES
        "src-cpp/*.cpp"
        "src-cpp/*.h"
)

if(WIN32)
    # --- DÀNH CHO WINDOWS (MSVC) ---
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(PLATFORM_LIBS
            ole32
            version
            shlwapi
            windowscodecs
    )

    # Nếu dùng WebView2 trên Windows, cần link loader của nó
    # find_library(WEBVIEW2_LIB WebView2Loader PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libs/windows)
    # list(APPEND PLATFORM_LIBS ${WEBVIEW2_LIB})

elseif(ANDROID)
    find_library(log-lib log)
    find_library(android-lib android)
    # Android cần OpenSL ES hoặc AAudio cho âm thanh
    set(PLATFORM_LIBS
            ${log-lib}
            ${android-lib}
            OpenSLES
    )

elseif(APPLE)
    # --- DÀNH CHO macOS VÀ iOS ---
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

# 5. LIÊN KẾT ONNX RUNTIME (Ví dụ)
# Bạn cần trỏ đường dẫn đến đúng file thư viện đã build cho từng OS
# set(ONNX_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/onnxruntime/${CMAKE_SYSTEM_NAME}")
# find_library(ONNX_RUNTIME onnxruntime PATHS ${ONNX_LIB_PATH})

# 6. Tạo file thực thi cuối cùng
add_executable(${TARGET_NAME} ${SOURCES}
        src-cpp/main.cpp
        src-cpp/main.h)

# 7. Link tất cả các thư viện đã tìm được vào dự án
target_link_libraries(${TARGET_NAME} PRIVATE ${PLATFORM_LIBS})

# 8. Chỉ định vị trí xuất file build (Tùy chọn)
set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "Voice Embedded Verification"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)