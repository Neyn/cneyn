cmake_minimum_required(VERSION 3.5)
project(CNeyn VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CNEYN_MASTER OFF CACHE INTERNAL "")
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CNEYN_MASTER ON CACHE INTERNAL "")
endif()

set(CNEYN_VERSION ${PROJECT_VERSION} CACHE INTERNAL "")
set(CNEYN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/" CACHE INTERNAL "")
set(CNEYN_LIBRARIES "cneyn" CACHE INTERNAL "")
option(CNEYN_BUILD_LIB "Build Neyn C Library" ON)
option(CNEYN_BUILD_TESTS "Build Neyn C Tests" ${CNEYN_MASTER})
option(CNEYN_INSTALL_LIB "Install Neyn C Library" ${CNEYN_MASTER})

file(GLOB HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cneyn/common.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cneyn/cneyn.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cneyn/server.h"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h")
file(GLOB SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cneyn/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cneyn/*.c")

if(CNEYN_BUILD_LIB)
    add_library(cneyn ${SOURCES})
    target_link_libraries(cneyn PUBLIC m pthread)
    target_include_directories(cneyn PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/>)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/cneyn/config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/config.h")
endif()

if(CNEYN_INSTALL_LIB)
    install(TARGETS cneyn
        EXPORT cneynConfig
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
    install(FILES ${HEADERS}
        DESTINATION include/cneyn)
    install(EXPORT cneynConfig
        NAMESPACE cneyn::
        DESTINATION lib/cmake/cneyn)
    export(TARGETS cneyn
        NAMESPACE cneyn::
        FILE ${PROJECT_BINARY_DIR}/cneynConfig.cmake)
endif()

if(CNEYN_BUILD_TESTS)
    add_executable(stage "test/stage.c")
    target_link_libraries(stage PUBLIC cneyn)
endif()
