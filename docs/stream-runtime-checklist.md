# Stream Runtime Checklist

## Current Implemented Scope

- Backend-managed `streams[]` is implemented and advertised only for currently running sessions.
- Managed stream planning is driven by authored stream-export metadata on blocks.
- Supported transport values for this slice are `http_snapshot`, `http_poll`, and `websocket`.
- Stored graphs may still contain legacy authored `endpoint` values, but the control-plane managed runtime path ignores them whenever stream-export metadata is valid.
- Current authoring status for this slice:
  - authored stream-export metadata is the managed-stream intent
  - `session.streams[]` plus browser-facing managed routes are the active-session runtime path
- Intentionally deferred:
  - additional sink families
  - generic transport abstractions
  - schema removal or payload redesign

## Phase 1

- Completed

- Add internal per-session lifecycle phase tracking separate from public `Session.state`
- Remove long-held `SessionService` lock coverage around slow runtime calls
- Add `SessionRuntimeResources` to the runtime layer
- Make stop/destroy completion explicit and timeout-aware
- Add tests for slow stop, restart-during-stop, remove-during-stop, and API responsiveness

## Phase 2

- Completed for the current supported families only

- Add `StreamRuntimePlan` for supported HTTP sink families
- Add `StreamBindingAllocator` and `InternalStreamBinding`
- Add optional `streams[]` serialization on `GET /sessions/{id}`
- Ignore authored `endpoint` during managed runtime planning for the supported families
- Keep payload contract unchanged

## Phase 3

- Completed for the current supported families only

- Add session-owned browser-facing HTTP exposure tied to session runtime resources
- Verify full teardown releases bindings before reuse
- Add concurrent-session tests for HTTP sink families

## Later

- Roll out additional websocket families one at a time
- Add teardown tests for lingering websocket listeners
