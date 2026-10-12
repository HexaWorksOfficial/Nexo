cmake_minimum_required(VERSION 3.16)

project(Nexo
    VERSION 0.1.0
    DESCRIPTION "A versatile command-line toolkit for Linux."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

file(GLOB_RECURSE NEXO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(nexo ${NEXO_SOURCES})

target_include_directories(nexo PRIVATE
    include
)

target_link_libraries(nexo PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_options(nexo PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)