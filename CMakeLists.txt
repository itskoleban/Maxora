cmake_minimum_required(VERSION 3.19...3.28)
project(maxora VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add libmaxminddb (static)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/libmaxminddb)

add_library(maxora SHARED
    src/main.cpp
    src/maxmind_store.cpp
    src/natives.cpp
)

set_target_properties(maxora PROPERTIES PREFIX "" OUTPUT_NAME "Maxora")

# Application Includes
target_include_directories(maxora PRIVATE
    src
)

# Open.mp SDK and Dependencies Includes (Marked as SYSTEM to suppress warnings)
target_include_directories(maxora SYSTEM PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/glm>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/robin-hood-hashing/src/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/span-lite/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/sdk/lib/string-view-lite/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/pawn-natives>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/compiler/source>
)

if (UNIX)
    target_include_directories(maxora SYSTEM PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/compiler/source/linux>)
endif()

target_compile_definitions(maxora PRIVATE
    PAWN_NATIVES_HAS_FUNC
    PAWN_NATIVES_HAS_AMX
    PAWN_NATIVES_HAS_AMX_INFO
    HAVE_STDINT_H=1
    HAVE_ALLOCA_H=0
    PAWN_CELL_SIZE=32
    GLM_FORCE_QUAT_DATA_WXYZ
    NOMINMAX
    nssv_CONFIG_SELECT_STRING_VIEW=nssv_STRING_VIEW_NONSTD
    span_CONFIG_SELECT_SPAN=span_SPAN_NONSTD
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    target_compile_definitions(maxora PRIVATE GLM_FORCE_SSE2)
else()
    target_compile_definitions(maxora PRIVATE GLM_FORCE_NEON)
endif()

target_link_libraries(maxora PRIVATE maxminddb)

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
