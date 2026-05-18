# Milestone Plan

## Completed Milestones

### Milestone 1: Project Foundation

- created the repository skeleton
- added CMake-based C++20 build setup
- added Windows and bash helper scripts
- added baseline documentation

### Milestone 2: Core Domain Model

- implemented `Job`
- implemented `Task`
- added scheduler type aliases
- added job/task status enums

### Milestone 3: Local Runtime Primitives

- implemented `BlockingQueue<T>`
- implemented `ThreadPool`
- implemented `TaskExecutor`

### Milestone 4: Scheduling and Storage

- implemented `JobRepository`
- implemented `InMemoryJobRepository`
- implemented `DependencyGraph`
- implemented `JobScheduler`

### Milestone 5: Validation, Failure Cases, and Examples

- added cycle detection handling
- added invalid dependency handling
- added duplicate task ID rejection
- added scheduler shutdown correctness fixes
- added example programs
- expanded automated test coverage

### Milestone 6: Documentation Refresh

- documented implemented local architecture
- documented current limitations
- documented future distributed direction without marking it as implemented

## Next Milestones

### Milestone 7: Retry and Richer Task Policies

- automatic retry handling using `maxRetries`
- retry backoff policy
- distinguish retryable vs terminal failures

### Milestone 8: Persistence and Recovery

- persistent repository backend
- load jobs after restart
- recover incomplete jobs safely

### Milestone 9: Public Interfaces

- CLI or service interface for job submission
- structured logging
- configuration support

### Milestone 10: Distributed Coordinator/Worker Design

- coordinator process
- worker registration
- heartbeat tracking
- remote task assignment
- task result reporting

### Milestone 11: Fault Tolerance and Reassignment

- dead worker detection
- task reassignment
- idempotency rules
- distributed retry semantics
