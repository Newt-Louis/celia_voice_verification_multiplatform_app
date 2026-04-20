set(TARGET whisper-cli)
add_executable(${TARGET} cli.cpp
        ../../../../tools/windows-installer/installer_stub.cpp)

include(DefaultTargetOptions)

target_link_libraries(${TARGET} PRIVATE common whisper ${FFMPEG_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS ${TARGET} RUNTIME)
