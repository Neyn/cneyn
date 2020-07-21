cmake_minimum_required(VERSION 3.5)
project(Neyn VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(NEYN_HEADERS
    "neyn/common.h"
    "neyn/neyn.h"
    "neyn/server.h"
    CACHE INTERNAL "")
string(REPLACE "neyn/" "${CMAKE_CURRENT_SOURCE_DIR}/neyn/"
    NEYN_HEADERS "${NEYN_HEADERS}")

set(NEYN_SOURCES
    "neyn/client.h"
    "neyn/parser.h"
    "neyn/client.c"
    "neyn/common.c"
    "neyn/parser.c"
    "neyn/server.c"
    CACHE INTERNAL "")
string(REPLACE "neyn/" "${CMAKE_CURRENT_SOURCE_DIR}/neyn/"
    NEYN_SOURCES "${NEYN_SOURCES}")

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

if (NEYN_STATIC_LIB)
    add_library(neyn STATIC ${NEYN_SOURCES})
else()
    add_library(neyn SHARED ${NEYN_SOURCES})
endif()

target_link_libraries(neyn PUBLIC m pthread)
target_include_directories(neyn PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/>)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/neyn/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.h")

if(NEYN_INSTALL_LIB)
    install(TARGETS neyn
        EXPORT neynConfig
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
    install(FILES ${NEYN_HEADERS}
        ${CMAKE_CURRENT_BINARY_DIR}/config.h
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
