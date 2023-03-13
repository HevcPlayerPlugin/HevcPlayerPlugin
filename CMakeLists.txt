cmake_minimum_required(VERSION 3.15)

project(HevcPlayerPlugin)
set(TARGET_NAME HevcPlayerPlugin)
set(CMAKE_CXX_STANDARD 17)

include_directories(3rdparty/websocketpp-0.8.2/include/asio)
include_directories(3rdparty/websocketpp-0.8.2/include)
include_directories(3rdparty/jsoncpp)

if(MSVC)
    include_directories(3rdparty/ffmpeg-5.0.1/win32/include)
    include_directories(3rdparty/SDL2-2.26.3/win32/include)
    link_directories(3rdparty/ffmpeg-5.0.1/win32/lib)
    link_directories(3rdparty/SDL2-2.26.3/win32/lib)
    list(APPEND LIBS avcodec avformat avutil swscale swresample sdl2 Dbghelp)
else(MSVC)
    include_directories(3rdparty/ffmpeg-5.0.1/mac/include)
    include_directories(3rdparty/SDL2-2.26.3/mac/include)
    link_directories(3rdparty/ffmpeg-5.0.1/mac/lib)
    link_directories(3rdparty/SDL2-2.26.3/mac/lib)
    list(APPEND LIBS avcodec avformat avutil swscale swresample sdl2)
endif(MSVC)

link_libraries(${LIBS})

file(GLOB SRC_DIR src/*.h
                  src/*.cpp
                  3rdparty/jsoncpp/*.h
                  3rdparty/jsoncpp/*.cpp)

add_executable(${TARGET_NAME} ${SRC_DIR})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/BuildOut)

if(MSVC)
    # Set VS Studio default startup
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT HevcPlayerPlugin)

    add_custom_command(TARGET HevcPlayerPlugin POST_BUILD
            COMMAND cd ${CMAKE_CURRENT_LIST_DIR}
            COMMAND xcopy .\\htdocs\\player .\\BuildOut\\$<CONFIGURATION>\\player\\ /s /e /y
            COMMAND xcopy .\\conf\\config.json .\\BuildOut\\$<CONFIGURATION>\\conf\\ /s /e /y
            COMMAND xcopy .\\3rdparty\\ffmpeg-5.0.1\\win32\\lib\\*.dll .\\BuildOut\\$<CONFIGURATION>\\ /y
            COMMAND xcopy .\\3rdparty\\SDL2-2.26.3\\win32\\lib\\*.dll .\\BuildOut\\$<CONFIGURATION>\\ /y
            COMMAND xcopy .\\3rdparty\\*.dll .\\BuildOut\\$<CONFIGURATION>\\ /y
            )
else(MSVC)
#    todo
endif(MSVC)