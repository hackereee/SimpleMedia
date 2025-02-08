cmake_minimum_required(VERSION 3.10)
project(OpenGlLearning VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
configure_file(${PROJECT_SOURCE_DIR}/config/config.h.in config.h)
option(MACOS 'macos' ON)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/../libs/glfw-3.3.8-source glfw)

file(GLOB_RECURSE TRANSFORM_FILES transform/*.c**)
file(GLOB_RECURSE MEDIA_FILES ${CMAKE_CURRENT_SOURCE_DIR}/media/*.cpp)

set(MEDIA_HEADER_DIR ${PROJECT_SOURCE_DIR}/media/include)

set(TOOLKIT_DIR ${PROJECT_SOURCE_DIR}/toolkit)
set(TOOLKIT_INCLUDE_DIR ${TOOLKIT_DIR}/include)
file(GLOB_RECURSE TOOLKIT_FILES ${TOOLKIT_DIR}/*.cpp)
#打印TOOLKIT_DIR
message("TOOLKIT_INCLUDE_DIR: ${TOOLKIT_INCLUDE_DIR}")
message("PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}")

file(COPY ${PROJECT_SOURCE_DIR}/../resources DESTINATION ${PROJECT_BINARY_DIR})
file(COPY ${PROJECT_SOURCE_DIR}/../shaders DESTINATION ${PROJECT_BINARY_DIR})
file(COPY ${PROJECT_SOURCE_DIR}/media DESTINATION ${PROJECT_BINARY_DIR})
#如果local文件夹存在，就复制到build文件夹下
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../local)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../local DESTINATION ${PROJECT_BINARY_DIR})
endif ()

add_executable(Start
        main.cxx
        practice/Part1.cpp
        glad.c
        Program/shader.cpp common/gl_common.cpp
        common/TextureSample.cpp
        common/TextureSample3D.cpp
        ${TRANSFORM_FILES}
        ${MEDIA_FILES}
        ${TOOLKIT_FILES})

set(ENV (PKG_CONFIG_PATH) "/opt/homebrew/Cellar/ffmpeg/7.1_3/lib/pkgconfig")

find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED
        libavformat
        libavcodec
        libavutil
        libswscale
)
message("FFMPEG_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS}")
message("FFMPEG_LIBRARY_DIRS" , ${FFMPEG_LIBRARY_DIRS})
message("FFMPEG_LIBRARIES: ${FFMPEG_LIBRARIES}")

find_package(SDL2 REQUIRED)
message("SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
message("SDL2_LIBRARIES: ${SDL2_LIBRARIES}")

target_include_directories(Start PUBLIC
        ${PROJECT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}/../libs/glfw-3.3.8-source/include
        ${PROJECT_SOURCE_DIR}/../include
        ${FFMPEG_INCLUDE_DIRS}
        ${TOOLKIT_INCLUDE_DIR}
        ${SDL2_INCLUDE_DIRS}
        ${MEDIA_HEADER_DIR})


# set_target_properties(Start
#  PROPERTIES  
# MACOSX_BUNDLE TRUE
# MACOSX_FRAMEWORK_IDENTIFIER 
# cc.hale.Start 
# RESOURCE "${RESOURCE_FILES}"
# RESOURCE "${SHADER_FILES}")
target_link_libraries(Start PUBLIC glfw)
find_package(OpenGL REQUIRED)
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libwscale)
# 添加头文件路径

set(FFMPEG_LIBRARIES
        ${FFMPEG_LIBRARY_DIRS}/libavformat.dylib
        ${FFMPEG_LIBRARY_DIRS}/libavcodec.dylib
        ${FFMPEG_LIBRARY_DIRS}/libavutil.dylib
        ${FFMPEG_LIBRARY_DIRS}/libswscale.dylib
        ${FFMPEG_LIBRARY_DIRS}/libavdevice.dylib
        ${FFMPEG_LIBRARY_DIRS}/libavfilter.dylib
        ${FFMPEG_LIBRARY_DIRS}/libswresample.dylib)

target_link_libraries(Start PUBLIC ${FFMPEG_LIBRARIES} ${SDL2_LIBRARIES})

target_link_libraries(Start PUBLIC OpenGL::GL)

