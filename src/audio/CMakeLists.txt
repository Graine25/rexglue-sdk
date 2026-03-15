# rexaudio - Audio subsystem library
# Audio processing and XMA decoding for Xbox 360 emulation

set(REXAUDIO_SOURCES
    audio_driver.cpp
    audio_system.cpp
    xma_context.cpp
    xma_decoder.cpp
    xma_register_file.cpp
    # NOP backend (always available)
    nop/nop_audio_system.cpp
)

if(NOT APPLE)
    list(APPEND REXAUDIO_SOURCES
        sdl/sdl_audio_system.cpp
        sdl/sdl_audio_driver.cpp
    )
endif()

add_library(rexaudio STATIC ${REXAUDIO_SOURCES})
add_library(rex::audio ALIAS rexaudio)

target_include_directories(rexaudio PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(rexaudio
    PUBLIC rexcore
    PRIVATE libavcodec libavutil
)

if(NOT APPLE)
    target_link_libraries(rexaudio PUBLIC SDL2::SDL2)
endif()
