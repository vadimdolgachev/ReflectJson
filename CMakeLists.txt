cmake_minimum_required(VERSION 3.30)
project(ReflectJson LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(ReflectJson STATIC
        src/JsonWriter.cpp
        src/JsonReader.cpp)
add_library(ReflectJson::ReflectJson ALIAS ReflectJson)

target_compile_features(ReflectJson PUBLIC cxx_std_26)
target_compile_options(ReflectJson PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:-freflection>" PRIVATE -Wall -Wextra -Werror)
target_include_directories(ReflectJson PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
set_target_properties(ReflectJson PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)

set(ReflectJson_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/ReflectJson")
install(TARGETS ReflectJson EXPORT ReflectJsonTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY include/rj DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT ReflectJsonTargets NAMESPACE ReflectJson::
        DESTINATION "${ReflectJson_INSTALL_CMAKEDIR}")
configure_package_config_file(cmake/ReflectJsonConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/ReflectJsonConfig.cmake"
        INSTALL_DESTINATION "${ReflectJson_INSTALL_CMAKEDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ReflectJsonConfig.cmake"
        DESTINATION "${ReflectJson_INSTALL_CMAKEDIR}")

if(PROJECT_IS_TOP_LEVEL)
    add_executable(example example/main.cpp)
    target_link_libraries(example PRIVATE ReflectJson)
endif()