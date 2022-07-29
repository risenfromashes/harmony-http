cmake_minimum_required(VERSION 3.10)

project(harmony-http)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "-fsanitize=address")
endif()

add_subdirectory(third_party/readerwriterqueue)
add_subdirectory(third_party/simdjson EXCLUDE_FROM_ALL)
add_subdirectory(third_party/stduuid)
add_subdirectory(third_party/memory)

# don't include nghttp2 source
# SET(ENABLE_STATIC_LIB TRUE)
# add_subdirectory(third_party/nghttp2)

# set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PostgreSQL REQUIRED)


add_library(
  harmony_http STATIC
  src/server.h
  src/server.cc
  src/worker.h
  src/worker.cc
  src/dbsession.h
  src/dbsession.cc
  src/dbquery.h
  src/dbresult.h
  src/dbresult.cc
  src/dbconnection.h
  src/dbconnection.cc
  src/httprouter.h
  src/httprouter.cc
  src/httprequest.h
  src/httprequest.cc
  src/httpresponse.h
  src/httpresponse.cc
  src/httpsession.h
  src/httpsession.cc
  src/stream.h
  src/stream.cc
  src/datastream.h
  src/filestream.h
  src/filestream.cc
  src/stringstream.h
  src/stringstream.cc
  src/buffer.h
  src/uuidgenerator.h
  src/task.h
  src/util.h
  src/util.cc
)


target_link_libraries(harmony_http PRIVATE  
  -static-libgcc -static-libstdc++
  readerwriterqueue 
  simdjson
  stduuid
  foonathan_memory
  PostgreSQL::PostgreSQL 
  jemalloc
  nghttp2
  ev 
  OpenSSL::SSL
  OpenSSL::Crypto
  Threads::Threads
)


target_sources(harmony_http PUBLIC FILE_SET HEADERS
  BASE_DIRS include
  FILES include/harmony/http2.h 
)

install(TARGETS harmony_http FILE_SET HEADERS)

add_executable(exp
  src/exp.cc
)




