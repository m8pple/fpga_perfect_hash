add_executable( generate_random_hash_input generate_random_hash_input.cpp)

#add_custom_command(OUTPUT generate_random_hash_input.test1 COMMAND generate_random_hash_input  )
#add_custom_target(test_generate_random_hash_input ALL DEPENDS generate_random_hash_input.test1)

add_executable( perfect_fpga_hash perfect_fpga_hash.cpp )
target_link_libraries(perfect_fpga_hash hls_parser_minisat_lib)

add_executable( random_fpga_hash random_fpga_hash.cpp )

add_executable( write_fpga_hash_cpp write_fpga_hash_cpp.cpp )
