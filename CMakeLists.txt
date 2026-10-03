cmake_minimum_required(VERSION 3.20)

project(blatman
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/functions/*.cpp"
)

add_executable(blatman ${SOURCES})

target_include_directories(blatman PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)