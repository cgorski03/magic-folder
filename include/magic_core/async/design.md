```mermaid
sequenceDiagram
    participant WorkerThread
    participant ProcessFileTask
    participant ProgressUpdater (Lambda)
    participant DatabaseManager
    participant TaskProgress (DB Table)

    WorkerThread->>ProcessFileTask: execute(services, on_progress_lambda)

    Note over ProcessFileTask: Begins processing...
    ProcessFileTask->>ProgressUpdater (Lambda): on_progress(0.5, "Embedding chunk 5/10")

    Note over ProgressUpdater (Lambda): Lambda captures task_id and db_manager
    ProgressUpdater (Lambda)->>DatabaseManager: update_task_progress(task_id, 0.5, "...")

    DatabaseManager->>TaskProgress (DB Table): INSERT or UPDATE progress record