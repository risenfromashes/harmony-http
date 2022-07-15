cmake_minimum_required(VERSION 3.10)

project(harmony-http)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# -fsanitize=address
if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
  set(CMAKE_CXX_FLAGS "-fno-exceptions -fsanitize=address")
else()
  set(CMAKE_CXX_FLAGS "-fno-exceptions")
endif()

add_subdirectory(third_party/readerwriterqueue)
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)


add_library(
  harmony-http STATIC
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


target_link_libraries(harmony-http PRIVATE  readerwriterqueue -static-libgcc -static-libstdc++
  ev.a nghttp2.a OpenSSL::SSL OpenSSL::Crypto Threads::Threads)


target_sources(harmony-http PUBLIC FILE_SET HEADERS
  BASE_DIRS src
  FILES src/server.h 
)

install(TARGETS harmony-http EXPORT harmony::http FILE_SET HEADERS)

