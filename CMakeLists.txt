cmake_minimum_required(VERSION 3.10)

project(harmony-http)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "-fno-exceptions -fsanitize=address")
else()
  set(CMAKE_CXX_FLAGS "-fno-exceptions")
endif()

add_subdirectory(third_party/readerwriterqueue)
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
  src/util.h
  src/util.cc
)


target_link_libraries(harmony_http PRIVATE  readerwriterqueue -static-libgcc -static-libstdc++
  ev nghttp2 OpenSSL::SSL OpenSSL::Crypto Threads::Threads)


target_sources(harmony_http PUBLIC FILE_SET HEADERS
  BASE_DIRS include
  FILES include/harmony/http2.h 
)

install(TARGETS harmony_http FILE_SET HEADERS)

add_executable(test_db
  src/test_db.cc
)

target_link_libraries(test_db PRIVATE PostgreSQL::PostgreSQL ev)



