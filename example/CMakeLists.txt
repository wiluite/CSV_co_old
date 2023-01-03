include_directories(../include)

add_executable(file2matrix file2matrix.cpp)

add_custom_command(
        TARGET file2matrix POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/example/uspop.csv
        ${CMAKE_CURRENT_BINARY_DIR}/uspop.csv)

