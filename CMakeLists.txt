cmake_minimum_required(VERSION 3.19)
project(maxora VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add open.mp SDK
add_subdirectory(lib/sdk)

# Add libmaxminddb (static)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/libmaxminddb)

add_library(maxora SHARED
    src/main.cpp
    src/maxmind_store.cpp
    src/natives.cpp
)

target_include_directories(maxora PRIVATE
    src
    lib/pawn-natives
    lib/compiler/source
)

target_compile_definitions(maxora PRIVATE
    PAWN_NATIVES_HAS_FUNC
    PAWN_NATIVES_HAS_AMX
    PAWN_NATIVES_HAS_AMX_INFO
    HAVE_STDINT_H
)

target_link_libraries(maxora PRIVATE
    OMP-SDK
    maxminddb
)

if (WIN32)
    target_link_libraries(maxora PRIVATE ws2_32)
endif()

# Backend Test Executable
add_executable(maxora_test test/test_backend.cpp)
target_include_directories(maxora_test PRIVATE lib/libmaxminddb/include src)
target_link_libraries(maxora_test PRIVATE maxminddb)
if (WIN32)
    target_link_libraries(maxora_test PRIVATE ws2_32)
endif()
