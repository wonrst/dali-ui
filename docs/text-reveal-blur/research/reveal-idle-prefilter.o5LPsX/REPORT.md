# Text::Reveal Runtime Blur — idle task suspension / prefiltered low-resolution Gaussian

2026-09-16. Analysis and external diagnostics only. A and B are independent.

## 요약

- **A — dormant task:** FBO/Texture를 유지한 채 Source/H/V의 반복 draw를 멈추는 것은 가능하다. 다만 reverse/seek, 애니메이션된 속성, 이미지 갱신을 첫 변경 프레임부터 반영하는 범용 wake 경로는 아직 입증되지 않았다. 이벤트 스레드의 PropertyNotification만으로 처리하는 production 변경은 권하지 않는다.
- `REFRESH_ALWAYS`가 앱 전체를 영원히 렌더링하게 만드는 것은 아니다. 다른 애니메이션 등으로 프레임이 계속 발생할 때 idle task 절감 효과가 있고, 전체 화면이 쉬면 기존 경로도 draw가 멈춘다.
- **B — prefilter + 저해상도 exact Gaussian:** 제한된 외부 native PoC에서 유의미한 GPU 절감이 확인되었다. FHD Label, radius 48 기준 Source부터 Output까지의 draw GPU 시간은 A8 **0.704 → 0.288 ms/frame**, RGBA **0.740 → 0.332 ms/frame**이었다. 각각 약 **59% / 55% 감소**이며, 전체 프레임 시간이나 타겟 FPS 측정값은 아니다.
- B의 단일 페이지 FBO 저장량 계산은 RGBA **11.93 → 10.80 MiB**로 약 **9.5% 감소**한다. 대신 offscreen task는 **3 → 4개**다. 총 VRAM/피크 메모리 실측 절감값은 아니다. CPU 개선은 편차 때문에 결론내리지 않았다.
- 한글/영문/gradient/emoji/ImageSpan을 비교했지만 현재 PERFORMANCE보다 항상 좋은 품질은 아니었다. **B의 위상 보정 prefilter를 대상으로 한 제한적 품질·PER_LINE 검증을 우선 추천**한다. A의 범용 wake는 별도 과제로 남긴다.
- **Production 수정 없음. HEAD 및 기존 adaptor 변경을 보존했다.** 아래 A–S에 코드 근거, 원시 측정 범위, 한계와 후속 조건을 정리했다.

## A. Repository / baseline state

Repository: `/home/bowonryuubuntu/tizen/dali/dali-ui`.

- Branch: `devel_blur_text`.
- HEAD: `5aae0d2b389bb9811bed6e21b754cee629f4ecfc`.
- Initial `git status --short`, unstaged diff and staged diff: empty. `git status`: clean.
- Recorded `git rev-parse HEAD`, branch, log -15, status, diff and cached diff before diagnostics.
- Stack: `5aae0d2b` adaptive prototype / `b7b0b712` source batching / `c0d745a5` samples / `7dbebd1d` blur / `a78547fa` Gaussian factory / `1f794aa3` upstream/devel.
- Existing unrelated adaptor modification in `gles-texture-dependency-checker.cpp` was preserved. Nothing in UI/core/adaptor was edited or rebuilt.
- Diagnostics live only in this directory. No commits, history operations, production implementation, full regression or sanitizer matrix.

Exact baseline: public PERFORMANCE with FadeDurationRatio=0, Unit::LINE, WHOLE_TEXT, BlurDurationRatio=1. HEAD's adaptive path requires Fade=1 and is therefore excluded. PER_LINE and HIGH control measurements are explicitly identified below. No adaptive coefficients or gates were changed.

Machine: NVIDIA GTX 1650, driver 595.91.07, GLES 3.2, MSAA=4. Current local DALi libraries; no release-build performance claim. Source/sample/native paths are all from the current checkout. CPU/driver findings cannot be extrapolated numerically to TV.

## B. Current runtime task graph

Per page: full Source → H → V → ordinary scene Output. Output is a renderer, **not a fourth offscreen FBO task**. Decorations may add a separate composition task. Multiple packed pages may share scratch storage, not final presentation textures.

HIGH uses full-size S/H/V. PERFORMANCE keeps full Source for Late Smooth, H is `ceil(W/4) × H`, V is `ceil(W/4) × ceil(H/4)`. Original label textures/metadata/gradient LUTs remain separate inputs and are not included in the FBO-only payload calculations below.

`RuntimeBlurActor` owns actors, framebuffers and tasks. WHOLE_TEXT borrows the foreground renderer; PER_LINE captures atlas/isolated renderer groups. ImageSpan keeps scene-connected originals on a hidden retention actor and captures proxies. Output inherits owner color once.

Lifecycle/code anchors in `dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`:

- `Initialize()` / allocation: lines 869–1669; source/H scratch sharing 1196–1214; target dimensions 1218–1230.
- `OnSceneConnection()`: 1691–1755, creates Source/H/V tasks, source actor, inverted-Y camera, exclusive/input-disabled flags, transparent clear and `REFRESH_ALWAYS` (1728). Decoration composition also ALWAYS (1752).
- `GetOffScreenRenderTasks()`: 863 onward, provides dependency order; `RequestRenderTaskReorder()` after creation.
- `OnSceneDisconnection()`: 1757 onward, removes task handles. Reconnect creates new ALWAYS tasks, not dormant old tasks.
- `ReleaseForeground()`: 724 onward, restores borrowed shader/textures where still owned; image capture is released. Owner is weak, task/renderer/FBO handles retained by the companion until normal retirement.

Full-resolution batches share Source/H only when same dimensions/format; PERFORMANCE shares H but **cannot share Source** because Output still samples it. Any suspension must treat all pages sharing scratch as a coherent dependency group. Freezing one page and resuming just H/V against another page's scratch is unsafe.

## C. Idle cost evidence

Native Label stays scene-connected and Reveal remains enabled. No sample auto-None cleanup. Owner 550×300, radius48, Source650×400, H163×400, V163×100, A8. Measurement starts after setup settles; ~2s each. `meter.cpp` uses asynchronous per-draw EXT timer queries; no glFinish in the timing runs. Clear/upload/present are excluded. Values are diagnostic single runs, not stable device benchmarks.

| State | Frames | Source/H/V draws | Offscreen draw GPU, ms/frame |
|---|---:|---:|---:|
| p=0, forced frame activity |123|123 / 123 / 123|0.0303|
| p=1, forced frame activity |123|123 / 123 / 123|0.0339|
| p=.5 stable, forced frame activity |120|120 / 120 / 120|0.0990|
| p=.5, manually armed ONCE and settled |123|0 / 0 / 0|0|
| p=.5, no KeepRendering / no other animation |0|0 / 0 / 0|0|
| HIGH p=1, forced frame activity |123|123 / 123 / 123|0.0432|
| PER_LINE p=.5, eight packed pages in this fixture |120|960 / 960 / 960|0.2117|

The initial p=.5 test sets and holds the property. A follow-up actually starts an animation, stops it, sets known final progress=.5 and settles before measurement: 120 frames,120 S/H/V draws each; offscreen draw GPU0.0944ms/frame. It tests capture behavior after stopping, not generic Animation::Stop() baking semantics. A retained-ONCE vs ALWAYS readback at stable p=.5 has **max difference0** for Source, H, V and Output.

**Important correction to the premise:** ALWAYS does not independently keep the entire application rendering forever. The public RenderTask contract explicitly says it executes when the scene graph is changing. A completely quiescent application already sleeps. Dormant tasks help when *other* content/animations or KeepRendering keep frames running. At p=0 H/V already return transparent; at p=1 they use cheap copies, not the full Gaussian loop. Their idle cost is real but much smaller than mid-blur cost.

## D. RenderTask refresh semantics

Core references: `dali/public-api/render-tasks/render-task.h:573`, `internal/event/render-tasks/render-task-impl.cpp:672`, `internal/update/render-tasks/scene-graph-render-task.cpp:220–335`.

- ALWAYS=1: eligible every rendered scene frame. ONCE=0: arm `RENDER_ONCE_WAITING_FOR_RESOURCES`, then `RENDERED_ONCE`, then notified; later `IsRenderRequired()` is false.
- Calling ONCE again sends a message even if the cached rate is already zero. It **re-arms**, not a permanent freeze command.
- Calling ALWAYS resumes continuous mode and resets the frame counter.
- ONCE keeps the FBO and color texture. `ClearRenderResult()` clears optional CPU readback state, not the GPU attachment image (`render-frame-buffer.cpp:134`). Output can continue sampling the retained texture; native settled-ONCE measurement confirms only Output draws remain.
- Existing raw RenderTask source reconnect changes active status, not the ONCE state machine (`scene-graph-render-task.cpp:636`). The Reveal companion instead destroys/recreates its tasks on reconnect, so its current path naturally starts active.
- Setting ONCE does not reorder tasks. All S/H/V must be armed together and their existing order preserved. Ordinary texture FBO completion is same-frame GPU ordered; FinishedSignal is not a substitute for dependency barriers or a guarantee about an arbitrarily later progress value.

The re-arm rule above is established from the core implementation, not claimed as a completed native wake validation. The extra `once-rearmed-content.log` diagnostic observed progress .25 after re-arming from settled .5, but its measurement window recorded no additional S/H/V draws. It therefore **does not prove a successful capture refresh**. This investigation did not isolate whether the additional suppression is in task/render-instruction processing or the diagnostic observation. No core defect is asserted from that single result; native re-arm/first-changed-frame verification remains an explicit gate before implementing A.

## E. Endpoint suspension feasibility

Proposed state machine only:

`ACTIVE → FINAL_REFRESH_PENDING → DORMANT`; any live input invalidation cancels pending dormancy; wake rearms the **entire capture dependency group** before its next changing frame. ONCE already provides the final-refresh-pending execution state; no second animation clock is needed.

Use exactly the renderer's endpoint normalization: `ResolveRenderProgress()` in `internal/text/reveal/text-reveal.h:58`: non-positive/NaN →0; `>=1−float epsilon` →1. Do not introduce .999/.99 cutoffs. Clamp/general animation inputs consistently with the shaders.

Final-frame requirement: progress endpoint bake and ONCE messages must be consumed before the same update, then Source→H→V must execute once with that endpoint. Merely observing the endpoint from the event thread is insufficient to promise a last-frame/wake transition, particularly if a reverse/seek begins immediately.

| Change from dormant | Required action | Why a delayed event notification is insufficient |
|---|---|---|
|1→.9 reverse|ALL S/H/V ALWAYS before first changed frame|Output/constraints already see changed progress while snapshots can still be old|
|0→.1 forward|same|Otherwise invisible first frame(s)|
|1→.5 direct seek|one fresh S/H/V chain; stay active unless input stability is proven|Old Source includes wrong fade/reveal coverage|
|1→0 direct seek|one full transparent refresh then dormant, or a separately proven output-hide rule|Cannot leave prior V visible, especially HIGH|
|1→1 identical value|no capture needed if all other inputs unchanged|Property equality alone does not prove source validity|

No fixed one-frame guarantee can be made for an event-side notification round trip. It is at least a later update, and event-thread load can add more latency.

### Detection without app callbacks

`LabelImpl::SetTextRevealProgress()` (893) only sets the animatable actor property. It is not called on each Animation/Constraint update. The renderers already consume update-side values via constraints.

PropertyNotification can detect endpoint crossings without event-thread polling, but `UpdateManager` performs Animate → constraints → property checks → task processing; checks queue an event-thread message (`update-manager.cpp:1112,1305–1378`). An event callback that calls `SetRefreshRate()` therefore **cannot wake the current frame's task**. Refresh rate is not an animatable task property that can simply be constrained. Calling event-side setters from a constraint functor on the update thread is not acceptable.

Consequently a small endpoint-notification hook is not a proven generic, frame-exact production solution. All progress mutation paths and other dynamic capture inputs must be covered, or core needs a separately reviewed update-side dirty/wake mechanism. No such extension is implemented here.

A2, 'any stable progress for N frames', adds per-frame observation, N-frame policy and the same wake problem. It is more complex and has no correctness advantage over endpoint-only A1. Do not pursue it first.

## F. Invalidation matrix

| Input | Existing path | Suspension requirement / risk |
|---|---|---|
|progress setter|actor property only|Explicit wake could cover this setter, but not generic Property/Animation/Constraint writes|
|progress animation / constraint|update-side mirrors|Same-update wake required; current event notification is late|
|Text / StyledText / font / font size|controller revision, relayout and new resource publication|New companion active; reject late old callbacks|
|text color setter|renderer/controller updates, not universally a new companion|Need capture invalidation where value is baked|
|animated text color/alpha|renderer constraints; A8 RGB can be output-only, alpha/RGBA can affect Source|Cannot treat all color changes as output-only|
|gradient bounds/properties / overlay / LUT|live renderer properties + texture bindings; clones mirror them|Must wake on live changes, not just new layout|
|Label transform / world opacity|some applied at Output only|No capture needed only after proving output-only; capture-local transforms are different|
|layout / requested size / padding|content relayout, geometry checks, publication|New tasks active; fixed padding handler requests sync/async work|
|UI/render scale|prepared publication and target geometry rebuilt|New active capture; never reuse mismatched size/scale|
|async completion|revision/source/layout revalidation before atomic activation|Dormancy belongs to current companion, not a stale result|
|ImageSpan resource ready|`OnInlineReplacementResourcesReady`→manager.Refresh→`RefreshRevealBlurImages`|Existing hook is a good place to rearm S/H/V; no ImageVisual changes required|
|ImageSpan renderer/texture/shader replacement|`RefreshImages()` replaces capture proxy and ownership binding|Must wake even if current progress is unchanged|
|in-place external Texture::Upload / mutable image contents|same handle can change without renderer identity replacement|Identity/readiness-only invalidation is not generic enough|
|decorations|separate unblurred planes/composition|Do not hide/disable the entire Label at p=0; composition may remain live|
|scene reconnect|current companion creates new ALWAYS tasks; Label refreshes image bindings after visual reconnect|Preserve current lifecycle; don't restore an obsolete dormant state|
|None / destruction / Adaptor stop|current release/disconnect guards|Remove any new notifications/pending weak callbacks before retirement; no new ownership cycle|

Key anchors: runtime-blur `CloneForeground:149` / `MirrorProperty:87`, `RefreshImages:805`; `LabelImpl:496,499,3941`; `TextVisual::PublishPreparedRevealBlur:3136+` / `RefreshRevealBlurImages:3558`; replacement manager `GetReadyBlurCaptureSources:1334`, `CaptureBlurRenderer:1356`, `ReleaseBlurCapture:1407`.

ImageSpan is not excluded: its resource-ready path is tractable, but that does not solve generic animated properties or same-handle texture updates. New-publication-only invalidation is insufficient.

## G. Endpoint benefit

If another animation keeps 60fps alive: 1s Reveal animation +9s stable means each task currently can execute 600 times; active 60 plus one endpoint refresh is about61, a potential **89.8% reduction in offscreen draw count** for that workload. This is not a 90% reduction of active GPU cost, process CPU, or whole-screen cost. Output still draws, allocations remain, and endpoint shaders are already cheap.

If the whole scene becomes idle, the measured benefit is **zero additional draw reduction**: the existing render loop already stops. PER_LINE/multi-Label scenes with continuing unrelated animations offer the higher ROI.

PERFORMANCE p=1 could theoretically retain/update only Source because Late Smooth is sharp-only; p=0 could avoid foreground work once hidden. These are separate optimizations with their own output/decorations and reverse guarantees. They do not justify skipping the generic wake audit.

## H. A risk / verdict

| Area | Risk |
|---|---|
|ONCE storage retention / task ordering without reuse changes|LOW|
|direct property wake + rapid reverse/seek|HIGH until same-frame path exists|
|dynamic gradient/color/texture invalidation|HIGH|
|ImageSpan resource-ready hook|MEDIUM|
|async stale publication / reconnect|MEDIUM|
|destruction / Adaptor shutdown|MEDIUM if adding callbacks; existing cleanup unchanged|
|memory|LOW: unchanged retained allocation, no VRAM saving promised|

**Verdict: FEASIBLE BUT INVALIDATION COMPLEX.** Dormancy itself is supported; safe generic wake is not a one-line refresh-rate change. Do not land an endpoint-only production patch based only on an Animation FinishedSignal or PropertyNotification. Minimum future diagnostic: same-update wake proof, endpoint→reverse/seek stress, and live gradient/ImageSpan capture invalidation, before any product implementation.

## I. Current PERFORMANCE cost

Native inventory confirms owner1920×1080, radius48: Source2020×1180, H505×1180, V505×295, including a 50px halo on each side. Both the one-byte alpha and four-byte RGBA cases were measured. WHOLE_TEXT has 3 offscreen tasks plus scene Output. These are not cropped PER_LINE/D2 estimates.

`GaussianBlurAlgorithm::CreateShader()` sets NUM_SAMPLES=radius>>1: radius48 means **24 positive sample pairs, 48 texture instructions per H/V fragment**, not 48 pairs. The kernel comes from `CalculateGaussianConstants()` in `internal/render-effects/gaussian-blur-algorithm.cpp:107`. H keeps full Y while filtering X; V filters Y in display-pixel coordinates. No radius division occurs in the current PERFORMANCE shader.

At nonzero strength, absent endpoint/start early-outs:

- H: 505×1180×48 = **28,603,200** texture instructions.
- V: 505×295×48 = **7,150,800**.
- H+V: **35,754,000** per frame. H is 80% of this count.

These are shader texture instruction estimates, **not bytes fetched from DRAM, pixel taps serviced by hardware, or GPU time**. Bilinear footprints, caches, overdraw, branches and driver scheduling affect measured cost.

## J. Prefilter candidates

All candidates retain production full Source and current Output/Late Smooth. Only the blur input and H/V resolution change. No CPU downsampling, Source removal, adaptive Gaussian, reduced-tap sparse full-radius sampling or Dual Kawase.

| Candidate | Downsample fetches | Extra stages over current | Strength / limitation |
|---|---:|---:|---|
|P1 single-pass tent-like 4×|16 bilinear / quarter pixel in this PoC|1|8-source-texel support per axis; stronger antialiasing than box; nonzero blur floor|
|P2 two 2× reductions|1 linear sample per half pixel +1 per quarter pixel; stronger tent versions cost more|2|At aligned integer grids, two 2× area averages equal a 4× box, not automatically higher quality; more targets/submissions|
|P3 single-pass box/area 4×|9 bilinear / quarter pixel in the phase-correct candidate|1|Exact pixel-area weights for ceil-quarter dimensions; weaker stop-band attenuation than a well-designed tent|
|P4 compact Gaussian prefilter|e.g. 9–16 bilinear / quarter pixel, depending on sigma/support|1|Can improve attenuation; needs explicit sigma/support and radius accounting; only analyzed, not tuned/implemented|

4× decimation has destination Nyquist at 1/8 cycles per original pixel. One center bilinear sample covers only a local 2×2 footprint and is not a 4× antialias prefilter. Box and tent are finite approximations, **not ideal band-limits**. Increasing their support improves alias attenuation but adds blur and cost. Low-pass filtering must happen before discarding samples; the subsequent low-resolution Gaussian cannot undo already aliased content. See [NVIDIA GPU Gems: High-Quality Filtering](https://developer.nvidia.com/gpugems/gpugems/part-iv-image-processing/chapter-24-high-quality-filtering) and [High-Quality Antialiased Rasterization](https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-21-high-quality-antialiased-rasterization).

This bounded native PoC compares P3 area and P1 tent. Tent uses per-axis `[1,3,5,7,7,5,3,1]/32`, packed into four bilinear reads per axis (16 2D reads). An initial P3 used four fixed diagonal reads at ±1 source pixel; **that was rejected by the odd-size impulse test**. The final P3 integrates overlap between each destination footprint and original pixel cells (at most five per axis), pairing weighted neighbors into3×3 bilinear reads. It works at arbitrary ceil-quarter phases without CPU bitmap processing. No Gaussian/adaptive coefficient search was performed.

## K. Radius / sigma derivation

The DALi kernel parameter is not directly sigma. The helper solves a tail-density criterion, generates a truncated discrete kernel, splits the center weight and merges adjacent positive taps into bilinear pairs. Exact extraction from the current source:

| Authored/kernel radius | Positive pairs | Bell-curve sigma parameter |
|---|---:|---:|
|4|2|0.936655|
|8|4|2.221083|
|12|6|3.507466|
|24|12|7.372480|
|48|24|15.102511|

Thus simply selecting low radius12 and multiplying by4 gives sigma14.0299, not15.1025. It is about7.1% narrower even before prefilter/reconstruction effects. For radius24→6 the discrepancy is larger. `BlurRadius` cannot be preserved merely by integer division.

Let `s` be the **unchanged** production sequence blur strength, `dX=W/ceil(W/4)`, `dY=H/ceil(H/4)`, and `vP` the prefilter variance in full-source pixels. Approximate per-axis low Gaussian strength:

```
sLow(axis) = sqrt(max(0, (sigmaOriginal*s)^2 - vP))
             / (sigmaLowKernel * dAxis)
```

Box vP=1.25; this tent vP=2.75 at aligned 4× phases. For radius48/full strength, residual low sigma is ~3.765 (box) or3.753 (tent), requiring slightly more than strength1 against the exact low radius12 kernel. The PoC uses that adjustment, the same kernel construction algorithm, and fixed low pair count6. This is **not** grouping/sparsifying the original24 pairs. Source opacity/reveal timing and Output's radius2→8 Late Smooth thresholds are unchanged.

This variance formula is a useful starting approximation, not a proof of exact equivalence: kernel truncation, bilinear interpolation, texel phase, output reconstruction and 8-bit quantization also change the response. A fixed prefilter imposes a nonzero blur floor as s→0; `max(0,...)` cannot invert that blur. Existing sharp handoff hides it at the endpoint but does not automatically preserve all intermediate frames. Very small authored radii require a separate quality decision before a generic replacement; this study did not reintroduce a small-radius FULL fallback or change public policy.

## L. Proposed topologies / dependencies

Current: `Source(full) → H(quarter-X/full-Y) → V(quarter-XY) → Output(Source,V)`.

B1, used in the external native PoC:

`Source(full) → Downsample(quarter-XY) → H(quarter-XY) → V(quarter-XY) → existing Output(Source,V)`.

Four offscreen tasks and four logical FBOs. No read/write feedback within a draw. Source/Output remain production renderers. H/V retain update-side strength/progress constraints. The diagnostic explicitly orders its isolated WHOLE_TEXT task chain; it is not a general PER_LINE/lifecycle implementation.

B2 candidate:

`Source → Downsample writes LowA → H reads LowA/writes LowB → V reads LowB/writes LowA → Output(Source,LowA)`.

Four tasks, three FBO textures. Sequential write→read→write with different input/output images per draw is legal GLES usage; sampling the same image being rendered into is not. See [OpenGL ES 2.0 specification, framebuffer feedback](https://registry.khronos.org/OpenGL/specs/es/2.0/es_full_spec_2.0.pdf). Existing Reveal scratch sharing already relies on ordered writes/reads. GLES tracks latest producer and cross-context dependencies (`gles-texture-dependency-checker.cpp:185,232,303`).

However, **B2 is not certified here across DALi backends**. Vulkan tracks generators and dependencies by texture/render-target identity (`vulkan-texture-dependency-checker.cpp:54–85`, `vulkan-render-target.h:117–140`); a repeated LowA target must preserve the right producer generation and avoid stale/cyclic semaphore bookkeeping. The code acknowledges repeated texture writes, but that is not a runtime proof for this exact loop. Begin with B1; it already lowers payload without that additional reuse risk. No adaptor modifications proposed.

PER_LINE extension must retain each line's bounds/clamps and strength, not prefilter an unbounded atlas across line boundaries. Source/H scratch groups must remain ordered. Output sampling needs current low-texture inverse sizes. None of this requires per-image blur algorithms, but it does require packed-page/odd-boundary validation before production integration.

## M. Theoretical filtering work

FHD/radius48: Q=505×295=148,975. Low radius12 has6 pairs,12 fetches per pass.

| Filter chain | Texture instructions/frame | Reduction vs current H/V |
|---|---:|---:|
|Current H+V|35,754,000|—|
|P3 phase-correct9-fetch area + low H/V|4,916,175|86.25%|
|P1 tent + low H/V|5,959,000|83.33%|
|P2 simple 2×+2× + low H/V|4,320,275|87.92%|
|P4 illustrative9-fetch prefilter + low H/V|4,916,175|86.25%|

Source and Output work is unchanged, so whole-pipeline savings will be smaller. P2 adds another clear/submission and a half-size intermediate; the estimate does not imply it is faster than P3. The initially considered four-fetch box would be4,171,300 instructions (−88.33%) but is **not a valid generic phase-correct implementation**. Exact endpoint/start branches also reduce current work and must not be counted as full48-fetch Gaussian frames.

## N. Memory

**Logical color-attachment payload**, width×height×bytes-per-pixel; not total VRAM, CPU memory, allocator peak, MSAA/swapchain storage or driver-aligned allocation. Single WHOLE_TEXT page; same full Source in all rows.

| Topology | FBO pixels | A8 MiB | RGBA MiB | vs current PERFORMANCE |
|---|---:|---:|---:|---:|
|HIGH, 3 full targets|7,150,800|6.820|27.278|—|
|Current PERFORMANCE, S+H+V|3,128,475|2.984|11.934|—|
|B1, full S +3 quarter targets|2,830,525|2.699|10.798|−9.52%|
|B2, full S +2 quarter targets|2,681,550|2.557|10.229|−14.29%|

Pass count increasing does **not** imply increased FBO payload: current H alone is4Q; replacing H+V=5Q with downsample+H+V=3Q saves2Q. Full Source dominates the remaining storage.

The external PoC modifies public task bindings of an already-created companion; its original unused H handle can remain retained internally. Therefore these proposed memory figures are topology calculations, **not measured PoC process peak/VRAM savings**. A production implementation would allocate only the selected topology. No Source texture release or resource admission-policy change was attempted.

For multi-page equal-format batches, current H scratch sharing must be accounted for separately: B1 downsample/H may be shared scratch when dependency ordering permits, while Source and final V remain per-page for Late Smooth. Do not multiply the single-page savings blindly by line count.

## O. Quality / phase / radius evidence

Native matrix: Korean24px, Korean32px, Latin thin-stroke corpus, gradient, color emoji, local procedural ImageSpan, mixed text+ImageSpan; radii24/48; progress .05/.5/.75/.9/.99. HIGH/current PERFORMANCE/area PoC/tent PoC: **280 captured states**. The font API uses pixel size (`LabelImpl::SetFontSize:520`). No remote image/network dependency. Each state is explicitly flushed to the update thread and its current progress logged. Source is bit-identical between all four modes at all70 corresponding states (max error0), so the measured differences are filtering/output differences, not changed Reveal layout/timing or image ownership.

Output error vs HIGH, mean RGB RMSE in a fixed680×420 owner+halo region,8-bit values; average over ten radius/progress states per case. This measures difference, not a perceptual pass/fail threshold:

| Case | Current PERFORMANCE | Phase-correct area | Fixed-phase tent |
|---|---:|---:|---:|
|Korean24|4.0145|4.2729|4.1446|
|Korean32|3.7959|4.1793|4.0660|
|Latin|3.7331|4.0297|3.9484|
|Gradient|1.8123|1.9904|1.9341|
|Emoji|3.4675|3.7665|3.7457|
|ImageSpan|0.9284|1.0089|1.0045|
|Mixed image+text|2.2838|2.5079|2.4648|

The tested candidates are **not uniformly closer to HIGH** than current PERFORMANCE. Large-radius blur looks broadly comparable in the unboosted crops, but residual stroke/grid structure and the sharp/blur mixture remain around handoff. Tent was generally closer than box/area in this text matrix, yet its fixed-phase implementation fails the more stringent arbitrary-phase thin-line test below. Do not select it solely from these text averages.

Artifact assessment against current PERFORMANCE:

- Isolated1px stroke alias/shape: phase-correct area improves the severe quarter-grid sensitivity in the diagnostic; it is not perfectly shift invariant.
- Dense Korean/grid during handoff: still visible / input-dependent; no blanket improvement demonstrated.
- Intermediate glow/double sharp+blur structure: not eliminated. Existing Late Smooth is unchanged, so its mismatch with the new low-frequency input still needs visual approval.
- Gradient/emoji/ImageSpan: no missing input or new source ownership exception in these native captures; similar trade-off, not a guarantee for every resource/lifecycle scenario.
- Near endpoint p=.99: all modes converge to the same full Source path; averaged error vs HIGH is ~.013 LSB. At p=.9 with these radii all PERFORMANCE candidates already choose sharp Source, so their output is identical to one another.

Unboosted3× crops are in `quality/`, e.g. `korean24-r48-p2.png`, `emoji-r48-p2.png`, `mixed-r48-p2.png`. Panel labels `box` refer to the final **phase-correct9-fetch area** variant. Raw frames are in `captures/`; metrics in `quality.json`.

### Impulse, edge and odd dimensions

`profiles.py` runs real GLES passes on1px vertical/horizontal lines, rectangle edge,1px point and5×5 spot. Both256×256 and257×257, radii24/48, strengths1/.5/(4/radius). No CPU image downsampling. Its kernel is an exact source excerpt. Replay H/V was cross-checked against40 native Korean captures: max difference≤1 LSB (area/tent both0); see `profile-validation.json`.

Selected radius48/strength1 profiles,8-bit intermediate targets:

| Input | Mode | Integrated 1D signal | Centroid | Peak | FWHM |
|---|---|---:|---:|---:|---:|
|256px vertical line at128|HIGH|249|128.00|7|35|
|same|Current PERFORMANCE|244|116.47|13|17|
|same|Phase-correct area|248|129.50|7|42|
|same|Fixed-phase tent|244|127.73|7|36|
|257px vertical line at128|HIGH|249|128.00|7|35|
|same|Current PERFORMANCE|181|128.00|7|25|
|same|Phase-correct area|242|128.00|8|41|
|same|Fixed-phase tent|53|128.00|2|59|

Horizontal lines have the same corresponding values in this symmetric diagnostic. An initially tested fixed-offset4-fetch box completely lost the257px centered1px line (signal0): **reject that shortcut for a generic implementation**. Fixed-phase tent also loses too much coverage at that phase. The final area integration preserves that line and removes the hole, but aligned-grid centroid quantization up to1.5 source pixels and profile-shape differences remain. A reduced grid cannot promise HIGH's shift invariance.

For the256px rectangle edge, measured derivative sigma: HIGH14.930, current15.308, area15.031, tent15.113. For257px: HIGH14.930, current15.009, area15.078. This supports approximate radius preservation for broad edges; the wider/non-identical FWHM and thin-line examples show that matching variance alone does not preserve every shape. The1px point at full radius48 quantizes to zero in all four8-bit pipelines, including HIGH; therefore do not claim a meaningful point-width measurement there.5×5 spot data is retained in `profiles.json`.

The normalized destination center is `(j+.5)/ceil(W/4)`; in source-index coordinates it is `(j+.5)*W/ceil(W/4)−.5`. Using `4j+1.5` for every width is wrong for odd sizes. The area candidate computes actual source-cell overlap for each destination footprint, not a guessed half-texel shift. PER_LINE also needs line-local rectangle boundaries and clamp guards at every stage; these were not generalized in this WHOLE_TEXT PoC.

## P. Host PoC performance / CPU

Measured **native DALi chain**, not an offline shader-only benchmark: real production Source and existing Output, with external task/renderer bindings changed only for the candidate. Owner1920×1080, Korean32px corpus,24 authored lines, radius48, WHOLE_TEXT, Fade0, BlurDuration1; looping2s linear progress. A8 white and RGBA gradient. Every run logs a changing progress range (approximately.025…1); each configuration has3 independent process runs after setup warm-up, ~120 rendered frames per measured2s window.

GPU timer: `GL_EXT_disjoint_timer_query`, around each actual draw, asynchronous result collection. No capture during timing, no glFinish, no dropped/disjoint queries. Includes added prefilter draw; excludes clears, upload, resolves, present and CPU waits. It is **draw GPU time**, not total frame time or measured TV FPS. Last in-flight queries can differ by≤2 draws; comparison scope/frames are the same and the effect is much larger than that uncertainty.

Mean ms/frame:

| Format / chain | Source | Prefilter | H | V | Output | Total draw |
|---|---:|---:|---:|---:|---:|---:|
|A8 current|.10654|—|.39122|.10819|.09811|**.70407**|
|A8 phase-correct area|.10382|.03029|.02900|.02849|.09668|**.28828**|
|A8 fixed-phase tent|.10583|.04363|.02888|.02808|.09785|.30426|
|RGBA current|.13329|—|.40065|.11431|.09157|**.73982**|
|RGBA phase-correct area|.13318|.04300|.03253|.03095|.09228|**.33195**|
|RGBA fixed-phase tent|.13410|.07124|.03326|.03126|.09305|.36291|

Final area candidate: total draw reduction **59.1% A8 /55.1% RGBA**. Filtering alone (prefilter+H+V vs old H+V) is about **82.4% /79.3% lower**. Tent shows56.8% /50.9% total reduction, but its arbitrary-phase quality failure disqualifies the current fixed-offset version for production.

The added native prefilter draw itself costs ~.030ms A8/.043ms RGBA in this host workload. Task clear/submission cost is not included in that draw timer; separate process-CPU runs below include task-side overhead. No low-end TV task-overhead guarantee is inferred.

CPU uses CLOCK_PROCESS_CPUTIME_ID, separate runs **without the GPU meter or readbacks**, same2s interval and3 independent processes. ms of CPU per wall second, mean[min,max]:

| Format | Current | Area | Tent diagnostic |
|---|---:|---:|---:|
|A8|167.8[149.7,178.3]|146.4[108.3,179.0]|145.9[129.9,154.6]|
|RGBA|143.5[112.3,175.0]|159.8[145.0,186.4]|143.2[108.3,173.6]|

**No robust CPU improvement claim.** Ranges overlap and driver scheduling/busy-wait effects are material on this host; RGBA area's mean is higher. Extra task/draw bookkeeping remains even when GPU shader work falls. Source raster/metadata setup is unchanged, and the prototype's retained obsolete H means it is not a fair process-memory-peak benchmark. New event-side state is one extra actor/renderer/task plus small kernel state for this isolated page; not another CPU bitmap.

Initial diagnostic attempts had a weakly connected UBO whose owner was not retained, and timer-driven property changes needed explicit event/update flushing. Those attempts and their numbers were **discarded**, not used in the final tables. The retained kernel, logged progress sweep, Source equality and replay/native validation establish the final measurements' validity. The later fixed-phase box was likewise replaced; final `gpu-*-box.log`, `cpu-*-box.log` and `captures/*-box-*` are the corrected area variant.

## Q. B compatibility / risks / verdict

| Area | Assessment |
|---|---|
|Visual quality|MEDIUM/HIGH: lower grid cost, not uniformly higher quality; handoff/thin strokes still matter|
|Blur radius semantics|MEDIUM: sigma/radius calibration is necessary; small-radius floor and discrete reconstruction not yet closed|
|Memory|LOW for B1: calculated FBO payload decreases9.52%; allocation peak still requires integration verification|
|RenderTask count/submission|MEDIUM:3→4 per page; small Labels may not repay added overhead|
|FBO dependency|LOW/MEDIUM for simple B1 ordering; B2 ping-pong not certified across backends|
|Odd dimensions / packed lines|MEDIUM/HIGH: fixed-offset prefilters rejected; final area addresses whole-image phase, packed-line bounds remain|
|Backend|MEDIUM: tested GLES3 only; GLES2/Vulkan need targeted validation before integration|
|ImageSpan|LOW/MEDIUM: captured RGBA content naturally follows the same filter; lifecycle remains production Source responsibility|
|Gradient/emoji|LOW/MEDIUM: premultiplied RGBA is filtered together, no special feature sampler added|

Prefilter and each H/V pass use one sampler; current Output uses Source+V. This does not add samplers to the text/gradient/image Source shader, so the GLES2 eight-sampler constraint is not made worse by this topology. Loops have fixed compile-time bounds; production would use DALi's existing shader translation for ES2 (no GLES3-only textureSize requirement in the native prefilter). Highp support remains an existing Reveal-shader assumption. A8 target support keeps its existing backend gate. Vulkan uses the same algorithm, but transitions/order must be tested rather than assuming GLES results prove it.

Decorations remain outside foreground Source and keep their existing separate composition. Downsampling the Label/root instead would incorrectly blur decorations; this PoC does not do that. ImageSpan/gradient/emoji inclusion is a genuine architectural advantage over narrowly eligible adaptive sampling: the filter runs **after their existing foreground composition**, not on guessed text-only inputs.

**Verdict: PREFILTERED LOW-RES GAUSSIAN PROMISING.** Evidence supports a further bounded quality/packed-page PoC, **not immediate production replacement**. Large-radius active GPU ROI is substantial and B1 need not increase FBO payload. Exact pixel equivalence, all radii, PER_LINE page boundaries, async lifecycle and backend readiness have not been established by this external WHOLE_TEXT experiment.

## R. ROI / next priority

| Candidate | Active FPS | Idle GPU | Quality risk | Memory | ImageSpan | Complexity | Recommendation |
|---|---|---|---|---|---|---|---|
|A endpoint suspension|No active-loop speedup expected|Helps when another activity keeps frames running; none for fully sleeping scene|High if wake is late/stale|Same retained resources|Readiness hook exists, other mutations still matter|High for generic same-frame invalidation|Defer production; first prove update-side wake|
|B prefiltered low-res Gaussian|Strong host draw-time reduction; target FPS unmeasured|No independent suspension; still redraws if frames run|Moderate/high, phase/handoff dependent|B1 FBO−9.52% for this page|Naturally included after Source|Moderate; +1 task, packed-line work remains|**First follow-up: phase-correct quality/packed-page PoC**|

Suggested next order (not executed as production changes):

1. Use B1 + phase-correct area as the verified low-cost reference. Visually A/B Korean thin strokes, handoff and fractional/odd layouts against current PERFORMANCE. A properly phase-aware tent is an optional quality candidate; the fixed16-fetch tent tested here is not acceptable generically. Keep the existing public HIGH path intact.
2. If acceptable, extend only the needed PER_LINE clamped page path, then a few small/multi-Label and targetGPU samples to check the extra-task trade-off. Preserve source/metadata/async publication and ImageSpan ownership; no new APIs or clocks.
3. Revisit A separately only with a concrete same-update invalidation/wake design. Do not ship a notification-only patch that freezes old source data for reverse/seek/color/image updates.

If both eventually succeed, conceptually:

```
ACTIVE:   full Source → prefilter → low H → low V → current Output
DORMANT:  capture tasks retain last textures; Output only
WAKE:     all capture dependencies current before first changed Output frame
```

This combined state was **not implemented or measured**.

## S. Production changes / deliverables

**Production changes: NONE. HEAD unchanged; UI working tree clean.** Existing adaptor modification preserved. No commit/push, production build, full UTC or sanitizer run.

Reproducible diagnostic files: `probe.cpp`, `poc.h`, `kernel.inc`, `meter.cpp`, `run-idle.py`, `run-b.py`, `analyze.py`, `profiles.py`, `replay.cpp`, `kernel-export.cpp`. Raw outputs: task/draw logs, `gpu.json`, `quality.json`, `profiles.json`, `profile-validation.json`, unboosted quality crops. `box` is the filename key for the final phase-correct area candidate; `tent` is the rejected fixed-phase comparison.

The final B tables use `gpu.json` regenerated from the final individual `gpu-*-box.log` runs and the final individual CPU logs. Earlier console-summary files from discarded candidate iterations are not the authoritative measurement set. The initial A looping-animation run is also excluded from the idle table because it predates explicit animation event/update flushing; active timing in section P uses verified changing progress.

The two core answers:

- **A:** retaining resources and making tasks dormant is supported and verified. Generic, frame-exact reverse/seek/live-content wake is **not yet safely established** with the current text-side notification hooks; ImageSpan readiness alone does not solve it.
- **B:** proper prefilter + exact low-grid Gaussian can materially reduce active drawGPU time and FBO payload while retaining the existing full-resolution sharp Source. It can cover composed text/gradient/emoji/ImageSpan with one architecture, but the tested quality trade-off and remaining packed-line/backend/small-radius work prevent calling it production-ready.
