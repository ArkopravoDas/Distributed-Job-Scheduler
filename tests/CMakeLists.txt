add_executable(test_blocking_queue
    test_blocking_queue.cpp
)

target_link_libraries(test_blocking_queue
    PRIVATE
        scheduler_core
)

add_test(NAME test_blocking_queue COMMAND $<TARGET_FILE:test_blocking_queue>)

add_executable(test_thread_pool
    test_thread_pool.cpp
)

target_link_libraries(test_thread_pool
    PRIVATE
        scheduler_core
)

add_test(NAME test_thread_pool COMMAND $<TARGET_FILE:test_thread_pool>)

add_executable(test_dependency_graph
    test_dependency_graph.cpp
)

target_link_libraries(test_dependency_graph
    PRIVATE
        scheduler_core
)

add_test(NAME test_dependency_graph COMMAND $<TARGET_FILE:test_dependency_graph>)

add_executable(test_job_scheduler
    test_job_scheduler.cpp
)

target_link_libraries(test_job_scheduler
    PRIVATE
        scheduler_core
)

add_test(NAME test_job_scheduler COMMAND $<TARGET_FILE:test_job_scheduler>)

add_executable(test_retry_support
    test_retry_support.cpp
)

target_link_libraries(test_retry_support
    PRIVATE
        scheduler_core
)

add_test(NAME test_retry_support COMMAND $<TARGET_FILE:test_retry_support>)

add_executable(test_timeout_support
    test_timeout_support.cpp
)

target_link_libraries(test_timeout_support
    PRIVATE
        scheduler_core
)

add_test(NAME test_timeout_support COMMAND $<TARGET_FILE:test_timeout_support>)

add_executable(test_failure_cases
    test_failure_cases.cpp
)

target_link_libraries(test_failure_cases
    PRIVATE
        scheduler_core
)

add_test(NAME test_failure_cases COMMAND $<TARGET_FILE:test_failure_cases>)
