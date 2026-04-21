# Stream Runtime Redesign

## Purpose

This document defines the first redesign of session stream runtime ownership from the current manual endpoint model to a session-owned model. The goal is to let `gr4-studio` consume session-advertised stream descriptors without requiring the user to author browser-facing address and port values for the active session.

This is an architecture document only. It does not authorize a broad rewrite.

## Current Implemented State

The first managed-stream slice is now implemented:

- `SessionService` owns lifecycle state transitions without holding its service mutex across slow runtime stop/destroy work.
- `RuntimeManager` owns session-scoped, generation-scoped managed stream bindings.
- `GET /sessions` and `GET /sessions/{id}` optionally return runtime-injected `streams[]` for running sessions only.
- Managed browser-facing HTTP and websocket routes are allocated per session generation.
- Stored `session.grc_content` is not rewritten; managed bindings are injected into the runtime-only graph.
- Authored stream-export metadata is the runtime input and authored `endpoint` is ignored by the control-plane managed path.

Current supported scope is intentionally narrow:

- authored stream-export metadata on blocks
- supported transport values:
  - `http_snapshot`
  - `http_poll`
  - `websocket`
- the planner no longer derives managed stream behavior from named Studio block families

Everything else in this document remains a design constraint for stabilizing and later expanding that narrow slice, not a license to broaden support opportunistically.

## Fresh Attachment Seams

The redesign should attach at these existing seams:

- `app::SessionService`
  - current lifecycle boundary
  - currently holds `mutex_` across slow runtime calls and must be narrowed
- `runtime::RuntimeManager`
  - current execution seam
  - the correct place to own runtime preparation, stream binding allocation, and teardown completion
- `domain::Session` plus HTTP session serialization
  - current public session DTO seam
  - the place to add optional runtime-injected `streams[]` without adding new product endpoints
- `runtime::Gr4RuntimeManager`
  - current GR4 execution owner
  - the place to introduce session-owned runtime resources and per-session teardown tracking
- `app::BlockSettingsService`
  - current proof that session-scoped runtime-only surfaces already exist and depend on session running state

There is no existing stream discovery or proxy subsystem in this repo to extend. This redesign starts from `SessionService` and `RuntimeManager`, not from proxy code.

## Design Goals

- Separate authored transport intent from runtime binding details.
- Make all session-owned stream resources explicit and testable.
- Keep HTTP thin and keep lifecycle rules in `SessionService`.
- Preserve legacy graph loading.
- Avoid holding service or global runtime locks across slow stop, destroy, or join paths.
- Keep the first rollout narrow: discovery plus lifecycle correctness, HTTP first, websocket later.

## Ownership Model

### Authored Data

Authored graph content remains the persisted source of block topology and sink intent.

For supported sink families, authored data should express intent such as:

- `transport = http_snapshot`
- `transport = http_poll`
- `transport = websocket`

Legacy endpoint fields may still be present in saved graphs. For the supported managed families, they are persisted data only and are not part of the control-plane runtime contract.

### Runtime-Injected Data

At prepare time, the control plane derives a session-scoped runtime stream plan from the authored graph.

Proposed internal types:

- `StreamRuntimePlan`
  - session-level immutable plan derived from the normalized graph before start
  - contains one entry per supported sink that the control plane will manage
- `RuntimeStreamBinding`
  - one planned stream instance tied to a block instance
  - contains transport intent, payload format, internal binding, browser-facing descriptor, and lifecycle state
- `InternalStreamBinding`
  - sink-internal listener/binding details owned by runtime
  - examples: listener host, listener port, bind mode, protocol family
  - never exposed as Studio's primary runtime model
- `BrowserStreamDescriptor`
  - Studio-facing session descriptor
  - examples: `id`, `block_instance_name`, `transport`, `payload_format`, `path`, `ready`
  - serializable on `GET /sessions/{id}` as optional `streams[]`
- `SessionRuntimeResources`
  - all runtime-owned state for one session
  - execution object, plan, allocated bindings, teardown status, and any registered browser exposure state
- `StreamBindingAllocator`
  - allocates and tracks internal sink bindings needed by supported transports
  - cannot release a binding until stop/destroy completion is explicit

### Browser-Facing Data

Studio should treat `session.streams[]` as the primary runtime model when present.

Rules:

- `streams[]` is runtime-injected, not authored.
- `streams[]` is session-scoped and valid only for the active prepared/running runtime generation.
- `streams[]` must not expose sink-internal bind coordinates as the source of truth.
- For the supported managed families, Studio should use `session.streams[]` plus the session-scoped browser routes instead of authored endpoint values.
- If `streams[]` is present but incomplete or invalid, the session should be treated as runtime-broken and the control plane should not silently fall back to authored endpoint values for that same runtime generation.

## Proposed Type Shape

The concrete names can vary slightly, but the ownership split should remain:

```text
SessionRuntimeResources
  session_id
  runtime_generation
  lifecycle_phase
  execution
  stream_plan : StreamRuntimePlan
  stream_bindings : vector<RuntimeStreamBinding>
  teardown_deadline
  teardown_error

StreamRuntimePlan
  streams : vector<RuntimeStreamBindingPlan>

RuntimeStreamBindingPlan
  stream_id
  block_instance_name
  transport
  payload_format

RuntimeStreamBinding
  plan
  internal_binding : InternalStreamBinding
  browser_descriptor : BrowserStreamDescriptor
  state

InternalStreamBinding
  protocol
  bind_host
  bind_port
  allocation_token

BrowserStreamDescriptor
  id
  block_instance_name
  transport
  payload_format
  path
  ready
```

## Lifecycle Invariants

The redesign must preserve these invariants:

1. A session is not fully stopped until all runtime-owned resources are released.
2. Stop completion means all of the following are complete:
   - GNU Radio execution has stopped
   - sink listener ownership is gone
   - allocated internal bindings are released
   - any control-plane-owned browser exposure registration is removed
   - session-owned stream runtime resources are destroyed
3. "Stop requested" is not equivalent to "safe to reuse resources".
4. A new start or restart generation must not reuse stream resources from a previous generation until teardown completion is confirmed.
5. A stop timeout is an explicit failure, not a silent degraded success.
6. `GET /sessions` and `GET /sessions/{id}` must remain responsive while a stop is delayed.
7. Removing a session has the same teardown completeness requirement as stopping it.

## Internal Lifecycle Phases

The current public `Session.state` can remain coarse in the first rollout: `stopped`, `running`, `error`.

Internally, the control plane should track a separate operation phase per session runtime generation:

- `idle`
- `starting`
- `running`
- `stopping`
- `failed`

This internal phase is not a new product API. It exists to make lifecycle gating deterministic without broad API changes.

Required command behavior:

- `start`
  - rejected while internal phase is `starting` or `stopping`
- `stop`
  - idempotent only when internal phase is `idle`
  - if already `stopping`, returns the current session view but does not claim completion twice
- `restart`
  - rejected while internal phase is `stopping`
  - otherwise performs a full stop-complete-then-start sequence
- `remove`
  - rejected or failed explicitly while teardown is incomplete
  - must not delete repository state before runtime-owned resources are fully released

## Lock Scope Rules

These rules are mandatory for the redesign.

### SessionService

- Do not hold `SessionService::mutex_` across `prepare`, `start`, `stop`, `destroy`, thread joins, or any wait with a timeout.
- Use the service lock only to:
  - load current session record
  - validate coarse command preconditions
  - coordinate any internal per-session operation bookkeeping
  - persist final state transitions
- Release the lock before invoking slow runtime work.

### Runtime Manager

- Do not hold a global runtime mutex across:
  - scheduler stop requests followed by waits
  - worker thread joins
  - socket/listener teardown
  - allocator release waits
- Use a short global lock only to find or create the per-session runtime entry.
- Use per-session synchronization for that session's execution and stream resources.
- Other sessions and read-only APIs must continue to progress while one session is stopping slowly.

### Repository

- The repository remains the source of persisted public session state.
- Repository operations stay small and independent.
- No repository lock may be held indirectly by a higher-level service lock while runtime calls are running.

## Compatibility With Legacy Endpoint Authorship

Legacy saved graphs with endpoint fields must still load.

Compatibility rules:

- Legacy authored endpoint values are accepted as graph input.
- For migrated supported sink families, those values are ignored by managed runtime planning and browser-route derivation.
- If the planner can derive a supported stream binding, it emits `streams[]` and the browser should prefer it.
- If the graph uses an unsupported sink family, or no managed stream can be derived, `streams[]` may be absent and legacy behavior remains unchanged.
- If the graph uses one of the currently managed sink families but the transport/config is invalid for that family, start/restart must fail explicitly.
- The endpoint UI in Studio is not removed in this phase.

This keeps old graphs loadable while shifting current-session truth to runtime-injected descriptors.

## Initial Transport Scope

Phase 1 covers only discovery and lifecycle correctness for HTTP-family transports:

- start with `http_poll`
- optionally include `http_snapshot` if it shares the same lifecycle machinery without broadening scope
- defer websocket to later phases
- do not introduce a generic transport framework beyond what HTTP-first needs

## API Shape For Existing Endpoints

No new product endpoints are required in the first implementation phase.

`GET /sessions/{id}` may grow an optional field:

```json
{
  "id": "sess_123",
  "name": "demo",
  "state": "running",
  "last_error": null,
  "created_at": "2026-04-20T00:00:00Z",
  "updated_at": "2026-04-20T00:00:05Z",
  "streams": [
    {
      "id": "sink0",
      "block_instance_name": "sink0",
      "transport": "http_poll",
      "payload_format": "existing",
      "path": "/sessions/sess_123/streams/sink0/http",
      "ready": true
    }
  ]
}
```

Notes:

- `streams[]` is optional.
- `payload_format` must refer to the existing sink payload contract. This redesign does not change payload semantics.
- The route path is a browser-facing descriptor shape, not an authorization to add new public endpoints in this step. Any future implementation of route serving must be reconciled with repo endpoint policy before code lands.

## Failure Handling

The redesign must surface these cases explicitly:

- graph A starts with `http_poll`, stop is slow, graph B start is rejected or blocked only at the session-operation seam, not via hidden port collision
- two sessions with supported sink families run concurrently with distinct allocated internal bindings
- restart during teardown is rejected explicitly instead of racing with resource release
- stop timeout leaves the session in `error` with a teardown-specific message
- lingering websocket or HTTP listener after stop request is treated as incomplete teardown, not success
- broken or partial `streams[]` generation fails the session start/restart rather than advertising a misleading descriptor

## Phased Implementation Plan

### Phase 1: Lifecycle Foundation

- Add internal runtime lifecycle tracking per session generation.
- Narrow `SessionService` lock scope so API reads remain responsive during slow stop.
- Introduce `SessionRuntimeResources` in the runtime layer.
- Make stop/destroy completion explicit and timeout-aware.
- Add tests for slow stop, concurrent reads during stop, restart rejection during stop, and remove semantics.
- Do not change Studio yet.

### Phase 2: Stream Discovery For HTTP

- Add `StreamRuntimePlan` derivation for supported HTTP sink families.
- Add `StreamBindingAllocator` and `InternalStreamBinding`.
- Populate optional `BrowserStreamDescriptor` values in `GET /sessions/{id}`.
- Ignore authored `endpoint` for the supported managed families.
- Keep payload contract unchanged.

### Phase 3: Session-Owned Browser Exposure

- Introduce the control-plane-owned exposure mechanism for HTTP session streams.
- Tie exposure registration and deregistration to `SessionRuntimeResources`.
- Verify no regression in stop/start/restart latency or stability for existing `http_poll` flows.

### Phase 4: Websocket Rollout

- Add one websocket sink family at a time.
- Reuse the same lifecycle and teardown completion rules.
- Add dedicated tests for lingering listener and bind-collision cases.

## Test Plan

### Session Service And API Responsiveness

- `GET /sessions` remains responsive while one session is in slow stop.
- `GET /sessions/{id}` remains responsive while stop is pending.
- `POST /sessions/{id}/restart` during slow stop fails explicitly and deterministically.
- `DELETE /sessions/{id}` does not remove repository state before teardown completion.

### Runtime Lifecycle Completion

- stop success requires listener release and allocator release confirmation
- stop timeout records `error` and leaves an actionable `last_error`
- second start after incomplete stop does not reuse previous stream bindings
- `destroy` after stop timeout is explicit and testable

### Stream Planning And Compatibility

- legacy graph with authored endpoint fields still loads
- managed HTTP sink graph emits `streams[]`
- unsupported or unmanaged graph omits `streams[]` without breaking legacy load
- malformed derived descriptor fails the start rather than exposing broken runtime data

### Concurrency

- two sessions with HTTP sinks can run at the same time
- one session stopping does not block settings calls or reads for another session
- per-session teardown does not require a global runtime lock

## Risks And Corner Cases

- Public session state is currently too coarse to explain `stopping`; internal operation tracking must prevent misleading success paths.
- Adding `streams[]` creates a new runtime contract and must be optional and strongly validated.
- If browser-facing exposure is implemented before teardown rules are solid, the same bind-collision failures will reappear under a different owner.
- Legacy endpoint compatibility can accidentally become the runtime truth again if planner output is allowed to silently degrade.

## First Safe Implementation Phase

The first safe code change is not stream proxying. It is lifecycle control:

- introduce internal session runtime resources
- narrow lock scope
- make stop completion explicit and timeout-aware
- add tests for slow-stop correctness and API responsiveness

Only after that foundation is proven should the repo add HTTP stream planning and `streams[]` advertisement.
