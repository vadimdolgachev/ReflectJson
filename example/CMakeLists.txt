cmake_minimum_required(VERSION 3.30)
project(ReflectJsonExample LANGUAGES CXX)

find_package(ReflectJson CONFIG REQUIRED)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE ReflectJson::ReflectJson)
