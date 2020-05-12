cmake_minimum_required(VERSION 3.5)

project(opengl-playground LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(opengl-playground main.cpp stb_image.h)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)

set(HEADER_FILES stb_image.h)

include_directories( {$OPENGL_INCLUDE_DIR} )
include_directories( {$SDL2_INCLUDE_DIR} )

target_link_libraries( opengl-playground GLEW GL )
target_link_libraries( opengl-playground SDL2 SDL2main)
target_link_libraries( opengl-playground assimp )
