cmake_minimum_required(VERSION 3.16)

project(distributed_job_scheduler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(scheduler_core INTERFACE)
target_include_directories(scheduler_core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(scheduler_app app/main.cpp)
target_link_libraries(scheduler_app PRIVATE scheduler_core)

enable_testing()
