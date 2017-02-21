cmake_minimum_required (VERSION 3.0.2)
project (fpga_perfect_hash)

enable_testing()

if( ${CMAKE_VERSION} VERSION_GREATER 3.1 )
 set (CMAKE_CXX_STANDARD 11)
else ( ${CMAKE_VERSION} VERSION_GREATER 3.1 )
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif(${CMAKE_VERSION} VERSION_GREATER 3.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

IF(CMAKE_COMPILER_IS_GNUCC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# It is common to label loops in HLS code, but it isn't
# a problem if we don't use them.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-label")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

# This is to make some stuff in minisat work on down-level compilers. At some
# point it should be removed.
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS")

include_directories( include )
include_directories( external/minisat )

add_subdirectory(src)
add_subdirectory(external)
add_subdirectory(experiments)

