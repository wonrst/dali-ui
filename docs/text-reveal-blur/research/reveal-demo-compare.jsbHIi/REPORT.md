# Text-effect-demo: GaussianBlurEffect vs Reveal PERFORMANCE

2026-09-16. Focused local comparison only. No repository changes, build changes,
commit, performance tuning, or target measurements in this task. The existing
unstaged sample comparison-button change and unrelated adaptor changes remain.

## Conditions

- Actual current `samples/text/text-effect-demo.cpp` included unchanged in an
  external automation driver. HEAD `cd9567612780b6fb46ea0ef829acb1732a0c8d52` plus
  the existing sample-only Blur Effect button change.
- GTX 1650, NVIDIA 595.91.07, GLES, window 1280x720, MSAA 4, default Sync.
- Strong: entrance radius 24, exit radius 48. Measure normal simultaneous exit:
  WHOLE_TEXT, Fade=1, Stagger=0, BlurTime=1, progress 1→0, Linear, authored 0.4 s.
- Introduction: 6 Labels. Results cards: 19 Labels. Markdown excluded.
- Use the sample's `ResetToState` to prepare each scene, then wait 3 s for text,
  badges, actions and layout entrances to finish. No workload/corpus substitutions.
- Execute actual `TransitionTo`. A wrapper around its existing layout completion
  callback ends measurement immediately before the sample installs the next scene,
  then calls the original callback unchanged. Thus next-scene construction is not
  in the interval. State preparation does not replay the entire skeleton sequence.
- One discarded warm-up per scene/mode, followed by 3 measured repetitions.
  Current project libraries are reused (foundation CMake build type empty, no `-O`
  flag). The external sample driver was compiled with `-O2`. These are comparative
  local diagnostics, not release-build/TV absolute performance numbers.
- PERFORMANCE includes the current eligible whole-text adaptive-exit path.
  GaussianBlurEffect uses the existing public Strength-animation policy, which
  keeps full-resolution H/V during animation. No effect algorithm changes.

## GPU and texture results

| Scene | Gaussian effect GPU ms/frame | PERFORMANCE GPU ms/frame | Change | Effect textures MiB | PERFORMANCE textures MiB | Change |
|---|---:|---:|---:|---:|---:|---:|
| Introduction, 6 Labels | 0.988 | 0.580 | -41.2% | 6.084 | 3.663 | -39.8% |
| Results, 19 Labels | 1.335 | 0.865 | -35.2% | 6.348 | 3.840 | -39.5% |

GPU values are mean per-frame sums of GPU timer queries around all GL draw calls
in the measured window, including scene output and offscreen draws. They exclude
texture upload, clears, resolve, presentation and CPU. This is **not total GPU
frame time or an FPS gain**. Queries were collected asynchronously; no glFinish,
readback or capture was added. Dropped queries and disjoint reports were zero.

Per-run GPU averages:

- Introduction: Effect 0.959–1.027 ms, PERFORMANCE 0.531–0.678 ms.
- Results: Effect 1.202–1.434 ms, PERFORMANCE 0.806–0.918 ms.
- Frames across 3 runs: introduction 72/70, results 77/73 (Effect/PERFORMANCE).
- GPU queries were run separately from the CPU and memory passes.

Texture numbers are a separate live-resource inventory about 100 ms into exit,
not an NVIDIA VRAM measurement or a continuous peak trace. Sum unique texture
handles reachable from all task FBOs, task source Actors and the window actor tree:
`width * height * Pixel::GetBytesPerPixel(format)`. Includes visible/hidden bound
text, metadata and blur textures; shared handles are counted once. Excludes
unbound caches, driver allocation/alignment, window swapchain/MSAA renderbuffers,
shader programs and CPU pixel buffers. Both modes use the same inventory rule.

| Scene | Effect blur FBO MiB | PERFORMANCE blur FBO MiB | Reduction | Offscreen tasks, both |
|---|---:|---:|---:|---:|
| Introduction | 4.702 | 2.281 | 51.5% | 18 |
| Results | 5.053 | 2.544 | 49.6% | 57 |

Total active texture increase from the ordinary steady scene:
introduction 5.803→3.381 MiB; results 6.083→3.574 MiB.
The FBO saving therefore explains the observed active-texture reduction.

## CPU results

CPU measurements do not preload the GPU query library or walk resource trees.
`CLOCK_PROCESS_CPUTIME_ID` includes all process threads and driver CPU work. It is
accumulated CPU time, not UI-thread blocking time or elapsed animation duration.

| Scene | Effect exit CPU ms | PERFORMANCE exit CPU ms | Effect direct setup wall ms | PERFORMANCE direct setup wall ms |
|---|---:|---:|---:|---:|
| Introduction | 106.43 | 200.84 | 6.61 | 7.60 |
| Results | 182.23 | 334.82 | 20.99 | 23.74 |

Direct setup is the synchronous `TransitionTo` call only. It excludes deferred
layout, raster/upload and subsequent frames. The complete interval includes that
setup plus animation through exit completion before next-scene installation.

- Introduction CPU range: Effect 79.70–158.53 ms, PERFORMANCE 120.66–259.66 ms.
- Results CPU range: Effect 180.51–184.75 ms, PERFORMANCE 316.30–345.44 ms.
- Mean measured wall intervals: intro 408/401 ms; results 423/418 ms.
- PERFORMANCE setup is ~13–15% longer in this run. Whole-interval accumulated
  CPU is also higher (~84–89%); intro variance is particularly large.
- Do not translate the desktop CPU figures directly to the TV or attribute the
  difference to a specific implementation stage without a separate profile.

## Host memory observation, not a stable comparison

One isolated memory pass per mode recorded heap/RSS changes from steady scene to
the same ~100 ms exit snapshot:

| Scene | Effect heap delta / RSS delta MiB | PERFORMANCE heap delta / RSS delta MiB |
|---|---:|---:|
| Introduction | 7.18 / 6.42 | 19.28 / 18.36 |
| Results | 10.00 / 6.57 | 7.64 / 0.37 |

These process-wide values depend on first-use driver allocations and allocator
reuse (intro precedes results in each fresh process); they are not per-Label CPU
memory, retained leak growth or a reliable mode ranking. No host-memory win is
claimed. GPU logical texture inventory above is the useful memory comparison.

## Interpretation and limits

PERFORMANCE is cheaper for GPU draws and bound texture storage in these two
normal-exit scenes, but it is **not uniformly cheaper on CPU**. The desktop has
ample GPU headroom, so neither these draw-time savings nor texture counts promise
the same FPS improvement on a TV. Both use the same 3 offscreen tasks per Label
in this WHOLE_TEXT comparison.

This is actual API-usage comparison, not equal-image/quality benchmarking:
GaussianBlurEffect captures the whole Label including its background/children;
Reveal blur treats text foreground with its own halo and downsample/handoff.
PER_LINE entrance blur timing is not equivalent. Only the completed-scene normal
exits above were timed; async, interrupted exits, entrance and Markdown performance
were not measured here.

## Reproduction

- `probe.cpp`: unchanged demo + external lifecycle measurement wrapper/inventory.
- `meter.cpp`: optional asynchronous GPU draw timer.
- `run.py`: fixed environment and separate memory/CPU/GPU processes.
- `analyze.py`: exclude warm-ups, aggregate 3 runs, check query-drop/disjoint zeros.
- `*-cpu.log`, `*-gpu.log`, `*-memory.log`: raw observations.
- `effect-memory.log` was the initial isolated memory smoke, run before `run.py`;
  rerun as `./probe effect memory` with the environment from `run.py` if needed.

No repository source was edited during measurement. `git diff --check` passed.
