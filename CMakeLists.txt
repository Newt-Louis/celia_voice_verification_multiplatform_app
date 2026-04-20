cmake_minimum_required(VERSION 3.20)
project("Voice Embedded Verification" VERSION 1.0.0 LANGUAGES CXX)
set(TARGET_NAME Voice_Embedded_Verification)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(NPM_EXECUTABLE npm REQUIRED)

file(GLOB_RECURSE SOURCES
        "src-cpp/*.cpp"
        "src-cpp/*.h"
)

if(WIN32)
    # --- DÀNH CHO WINDOWS (MSVC) ---
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
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

add_custom_target(frontend_build
        COMMAND ${NPM_EXECUTABLE} --prefix "${CMAKE_CURRENT_SOURCE_DIR}/frontend" run build
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building Vue frontend"
        VERBATIM
)

# 5. LIÊN KẾT ONNX RUNTIME (Ví dụ)
# Bạn cần trỏ đường dẫn đến đúng file thư viện đã build cho từng OS
# set(ONNX_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/onnxruntime/${CMAKE_SYSTEM_NAME}")
# find_library(ONNX_RUNTIME onnxruntime PATHS ${ONNX_LIB_PATH})

# 6. Tạo file thực thi cuối cùng
add_executable(${TARGET_NAME} ${SOURCES})
add_dependencies(${TARGET_NAME} frontend_build)

target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src-cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src-cpp/includes
)

# 7. Link tất cả các thư viện đã tìm được vào dự án
target_link_libraries(${TARGET_NAME} PRIVATE ${PLATFORM_LIBS})

# 8. Chỉ định vị trí xuất file build (Tùy chọn)
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

set(CELIA_PACKAGE_LABEL "[WINDOWS]_TEST_V1.0.0" CACHE STRING "Build artifact folder name under builds/")
set(CELIA_PACKAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/builds/${CELIA_PACKAGE_LABEL}" CACHE PATH "Final package output directory")

add_custom_target(package_app
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CELIA_PACKAGE_DIR}/app"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CELIA_PACKAGE_DIR}/distribute"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CELIA_PACKAGE_DIR}/app"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CELIA_PACKAGE_DIR}/distribute"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${TARGET_NAME}>" "${CELIA_PACKAGE_DIR}/app/"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CELIA_PACKAGE_DIR}/app/frontend"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/frontend/dist" "${CELIA_PACKAGE_DIR}/app/frontend/dist"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "${CELIA_PACKAGE_DIR}/README.md"
        COMMAND ${CMAKE_COMMAND} -E chdir "${CELIA_PACKAGE_DIR}" ${CMAKE_COMMAND} -E tar cf "distribute/Voice Embedded Verification.zip" --format=zip app
        DEPENDS ${TARGET_NAME} frontend_build
        COMMENT "Packaging app into ${CELIA_PACKAGE_DIR}"
        VERBATIM
)
