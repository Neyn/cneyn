cmake_minimum_required(VERSION 3.5)
project(Neyn VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(NEYN_MASTER OFF CACHE INTERNAL "")
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(NEYN_MASTER ON CACHE INTERNAL "")
endif()

set(NEYN_VERSION ${PROJECT_VERSION} CACHE INTERNAL "")
set(NEYN_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/neyn" CACHE INTERNAL "")
set(NEYN_LIBRARIES "neyn" CACHE INTERNAL "")
option(NEYN_BUILD_TESTS "Build Neyn C Tests" ${NEYN_MASTER})
option(NEYN_INSTALL_LIB "Install Neyn C Library" ${NEYN_MASTER})
option(NEYN_STATIC_LIB "Build Neyn C Static Library" OFF)

file(GLOB HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/neyn/common.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/neyn/neyn.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/neyn/server.h"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h")
file(GLOB SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/neyn/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/neyn/*.c")

if (NEYN_STATIC_LIB)
    add_library(neyn STATIC ${SOURCES})
else()
    add_library(neyn SHARED ${SOURCES})
endif()

target_link_libraries(neyn PUBLIC m pthread)
target_include_directories(neyn PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/>)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/neyn/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.h")

if(NEYN_INSTALL_LIB)
    install(TARGETS neyn
        EXPORT neynConfig
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
    install(FILES ${HEADERS}
        DESTINATION include/neyn)
    install(EXPORT neynConfig
        NAMESPACE neyn::
        DESTINATION lib/cmake/neyn)
    export(TARGETS neyn
        NAMESPACE neyn::
        FILE ${PROJECT_BINARY_DIR}/neynConfig.cmake)
endif(NEYN_INSTALL_LIB)

if(NEYN_BUILD_TESTS)
    add_executable(stage "stage/main.c")
    target_link_libraries(stage PUBLIC neyn)
endif(NEYN_BUILD_TESTS)
