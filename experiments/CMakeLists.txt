
# This wierdness is because I somtimes use clion to build things, and the
# temporary files end up in a strange place with a hashed file name.
# In clion you can set the place where the executables end up, but not where
# everything else ends up.
# So this exists so I can override in the IDE if necessary
set(EXPERIMENT_DIR ${CMAKE_CURRENT_BINARY_DIR})

file(MAKE_DIRECTORY ${EXPERIMENT_DIR})

set( MAX_TIME 1200 )
set( MAX_MEM 8000 )

add_custom_target(test_input)
add_custom_target(test_csv)

foreach( D IN ITEMS uniform exponential)
    add_custom_target(test_csv_${D})
    foreach( wO IN ITEMS 4 5 6 7 8 9)
        add_custom_target(test_csv_${D}_${wO})
        foreach( LF IN ITEMS 0.125 0.25 0.5 0.75 0.9375 0.96875 1.0 )
            set(acc "")
            foreach( I RANGE 16)
                add_custom_command(OUTPUT ${D}/wO_${wO}/input_${wO}_${LF}_${I}.key
                        WORKING_DIRECTORY ${EXPERIMENT_DIR}
                        COMMAND mkdir -p ${EXPERIMENT_DIR}/${D}/wO_${wO}
                        COMMAND pwd
                        COMMAND ls
                        COMMAND generate_random_hash_input --wi 16 --seed ${I} --wo ${wO} --load-factor ${LF} > ${D}/wO_${wO}/input_${wO}_${LF}_${I}.key
                        )
                #add_custom_target(input_${D}_${wO}_${LF}_${I}_key DEPENDS ${D}/wO_${wO}/input_${wO}_${LF}_${I}.key)
                #add_dependencies(test_input input_${D}_${wO}_${LF}_${I}_key)

                add_custom_command(OUTPUT ${D}/wO_${wO}/input_${wO}_${LF}_${I}.csv ${D}/wO_${wO}/input_${wO}_${LF}_${I}.sol
                        WORKING_DIRECTORY ${EXPERIMENT_DIR}
                        COMMAND perfect_fpga_hash --verbose 1 --max-time ${MAX_TIME} --max-mem ${MAX_MEM} --csv-log "${D},${wO},${LF},${I}" ${D}/wO_${wO}/input_${wO}_${LF}_${I}.csv < ${D}/wO_${wO}/input_${wO}_${LF}_${I}.key > ${D}/wO_${wO}/input_${wO}_${LF}_${I}.sol
                        #DEPENDS input_${D}_${wO}_${LF}_${I}_key
                        DEPENDS ${D}/wO_${wO}/input_${wO}_${LF}_${I}.key
                        )
                #add_custom_target(input_${D}_${wO}_${LF}_${I}_csv DEPENDS ${D}/wO_${wO}/input_${wO}_${LF}_${I}.csv)
                #add_dependencies(test_csv_${D}_${wO}_${LF} ${D}/wO_${wO}/input_${wO}_${LF}_${I}.csv)

                set(acc ${acc} ${D}/wO_${wO}/input_${wO}_${LF}_${I}.csv)
            endforeach(I)
            add_custom_target(test_csv_${D}_${wO}_${LF} DEPENDS ${acc})

            add_dependencies(test_csv_${D}_${wO} test_csv_${D}_${wO}_${LF})
        endforeach(LF)
        add_dependencies(test_csv_${D} test_csv_${D}_${wO})
    endforeach(wO)
    add_dependencies(test_csv test_csv_${D})
endforeach(D)
