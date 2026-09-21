# Text::Reveal Blur — 256 MiB policy closure / A8 Source batching PoC

2026-09-14 · `devel_blur_text` · NVIDIA GTX 1650 / driver 595.91.07 / GLES / MSAA 4.

## Summary

**MEMORY ADMISSION POLICY FINALIZED.** The 256 MiB conservative admission policy and existing estimator/UTC work are consolidated into `7dbebd1d Add blur to text reveal`. No push was performed.

**SOURCE BATCHING REJECTED — current PoC is not recommended for production.** Source batching is technically feasible: all 348 framebuffer pairs matched byte-for-byte, Source draws fell 31→6 / 23→4, and total measured GPU draw time fell 13.3–17.5%. However, F24 HIGH process CPU increased consistently in all three independent runs: **180.37→231.42 ms/s (+28.3%)**, with non-overlapping ranges. This fails the requested no-CPU-regression gate. No further redesign or replacement implementation was attempted.

This rejects this implementation/ROI result, not the possibility of every future Source-batching design. The exact CPU call site causing the difference was not profiled; it must not be attributed to the one-time atlas copy or declared a generic driver defect without evidence.

## PART I — Memory policy

### A. Final policy and changes

`MAX_PREPARATION_BYTES = 256.0 * 1024.0 * 1024.0`.

- Conservative **per-publication admission estimate**, not actual CPU/GPU memory usage, a physical allocation ceiling, or a process-wide budget.
- The existing cheap estimator was retained. Only the threshold and explanatory comment changed in this phase.
- Updated the normal admission UTC's policy comment; removed the obsolete 128–256 MiB whole-text rejection expectation.
- Retained sync/async × HIGH/PERFORMANCE normal acceptance, pathological requests, and invalid/NaN/infinity/overflow rejection.
- No FHD corpus was added to production UTC. No new threshold matrix or estimator refinement was performed.

### B. Validation

Foundation/components and the internal test executable built successfully. Focused tests **7/7** passed:

1. `UtcDaliTextRevealBlurMemoryAdmissionP`
2. `UtcDaliTextRevealBlurMemoryRejectionP`
3. `UtcDaliTextRevealRuntimeGaussianSamplingPathsP`
4. `UtcDaliTextRevealRuntimeGaussianAlphaBackendFallbackP`
5. `UtcDaliTextVisualAsyncBlurWorkerPayloadP`
6. `UtcDaliTextVisualAsyncBlurValidInvalidValidP`
7. `UtcDaliTextVisualAsyncBlurStaleRevisionP`

Logs: foundation/components (로컬 자료: `../reveal-admission-final.Ec9ylT/foundation-components-build.log`), UTC build (로컬 자료: `../reveal-admission-final.Ec9ylT/utc-build.log`), normal admission UTC (로컬 자료: `../reveal-admission-final.Ec9ylT/UtcDaliTextRevealBlurMemoryAdmissionP.log`), rejection UTC (로컬 자료: `../reveal-admission-final.Ec9ylT/UtcDaliTextRevealBlurMemoryRejectionP.log`). An old gcov profile timestamp warning appeared; the test exit status was zero. No full regression or sanitizer was rerun. `git diff --check` passed.

### C. Commit

The original target was not local-only; `origin/devel_blur_text` contained both affected commits. History rewriting was performed only after the user's explicit approval.

| Old | New | Title |
|---|---|---|
| `6967ae13` | `7dbebd1d4f4641868eb2765f160ad6bb58b755c8` | Add blur to text reveal |
| `c7f2303e` | `47be37cfeab40afb36382574ebed464b19697623` | Add samples for testing text reveal blur |

Both retain exactly one `Signed-off-by: Bowon Ryu <bowon.ryu@samsung.com>`. The sample patch content is unchanged. `a78547fa Add custom Gaussian blur shader factory` is unchanged. Backup ref: `backup/reveal-before-admission-256-c7f2303e`. Remote-tracking HEAD remains `c7f2303e`; no push was performed. The UI working tree was clean after consolidation and before Phase 2.

**Verdict: MEMORY ADMISSION POLICY FINALIZED / CLOSED.**

## PART II — A8 Source batching PoC

### A. Architecture and dependency audit

The Source Actor was already page-local, but each line used `CloneForeground()` with a separate TextureSet/Renderer. HIGH and PERFORMANCE share this source stage; only their subsequent filtering/output policies differ.

| Dependency | Existing line-local state | PoC handling |
|---|---|---|
| Foreground and metadata | Two separate textures per line | One tight L8 + RGBA8888 atlas pair per publication |
| Geometry | Shared ordinary unit quad | One indexed multi-quad geometry per page; original order preserved |
| Crop, position, size | Absolute crop constraints plus page offset | Per-vertex crop/page offset; same transform algebra in Source vertex shader |
| UV / sampling | Standalone local UV, foreground LINEAR, metadata NEAREST | Local UV retained; atlas rectangle maps the two samplers |
| Color, opacity, progress, fade | Original renderer remains authority | Mirrored once per page using the existing clone mechanism |
| Timing | Metadata encodes unit/sequence start; fade duration is common | Existing fragment decode and endpoint logic reused; no line timing uniform added |
| Clipping / pixel snap | Original capture transform and page camera | Same page/camera and vertex snapping expression; fractional scale compared |
| Mask, gradient, color glyph, images | Additional samplers/semantics | Not eligible; unchanged legacy Source path |

Eligibility is resolved once at publication by the temporary `DALI_REVEAL_A8_SOURCE_BATCH_POC=1` switch. It requires PER_LINE, resolved plain single-color L8, exactly the foreground/metadata texture pair, no ImageSpan, gradient, emoji/multicolor, mask, decorations, emboss or cutout. Runtime also retains the existing GLES3+ A8 capability gate. Missing inputs or an atlas exceeding maximum texture dimensions retain legacy Source. The switch is not read per frame; absent/0 means OFF.

The fragment calculations are taken from the production text fragment shader, with only the two eligible sampler coordinates remapped in the private PoC variant. There is no change to Gaussian kernels, H/V geometry or dimensions, page packing, shared scratch, output geometry, Late Smooth handoff, progress schedule, radius, task refresh or task topology. No ordinary full-text crop shortcut was used.

### B. Atlas design / packing / cost

Prepared line-isolated foreground/metadata PixelData are packed in a deterministic vertical stack. Both atlas planes use identical entry rectangles. One replicated texel surrounds each entry, including corners, to preserve the standalone CLAMP_TO_EDGE edge value under LINEAR filtering; unused shelf tails are zeroed. Metadata remains NEAREST. Transparent padding was not assumed equivalent to edge replication.

| Input storage | F24: 31 lines | F32: 23 lines |
|---|---:|---:|
| Existing line foreground + metadata | 7.8207 MiB | 7.1779 MiB |
| Naive Source-page-sized input atlas pair, all pages | 27.0538 MiB | 19.5543 MiB |
| Tight per-page atlas pairs | 8.4848 MiB / 12 textures | 7.7330 MiB / 8 textures |
| Chosen per-publication atlas pair | **8.5144 MiB / 2 textures** | **7.7668 MiB / 2 textures** |
| Chosen dimensions, each plane | 1920×930 | 1914×851 |
| Extra guards + shelf tails over line inputs | 727,360 B / 0.6937 MiB | 618,545 B / 0.5899 MiB |
| Copy writes including replicated guards | 8,795,700 B | 7,964,065 B |
| Buffer zero initialization, separately | 8,928,000 B | 8,144,070 B |

Per-publication packing adds only 31,050 / 34,410 bytes versus independent tight per-page packing. It avoids 10 / 6 texture objects without a global allocator. Exact layout calculations (로컬 자료: `correctness.json`).

The assembly currently occurs on the **event thread**, in detached runtime construction called by TextVisual publication, also for async results. The worker still prepares the same line PixelData. No worker/publication architecture was redesigned.

Two atlas PixelBuffers are allocated; `PixelBuffer::Convert()` transfers their buffers to PixelData without a second pixel copy. Additional simultaneous CPU atlas plane bytes are **8.51 / 7.77 MiB**, plus small O(lines) rectangles/vertices/index arrays. Upload messages can retain PixelData until consumed; this is not a measured whole-process transient peak. Existing prepared line buffers and their original texture uploads are still present in this PoC.

Atlas allocation + zeroing + copying + texture/upload-message creation, measured six times per quality (two publications × three CPU-only processes):

| Case | Event-thread CPU mean | Wall mean [min–max] |
|---|---:|---:|
| F24 PERFORMANCE | 3.249 ms | 3.252 [3.012–3.783] ms |
| F24 HIGH | 3.221 ms | 3.223 [2.867–3.406] ms |
| F32 PERFORMANCE | 3.144 ms | 3.147 [2.739–3.742] ms |
| F32 HIGH | 2.980 ms | 2.983 [2.565–3.430] ms |

The wall/CPU interval excludes actual later GL texture upload. Copy counts above count destination writes, not estimated hardware memory-bus traffic.

### C. Structural result

| Case | Source draws | H / V / Output draws | Total draws | Extra tasks | Reachable renderers | Actors | Geometries |
|---|---:|---:|---:|---:|---:|---:|---:|
| F24 both qualities | **31→6** | 6 / 6 / 6 unchanged | **49→24** | 18 unchanged | 50→25 | 51 unchanged | 19→25 |
| F32 both qualities | **23→4** | 4 / 4 / 4 unchanged | **35→16** | 12 unchanged | 36→17 | 35 unchanged | 13→17 |

The new per-page input geometry accounts for the geometry increase. Counters from timer queries differ fractionally at measurement boundaries; the table states the complete-frame structure. No partial batching occurred in the four primary cases.

### D. Correctness / fallback / lifecycle

At progress **0 → 0.4 → 1 → 0.4**, all **348 baseline/candidate framebuffer pairs** are byte-identical, including final Output. The last checkpoint is a backward direct seek, not a separate eased continuous reverse-animation test.

| Case | Framebuffer pairs | Atlas selected |
|---|---:|---|
| F24 PERFORMANCE | 76 | Yes |
| F32 PERFORMANCE | 52 | Yes |
| F24 HIGH | 76 | Yes |
| F32 HIGH | 52 | Yes |
| 550×300, render scale 1.25, six Korean lines | 16 | Yes |
| RGBA gradient | 16 | No |
| Color emoji + L8 mask | 16 | No |
| Local ImageSpan, A8 + RGBA pages | 28 | No |
| Async six-line Korean | 16 | Yes |

Each primary case includes every page's Source/H/V at each progress plus the full 1920×1080 window output. The capture hook snapshots a completed pass before the next pass/page consumes/reuses scratch; it does not merely read the last shared Source/H at frame end. All progress-zero Source red channels are zero; nonzero visible states are present, so identical empty captures are not being counted as success. The mixed emoji case really had RGBA foreground + L8 mask + RGBA metadata; the image case really had separate A8 and RGBA source targets.

Comparison details (로컬 자료: `correctness.json`), capture runner (로컬 자료: `run-captures.sh`), capture summary (로컬 자료: `captures.log`). An initial capture run failed to flush timer-driven property mutations and remained at progress 0.5 despite different filenames. Those captures were invalid for endpoint/seek coverage and are excluded (saved under `capture-unflushed/`). The harness was corrected to request event/update processing explicitly, matching the existing quality probe, and logs actual `GetTextRevealProgress()` before every capture. The reported comparisons use the rerun at verified 0 / 0.4 / 1 / 0.4, with the final measured candidate. The performance matrix intentionally stayed at 0.5 and was not affected or repeated.

All **48 timing processes** and **18 parity processes** performed create/publish/destroy/settle twice. Additional task count returned to **0** each time. Weak handles for new atlas textures, Source renderers and Source geometries returned empty after destruction; no live new atlas resources were observed. This is focused lifecycle smoke, not full leak/sanitizer/reentrant-cancellation coverage. No image lifecycle code was changed.

### E. Measurement method

- Baseline is the real **256 MiB production build**, saved before PoC under `baseline/`; no admission bypass library or patched guard was used.
- F24/F32 corpora are reused unchanged from the native FHD study: respectively 3208 characters / 31 lines / 1054px text extent and 1800 / 23 / 1058px. Label 1920×1080, fonts 24/32, natural wrapping/default line spacing.
- A8 white, PIXEL, PER_LINE, stagger 0.25, fade 0, blur radius 24, blur duration ratio 0.5. Both HIGH and PERFORMANCE.
- Three independent baseline and three candidate processes per case, separately for CPU and GPU: **4 × 2 × 3 × 2 = 48 processes**. Order is reversed in run 2. Each process settles ordinary layout, enables Reveal, warms up, then measures about **3.008 seconds at fixed progress 0.5**, followed by destroy/recreate/destroy.
- CPU: `CLOCK_PROCESS_CPUTIME_ID`, all process threads, reported as CPU ms per wall second. CPU runs have no GPU timer preload.
- GPU: GL disjoint timer queries around draw calls, sum divided by measurement frames (~180). Excludes clear/upload/swap/compositor, so this is **not whole-frame latency or a target FPS estimate**. Dropped/disjoint counts were zero.
- This reproduces the prior steady GPU/CPU metric; it is not a full animated-scene CPU benchmark. No new broad benchmark suite was introduced.

### F. GPU results — mean ms/frame

| Case | Source | H | V | Output | Total | Total reduction |
|---|---:|---:|---:|---:|---:|---:|
| F24 PERFORMANCE | .25618→.13986 | .04993→.05316 | .05022→.05290 | .27998→.30161 | **.63631→.54753** | **14.0%** |
| F24 HIGH | .25086→.11552 | .11949→.12340 | .31347→.31904 | .24618→.24828 | **.93000→.80624** | **13.3%** |
| F32 PERFORMANCE | .21901→.12941 | .05022→.05116 | .04856→.04452 | .22753→.22489 | **.54532→.44999** | **17.5%** |
| F32 HIGH | .22025→.10555 | .13103→.12567 | .28843→.28069 | .19666→.19466 | **.83636→.70656** | **15.5%** |

Total GPU baseline/candidate ranges do not overlap in these four cases. Source improvement does not scale with draw-count reduction because the fragment work remains. F24 PERFORMANCE Output was modestly slower despite identical output geometry/results; no claim is made that every downstream timing stays numerically identical when submission/input storage changes.

### G. CPU and setup

| Case | Process CPU ms/s mean | Baseline range | PoC range | Interpretation |
|---|---:|---:|---:|---|
| F24 PERFORMANCE | 206.77→230.49 | 187.88–235.81 | 227.32–236.59 | +11.5% mean; ranges overlap |
| F24 HIGH | **180.37→231.42** | **178.87–181.88** | **226.53–235.45** | **+28.3%, regression in all 3 runs** |
| F32 PERFORMANCE | 210.35→167.44 | 198.59–230.81 | 124.33–202.91 | Large variance; not a reliable universal CPU win |
| F32 HIGH | 210.78→187.98 | 146.22–249.24 | 126.22–235.77 | Large variance; ranges overlap |

Do not average away F24 HIGH using F32's lower but noisy values. In particular, Source draw reduction alone is not evidence of CPU improvement. The specific CPU function/thread responsible was not isolated; no further optimization was attempted after the no-go gate failed.

Publication wall measurements:

| Case | Ordinary already laid out → blur companion observed | New warm Label + Reveal → companion observed |
|---|---:|---:|
| F24 PERFORMANCE | 141.19→142.83 ms | 155.88→156.20 ms |
| F24 HIGH | 140.30→141.68 ms | 156.28→155.99 ms |
| F32 PERFORMANCE | 120.67→122.78 ms | 127.50→128.19 ms |
| F32 HIGH | 121.26→120.55 ms | 129.29→127.68 ms |

These are event-side publication observations with a 16ms polling timer, **not first visible GPU-frame/fence times**. Warm recreation includes ordinary text layout/raster; the first enable measurement starts after ordinary layout. Setup differences are small relative to observation granularity; no creation-time improvement is claimed. Full process setup CPU and per-run values are in tables.txt (로컬 자료: `tables.txt`) / summary.json (로컬 자료: `summary.json`).

### H. Memory and input ownership

Logical unique texture payload, MiB; not physical VRAM allocation (driver alignment/caches excluded):

| Case | Baseline total | Actual PoC total (duplicate lines + atlas) | Hypothetical replacement total | FBO, unchanged |
|---|---:|---:|---:|---:|
| F24 PERFORMANCE | 23.4454 | **31.9598** | 24.1390 | 5.9750 |
| F24 HIGH | 24.6848 | **33.1992** | 25.3784 | 7.2144 |
| F32 PERFORMANCE | 21.2638 | **29.0306** | 21.8537 | 4.4007 |
| F32 HIGH | 22.7295 | **30.4963** | 23.3194 | 5.8663 |

The native actor traversal sees the atlas inputs but no longer sees unused line textures held only in `mSequences`. Thus **actual PoC total = traversed unique payload + retained original line-input bytes** (8,200,640 / 7,525,525 B). Omitting these hidden references would incorrectly present replacement memory as measured actual memory. Actual texture object counts are baseline +2: 77→79 / 72→74 / 57→59 / 54→56.

Production projection is `baseline - eligible line inputs + atlas`. The 0.694 / 0.590 MiB net increase is guards/shelf padding, not an unavoidable extra 8 MiB atlas on top of old inputs. This is a projection, **not implemented**.

Consumer audit: in eligible text-only PER_LINE, `RuntimeRevealBlurSequence::textures` is read to validate construction and to create the Source clone. H binds Source FBO, V binds H, and Output binds V and, for PERFORMANCE, full-resolution Source FBO. Image binding logic does not consume these line textures, and images are excluded anyway. `mSequences` currently retains them for companion lifetime; it could instead retain atlas binding/entry data in a future replacement design. Original full-text `mSourceTextures`, ordinary metadata, restoration/fallback and image resources are separate and were not removed.

Observed mean RSS / live heap checkpoints, MiB (baseline→PoC):

| Case | Steady RSS | Steady heap | Destroyed RSS | Destroyed heap |
|---|---:|---:|---:|---:|
| F24 PERFORMANCE | 173.08→182.68 | 40.47→48.43 | 165.39→174.99 | 14.22→14.15 |
| F24 HIGH | 175.44→182.32 | 41.42→49.42 | 167.75→174.61 | 14.18→14.20 |
| F32 PERFORMANCE | 174.75→179.90 | 37.28→44.63 | 167.02→172.16 | 14.16→14.17 |
| F32 HIGH | 172.87→179.83 | 38.63→45.97 | 165.15→172.09 | 14.13→14.14 |

RSS does not return to the initial process baseline after destruction because allocator/driver caches can remain. The live heap and weak-handle/task evidence do not show retained new atlas resources in these runs; RSS alone neither proves a leak nor proves universal leak freedom. CPU and texture figures must not be summed as disjoint physical allocations.

### I. Verdict / next step boundary

**SOURCE BATCHING REJECTED** for this PoC under the requested gate.

- Feasibility, draw reduction, bounded atlas padding, framebuffer parity and focused resource retirement were demonstrated.
- GPU benefit is real within this host's draw-query measurement.
- CPU non-regression is **not** demonstrated; F24 HIGH instead has a consistent measured regression.
- Therefore no production input-replacement rewrite, worker atlas packing, global allocator, new Source-cache or shader architecture expansion was started.

Only if a future task explicitly reopens this candidate should CPU attribution precede productionization. Were it reopened successfully, the least invasive memory design would reuse isolated prepared line planes, create one atlas pair, upload **only atlas inputs**, and retain logical line rectangles/timing rather than unused line textures. Async worker placement and reentrant allocation/cancellation validation would then be separate required design work, not assumed solved by this smoke test.

### J. Working tree / artifacts

- Phase 1 committed in the existing Blur commit; no extra permanent memory-policy commit.
- Phase 2 remains **unstaged/uncommitted** in exactly three UI files: `text-reveal-runtime-blur.cpp`, its header, and `text-visual.cpp`.
- PoC is **OFF by default** and only enabled by `DALI_REVEAL_A8_SOURCE_BATCH_POC=1` at publication. Experimental code is retained for review, not adopted as production.
- No sample/public API/tests/H/V shader/Gaussian/adaptor code was changed in Phase 2.
- Existing adaptor `gles-texture-dependency-checker.cpp` +13-line local change was preserved.
- Final foundation/components candidate build and `git diff --check` passed. No sanitizer/full regression was run for the PoC.
- CPU/GPU raw summary (로컬 자료: `summary.json`), human-readable full metrics (로컬 자료: `tables.txt`), timing runner (로컬 자료: `run-timing.sh`), 48-run completion log (로컬 자료: `timing.log`), final build (로컬 자료: `build-final.log`).
- `baseline/libdali2-ui-foundation.so.2` and `candidate/libdali2-ui-foundation.so.2` preserve both measured binaries. Candidate rendering and diagnostics are not in the committed production baseline.

The prior FHD report supplied unchanged corpora and comparison context. Only the requested four A8 comparisons were remeasured; the previous RGBA performance matrix was not repeated.
