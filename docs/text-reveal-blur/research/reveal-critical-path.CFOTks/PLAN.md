# Critical-path attribution plan (before implementation)

Scope: diagnostics only, production b54bb666 versus Source-only 05087317.
All three repositories start clean. No reset/restore/stash/history rewrite,
commit, push, shader/geometry/FBO/task/constraint/timing changes.

## Existing facilities

- Core `integration-api/trace.{h,cpp}`: `ENABLE_TRACE=ON` defines
  `TRACE_ENABLED`; runtime filters read environment variables once. Scoped
  begin/end events reach a thread-local callback. No frame ID or CPU clock.
- Adaptor `combined-update-render-controller-debug.h`: `DALI_TRACE_COMBINED=1`
  exposes the full update/render iteration, Update, PreRender, Render,
  PostRender, sleep and condition waits. Reuse these as frame boundaries.
- Core `update-manager.cpp`: `DALI_TRACE_UPDATE_PROCESS=1` exposes Animation,
  UpdateRenderers, UpdateInternal and aggregate ProcessRenderTask. No need
  to trace individual constraints/tasks. Node update lacks a duration trace.
- Adaptor `egl-graphics-controller.cpp`: `DALI_TRACE_EGL=1` exposes queue
  creation/discard/upload/command processing. These are CPU-side API durations,
  not GPU execution durations.
- TV `trace-manager-impl-tizen.cpp`: `DALI_PROFILE_TV` excludes ttrace writes;
  `DALI_TRACE_ENABLE_PRINT_LOG=1` instead emits synchronous per-event dlog.
  Swap trace is additionally excluded on TV and Ubuntu. Do NOT enable print.
- Mobile ttrace supports begin/end graphics markers. Streamline is a separate
  build option requiring its collector. Generic performance/kernel tracing
  uses a mutex/string formatting and depends on a writable tracefs backend;
  it does not supply thread CPU time. No verified target collector is connected.
- No usable existing GPU elapsed-time facility was found in the GLES path.
  GPU execution stays unknown; do not introduce timer waits, flushes or readback.

## Minimal additions

1. Diagnostic-only core trace receiver, guarded by `TRACE_ENABLED`, Linux and
   `RYU_REVEAL_ATTRIBUTION`. Reuse existing events; bounded per-thread memory,
   monotonic wall time + thread CPU time, local frame sequence; save only on
   normal thread exit. No per-frame console/file I/O or shared hot-path atomic.
   Selected fixed tags only; discard generated message strings. Report overflow
   explicitly. All emitted lines and new marker names start `RYU - `.
2. One aggregate node-update marker. No per-constraint markers.
3. Adaptor: offscreen/window scopes, TV/Ubuntu-safe swap scopes, program creation
   scope. No per-task/draw/fence instrumentation. Fence vs scheduler vs GPU wait
   remains unresolved inside wall-minus-thread-CPU time.
4. UI: preparation, publication, companion construction and scene attach scopes.
   Sample: phase markers only (Cards entrance/completion and normal exit).
   No label/page names, formatting or behavioral changes.

## Overhead / limitations

Two clocks per trace boundary; bounded buffer allocation once per traced thread
(startup cost explicitly excluded from warm comparisons). Existing message
generators still have some formatting overhead. Host OFF/ON check is a gate,
not proof of negligible target overhead. Normal exit is required to save;
kill/crash may lose data. CPU-clock deltas distinguish running from non-running
time but cannot distinguish runnable vs blocked without a scheduler trace.
Event-thread intervals overlap frame timestamps; this is NOT a proven
event-to-present flow ID. No additive sum of nested/parallel scopes.

## Build / execution strategy

Keep working checkout on Source-only; export identical UI instrumentation diff
and check application to both clean revisions. Prepare detached worktrees only
in new directories on explicit script invocation. Reuse one instrumented
core/adaptor pair for both UI builds, matching target compiler/build settings.
No build-system source changes. Supply scripts for build preparation, trace
environment, normal-exit capture and offline TSV/Chrome-trace/frame summaries.
Target toolchain/package/install commands cannot be invented: document the
required flags for the user's existing TV build workflow, plus native CMake
commands and per-prefix runtime selection where supported.

Run each variant first transition + three warm repeats, Strong, Sync,
Skeleton→Results then completed normal exit. Capture setup and steady periods;
keep all unrelated environment variables equal. FPS logging optional and equal.
Analyse >18/>25/>33ms buckets plus adjacent frames, iteration wall/CPU,
present interval, update/render/queue/swap and overlapping Event setup.
GPU column remains unknown. Only after target data select the next direction.
