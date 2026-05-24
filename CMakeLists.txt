cmake_minimum_required(VERSION 3.16)

project(distributed_job_scheduler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(scheduler_core
    src/concurrency/ThreadPool.cpp
    src/core/Task.cpp
    src/core/Job.cpp
    src/distributed/CoordinatorServer.cpp
    src/distributed/WorkerClient.cpp
    src/distributed/WorkerRegistry.cpp
    src/execution/TaskExecutor.cpp
    src/scheduling/DependencyGraph.cpp
    src/scheduling/JobScheduler.cpp
    src/storage/InMemoryJobRepository.cpp
)
target_include_directories(scheduler_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(scheduler_core PUBLIC Threads::Threads)

add_executable(scheduler_app app/main.cpp)
target_link_libraries(scheduler_app PRIVATE scheduler_core)

add_executable(coordinator_app app/coordinator_main.cpp)
target_link_libraries(coordinator_app PRIVATE scheduler_core)

add_executable(worker_app app/worker_main.cpp)
target_link_libraries(worker_app PRIVATE scheduler_core)

add_executable(example_simple_job examples/simple_job.cpp)
target_link_libraries(example_simple_job PRIVATE scheduler_core)

add_executable(example_dependent_tasks examples/dependent_tasks.cpp)
target_link_libraries(example_dependent_tasks PRIVATE scheduler_core)

add_executable(example_failure_handling examples/failure_handling.cpp)
target_link_libraries(example_failure_handling PRIVATE scheduler_core)

enable_testing()
add_subdirectory(tests)
