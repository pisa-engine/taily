cmake_minimum_required(VERSION 3.9)
project(taily VERSION 0.1 LANGUAGES CXX)

#
# CONFIGURATION
#
set(TAILY_TARGET_NAME ${PROJECT_NAME})
set(TAILY_CONFIG_INSTALL_DIR "lib/cmake/${PROJECT_NAME}" CACHE INTERNAL "")
set(TAILY_INCLUDE_INSTALL_DIR "include")
set(TAILY_TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(TAILY_CMAKE_CONFIG_TEMPLATE "cmake/config.cmake.in")
set(TAILY_CMAKE_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(TAILY_CMAKE_VERSION_CONFIG_FILE
    "${TAILY_CMAKE_CONFIG_DIR}/${PROJECT_NAME}ConfigVersion.cmake")
set(TAILY_CMAKE_PROJECT_CONFIG_FILE
    "${TAILY_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Config.cmake")
set(TAILY_CMAKE_PROJECT_TARGETS_FILE
    "${TAILY_CMAKE_CONFIG_DIR}/${PROJECT_NAME}Targets.cmake")

#
# ADD LIBRARY
#
add_library(taily INTERFACE)
add_library(taily::taily ALIAS taily)

target_include_directories(
  taily
  INTERFACE $<BUILD_INTERFACE:${taily_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)

find_package(Boost REQUIRED)
target_link_libraries(taily INTERFACE Boost::boost)

add_subdirectory(examples)
enable_testing()
add_subdirectory(test)

#
# INSTALL install header files, generate and install cmake config files for
# find_package()
#
include(CMakePackageConfigHelpers)
write_basic_package_version_file(${TAILY_CMAKE_VERSION_CONFIG_FILE}
                                 COMPATIBILITY SameMajorVersion)
configure_file(${TAILY_CMAKE_CONFIG_TEMPLATE} ${TAILY_CMAKE_PROJECT_CONFIG_FILE}
               @ONLY)

install(DIRECTORY ${TAILY_INCLUDE_BUILD_DIR}
        DESTINATION ${TAILY_INCLUDE_INSTALL_DIR})
install(FILES ${TAILY_CMAKE_PROJECT_CONFIG_FILE}
              ${TAILY_CMAKE_VERSION_CONFIG_FILE}
        DESTINATION ${TAILY_CONFIG_INSTALL_DIR})
export(TARGETS ${TAILY_TARGET_NAME}
       NAMESPACE ${PROJECT_NAME}::
       FILE ${TAILY_CMAKE_PROJECT_TARGETS_FILE})
install(TARGETS ${TAILY_TARGET_NAME}
        EXPORT ${TAILY_TARGETS_EXPORT_NAME}
        INCLUDES
        DESTINATION ${TAILY_INCLUDE_INSTALL_DIR})
install(EXPORT ${TAILY_TARGETS_EXPORT_NAME}
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${TAILY_CONFIG_INSTALL_DIR})
