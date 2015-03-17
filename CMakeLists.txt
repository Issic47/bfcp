cmake_minimum_required(VERSION 2.6)

project(bfcp C CXX)

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CXX_FLAGS
 -g
 # -DVALGRIND
 # -DMUDUO_STD_STRING
 -DCHECK_PTHREAD_RETURN_VALUE
 -D_FILE_OFFSET_BITS=64
 -Wall
 -Wextra
 -Werror
 #-Wconversion
 -Wno-unused-parameter
 #-Wold-style-cast
 -Woverloaded-virtual
 -Wpointer-arith
 #-Wshadow
 -Wwrite-strings
 -march=native
 # -MMD
 -std=c++0x
 -rdynamic
 )
if(CMAKE_BUILD_BITS EQUAL 32)
  list(APPEND CXX_FLAGS "-m32")
endif()
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -finline-limit=1000 -DNDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

set(MUDUOX_LIBS_DIR ${MUDUOX_DIR}/build/release/lib)

find_package(Boost REQUIRED)
find_path(LIBUV_INCLUDE_DIR uv.h PATHS ${LIBUV_DIR}/include REQUIRED)
find_library(LIBUV_LIBRARY uv PATHS ${LIBUV_DIR}/out/Release REQUIRED)
find_path(LIBRE_INCLUDE_DIR re.h PATHS ${LIBRE_DIR}/include REQUIRED)
find_library(LIBRE_LIBRARY re PATHS ${LIBRE_DIR} REQUIRED)
find_library(MUDUOX_BASE_LIBRARY muduo_base PATHS ${MUDUOX_LIBS_DIR} REQUIRED)
find_library(MUDUOX_NET_LIBRARY muduo_net PATHS ${MUDUOX_LIBS_DIR} REQUIRED)
find_library(TINYXML2_LIBRARY tinyxml2 PATHS ${TINYXML2_DIR} REQUIRED)
find_package(Protobuf)
find_package(CURL)
find_package(ZLIB)
find_path(CARES_INCLUDE_DIR ares.h)
find_library(CARES_LIBRARY NAMES cares)
find_path(MHD_INCLUDE_DIR microhttpd.h)
find_library(MHD_LIBRARY NAMES microhttpd)
find_library(BOOSTTEST_LIBRARY NAMES boost_unit_test_framework)
find_library(BOOSTPO_LIBRARY NAMES boost_program_options)
find_path(TCMALLOC_INCLUDE_DIR google/heap-profiler.h)
find_library(TCMALLOC_LIBRARY NAMES tcmalloc_and_profiler)
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY NAMES hiredis)

if(CARES_INCLUDE_DIR AND CARES_LIBRARY)
  message(STATUS "found cares")
endif()
#if(LIBUV_INCLUDE_DIR AND LIBUV_LIBRARY)
#  message(STATUS "found libuv")
#endif()
#if(LIBRE_INCLUDE_DIR AND LIBRE_LIBRARY)
#  message(STATUS "found libre")
#endif()
#if(TINYXML2_LIBRARY)
#  message(STATUS "found tinyxml2")
#endif()
#if(MUDUOX_BASE_LIBRARY AND MUDUOX_NET_LIBRARY)
#  message(STATUS "found muduox")
#endif()
if(CURL_FOUND)
  message(STATUS "found curl")
endif()
if(PROTOBUF_FOUND)
  message(STATUS "found protobuf")
endif()
if(TCMALLOC_INCLUDE_DIR AND TCMALLOC_LIBRARY)
  message(STATUS "found tcmalloc")
endif()
if(ZLIB_FOUND)
  message(STATUS "found zlib")
endif()
if(HIREDIS_INCLUDE_DIR AND HIREDIS_LIBRARY)
  message(STATUS "found hiredis")
endif()

include_directories(${LIBUV_INCLUDE_DIR})
include_directories(${LIBRE_INCLUDE_DIR})
include_directories(${TINYXML2_DIR})
include_directories(${MUDUOX_DIR})
set(MUDUOX_LIBS ${MUDUOX_BASE_LIBRARY} ${MUDUOX_NET_LIBRARY})

include_directories(${Boost_INCLUDE_DIRS})

include_directories(${PROJECT_SOURCE_DIR})

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "CXX_FLAGS = " ${CMAKE_CXX_FLAGS} " " ${CMAKE_CXX_FLAGS_${BUILD_TYPE}})

add_subdirectory(bfcp)
add_subdirectory(bfcp_client)
add_subdirectory(bfcp_server)
if (NOT CMAKE_BUILD_NO_SOAP_SERVER)
  add_subdirectory(bfcp_server_soap)
  add_subdirectory(server_soap_test)
endif()
#if(NOT CMAKE_BUILD_NO_EXAMPLES)
#  add_subdirectory(examples)
#else()
#  if(CARES_INCLUDE_DIR AND CARES_LIBRARY)
#    add_subdirectory(examples/cdns)
#  endif()
#endif()

