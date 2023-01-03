include_directories(../include)

add_executable(test main.cpp)

add_custom_command(
        TARGET test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/test/game.csv
        ${CMAKE_CURRENT_BINARY_DIR}/game.csv)

add_custom_command(
        TARGET test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/example/smallpop.csv
        ${CMAKE_CURRENT_BINARY_DIR}/smallpop.csv)
