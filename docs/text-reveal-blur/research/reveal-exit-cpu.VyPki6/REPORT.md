# Text-effect-demo exit CPU attribution

2026-09-16. Analysis only. No DALi UI/core/adaptor/sample source edits,
commits, driver settings, or system permission changes.

## Conclusion

The previously observed ~182→335 ms process-CPU difference is primarily CPU
consumption **inside NVIDIA `glMapBufferRange` while DALi maps its offscreen
uniform buffer**, not extra PER_LINE rasterization, fade calculations, or
constraint evaluation. CPU sampling finds repeated NVIDIA wait-loop instructions
and clock polling. This identifies a buffer-mapping synchronization/wait hotspot;
it does not by itself establish a driver correctness bug or the precise GPU
dependency responsible for the longer wait.

## Reproduction and controls

- Same unchanged text-effect-demo source, 1280x720, Sync, Strong exit radius 48,
  WHOLE_TEXT / Fade=1 / Stagger=0 / BlurTime=1 / Linear / 0.4 s.
- Default scene/library configuration, GTX 1650 / NVIDIA 595.91.07.
- Prepare ordinary completed introduction/results scenes; wait 3 s for entrances.
  Check progress before exiting. All measured exits are completed-state exits,
  not interrupted PER_LINE entrances.
- One warm-up and three retained repetitions per mode/scene. Tables below focus
  on the 19-Label results scene responsible for the original 182→335 ms question.
- External driver includes current sample without editing it. Observe exit until
  its existing layout-completion callback, before next-scene construction.
- Separate runs for thread attribution, coarse core-stage timing, GL/EGL CPU
  attribution, PC sampling, and a one-off call-stack diagnostic.
- Existing libraries are unmodified; current foundation build has no `-O` flag.
  External driver/profilers compile with `-O2`. This is not a release/TV benchmark.
- Normal ordinary text Reveal remains enabled in GaussianBlurEffect mode.
- No GPU timers, readbacks, glFinish additions, GL argument changes, buffer-policy
  changes or new environment tuning in these CPU measurements.

## 1. Thread attribution (scheduler runtime, uninstrumented GL)

| Mean accumulated CPU over exit | Blur Effect | PERFORMANCE |
|---|---:|---:|
| Event/main thread | 32.03 ms | 29.02 ms |
| Combined update/render thread | 153.83 ms | 318.70 ms |
| Other sampled threads | ~0 | ~0 |

`/proc/self/task/*/schedstat` snapshots at setup, ~50 ms increments and completion
identify which thread is active. Process CPU uses CLOCK_PROCESS_CPUTIME_ID.
The extra time continues to accumulate during playback, not just initial setup.
Snapshots/printing add small diagnostic overhead but cannot explain ~165 ms.

## 2. Core stage attribution (separate pass)

Unchanged `Dali::Integration::Core` calls were timed with ABI-identical external
pass-through wrappers and CLOCK_THREAD_CPUTIME_ID. No stage was skipped.

| Mean CPU | Blur Effect | PERFORMANCE |
|---|---:|---:|
| Update (including animation/constraints) | 21.25 ms | 20.57 ms |
| PreRenderScene | 1.73 ms | 1.65 ms |
| Offscreen RenderScene | 98.23 ms | 259.22 ms |
| Surface RenderScene | 22.86 ms | 25.90 ms |

Thus the dominant difference is the offscreen rendering stage, not update-side
Reveal timeline calculations or main-thread metadata/raster preparation.

## 3. Exact GL/EGL CPU attribution (separate pass)

Timed API calls forward every original argument unchanged. CPU and wall clocks
are read around the calls, with nesting excluded. This run includes profiler
overhead, so its absolute totals differ from the original 182/335 ms run.

| Mean CPU, same instrumented runs | Blur Effect | PERFORMANCE |
|---|---:|---:|
| Whole-process exit interval | 202.41 ms | 351.47 ms |
| `glMapBufferRange` | **1.86 ms** | **158.57 ms** |
| Arithmetic remainder after subtracting mapping | 200.55 ms | 192.89 ms |

Total difference +149.05 ms; mapping difference +156.71 ms. Other measured CPU
costs therefore do not explain the observed large increase. The subtraction is
an attribution calculation, **not predicted performance if waiting were removed**.

Mapping CPU per retained run:

- Effect: 0.10 / 5.38 / 0.11 ms.
- PERFORMANCE: 123.70 / 165.70 / 186.32 ms.
- Mapping wall time is almost equal to mapping CPU time, consistent with active
  waiting rather than sleeping for most of the duration.
- Mapping call count is roughly two per frame in both modes, not one per text
  glyph/line. PERFORMANCE does not have hundreds of extra map calls.
- Earlier wrappers that did not include `glMapBufferRange` found ~59 ms in their
  covered APIs in each mode; adding mapping explains the previously unattributed
  CPU. EGL create/wait sync, clears, binds, draws and swaps are not the main delta.

## 4. Sampling and call-stack confirmation

Own-process POSIX CPU timer samples only RenderThread at nominal 1 ms of thread
CPU time. Its signal handler records a program counter in a fixed-size array;
symbol lookup and output occur outside the handler. No perf privilege or system
configuration changes were required.

Warm runs, results scene:

| Module | Effect samples | PERFORMANCE samples |
|---|---:|---:|
| NVIDIA eglcore | 112 | 444 |
| vDSO (clock polling) | 2 | 117 |
| DALi core | 131 | 108 |
| DALi adaptor | 122 | 123 |
| Total | 450 | 914 |

PERFORMANCE samples repeatedly hit NVIDIA offsets around 0xaf61b0–0xaf61c5.
Read-only disassembly shows repeated x86 `pause` instructions followed by time
polling/loop branches. Other hot PCs include 0x9e51dc and vDSO clock_gettime.
This is evidence of active waiting, not evidence of more text arithmetic.

A separate single-shot slow-map backtrace identifies:

```
Integration::Core::RenderScene(offscreen)
  RenderManager::RenderScene
    UniformBufferV2::ReSpecifyGPU
      UniformBufferV2::MapGPU
        GLES::Memory3::LockRegion
          glMapBufferRange
```

Observed slow results-scene call: offset 0, size 96,256 bytes, target
GL_COPY_WRITE_BUFFER, access GL_MAP_WRITE_BIT, ~14.55 ms CPU in one call.
This is an ordinary uniform-buffer mapping, not text texture rasterization.

Relevant unchanged code:

- `dali-core/dali/internal/render/renderers/uniform-buffer.cpp:339` MapGPU.
- Same file: INTERNAL_UBO_BUFFER_COUNT is 2; Flush rotates the current GPU buffer.
- `dali-adaptor/dali/internal/graphics/gles-impl/gles3-graphics-memory.cpp:70`
  performs the write mapping. No invalidate/unsynchronized flags are requested.

Two early GDB diagnostics are retained for transparency: the first stopped with
a debugger callback error, and the corrected run captured only one useful stack
because debugger pauses perturb a 0.4 s animation. They are not timing evidence.
The post-call slow-map stack above is the useful caller-path confirmation.

## 5. HIGH control

An additional control selected the sample's existing HIGH option, with all other
settings unchanged, and used the same mapping profiler:

| Mean CPU | HIGH |
|---|---:|
| Whole interval | 236.91 ms |
| Mapping | 38.34 ms |
| Arithmetic remainder | 198.58 ms |

Mapping varied 51.24 / 62.75 / 1.02 ms. Mapping stalls can occur in HIGH as well;
they are not uniquely a mathematical cost of quarter-resolution blur. After
accounting for mapping, all three modes are close (~193–201 ms in this diagnostic).
That is not a proof of identical CPU work on all hardware or in all UX cases.

## Interpretation / unresolved boundary

Confirmed: the large local CPU delta is in driver-side UBO mapping wait, and the
current PERFORMANCE path encounters it much more strongly in these runs.

The normal explanation is synchronization with earlier GPU use of a buffer being
mapped for new writes. The precise outstanding GPU submission/context/dependency
that makes PERFORMANCE wait longer has **not** been traced. GPU timing, two-buffer
reuse, surface/offscreen synchronization and PERFORMANCE's extra source sampling
are candidates for a subsequent focused investigation, not established causes.

Do not conclude that:

- PER_LINE is running in these normal exits;
- Reveal blur inherently needs ~150 ms more text/constraint CPU work;
- TV hardware will reproduce NVIDIA's mapping CPU gap;
- NVIDIA has a correctness bug;
- unsynchronized mapping, removing fences, or a general adaptor/core modification
  is justified. Those would require separate correctness and lifecycle review.

The previous GPU draw-time/texture-saving observations are not contradicted:
GPU draw timers do not measure CPU waiting before new commands can be submitted.

## Artifacts

- `*-threads.log`, `*-corecpu.log`, `*-glcpu.log`, `*-glcpu-map.log`.
- `*-samples.log`: CPU sample counts and resolved modules/PCs.
- `performance-maptrace.log`: slow mapping call stack (not performance data).
- `probe.cpp`, pass-through profiler sources, `run.py`, `analyze.py`.
- No benchmark/demo/GDB processes remained after measurement.
- Existing sample modification and adaptor +13-line worktree change preserved.
