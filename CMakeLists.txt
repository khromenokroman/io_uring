cmake_minimum_required(VERSION 3.28)
project(app_io_uring)

set(CMAKE_CXX_STANDARD 20)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE uring)
