# A8 direct-source GPU gate — measurement contract

Production is untouched. BASE and DIRECT are the exact private foundation
libraries from `reveal-packing-sweep.7E5egr/BASE` and
`reveal-source-elision.dw5WOw/direct`. No direct shader, Gaussian kernel,
packing policy, timing curve, or sample source was edited.
Library and helper hashes are recorded in BASELINE.txt (로컬 자료: `BASELINE.txt`).

## GPU timer

- NVIDIA GTX 1650, driver 595.91.07, GLES 3.2; desktop MSAA=4.
- `GL_EXT_disjoint_timer_query`, `GL_TIME_ELAPSED_EXT`, 64 counter bits.
- Query pools are **per EGL context**, because query objects cannot be treated
  as shared texture objects. Offscreen and window scopes have separate pools.
- Results are polled only after at least four window frames. Read RESULT only
  after AVAILABLE is true. No waiting loop, `glFinish`, framebuffer readback,
  extra fence, or change to refresh rate/dependency ordering.
- Check GPU_DISJOINT across submission/retrieval. Any nonzero indication,
  dropped query, incomplete drain, process failure, or registry collision
  invalidates the run; `analyze.py` refuses it.
- One sampled frame per four. Timers bracket individual GL clear/draw calls;
  query results are summed by stage and page category. No queries on the other
  three frames. Shader reflection is cached before measurement.
- CPU and per-frame records are held in memory and written only at shutdown.
- Existing task ordering interleaves page stages: a single category-wide timer
  would include unrelated work or require reordering. Sparse queries preserve
  ordering and provide the requested category aggregates without such changes.

The timer measures **GPU elapsed clear+draw scopes**, not pure ALU cycles, full
application frame cost, or target FPS. It excludes FBO binds, dependency waits,
present, and most CPU submission gaps. Individual queries can still include GPU
scheduling/preemption delays. Full arithmetic means are retained alongside
medians and independent-run spread; no outlier filtering is used.

Specification: [Khronos EXT_disjoint_timer_query](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt).

## Scope attribution

The native probe reads actual RenderTasks and registers `(stage,width,height)`
with one of three categories. The interposer observes FBO attachments, shader
uniform interfaces, and texture producer chains; Output is included only when
it samples a registered V texture. Ordinary UI draws are **not** counted as
Reveal Output. Conflicting dimension/category keys abort the measurement.

- Group 0: Cards labels 2/3/7/10/11, the existing **8 direct-eligible pages**.
- Group 1: **7 fallback pages** (six non-atlased A8, one gradient).
- Group 2: other Reveal blur pages already in the unchanged demo scene.
- Group 3: unknown, required to be zero during the selected window.

Cards total is groups 0+1; scene-relevant total is groups 0+1+2. Both include
Source+H+V+Output. No attempt is made to remove fallback pages from the real app.

## Actual Cards workload

The probe includes the actual `text-effect-demo.cpp`, unchanged, and uses its
`ResetToState(REVEAL_RESULTS)`. Current text, fonts, geometry, R24 entrance,
stagger, delays, alpha functions, and completion behavior are retained.

1280×720; one initial scene for 3.9 seconds, then reset identically. Measurement
is from 0.30 to 3.20 seconds after reset. The 0.22-second inventory precedes the
window. Another 0.60 seconds allows nonblocking query drain before shutdown.
Five independent processes per quality/arm, with BASE/DIRECT order alternating.
ECONOMY is primary; PERFORMANCE is a Cards-only reference, not a new candidate.

## Static matrix

Four A8 multiline labels, simultaneously visible, with English, Korean, thin
and bold corpora from the previous quality fixture. 430×130 each; 20px fonts
except thin=16px; SamsungOneUI_400 and DejaVu Sans/BOLD. Window 1280×900.
PIXEL/PER_LINE, Fade=0, Stagger=.25, BlurDurationRatio=1.

Each independent process measures R16/R24/R48 and p=.20/.50/.75/.90, plus 0/1
endpoint references. It waits 0.80 seconds after each configuration before a
1.00-second measurement. Five independent processes per arm, alternating order.
The matrix is a separate fixture, **not** a change to the Cards workload.
Radius-dependent existing packing produces 7/6/6 pages at R16/R24/R48, equal in
both arms; only their Source tasks disappear in DIRECT.

Both scopes discard the first/last two frame records around phase boundaries.
Frames from the different EGL contexts are merged by frame ID, then stage sums
are computed. Each process contributes one mean and one median per scope;
independent-process mean/median/min/max/Q1/Q3 are all available in the report.

## Instrumentation development results excluded from the gate

`smoke-*` initially tested query mechanics and a wider FBO scope. An early
single-context-pool version could not retrieve cross-context queries; it was
rejected (pending/dropped). A later wider FBO timer also included submission
gaps, causing large unrelated V variation. `matrix-smoke-*` uses this discarded
wider scope. `draw-smoke-*` validates the final clear/draw approach. None of
these smoke runs contributes to the final independent-run statistics.

## Reproduce

```bash
cd /home/bowonryuubuntu/tizen
python3 reveal-direct-gpu.VMDaVs/build.py
python3 reveal-direct-gpu.VMDaVs/run.py cards
python3 reveal-direct-gpu.VMDaVs/run.py economy-matrix
python3 reveal-direct-gpu.VMDaVs/run.py controls
python3 reveal-direct-gpu.VMDaVs/aggregate.py
```

Run modes sequentially, never concurrently. The scripts use private libraries
through a child process's LD_LIBRARY_PATH; nothing is installed system-wide.
The existing output names are overwritten if deliberately re-running these
commands. The old PoC/report directories are not modified.
