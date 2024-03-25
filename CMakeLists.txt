cmake_minimum_required(VERSION 3.20)
project(sqlite3trans)

set(CMAKE_CXX_STANDARD 17)

add_executable(sqlite3trans main.cpp sqlite3.c DatabaseConnection.cpp DatabaseConnection.h Bean.h)
target_link_libraries(sqlite3trans pthread dl)
