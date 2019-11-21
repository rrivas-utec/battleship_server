cmake_minimum_required(VERSION 3.15)
project(battleship_server)

set(CMAKE_CXX_STANDARD 17)

add_executable(battleship_server main.cpp common_types.h player.cpp player.h controller.cpp controller.h)