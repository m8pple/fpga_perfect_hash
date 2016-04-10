add_executable( test_shuffle test_shuffle.cpp )

add_test(NAME test_shuffle COMMAND test_shuffle)

add_executable( test_bit_hash test_bit_hash.cpp )
target_link_libraries(test_bit_hash hls_parser_minisat_lib)

add_executable( test_bit_hash_history test_bit_hash_history.cpp )