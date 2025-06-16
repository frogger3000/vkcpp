cmake_minimum_required(VERSION 3.15)
project(metrics_collector)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Основная программа
add_library(metrics_collector
src/metrics.cpp
src/util.cpp
)

target_include_directories(metrics_collector PUBLIC include)

add_executable(example src/example.cpp)
target_link_libraries(example metrics_collector)
