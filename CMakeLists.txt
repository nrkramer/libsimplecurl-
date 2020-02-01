cmake_minimum_required(VERSION 3.16.2)

project(simplecurl++ VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_library(LIBEVENT_LIB event)
set(LIBEVENT_INCLUDE_DIR /usr/local/include)

find_library(LIBCURL_LIB curl)

include_directories(src SYSTEM ${LIBEVENT_INCLUDE_DIR})

set(SOURCES 
    cURLHelper.hpp
    cURLHelper.cpp
)

add_library(simplecurl++ SHARED ${SOURCES})
add_library(simplecurl++_static STATIC ${SOURCES})

target_link_libraries(simplecurl++ ${LIBEVENT_LIB} ${LIBCURL_LIB})