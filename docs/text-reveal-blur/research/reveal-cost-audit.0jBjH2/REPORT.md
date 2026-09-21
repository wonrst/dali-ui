# Text::Reveal Blur — 128 MiB policy / remaining cost audit

대상: `devel_blur_text`, HEAD `c7f2303edcc6339da0d650fef0e68d3f4a71f8b7`, 2026-09-14.

## 결론

- **MEMORY ADMISSION POLICY FINALIZED**: ceiling을 128 MiB로 변경했다. 정상 15줄은 sync/async × HIGH/PERFORMANCE 모두 허용되며, pathological/invalid 요청은 거부된다. focused UTC **7/7**, foundation/components 및 UTC build 성공.
- **SOURCE BATCHING WORTH PROTOTYPING**: 다음 후보는 A8 text-only Source 입력 atlas + draw batching이다. 아직 구현하거나 성능 향상을 입증한 것은 아니다. 서로 다른 줄 텍스처를 묶는 작업이 필요하므로 Tier 2이다.
- **NO FURTHER DEMONSTRATED TIER-1 WIN**: V 가로 crop, CPU prepared payload release, 같은 크기 H scratch sharing은 이미 적용되어 있다. 실제 4개 구성의 page occupancy도 **94.6~100%**였다. 품질·수명·backend 변경 없이 즉시 채택할 만한 추가 절감을 이번 감사에서는 찾지 못했다.
- 렌더링 최적화 prototype은 만들지 않았다. 이번 추가 수정은 128 MiB 상수와 기존 memory UTC 보강뿐이다. 이전 unstaged estimator/UTC 변경과 adaptor 변경을 보존했고, HEAD/index를 변경하지 않았다.

## PART I — Memory admission

### A. 정책과 계산

`dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp:326`

```cpp
constexpr double MAX_PREPARATION_BYTES = 128.0 * 1024.0 * 1024.0;
```

한 Reveal publication의 병적으로 큰 preparation을 사전에 거르는 cheap safety limit이다. process RSS, GPU VRAM, 여러 Label/worker 합계, 정확한 peak memory의 상한이 아니다.

기존 unstaged cheap estimator는 유지했다:

- full-raster 여유분을 한 번 계산한다.
- 각 visible sequence의 기존 ascender/descender를 이용한 generous full-width band와 radius halo를 더한다.
- 3개의 RGBA target 및 25% page 여유를 가정하고 A8/quarter/scratch sharing 절감은 차감하지 않는다.
- ImageSpan bounds는 기존 placement visit에서 합산한다. overlap은 보수적으로 중복 계산한다.
- WHOLE_TEXT는 `12 × raster pixels + 12 × halo-expanded target pixels` 및 decoration allowance이다.
- dimension/radius 검증 뒤 double 산술을 사용한다. invalid/nonfinite 값을 거부한다.

추가 계산은 O(visible sequences), estimator 전용 allocation/bitmap scan/raster/packing/GL query는 모두 0이다. 기존 sequence/placement 준비 및 ordinary rendering 자체의 비용까지 0이라는 뜻은 아니다. metrics band는 임의 font bitmap의 수학적 상한이 아니며, 후속 실제 raster/crop/texture-size/allocation 검증을 대체하지 않는다. 이 모델을 더 세분화하지 않았다.

### B. 확인 결과

| 구성 | 결과 |
|---|---|
| 1500×850, 15줄, font32, line height50, radius24 | 실제 raster 1500×750, estimate **67.745 MiB**, sync/async × HIGH/PERFORMANCE 모두 blur 활성 |
| FHD 근처 산술 sanity check | 같은 UTC의 실제 line metrics를 1920×1080 / raster1920×1000 / 20줄에 대입하면 **113.428 MiB**, overflow 없음 |
| 2560×2304, WHOLE_TEXT, radius64 | estimate **142.547 MiB**: dimensions는 허용 범위지만 새 128 MiB guard에서 거부 |
| 2048² 다수 line/radius64, 5120² | 거부 |
| NaN / infinity / 음수 / 1e30 size / UINT32_MAX radius | 거부 |

FHD 숫자는 **실제 UTC metrics를 이용한 산술값**이지 모든 native font/DPI에서 accept된다는 보장이 아니다. 별도 native FHD probe는 ordinary fallback이 관측됐으나 해당 실행에서 guard의 실제 estimate/rejection 지점을 추적하지 못했다. 따라서 그 native 실행의 accept나 원인을 주장하지 않는다. 정확한 native 경계값 연구는 이번 정책 확정 범위에서 더 진행하지 않았다.

### C. 검증 및 변경 범위

focused UTC 결과 (로컬 자료: `memory-tests.tsv`):

1. `UtcDaliTextRevealBlurMemoryAdmissionP`
2. `UtcDaliTextRevealBlurMemoryRejectionP`
3. `UtcDaliTextRevealRuntimeGaussianSamplingPathsP`
4. `UtcDaliTextRevealRuntimeGaussianAlphaBackendFallbackP`
5. `UtcDaliTextVisualAsyncBlurWorkerPayloadP`
6. `UtcDaliTextVisualAsyncBlurValidInvalidValidP`
7. `UtcDaliTextVisualAsyncBlurStaleRevisionP`

**7/7 PASS**. foundation/components build (로컬 자료: `foundation-build.log`), UTC build (로컬 자료: `utc-build.log`) 성공. 정상 case estimate trace (로컬 자료: `normal-trace.log`). `git diff --check` 통과.

이번 turn의 변경은 ceiling 256→128, normal UTC의 정책 comment, 기존 rejection UTC 안에 128~256 MiB 사이 WHOLE_TEXT 요청 추가이다. 이전 turn의 estimator/두 UTC 구현은 그대로 남는다. full regression/sanitizer/quality matrix는 실행하지 않았다.

**Verdict: MEMORY ADMISSION POLICY FINALIZED.**

## PART II — Performance / memory audit

### A. 현재 frame 비용 — 기존 측정 재사용

기준은 Korean 6줄, 551×303 Label, font24, line height40, radius40, PER_LINE, stagger .25, **8 Labels**이다. GTX1650 / NVIDIA595.91.07 / GLES / MSAA4. 기존 probe의 2초 반복 progress animation에서 약 3초를 측정한 trace를 재사용했다. 아래 CPU/GPU timing을 이번에 다시 측정하지 않았다.

원본: PERFORMANCE A8 (로컬 자료: `../reveal-memory-quality.InrGZf/timing-performance-a8.log`), HIGH A8 (로컬 자료: `../reveal-memory-quality.InrGZf/timing-high-a8.log`), PERFORMANCE RGBA (로컬 자료: `../reveal-memory-quality.InrGZf/timing-performance-rgba.log`), HIGH RGBA (로컬 자료: `../reveal-memory-quality.InrGZf/timing-high-rgba.log`).

| GPU draw 구간 | PERFORMANCE A8 ms/frame | 비중 | 정상 frame의 draw / 8 Labels | HIGH A8 ms/frame |
|---|---:|---:|---:|---:|
| Source | 0.2514 | 39.7% | 48 | 0.2399 |
| H | 0.1153 | 18.2% | 8 | 0.1666 |
| V | 0.1133 | 17.9% | 8 | 0.4363 |
| Output | 0.1533 | 24.2% | 8 | 0.1217 |
| 합계 | **0.6332** | 100% | **72** | **0.9644** |

PERFORMANCE A8 trace의 실제 draw 카운트는 180 frame 동안 Source8640/H1440/V1440/Output1439이다. Output은 측정 구간 경계 때문에 한 draw 적다. 표의 8은 정상 완전한 frame의 구조상 수치이다.

RGBA의 PERFORMANCE Source/H/V/Output은 각각 **0.2992 / 0.1300 / 0.1217 / 0.1391 ms**, 합계 **0.6900 ms**. HIGH 합계는 **1.1483 ms**이다. 두 포맷 모두 현재 가장 큰 단일 구간은 Source이다. 그렇다고 H+V 전체보다 항상 크거나, draw 제출만이 Source 비용의 원인이라는 뜻은 아니다.

기존 process CPU는 A8 PERFORMANCE/HIGH **296.94 / 289.40 ms/s**, RGBA **301.23 / 301.46 ms/s**였다. 단일 trace의 이 차이를 특정 constraint 비용이나 확정적인 회귀로 귀속하지 않는다. CPU ms/s는 모든 process thread 합계이며 frame latency가 아니다.

GPU 값은 draw query 합계이다. clear/upload/swap/compositor 전체 비용과 target의 프레임 시간을 포함하지 않는다. 따라서 desktop draw timing만으로 Mali clear 비용이 작다고 결론내리지 않는다.

### B. 메모리와 object 구성

위 A8 8 Label의 native inventory 기준이다. **논리적인 texture payload = width × height × format bytes**이며 driver padding/allocation/physical VRAM 수치는 아니다. 같은 handle은 한 번만 센다.

| 텍스처 | 1 Label dimensions/format | 8 Label payload MiB |
|---|---|---:|
| Source FBO | 418×672 A8 | 2.1431 |
| H scratch FBO | 105×672 A8 | 0.5383 |
| V FBO | 105×168 A8 | 0.1346 |
| FBO 소계 | 3개/Label | **2.8160** |
| ordinary foreground | 551×240 L8 | 1.0089 |
| ordinary/global metadata | 551×240 RGBA8888 | 4.0356 |
| 6개 line foreground | 폭326/320/334/288/312/289 × 높이28, L8 | 0.3993 |
| 6개 line metadata | 같은 각 dimensions, RGBA8888 | 1.5970 |
| 전체 관련 texture 합계 | 17개/Label | **9.8568** |

HIGH A8 FBO/전체는 **6.4292 / 13.4701 MiB**. RGBA PERFORMANCE는 **11.2639 / 18.3165 MiB**, HIGH는 **25.7168 / 32.7694 MiB**이다. RGBA 전체 수치에는 gradient LUT도 포함하며, 이 gradient fixture의 glyph foreground plane 자체는 여전히 L8이다.

대표 8 Label inventory는 Actor88, Renderer80, Geometry25, Texture136, 추가 RenderTask24이다. Actor 수에는 Label과 camera/companion 등 실제 reachable actor가 포함되고, task24는 기본 화면 task를 제외한다. Geometry25는 Source가 이미 기존 quad geometry를 공유하는 점도 반영한다.

CPU 준비 payload는 GPU texture와 구분해야 한다. 대표 A8 한 Label에서 line foreground+metadata의 pixel bytes는 261,660B, normalized global metadata는 528,960B, 합계 약 **0.754 MiB**이다. 이 값은 prepared result의 plane payload 산술이며 full raster/crop 중간 버퍼, FontClient/model/cache, 구조체, upload queue, driver 메모리를 제외한다. 여러 Label의 실제 동시 CPU peak라고 주장하지 않는다.

publication 뒤 `PreparedRevealBlur` 자체는 runtime에 남지 않는다. GPU upload가 아직 소비하지 않은 PixelData는 별도의 upload 메시지 수명까지 필요할 수 있다. 이를 임의로 먼저 free하는 것은 최적화가 아니다.

### C. Source batching feasibility — 다음 후보

근거: `text-reveal-runtime-blur.cpp`의 `CloneForeground()`와 per-line Source construction; `text-visual.cpp`의 `PublishPreparedRevealBlur()`.

Source의 foreground Actor는 **이미 page당 하나**이다. 그러나 line별 Renderer/Draw는 다음 이유로 남아 있다.

| 항목 | 줄 사이 공통 / 차이 |
|---|---|
| Geometry | 기본 foreground quad를 공유한다. geometry 복사만 없애는 것으로 해결되지 않는다. |
| Texture | 각 줄마다 별도의 foreground와 metadata texture를 bind한다. 필요하면 mask도 있다. 가장 큰 batching 제약이다. |
| Reveal timing | 줄/단위의 시작 시간은 각 metadata 내용에 들어간다. 공통 progress를 읽어도 metadata는 서로 다르다. |
| Transform / clipping | owner-local crop, page상의 배치 위치, 크기가 다르다. 줄별 격리와 sampling guard를 보존해야 한다. |
| Properties | 원래 foreground의 animatable color/gradient/visual transform 등을 mirror한다. gradient bounds는 line crop에 맞게 remap한다. |
| ImageSpan | image capture proxy는 별도 이미지 texture/준비 상태/placement를 가지며 일반 text shader로 합칠 수 없다. |
| Format | A8 text-only page와 image/gradient/color가 필요한 RGBA page는 분리된다. |

Output은 이미 Source/V **page texture를 공유**하므로 여러 quad의 UV rect를 한 geometry에 넣을 수 있었다. Source는 그 page를 만드는 단계라서 동일한 방법만으로는 묶이지 않는다.

후보는 **A8 text-only 입력 atlas**이다:

1. 이미 생성한 줄 foreground/metadata를 동일 layout의 입력 atlas 두 장에 pack한다.
2. 각 quad에 입력 atlas UV와 page placement를 싣는다.
3. 기존 Reveal opacity decode/progress 계산 및 sampling guard를 유지하는 Source shader로 한 draw에 처리한다.
4. ImageSpan/gradient/mask/mixed-format은 우선 기존 경로를 유지한다.

이 형태는 추가 FBO/pass 없이, fragment당 기존 foreground+metadata fetch 수를 늘리지 않고 설계할 수 있다. 하지만 **아직 구현/동일 출력 검증을 하지 않았다**. CPU atlas copy, padding, transient payload, line/atlas publication validation, vertex/property 전달의 변경 비용이 발생한다. 출력 atlas를 재활용해서 CPU 입력 atlas까지 공짜가 되는 구조는 아니다.

6줄 × 8 Label이 모두 대상이면 Source draw **48→8**, 전체 draw **72→32**, Source renderer는 최대 40개 감소할 여지가 있다. task24와 Source의 실제 text fragment 수, Gaussian 비용은 그대로다. Source 전체 **0.2514 ms/frame**은 가능한 GPU 절감의 느슨한 상한이지 예상 절감량이 아니다. fragment 작업은 남으므로 실제 이득은 이보다 작다. 초기 CPU copy/메모리가 이득을 상쇄할 가능성도 함께 측정해야 한다.

권장하지 않는 지름길:

- sampler array로 줄마다 두세 texture를 선택: texture unit 수와 GLES2 indexing/지원성이 걸린다.
- ordinary 전체 foreground를 각 줄에 잘라 그리기: line overlap/overhang/compressed spacing에서 다른 줄의 ink/metadata까지 들어올 수 있어 현재 격리와 같지 않다.
- ImageSpan까지 한 번에 통합: resource readiness/binding/alpha semantics를 불필요하게 확대한다.

**판정: Tier 2, A8 text-only로 한정한 feasibility PoC가 다음 1순위. 지금 production에는 미적용.**

### D. V X coverage — 이미 적용됨

`ResolveRuntimeRevealBlurTarget()`는 실제 coverage X bounds를 renderer transform으로 옮기고 **radius+4** guard를 더해 각 line target을 만든다. 이미지가 있으면 bounds를 합친다. V quad는 `lineSizes[sequence]`를 사용하며, page의 최대 폭까지 모든 줄을 늘려 그리지 않는다.

따라서 짧은 줄 오른쪽의 page padding은 V에서 이미 raster하지 않는다. H가 가로로 확장한 blur halo는 V에서 읽고 보존해야 하므로, D2 H의 세로 halo 제거와 대칭적으로 X halo를 지울 수 없다. 잔여 몇 pixel guard를 조이는 것은 filtering/scale/rounding correctness 위험 대비 이득이 작다. **추가 V X-band crop 없음.**

### E. Full FBO clear

Source/H/V는 모두 `SetClearEnabled(true)`, transparent clear, `REFRESH_ALWAYS`이며 별도 viewport를 지정하지 않는다. 실행되는 각 pass는 full target을 clear한다. D2 H geometry 축소는 clear 범위를 줄이지 않는다.

clear가 필요한 이유:

- Source: progress 역방향/seek, image READY·제거, 투명 texel의 source-over blending이 이전 내용을 자동으로 지우지 않는다.
- H: band 밖에 draw하지 않는 halo가 있고, shared scratch에는 직전 다른 page 내용이 남을 수 있다.
- V: 미시작/완료 gate와 투명 영역, 여러 line/page의 빈 공간에 stale texel이 남으면 output에 섞일 수 있다.

공통 backend에 **rectangular render-area clear 기반은 있다**. Core `render-manager.cpp:1208–1290`은 viewport/clipping에서 render area를 만들고, GLES `gles-context.cpp:1181`은 scissor+clear, Vulkan `vulkan-command-buffer-executor.cpp:324`는 동일 render area의 render-pass clear로 연결한다.

하지만 현재 TextReveal에서 이용할 수 있는 독립적인 **여러 H band만 clear하는 API**는 아니다. task viewport를 바꾸면 drawing viewport/projection에도 영향을 주며, 한 page의 여러 band 사이 및 scratch의 이전 contents까지 보장해야 한다. 단순히 viewport만 줄이는 patch는 equivalent하지 않다. tile GPU의 loadOp CLEAR 비용도 실제 target에서 따로 확인해야 한다.

**판정: backend-neutral 부분 기반은 있으나 현재 simple/local 후보는 아님. clear 유지.**

### F. H scratch / Source·V 수명

| Resource | 필요 시점 | 현재 reuse |
|---|---|---|
| Source | PERFORMANCE 최종 output이 sharp handoff를 위해 계속 sample | PERFORMANCE page별 유지. HIGH는 같은 크기·format의 page에서 scratch 공유 |
| H | 해당 page의 V pass까지 | 같은 companion 안에서 같은 full page size·format이면 공유 |
| V | 최종 output까지 | page별 유지 |

task는 page0 Source→H→V, page1 Source→H→V 순으로 등록·정렬된다. 같은 H를 다음 page가 덮기 전에 앞 page의 V가 소비한다. label/format 사이의 global pool은 없다.

24줄 native case에서는 page466×960 네 개가 실제 **H117×960 한 장**을 공유한다. Source4 + H1 + V4, 총 FBO payload **2,014,080B**이다. H를 네 장 따로 만들었을 때보다 **336,960B**를 이미 아낀다.

미세한 잠재 후보: PERFORMANCE에서 서로 다른 full width가 `ceil(width/4)` 결과는 같을 수 있다. 현재는 full page size를 비교하므로 이런 H 공유 기회를 놓칠 수 있다. 실제 H dimensions/format 비교로 넓히는 것은 비교적 국소적일 수 있지만, 이번 4개 native fixture에는 추가 공유 대상이 없었다. **확인된 절감량 0이므로 변경하지 않았다.**

max-sized H 하나를 다른 크기 page에도 사용하려면 viewport/UV/inverse-size 및 sampling 위치가 일치해야 한다. 메모리만 합칠 문제가 아니며, 큰 clear 면적 때문에 손해도 가능하다. Source/V 해제 및 endpoint teardown은 현재 sharp handoff/reverse/seek 계약과 맞지 않아 제안하지 않는다.

### G. CPU payload와 async wasted work

`text-visual.cpp:3136` publication은 Prepared 결과에서 GPU TextureSet/작은 timing·placement 정보만 runtime으로 넘긴다. prepared shared_ptr 자체는 저장하지 않는다. sync의 지역 shared_ptr는 scope 종료 시 해제된다. async는 `renderInfo.revealBlur.reset()`을 **publication attempt 후 client callback 전에** 수행한다(`:1667` 근처).

실패·stale completion의 payload는 loading task/result reference가 사라질 때 해제된다. queue 지연 중 payload가 남을 수 있지만 TextVisual이 이를 영구 보관하는 경로는 찾지 못했다. 마지막 normalized metadata upload와 reentrant publication 검증 전에 payload를 자르는 것은 안전하지 않다. **즉시 해제할 새 retained Prepared payload 없음.**

반면 **running stale work는 남는다**. AsyncTextManager cancellation은 waiting 작업 제거와 running observer/publication 대상 제거를 처리하지만, worker의 `PrepareRevealBlur()` line loop를 중단시키지는 않는다. worker parameters revision은 요청 시 snapshot이므로 이후 event-side revision을 그 값만으로 알 수 없다.

직접 controller state를 worker에서 읽는 것은 안전한 cheap generation check가 아니다. task-owned cancellation token과 loader 반환/early-exit 처리를 추가하면 별도 후보가 될 수 있으나, 현재 사용할 수 있는 기존 generation check는 없다. 이번에는 새 cancellation/lifecycle 체계를 만들지 않았다.

### H. Constraints / tasks / idle

기존 [constraint audit](../reveal-perf-opt.upcgEj/REPORT.md)의 실제 code count를 재확인했다. 6줄 한 page에서 H strength6+progress1, V strength6+progress1, PERFORMANCE output strength6으로 timing constraint **20개**이다. HIGH는 **14개**이다. Source의 property mirror와 A8 output color mirror는 별도다.

PERFORMANCE 추가분은 8 Label에서 strength evaluator48/update, 60 active updates/s라면 2880/s이다. 이는 전체 constraint CPU profiler 측정값이 아니다. 기존 cold Apply registration96/90(A8),155/149(RGBA)도 live evaluation 수와 혼동하면 안 된다. 현 target의 CPU bottleneck이나 evaluator별 유의미한 비용 근거가 없으므로 timing을 GLSL로 옮기지 않는다. epsilon/reverse/seek parity 위험을 다시 만들 이유가 부족하다.

page당 offscreen task는 Source/H/V 3개이다. 대표8 Label은24개, ImageSpan 혼합의 format 분리는 page/task 수를 늘릴 수 있다. Source draw batching이 task를 줄여 주지는 않는다. Source/H 및 H/V fusion은 source filtering/timing 또는 separable Gaussian dependency를 바꾸므로 Tier 3이다.

`REFRESH_ALWAYS`는 Reveal progress의 endpoint를 자동 인식하지 않는다. 연결된 active task가 실행되는 frame에는 0/1/정지 상태여도 clear와 pass 작업이 남을 수 있다. RenderTask 자체는 refresh rate/once를 지원하지만 이를 안전하게 복원할 Reveal-aware dirty/resume 처리는 별도다. half-rate, endpoint resource teardown, 새 idle state machine은 이번에 적용하지 않았다. 새 endpoint 성능 측정도 하지 않았다.

### I. Auxiliary texture / format

| Resource | Dimensions / format | 실제 channel / filtering | 수명 |
|---|---|---|---|
| line foreground | 줄 crop / L8 또는 RGBA8888 | coverage 또는 premultiplied color+alpha / LINEAR | 해당 runtime publication |
| line/global Reveal metadata | 대응 foreground crop 또는 전체 raster / RGBA8888 | RG=16bit start, B=ownership valid; A=CPU raster overlap의 coverage 비교 / NEAREST | 원본/line publication 및 upload |
| mixed color/gradient mask | 대응 foreground / L8 | monochrome와 color glyph 구별 / LINEAR | 해당 publication |
| gradient LUT | 현재 gradient helper의 lookup texture, 대표512×1 RGB888 | RGB color lookup / LINEAR | gradient resource 수명, 공유 가능 |
| background/overlay decoration | 해당 raster/target / RGBA8888 | color+alpha / LINEAR | decoration composition |
| Source/H/V | page / A8 또는 RGBA8888 | A8 capture는 sampled R coverage, RGBA는 premultiplied color+alpha / LINEAR | 위 Source/H/V 표 참조 |
| ImageSpan | 이미지 실제 resource size/format | color+alpha 및 image shader semantics / 원래 sampler | image resource와 capture binding 수명 |

metadata A는 최종 Reveal shader에서 읽지 않지만 **CPU raster 단계에서는 overlap ownership 선택에 사용**된다(`text-typesetter-impl.cpp:110` 근처). GPU도 최소 RG start+B validity가 필요하므로 단순 RG8 치환은 동일 계약이 아니다. RGB888 upload만 하면 이론상 metadata payload25%를 줄일 수 있으나 CPU 재포장, backend texture format 지원 및 driver 내부 RGB→RGBA 저장 가능성을 고려해야 한다. 대표 전체 metadata의 25%인 약1.408 MiB/8 Label은 **논리적 상한**이지 실제 VRAM 절감 보장이 아니다. 포맷 변경은 이번에 하지 않았다.

### J. 실제 page packing — 4개 구성만 확인

기존 native `quality-probe`를 재사용하여 production packing 함수의 실제 입력 lineSizes와 반환 batch를 GDB로 관찰했다. production 파일/라이브러리는 instrumentation용으로 수정하지 않았다. 이것은 **구조 inventory**이며 GDB 아래의 setup/runtime 시간은 성능 결과로 사용하지 않는다.

occupied는 font ink 픽셀 수가 아니라 **현재 계약상 필요한 각 line target(halo 포함) 면적의 합**이다. atlas allocated 대비 추가 빈 공간만 비교한다.

| Native fixture (1 Label) | Source page 크기 | allocated pixels | occupied pixels | occupancy / waste |
|---|---|---:|---:|---:|
| Korean6줄, r40, A8 | 418×672 | 280,896 | 265,776 | **94.62% / 5.38%** |
| Latin6줄, r40, A8 | 434×666 | 289,044 | 279,831 | **96.81% / 3.19%** |
| text4줄 + image혼합2줄, r40 | A8 263×444 + RGBA296×224 | 183,076 | 183,076 | **100% / 0%** |
| Korean24줄, r64, A8, 551×1000 | 466×960 × 4 | 1,789,440 | 1,703,040 | **95.17% / 4.83%** |

마지막 case의 각 page는6줄이다. 특정 줄 수를 고정한 결과가 아니라 현재 task/storage ceiling을 지키는 planner 결과이다. ImageSpan 두 format의 bytes는 단순 pixel 합계와 다르며 위 표는 packing 효율만 나타낸다. quarter rounding도 이 Source-space occupancy와 별개다.

로그: Korean6 (로컬 자료: `pages-korean-r40.log`), Latin6 (로컬 자료: `pages-latin-r40.log`), ImageSpan (로컬 자료: `pages-images-r40.log`), Korean24 (로컬 자료: `pages-korean-24-r64.log`). 네 실행 모두 정상 종료했고 `RELEASE ... 0`으로 추가 task 회수도 관측됐다. 폭·높이 산술만으로 실제 texel 값을 비교한 것은 아니다.

현재 planner의 storage score는 same-sized Source/H 공유를 기준으로 한 full-resolution 보수 모델이다. PERFORMANCE는 Source를 공유하지 않으므로 **PERFORMANCE 전용 실제 byte 최적해를 찾는 planner는 아니다**. 그럼에도 이번 typical cases의 빈 공간은 작고 H sharing도 동작한다. 몇 %를 위해 quality별 packing/state를 추가하거나 complex bin packing으로 확장하지 않는다. 큰 불균형 workload의 증거가 생기면 별도 검토할 수 있다.

### K. Candidate 평가

미구현 후보의 이득은 예상 방향이며 측정 성과가 아니다. '동일 목표'는 아직 byte-identical을 증명했다는 뜻이 아니다.

| Candidate | Expected CPU gain | Expected GPU gain | Memory gain | Quality impact | Lifecycle risk | Backend risk | Complexity / 판정 |
|---|---|---|---|---|---|---|---|
| A8 Source input atlas + batching | draw/renderer/property 비용 감소 가능, setup copy는 증가 가능 | Source48→8 draw 가능; fragment work는 남음 | object 감소, atlas padding/임시 CPU 비용 추가 가능 | 동일 목표, 검증 필요 | 중: publication/UV binding | 낮음~중: 기존 2D textures 활용 | **Tier2 / 다음 PoC** |
| V X crop | 없음 | 필요한 crop이 이미 있음 | 없음 | 추가 halo 축소는 위험 | 낮음 | 낮음 | **추가 이득 없음** |
| equal actual H dimensions 공유 확대 | 거의 없음 | 거의 없음 | 일부 ceil 동률 page만; 관측 절감0 | 동일 목표 | 낮음: 기존 ordering 활용 | 낮음 | Tier1 후보지만 **보류** |
| unequal-size H max scratch | 거의 없음 | 큰 clear로 악화 가능 | 여러 scratch 합을 max로 줄일 가능성 | UV/viewport parity 필요 | 중 | 중 | Tier2 / 우선순위 낮음 |
| Prepared payload 즉시 release | 없음 | 없음 | 이미 scope/reset 해제 | 변화 없음 | 강제 free는 위험 | 낮음 | **이미 적용** |
| stale worker early cancellation | rapid replacement CPU 절감 가능 | 통상 없음 | running discarded payload 감소 가능 | 동일 목표 | 높음: 새 worker token/종료 경로 | thread portability 검증 | Tier3 / 기존 cheap check 없음 |
| metadata RGB/RG 축소 | conversion CPU 증가 가능 | bandwidth 감소 가능 | RGB logical25%, 실제 GPU 미확정 | RG는 encoding 변경 필요 | 중 | 높음: formats/GLES2/Vulkan | Tier3 / 보류 |
| timing constraint→GLSL | evaluator 감소 가능, 실이득 미확정 | blur 비용 변화 없음 | 소규모 object 감소 | epsilon/reverse/seek 위험 | 중 | 중 | Tier3 / 보류 |
| partial clear | 대체로 없음 | target-dependent | 저장량 동일 | stale texel 위험 | 높음: 초기화/alias 영역 | 중: 공통 기반 있으나 계약 부족 | Tier3 / 유지 |
| page packing 강화 | 후보 탐색 CPU 증가 가능 | workload-dependent | 관측 padding3~5% 수준 | UV/placement parity 필요 | 낮음~중 | 낮음 | **현재 효율 충분** |
| pass/task fusion | submission 감소 가능 | 다중 fetch 등 반대급부 | intermediate 감소 가능 | filtering/timing 변경 위험 | 높음 | 높음 | Tier3 / 금지 범위 |
| endpoint release/refresh 억제 | idle 때만 가능 | idle 때만 가능 | release 시 감소하나 재생성 필요 | reverse/seek/resume 위험 | 높음 | 중 | Tier3 / 보류 |
| PERFORMANCE Source/V 제거 | 해당 없음 | sharp handoff/blur 손상 | 필수 resource임 | 계약 위반 | 높음 | 공통 | **제안하지 않음** |

### L. 이번 실험과 다음 단계

이번에 실제 수행한 것은 128 MiB focused build/UTC, 기존 timing 재계산, 코드 ownership/backend 감사, native4개 packing inventory이다. 렌더링 최적화 prototype, 신규 A/B 성능 benchmark, 품질 matrix는 **수행하지 않았다**. 따라서 새 GPU/CPU 성능 향상 수치도 없다.

다음 작업은 **한 가지만 추천**한다: A8 text-only Source input atlas batching PoC.

- 우선 6줄 × 8 Label에서 renderer/draw가 실제48→8로 줄어드는지 확인한다.
- foreground/metadata fetch 수, Source/H/V/FBO/task 수, timing 계산은 유지한다.
- line isolation, fractional scale, reverse/seek가 현재와 byte-identical인지 먼저 확인한다. RGBA/ImageSpan은 untouched fallback 유지 여부만 확인한다.
- 기존 harness로 baseline/candidate 약3회만 비교한다: CPU ms/s, Source/H/V/Output GPU, setup CPU, peak 임시 atlas bytes 및 retained texture payload.
- source draw 이득보다 setup copy/메모리/분기 복잡도가 크면 폐기한다. 모든 format을 한꺼번에 새 Source pipeline으로 옮기지 않는다.

그 외 항목은 위 이유로 종료/보류한다. 새로운 품질 연구, reduced taps, global budget/pool, lifecycle state machine을 열지 않는다.

## Working tree

- HEAD unchanged: `c7f2303edcc6339da0d650fef0e68d3f4a71f8b7`.
- index empty; changes unstaged; no commit/amend/rebase/stash/reset/restore.
- 기존 dirty 두 파일을 보존하고 그 안에서 threshold/UTC만 최소 추가 수정했다.
- 기존 adaptor의 `gles-texture-dependency-checker.cpp` 13-line 변경은 손대지 않았다.
- 이 보고서/관찰 script/log는 dali-ui repository 밖의 `reveal-cost-audit.0jBjH2`에 있다.
