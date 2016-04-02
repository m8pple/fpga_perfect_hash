set ( CMAKE_VERBOSE_MAKEFILE true )

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS")

add_library(hls_parser_minisat_lib
        core/Solver.cc
        utils/Options.cc
        utils/System.cc
        )

include_directories(hls_parser_minisat_lib PRIVATE .)


find_package(ZLIB)
target_link_libraries(hls_parser_minisat_lib ${ZLIB_LIBRARIES})

add_executable(hls_parser_minisat
        core/Main.cc
        )
target_include_directories(hls_parser_minisat PRIVATE .)
target_link_libraries(hls_parser_minisat hls_parser_minisat_lib)
