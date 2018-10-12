cmake_minimum_required(VERSION 3.9)
project(taily VERSION 0.1 LANGUAGES CXX)

add_library(taily INTERFACE)
add_library(taily::taily ALIAS taily)

target_include_directories(taily INTERFACE
    $<BUILD_INTERFACE:${taily_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

find_package(Boost COMPONENTS iostreams REQUIRED)
target_link_libraries(taily INTERFACE Boost::boost)
install(FILES include/taily.hpp DESTINATION include)
export(PACKAGE taily)

add_subdirectory(examples)
enable_testing()
add_subdirectory(test)
