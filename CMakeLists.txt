cmake_minimum_required(VERSION 3.10)
project(ErasureKVCache)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ISAL REQUIRED libisal)
find_library(MEMCACHED_LIBRARY memcached REQUIRED)

include_directories(${ISAL_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(ECKVCache
    main.cpp
    KVStatReader.cpp
    ErasureCacheClient.cpp
    TraceReplayRunner.cpp
)

target_link_libraries(ECKVCache
    ${ISAL_LIBRARIES}
    ${MEMCACHED_LIBRARY}
)