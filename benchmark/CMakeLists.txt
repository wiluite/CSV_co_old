include_directories(../include)

add_executable(spanbench spanbench.cpp)

add_custom_command(
        TARGET spanbench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/benchmark/game.csv
        ${CMAKE_CURRENT_BINARY_DIR}/game.csv)
