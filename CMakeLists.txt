cmake_minimum_required (VERSION 3.3)
project (fpga_perfect_hash)

enable_testing()

set (CMAKE_CXX_STANDARD 11)

IF(CMAKE_COMPILER_IS_GNUCC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# It is common to label loops in HLS code, but it isn't
# a problem if we don't use them.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-label")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

include_directories( include )
include_directories( external/minisat )

add_subdirectory(src)
add_subdirectory(external)
add_subdirectory(experiments)

