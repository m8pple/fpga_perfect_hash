
#add_custom_command(OUTPUT generate_random_hash_input.test1 COMMAND generate_random_hash_input  )
#add_custom_target(test_generate_random_hash_input ALL DEPENDS generate_random_hash_input.test1)

add_custom_target(test_input)
add_custom_target(test_csv ALL)

foreach( wO IN ITEMS 4 5 )
    foreach( LF IN ITEMS 0.8 0.9)
        foreach( I RANGE 4)
            add_custom_command(OUTPUT input_${wO}_${LF}_${I}.key WORKING_DIRECTORY . COMMAND pwd COMMAND generate_random_hash_input --wi 16 --seed ${I} --wo ${wO} --load-factor ${LF} > input_${wO}_${LF}_${I}.key )
            #add_custom_command(OUTPUT input_${wO}_${LF}.key COMMAND exit 1 WORKING_DIRECTORY . )
            add_custom_target(input_${wO}_${LF}_${I}.keys DEPENDS input_${wO}_${LF}_${I}.key)
            add_dependencies(test_input input_${wO}_${LF}_${I}.keys)

            add_custom_command(OUTPUT input_${wO}_${LF}_${I}.csv WORKING_DIRECTORY . COMMAND ls COMMAND perfect_fpga_hash --wi 16 --wo ${wO} --csv-log wibble input_${wO}_${LF}_${I}.csv < input_${wO}_${LF}_${I}.key
                    DEPENDS input_${wO}_${LF}_${I}.keys)
            #add_custom_command(OUTPUT input_${wO}_${LF}.key COMMAND exit 1 WORKING_DIRECTORY . )
            add_custom_target(input_${wO}_${LF}_${I}.csvs DEPENDS input_${wO}_${LF}_${I}.csv)
            add_dependencies(test_csv input_${wO}_${LF}_${I}.csvs)
        endforeach(I)
    endforeach(LF)
endforeach(wO)
