cmake_minimum_required(VERSION 3.20)
project(sqlite3trans)

set(CMAKE_CXX_STANDARD 17)

add_executable(sqlite3trans main.cpp sqlite3.c ac_correction.cpp ac_correction.h DatabaseConnection.cpp DatabaseConnection.h Bean.h)
