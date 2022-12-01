project(HW2 C)

set(CMAKE_C_STANDARD 11)

add_executable(HW2 myshell.c shell.c)
target_link_libraries(HW2 m)