cmake_minimum_required(VERSION 3.20)

project(blatman
    VERSION 1.0
    LANGUAGES CXX
)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Destino de instalacao" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

find_package(nlohmann_json 3.6.0 REQUIRED)
add_executable(blatman ${SOURCES})
target_link_libraries(blatman PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(blatman PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

install(TARGETS blatman
    RUNTIME DESTINATION bin
)
option(BUILD_TESTING "Compilar testes de regressao" OFF)
include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(blatman-publication-test tests/publication_driver.cpp src/installTransaction.cpp)
    target_include_directories(blatman-publication-test PRIVATE include)
    add_test(NAME regression COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/regression.py $<TARGET_FILE:blatman>
        $<TARGET_FILE:blatman-publication-test> ${CMAKE_CURRENT_BINARY_DIR}/test-results)
endif()
