PROJECT(PackageTest)
cmake_minimum_required(VERSION 2.8.12)

find_package(taily REQUIRED)
ADD_EXECUTABLE(example example.cpp)
TARGET_LINK_LIBRARIES(example taily::taily)
