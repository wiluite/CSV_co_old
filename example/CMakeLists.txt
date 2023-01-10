include_directories(../include)

add_executable(csv_2_matrix csv_2_matrix.cpp)
add_executable(csv_2_Ram csv_2_Ram.cpp)
add_executable(csv_sum csv_sum.cpp)

add_custom_command(
        TARGET csv_2_matrix POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/example/smallpop.csv
        ${CMAKE_CURRENT_BINARY_DIR}/smallpop.csv)

add_custom_command(
        TARGET csv_2_Ram POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/example/smallpop.csv
        ${CMAKE_CURRENT_BINARY_DIR}/smallpop.csv)

add_custom_command(
        TARGET csv_sum POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/example/smallpop.csv
        ${CMAKE_CURRENT_BINARY_DIR}/smallpop.csv)
