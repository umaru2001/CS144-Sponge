cmake_minimum_required (VERSION 2.8.5)
cmake_policy (SET CMP0054 NEW)
project (Sponge)

# 关闭优化并生成调试信息
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")

include (etc/build_defs.cmake)
include (etc/build_type.cmake)
include (etc/cflags.cmake)

include (etc/doxygen.cmake)

include (etc/clang_format.cmake)
include (etc/clang_tidy.cmake)
include (etc/cppcheck.cmake)

include_directories ("${PROJECT_SOURCE_DIR}/libsponge/util")
include_directories ("${PROJECT_SOURCE_DIR}/libsponge/tcp_helpers")
include_directories ("${PROJECT_SOURCE_DIR}/libsponge")

add_subdirectory ("${PROJECT_SOURCE_DIR}/libsponge")

add_subdirectory ("${PROJECT_SOURCE_DIR}/apps")

add_subdirectory ("${PROJECT_SOURCE_DIR}/tests")

add_subdirectory ("${PROJECT_SOURCE_DIR}/doctests")

include (etc/tests.cmake)
