include_directories(../include)

add_executable(CSV_To_matrix CSV_To_matrix.cpp)
add_executable(CSV_To_RAM CSV_To_RAM.cpp)

add_custom_command(
        TARGET CSV_To_matrix POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/example/smallpop.csv
        ${CMAKE_CURRENT_BINARY_DIR}/smallpop.csv)

add_custom_command(
        TARGET CSV_To_RAM POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/example/smallpop.csv
        ${CMAKE_CURRENT_BINARY_DIR}/smallpop.csv)
