set(TARGET whisper-vad-speech-segments)
add_executable(${TARGET} speech.cpp
        ../../../../tools/windows-installer/installer_stub.cpp)

include(DefaultTargetOptions)

target_link_libraries(${TARGET} PRIVATE common whisper ${FFMPEG_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS ${TARGET} RUNTIME)
