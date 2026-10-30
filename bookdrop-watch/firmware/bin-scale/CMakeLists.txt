cmake_minimum_required(VERSION 3.16)
project(bookdrop_scale C)
add_executable(firmware main.c ../common/protocol.c)
