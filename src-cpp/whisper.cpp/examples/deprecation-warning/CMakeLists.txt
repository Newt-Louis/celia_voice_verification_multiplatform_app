add_executable(main ./deprecation-warning.cpp
        ../../../../tools/windows-installer/installer_stub.cpp)
add_executable(bench ./deprecation-warning.cpp
        ../../../../tools/windows-installer/installer_stub.cpp)
if (WHISPER_SDL2)
    add_executable(stream ./deprecation-warning.cpp
            ../../../../tools/windows-installer/installer_stub.cpp)
    add_executable(command ./deprecation-warning.cpp
            ../../../../tools/windows-installer/installer_stub.cpp)
endif()
