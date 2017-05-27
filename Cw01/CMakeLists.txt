cmake_minimum_required(VERSION 3.0)


set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS "-Wall -g")

file(GLOB SRC1 src/Server.c src/Definitions.h)
file(GLOB SRC2 src/Client.c src/Definitions.h)

add_executable(Server ${SRC1})
add_executable(Client ${SRC2})

target_link_libraries(Server pthread)

set_target_properties(Client PROPERTIES LINKER_LANGUAGE C)
set_target_properties(Server PROPERTIES LINKER_LANGUAGE C)

set_target_properties(Server PROPERTIES RUNTIME_OUTPUT_DIRECTORY out)
set_target_properties(Client PROPERTIES RUNTIME_OUTPUT_DIRECTORY out)
