# Future Distributed Design

## Current Starting Point

The repository now includes an initial distributed mode alongside the existing single-machine scheduler. It is still intentionally narrow:

- a text-based message format
- worker registration
- periodic worker heartbeats every 2 seconds
- coordinator-side worker liveness tracking
- dead worker marking when heartbeats are missing
- coordinator-side ready-task assignment
- worker-side task polling and result reporting

It does **not** yet include real network transport, code shipping, persistent distributed state, or production-grade coordination.

## Coordinator

The coordinator role is represented by `CoordinatorServer`.

Current responsibilities:

- accept text-based worker messages
- register workers
- record heartbeats
- track which workers appear alive
- mark workers dead after a timeout window
- keep job dependency state in memory
- assign ready tasks to polling workers
- accept task results and update job/task state
- requeue running tasks when a dead worker is detected

The current implementation is a local scaffold for protocol and registry behavior, not a production network service.

## Worker Nodes

The worker role is represented by `WorkerClient`.

Current responsibilities:

- send a registration message
- send heartbeat messages every 2 seconds
- poll the coordinator for available tasks
- execute assigned tasks through `TaskExecutor`
- report task results back through the same text protocol
- use a simple transport callback to deliver text protocol messages

Workers currently execute tasks by task name using locally registered handlers. The task callable itself is not serialized or shipped over the protocol.

## Worker Registration

Registration currently uses a text message with the shape:

```text
REGISTER worker_id=<worker-id>
```

The coordinator records:

- worker ID
- last heartbeat time
- alive/dead state

## Heartbeat

Heartbeat currently uses a text message with the shape:

```text
HEARTBEAT worker_id=<worker-id>
```

`WorkerClient` sends heartbeats every 2 seconds in a background thread. `CoordinatorServer` forwards these updates into `WorkerRegistry`.

## Dead Worker Detection

`WorkerRegistry` tracks the last heartbeat timestamp for each worker.

The coordinator can call `markDeadWorkers(timeout)` to:

1. compare the current time with the last heartbeat time
2. mark stale workers as dead
3. return the IDs that were newly marked dead

This is enough to support early liveness testing without adding a full networking layer.

## Task Assignment

Task assignment now exists in a minimal polling model:

1. the coordinator tracks job state and dependency readiness in memory
2. a worker sends `REQUEST_TASK worker_id=<worker-id>`
3. the coordinator chooses one ready task and returns an assignment
4. the worker executes the assignment locally and reports the outcome

This keeps the protocol simple while avoiding full RPC or streaming transport.

## Task Result Reporting

Task result reporting is now part of the current implementation.

Workers report:

- job ID
- task ID
- success/failure
- error message
- worker ID

The coordinator updates task state, unlocks dependents, retries if policy allows, or fails the job.

## Task Reassignment

Task reassignment now exists in a simple form:

1. detect dead worker
2. find tasks assigned to that worker but not terminal
3. move those tasks back to `Ready`
4. assign them to another live worker

This currently provides at-least-once behavior. If a worker is declared dead while still finishing a task, the stale result may arrive after the task has already been reassigned. Stronger idempotency and exactly-once semantics remain future work.
