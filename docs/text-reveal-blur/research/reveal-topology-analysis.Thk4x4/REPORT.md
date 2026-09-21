# Text Reveal PERFORMANCE — resource topology / setup analysis

2026-09-17 · Analysis only · production 변경 없음

## A. Executive verdict / baseline

**MULTI-PAGE RETAINED SOURCE IS A REAL STRUCTURAL DISADVANTAGE;
THE CARD TRANSITION BOTTLENECK IS NOT YET PROVEN.**

확인된 결론은 다음과 같다.

1. prefilter 이전 HIGH/PERFORMANCE는 **동일하게 페이지당 3 RenderTask**다.
   PERFORMANCE가 항상 task/Actor를 더 만든다는 설명은 맞지 않는다.
2. 같은 크기·포맷의 여러 페이지에서 HIGH는 Source/H를 공유한다.
   PERFORMANCE는 Late Smooth에서 Source도 읽으므로 **Source를 페이지마다 유지**한다.
   6페이지라면 FBO/그 attachment Texture가 각각 **8 → 13개**다.
3. 그렇다고 PERFORMANCE의 GPU texture bytes가 반드시 더 큰 것은 아니다.
   아래 실제 FHD fixture에서는 HIGH 7.214 MiB, PERFORMANCE 5.975 MiB(A8)다.
4. page 예산 1.5배 dry-run: FHD **6 → 4페이지 / 18 → 12 tasks**,
   PERFORMANCE FBO bytes **−9.43%**, HIGH **동일**. 우선 시도할 가장 작은 후보다.
5. 중간 크기 fixture는 이미 1페이지다. 데모 Card의 작은 Label도 보통 이 범주로 예상된다.
   **page 확대나 Source scratch 공유만으로 Card 초반 프레임 누락을 해결한다고 주장할 수 없다.**
6. A8 sharp 재구성은 수식상 가능하지만, 일반적인 좌표에서 기존 capture→보간 결과와
   pixel-exact하지 않다. sampler/feature duplication과 lifecycle 부담 때문에 2순위다.

### 실제 HEAD와 요청한 분석 baseline의 차이

실제 branch는 `devel_blur_text`, HEAD는
`38d7611b6a56270da6ff84a2a412e0a36d3a8cd9 Optimize performance reveal blur filtering`이다.
**A-R가 아직 남아 있다.** 요청의 금지 조건에 따라 제거하지 않았다.

이번 분석은 바로 아래 **`b54bb666 Batch text reveal blur sources`**를 사용한다.
이 디렉터리의 production 파일들은 `git archive b54bb666`으로 읽기용 추출한 사본이다.
현재 설치된 A-R library로 pre-A-R 성능을 새로 측정하지 않았다.
Source/output batching, D2, memory admission, lifecycle guards는 이 baseline에 있다.
adaptive Gaussian이나 A-R/prefilter 결과는 이번 후보 비교에 포함하지 않았다.

새로 수행한 것은 소스 추적과 **정수 page planner dry-run**뿐이다.
기존 bitmap/raster dimensions와 object inventory를 재사용했다.
target latency / 새 GPU timing / 새 setup timing / RSS / VRAM 실측은 없다.

## B. HIGH vs PERFORMANCE: ownership / resource inventory

기호:

- P: 한 Label의 compatible pages 수. 공유는 **동일 companion 안**, 같은 Source 크기/포맷에 한정.
- S=W×H: 한 페이지의 full-resolution 면적.
- Hq=ceil(W/4)×H, Vq=ceil(W/4)×ceil(H/4).
- c: bytes/pixel. A8 논리 payload는 1, RGBA8888은 4.
- 별도 annotation이 없으면 image/decoration 없음, page당 ≤64 lines, 연속적인 source/output 1 draw 기준.
- task 수는 **offscreen만**. 기본 window task는 미포함. FBO bytes는 driver VRAM이 아니라 논리 payload.

### B1. 기본 topology

```text
HIGH, compatible PER_LINE pages
 page 0: shared Source ─→ shared H ─→ V0 ─┐
 page 1: shared Source ─→ shared H ─→ V1 ─┤→ screen Output
 page n: shared Source ─→ shared H ─→ Vn ─┘
         이전 페이지 H/V가 소비한 뒤 scratch를 덮어쓴다.

PERFORMANCE, prefilter 이전
 page 0: Source0 ─→ shared H ─→ V0 ─┐
              └─────────────────────┤→ Output0: mix(Source0, V0)
 page 1: Source1 ─→ shared H ─→ V1 ─┤
              └─────────────────────┤→ Output1: mix(Source1, V1)
 page n: SourceN ─→ shared H ─→ Vn ─┤
              └─────────────────────┘
```

Source/H sharing: runtime:1197 (로컬 자료: `dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`).
`quarterBlur`이면 Source handle을 재사용하지 않고 H만 재사용한다.
`Pass::buffers[3]`가 핸들을 보유하며 Output TextureSet의 slot 1도 Source를 참조한다.
source/H task가 끝나자마자 C++ handle을 해제하는 구조가 아니라 **동시에 살아 있는 공유 scratch**다.
PERFORMANCE Source는 output까지 실제 읽히므로 page간 overwrite가 불가능하다.

| 항목 | HIGH | PERFORMANCE |
|---|---|---|
| WHOLE_TEXT tasks/FBO | 3 / 3 | 3 / 3 |
| PER_LINE tasks | 3P | 3P |
| Source capture | full-res | full-res, 동일 입력/Reveal |
| Source unique FBO, compatible P | 1 | P |
| H unique FBO | 1 full-res | 1 Hq |
| V unique FBO | P full-res | P Vq |
| Source 유지 이유 | H 입력, 다음 페이지에서 overwrite 가능 | H 입력 + 최종 sharp read |
| Output sample 표현식 | V 1개 | V + Source 2개 |
| Output strength | 별도 line strength 없음 | WHOLE 1개 / PER_LINE line마다 constraint |
| WHOLE FBO payload | 3Sc | (S+Hq+Vq)c |
| PER_LINE unique FBO/attachment textures | P+2 | 2P+1 |
| PER_LINE FBO payload | (P+2)Sc | (PS+Hq+PVq)c |

Output reads는 shader에 존재하는 texture lookup 표현식 수다. 실제 hardware가 endpoint에서
얼마나 제거/실행하는지는 compiler에 달려 있으며, 특정 프레임의 transaction 수라고 해석하지 않는다.
Output/Late Smooth 구현은 runtime:406–560 (로컬 자료: `dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`),
texture binding/constraint는 같은 파일 1474–1496, 1610–1642.

### B2. Actor / Camera / Renderer / Geometry

기본 Actor/Camera/Renderer **개수는 동일**하다. PERFORMANCE에서는 output vertex/property 내용이 늘어난다.
Geometry는 아래 A8/일반 PER_LINE 기준으로 같으며 WHOLE RGBA에는 예외가 있다.

| 범위 | HIGH | PERFORMANCE |
|---|---:|---:|
| WHOLE Actor, owner Label 포함 / 그중 Camera | 10 / 3 | 10 / 3 |
| WHOLE A8 Renderer / unique Geometry | 4 / 2 | 4 / 2 |
| WHOLE RGBA Renderer / unique Geometry | 4 / 3 | 4 / 2 |
| PER_LINE Actor, owner Label 포함 | 8P+3 | 8P+3 |
| 그중 Camera | 3P | 3P |
| PER_LINE Renderer, ordinary foreground 포함 | 4P+1 | 4P+1 |
| PER_LINE Geometry, D2 전용 H geometry가 있는 fixture | 4P+1 | 4P+1 |

PER_LINE의 고정 부분은 owner + companion + hidden original foreground actor.
페이지당 Source/H/V actor 3 + source foreground actor 1 + Output actor 1 + camera 3이다.
Renderer는 Source/H/V/Output 4개와 보존된 original foreground 1개.
V/full geometry를 H에서도 쓰면 D2 geometry 1개가 줄 수 있다.
WHOLE RGBA HIGH의 `CreatePlainOutput`은 별도 grid geometry를 만들며,
PERFORMANCE Output은 foreground geometry를 재사용한다(runtime:564/1474).
64-line draw split, atlas/format split, noncontiguous output, ImageSpan/decoration은 별도 증분이다.
즉 위 식을 모든 복합 Label의 보편적 상수로 사용하면 안 된다.

plain A8 input atlas가 하나인 PER_LINE fixture는 ordinary foreground+global metadata 2개,
atlas foreground+atlas metadata 2개가 FBO 외에 남는다.
따라서 전체 unique texture는 HIGH **P+6**, PERFORMANCE **2P+5**.
gradient/mask/LUT/여러 atlas가 있으면 그만큼 더한다.

### B3. PERFORMANCE-only / 공통 비용 구분

PERFORMANCE-only:

- compatible multi-page에서 Source attachment/FBO **P−1개 추가**.
- Source→H뿐 아니라 Source→Output 의존성, Source의 page별 동시 생존.
- Output 두 번째 texture/sampler binding + Late Smooth 연산 + sharp texture read.
- Output strength constraint: WHOLE 1개 또는 PER_LINE N개. line-index vertex 속성/strength array도 추가.
  N=31 fixture라면 output용 strength constraint 31개가 추가되는 것이지 task 31개가 늘지 않는다.
- PER_LINE output vertex에 line index float가 더 있어 단순 quads라면 약 16N bytes의 추가 vertex payload.
- 별도 handoff shader variant의 첫 사용 compilation/realization 가능성. event Shader handle cache가
  있어도 GPU 첫 사용 비용이 없어지는 것은 아니다. 이번에는 compilation 시간을 측정하지 않았다.
- 축소된 H/V 크기와 실제 input texel clamp 설정. allocation bytes는 오히려 감소하는 쪽.

공통:

- shaping/layout/Reveal plan/line raster/metadata/atlas 준비, CPU 임시 buffers.
- Source batching, source texture upload, source shader property mirroring.
- page planner, Actor/Renderer/Camera/RenderTask 생성 수, lifecycle/reentry 검증.
- H/V line timing constraints, Gaussian kernel 계수/샘플 수, D2 geometry.
- 정상 progress animation 도중 매 프레임 CPU raster/FBO 재생성을 하지 않는 점.

`PrepareRevealBlur`의 options에는 quality가 없고, quality 분기는 runtime sampling 설정에서 한다.
preparation:355 (로컬 자료: `dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp`),
TextVisual:3110/3185 (로컬 자료: `dali-ui-foundation/internal/visuals/text/text-visual.cpp`), runtime:2068 참조.
따라서 동일한 text/style/radius에서 PERFORMANCE만 metadata를 더 만든다는 설명은 근거가 없다.

## C. Publication/setup sequence: 어느 thread의 일인가

```text
Event/UI                     Worker (Async만)           Update / Render
SetTextReveal / layout
  │
  ├─ Sync: Controller/Typesetter → plan/raster/metadata/atlas
  │
  └─ Async request ─────────→ TextLoadingTask::Process
                             AsyncTextLoader::RenderText
                             PrepareRevealBlur
          completion ←────── prepared CPU payload
  │
  ├─ stale revision / source / layout / scene 검증
  ├─ foreground/atlas Texture::New + Upload 요청
  ├─ PrepareRuntimeRevealBlur (detached candidate)
  │    planner + FBO/Texture handles + Actor/Renderer/Geometry + constraints
  ├─ own candidate → attach → OnSceneConnection
  │    3 task/page + builtin camera/task + dependency reorder 요청
  ├─ 재진입/변경 검증 → publish metadata / image bindings
  │
  └─ 메시지 전송 ─────────────────────────────────────→ update 객체/constraints/animation
                                                        render resource realization
                                                        texture upload/allocation/FBO attach
                                                        shader/pipeline 준비
                                                        Source → H → V → Output → present
```

코드 근거:

- Sync 준비: TextVisual 3110–3150, async publication: 1671.
- Worker: TextLoadingTask:87 (로컬 자료: `dali-ui-foundation/internal/text/async-text/text-loading-task.cpp`),
  AsyncTextLoader:1813 (로컬 자료: `dali-ui-foundation/internal/text/async-text/async-text-loader-impl.cpp`).
- completion dispatch: AsyncTextManager:366 (로컬 자료: `dali-ui-foundation/internal/text/async-text/async-text-manager-impl.cpp`).
- event-side 후보 생성/attach/reentry guard: TextVisual 3155–3560.
- task/camera 생성: runtime 1692–1750. `SetBuiltinCameraActor`는 각 task의 camera를 만든다.
- Core `internal/event/rendering/frame-buffer-impl.cpp:66`의 AddFrameBufferMessage,
  texture-impl의 event→update 메시지 경로, Adaptor
  `internal/graphics/gles-impl/gles-graphics-framebuffer.cpp:118`의 실제 GL FBO 생성/attach,
  `gles-graphics-texture.cpp:182`의 texture initialization.

Async는 CPU text preparation을 옮기지만 **event publication과 GPU realization을 worker로 옮기지 않는다**.
Sync의 preparation 지연, event의 object 생성 지연, update/render의 allocation/driver 대기는 서로 다르다.
첫 visible frame이 늦어졌다고 모두 main-thread 문제라 하거나, process CPU를 UI blocking ms로 읽으면 안 된다.

PERFORMANCE-only 차이는 주로 event의 추가 bindings/constraints, multi-page allocation,
update의 추가 output constraints, render의 source fan-out/두 번째 read에 위치한다.
CPU raster/atlas 준비는 두 quality 공통이다.

### Card 초반 frame 누락을 설명할 수 있는 범위

데모는 source-ready를 기다리는 별도 앱 콜백 없이 animation과 layout transition을 시작한다.
Sample `StartSceneTextEntrance`/`StartResultReveal`은 같은 생성 구간에 여러 Label을 준비하고
delay 0/.48/.96으로 animation을 예약한다. delay가 있어도 앞서 모든 runtime이 생성될 수 있다.
GPU 첫 결과가 늦으면 그동안 update-side animation이 진행되어 중간 위치부터 보일 수 있다.
이는 가능한 경로 설명이며 **이번 target 문제의 timing trace를 확보한 것은 아니다**.

원인 후보의 우선순위는 workload에 따라 다르다.

1. **Multi-page:** 추가 retained Source/FBO와 publication churn은 확정된 구조 차이.
2. **1-page Card:** 추가 FBO/task 없음. Output binding/shader 첫 사용, 추가 constraint,
   Source fan-out과 render/driver synchronization이 차이 후보. 어느 것이 병목인지는 미확정.
3. 여러 Label의 공통 raster/publication 몰림과 Card layout 시작의 겹침은 양쪽 quality 모두의 부담.

이전 [exit CPU 보고서](../reveal-exit-cpu.VyPki6/REPORT.md)는 desktop exit에서
추가 CPU가 NVIDIA offscreen uniform-buffer `glMapBufferRange` 대기에 집중됐음을 보여줬다.
그 결과는 **TV entrance / 현 baseline의 원인을 증명하지 않는다**. 다만 낮은 GPU draw time와
높은 process CPU가 동시에 나올 수 있고, allocation 개수만으로 단정하면 안 된다는 선행 근거다.

## D. 실제 fixture / 1·3·6-page resource graph

같은 크기/포맷 페이지의 unique object 수:

| P | HIGH Source/H/V | HIGH FBO/attachment textures | PERFORMANCE Source/H/V | PERFORMANCE FBO/attachment textures | tasks / cameras, 양쪽 |
|---:|---|---:|---|---:|---:|
| 1 | 1 / 1 / 1 | 3 | 1 / 1 / 1 | 3 | 3 / 3 |
| 3 | 1 / 1 / 3 | 5 | 3 / 1 / 3 | 7 | 9 / 9 |
| 6 | 1 / 1 / 6 | 8 | 6 / 1 / 6 | 13 | 18 / 18 |

FBO와 attachment Texture가 각각 위 개수다. task를 공유하는 것은 아니다.
format/dimensions가 모두 다르면 양쪽 모두 3P FBO이며 Source 공유에 따른 차이는 사라진다.

기존 [Source batching metrics](../reveal-source-production.hWQHqE/METRICS.md)의
candidate (= Source batching 적용 후) 데이터를 재사용했다. radius24, PER_LINE, PIXEL,
Fade0, Stagger.25, BlurTime.5. 같은 r24 corpus의 prepared foreground width/height가 남아 있다.

| One Label | visible lines | page dimensions | pages/tasks | HIGH FBO / A8 MiB | PERFORMANCE FBO / A8 MiB |
|---|---:|---|---:|---:|---:|
| 500×350 | 6 | 542×480 | 1 / 3 | 3 / .7443 | 3 / .3259 |
| 500×500 | 14 | 549×1120 | 1 / 3 | 3 / 1.7592 | 3 / .7706 |
| 1920×1080 | 31 | 1970×480 ×6 | 6 / 18 | 8 / 7.2144 | 13 / 5.9750 |

같은 FBO dimensions의 RGBA라면 bytes는 4배다. ordinary textures/metadata/atlas는 표에서 제외했다.
한편 기존 실측의 전체 texture inventory는 아래와 같다. A8 단색 text도 metadata는 RGBA이므로
**전체 texture bytes까지 일률적으로 4배가 되는 것은 아니다**.

| Existing workload | HIGH total texture MiB / objects | PERFORMANCE total texture MiB / objects |
|---|---:|---:|
| 500×350 A8 ×8 Labels | 13.224 / 56 | 9.877 / 56 |
| 500×500 A8 ×8 Labels | 31.147 / 56 | 23.239 / 56 |
| FHD A8 ×1 | 25.378 / 12 | 24.139 / 17 |
| FHD RGBA ×1 | 61.554 / 15 | 56.597 / 20 |

큰 한 장과 작은 여러 allocation의 driver overhead는 backend에 의존한다.
이 표는 bytes와 object 수가 별도 지표임을 보여준다. object 증가율로 시간을 예측하지 않는다.

## E. Candidate A — larger pages / fewer tasks

### E1. Existing planner

BuildRuntimeRevealBlurBatches:1817 (로컬 자료: `dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`):

- combined page 기준: **1,048,576 pixels**. 이미 큰 single target은 분할하지 않으므로 절대 상한은 아니다.
- max width/height: runtime `GetMaxTextureSize()`. publication 측 크기 검증도 별도로 있다.
- occupied/page ≥80%, 즉 page는 occupied area의 1.25배까지 허용.
- 먼저 whole-Label 1page 후보를 확인한다. 실패하면 4-line fallback을 만들고 storage/task ceiling으로 사용한다.
- 이후 balanced partition을 시도하고, 같은 크기로 padding하여 Source/H 공유를 회복한다.
  **최종 결과가 반드시 4줄/page인 것은 아니다**. 이번 6/14/31-line 결과도 그렇지 않다.
- 탐색은 distinct integer quotient 후보로 O(√N). 새로운 packing solver를 추가할 필요는 없다.
- 평가 storage는 `ΣpageArea + 2×ΣdistinctDimensionArea`. 즉 HIGH형 Source/H 공유 모델이다.
  PERFORMANCE 전용 retained Source 비용을 최적화하는 식은 아니다.
- ImageSpan 혼합 A8는 image 없는 A8 / image 있는 RGBA를 별도로 packing한다.
  image의 예정 bounds도 union에 포함하여 ready 시 page를 갑자기 넓히지 않는다. Output은 원래 논리순으로 복원한다.
- Source draw는 atlas/texture set, shader, 연속 논리순, image boundary, 64-line cap으로도 나뉜다.
  draw split과 page/task split은 같은 의미가 아니다.
- Source/H 공유 조건은 동일한 `pass.size`와 target format. quarter H dimensions만 우연히 같아도
  현재는 공유하지 않는다. companion 간/Label 간 공유도 없다.

기존 95–100% occupancy는 특정 fixture의 결과이지 보장치가 아니다.
이번 500×350은 97.08%, 500×500은 93.57%, FHD는 **84.87%**다.
FHD는 31줄을 6page에 나누고 줄이 적은 page도 1970×480으로 normalize한다.
빈 padding은 있지만 equal-size scratch 공유를 위한 의도적인 trade-off다.
**이 FHD에서는 1M size limit과 공유/점유율 조건이 page 수를 결정하며, 단순 packing 불량이 아니다.**

### E2. Dry-run 방법과 검산

planner_dry_run.py (로컬 자료: `planner_dry_run.py`)는 b54bb666 planner를 수치적으로 옮긴 코드다.
입력은 기존 `dimensions-350/500/fhd.txt`. 이번 r24·scale1 fixture에 한해
각 line target을 `(rasterWidth+52, rasterHeight+52)`로 계산한다.
다른 scale/image fixture에 적용하는 일반식은 아니다.

current의 **page dimensions / unique FBO bytes / task count**를 기존 inventory의
HIGH/PERFORMANCE×3fixture 모두와 비교해 일치함을 assert했다.
새 bitmap/Label/app은 생성하지 않았다. MAX_TEXTURE_SIZE=4096과 2048로 계산했고
이 fixture에서는 결과가 같았다. 임의의 target 상한을 가정한 것은 아니다.
전체 결과: planner-results.json (로컬 자료: `planner-results.json`).

### E3. FHD, 기존 planner를 유지한 budget 변경

표의 Source/H/V는 **unique allocated pixels**다. clear는 전체 page의 3 task에서 clear하는 면적 합으로,
공유 scratch도 page마다 다시 clear한다. GPU 시간/실제 bandwidth 측정값이 아니다.

| Budget | page size / pages | tasks | occupancy | PERFORMANCE unique S / H / V pixels | FBOs | A8 MiB | bytes 변화 | clear pixels/frame |
|---:|---|---:|---:|---|---:|---:|---:|---:|
| 1× | 1970×480 / 6 | 18 | 84.87% | 5,673,600 / 236,640 / 354,960 | 13 | 5.9750 | 기준 | 7,448,400 |
| 1.5× | 1970×640 / 4 | 12 | 95.48% | 5,043,200 / 315,520 / 315,520 | 9 | 5.4114 | −9.43% | 6,620,800 |
| 2× | 1970×880 / 3 | 9 | 92.58% | 5,200,800 / 433,840 / 325,380 | 7 | 5.6839 | −4.87% | 6,827,700 |
| 4× diagnostic | 1970×1280 / 2 | 6 | 95.48% | 5,043,200 / 631,040 / 315,520 | 5 | 5.7123 | −4.40% | 6,620,800 |

| Budget | HIGH unique S / H / V pixels | FBOs | A8 MiB | bytes 변화 | clear pixels/frame |
|---:|---|---:|---:|---:|---:|
| 1× | 945,600 / 945,600 / 5,673,600 | 8 | 7.2144 | 기준 | 17,020,800 |
| 1.5× | 1,260,800 / 1,260,800 / 5,043,200 | 6 | 7.2144 | 0% | 15,129,600 |
| 2× | 1,733,600 / 1,733,600 / 5,200,800 | 5 | 8.2664 | +14.58% | 15,602,400 |
| 4× diagnostic | 2,521,600 / 2,521,600 / 5,043,200 | 4 | 9.6191 | +33.33% | 15,129,600 |

2×에서 task는 −50%지만 HIGH는 shared scratch 자체가 커져 bytes가 증가한다.
planner의 storage ceiling은 fallback 기준이지, **변경 전에 채택된 최적 plan의 bytes를
넘지 않는다는 보장이 아니다**. 이 둘을 혼동하면 안 된다.

500×350, 500×500은 1×/1.5×/2×/4× 모두 1page / 3tasks이며 bytes도 같다.
1.5× FHD에서는 Actor **51→35**, Renderer/Geometry **25→17**, camera **18→12**도 기대할 수 있다
(위 단순 format·1draw/page 조건). CPU raster/atlas는 같으므로 setup 전체가 1/3 줄어든다는 뜻은 아니다.

### E4. 성능/메모리 trade-off

- retained Source 때문에 큰 page가 반드시 불리하다고 할 수 없다. 여기서는 padding 감소가 유리하다.
- HIGH는 공유 Source/H 자체가 커지는 영향이 크다. 이번에는 1.5×가 균형 잡힌 후보다.
- page를 줄이면 task traversal, camera, FBO bind/clear 횟수, submission 대상이 줄어든다.
- tile GPU는 큰 target의 tile 수, load/store/cache/local memory 동작도 달라진다.
  pass 횟수 감소와 pass당 footprint 증가 중 어느 쪽이 우세한지는 target에 달려 있다.
- 논리적 clear 면적 감소가 tile fast-clear/compression을 포함한 실제 비용의 같은 비율 감소는 아니다.
- line quad 수와 Gaussian taps는 바꾸지 않는다. D2 geometry, filtering 품질, timing도 유지하는 후보다.
- 256MiB admission은 per-publication의 보수적 추정치이지 실제 VRAM/process 공통 quota가 아니다.
  이번에는 변경하지 않는다. 큰 page를 채택해도 dimension/occupancy/storage 검사를 유지한다.

## F. Candidate B — retained Source 제거

### F1. B1: authoritative input에서 sharp 재구성

**Source capture pass 자체를 없애는 후보가 아니다.** H Gaussian의 입력은 여전히 필요하다.
Output이 Source를 읽지 않게 하여, page 0의 H/V가 끝난 뒤 같은 Source scratch를 page 1이
덮어쓸 수 있게 만드는 후보다. tasks는 **3P 그대로**, Source FBO만 P→compatible-size groups로 줄어든다.
1page에서는 줄일 FBO가 없다. strength에 따라 임시로 handle을 놓아도 애니메이션 중 다시 필요하므로
일반적인 peak/live allocation 개선으로 계산하지 않는다.

기존 authoritative inputs:

- foreground/metadata atlas + 필요할 경우 gradient/color mask.
- gradient/overlay LUT 및 foreground renderer의 live properties/constraints.
- line texture crop, atlas rectangle, owner-local offset/transform, Reveal progress/fade.
- 별도로 ImageSpan proxy의 renderer/ready resource/이미지 timing.

Source batching:runtime 1245–1400 (로컬 자료: `dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`),
text shader (로컬 자료: `dali-ui-foundation/internal/graphics/shaders/text-visual-shader.frag`),
atlas preparation:146 (로컬 자료: `dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp`).

| 입력 case | sharp 재구성 가능성 | 필요한 조건 / 부담 |
|---|---|---|
| plain A8 | 가장 작게 실험 가능 | foreground LINEAR + metadata NEAREST, color alpha, 동일 progress/fade/좌표/coverage |
| Colored StyledText RGBA | 원리상 가능 | premultiplied RGBA와 text-color 처리 분기 보존; A8 fast path로 일반화 금지 |
| Global Gradient + mask | 가능하나 복잡 | foreground/mask/LUT, gradient bounds/transform, live gradient animation을 다시 평가 |
| GradientSpan | 현재 준비된 결과가 authoritative | 결과가 foreground에 bake된 부분은 그대로 읽을 수 있으나 실제 texture variant를 검사해야 함; 단색 취급 금지 |
| emoji / multicolor | 별도 variant 필요 | RGBA + 상황별 color mask; foreground tint와 color-glyph 보존 규칙 동일해야 함 |
| ImageSpan | 단순 atlas 입력으로 불가 | 이미지 내용은 text atlas에 없음. 별도 image proxy/원본 image shader의 crop·corner·opacity·ready ordering까지 재현 필요 |
| decorations | 별도 합성 유지 | foreground blur와 ordinary style/overlay의 현재 순서 유지. emboss/cutout 등 기존 unsupported/fallback도 확대하지 않음 |

GradientSpan이나 RGBA를 무조건 새로 raster할 필요는 없다. 이미 resolve된 foreground는 재사용할 수 있다.
다만 **현재 Source shader가 수행하는 남은 GPU 평가**는 Output 쪽에도 동일하게 필요하다.
ImageSpan은 이와 다르게 text foreground texture 안에 들어 있지 않다.

### F2. A8 수식과 exact-equivalence 조건

text shader 177–211의 `ResolveTextRevealOpacity`는 metadata의 16-bit start,
fade=0 STEP, progress=0/1 endpoint 보정까지 포함한다. 단순 `alpha=progress`로 대체할 수 없다.

개념적으로 source texel i의 값은 다음과 같다.

```text
M(u,p) = 기존 ResolveTextRevealOpacity(u)
F(u)   = LINEAR로 읽은 foreground coverage
C.a    = animatable text color의 alpha

S_i = Q8(coverage_i × C.a × F(u_i) × M(u_i,p))
sharp_current(x) = LINEAR(Source, x) = Σ b_i(x) S_i

단순 direct 후보:
sharp_direct(x) = C.a × F(u(x)) × M(u(x),p)
```

Q8은 A8 render target 저장 시의 정규화/양자화, coverage는 rasterized quad/clipping 경계 효과를
표현한다. owner opacity/color는 현재 Source에서 중복 적용하지 않고 Output에서 적용하는 계약도 유지해야 한다.

**곱셈과 보간은 일반적으로 교환되지 않는다.** 특히 metadata는 NEAREST, foreground는 LINEAR다.
현재는 Reveal 곱셈 후 저장된 이웃 texel들을 Source read에서 보간한다.
direct는 보간된 foreground에 현재 위치의 metadata를 곱하므로 fade/ownership 경계에서 달라질 수 있다.
또 Source의 양자화, fractional transform/UI scale/render scale, atlas guard/clamp,
원래 quad 밖의 transparent halo 처리도 같아야 한다.

단순 2-input reconstruction이 exact해질 충분조건은 매우 제한적이다:

- Source texel center와 screen/output sample의 대응이 1:1이고 재보간이 없거나,
  그 interpolation footprint 안에서 opacity/coverage가 일정하며 동일한 interpolation이 되는 경우.
- 동일한 source UV transform, texel convention, foreground/metadata sampler, crop, guard 및 clipping.
- 동일한 frame의 progress/fade/color-alpha와 endpoint 처리, 동일한 source format 양자화 결과.
- image/gradient/overlay/cutout/emboss 등 추가 source composition 없음.

일반 transform에서 capture 보간까지 재현하려면 인접 Source texel 위치마다 source 식을 평가한 뒤
보간하는 방식이 필요할 수 있다. A8만 해도 최대 4 foreground + 4 metadata reads와 V read로 커진다.
그것으로 모든 raster coverage/MSAA/정밀도 차이를 자동 해결한다고 보장할 수는 없다.
**plain A8는 feasibility가 있지만, 지금 바로 품질 손해 없는 drop-in으로 확정할 수 없다.**

### F3. 비용의 정직한 범위

| 항목 | 현재 | 단순 A8 B1 | capture 보간 재현까지 필요한 경우 |
|---|---:|---:|---:|
| Output texture read 표현식 | V+Source =2 | V+foreground+metadata =3 | 예: V+4foreground+4metadata =9 |
| tasks | 3P | 3P | 3P |
| compatible Source FBO | P | 1 | 1 |
| immutable line atlas / metadata | 유지 | 계속 유지 | 계속 유지 |
| GPU source capture | 유지 | 유지 | 유지 |

추가 mask/LUT는 별도 reads다. halo 바깥을 early reject할 수 있어도 평균 shader 비용을 측정하지 않은 채
이득으로 산입하지 않는다. Output draw가 서로 다른 atlas/variant를 사용하면 split으로 renderer 수가
오히려 증가할 수도 있다. 따라서 saved FBO objects만 보고 전체 setup이 같은 비율로 감소한다고 할 수 없다.

FHD current 6page에서 B1 성공 가정 시:

- FBO/attachment textures **13→8**, tasks18 유지.
- A8 FBO payload **5.9750→1.4660 MiB (−75.46%)**.
- topology 개수는 HIGH의 Source1/H1/V6과 같고, H/V 축소 덕분에 bytes는 HIGH보다 작다.
- 1page medium/Card는 **FBO3 / tasks3 그대로**. 이 경우 B1은 memory/setup 이득 없이
  sharp shader 비용/복잡도만 더할 수 있으므로 적용 후보에서 제외하는 것이 합리적이다.

### F4. B2: ordinary renderer로 sharp handoff

WHOLE_TEXT는 하나의 strength로 전체가 전환하므로 PER_LINE보다 단순하다.
그래도 현재 ordinary renderer의 Reveal metadata/fade가 runtime schedule과 동일해야 하고,
foreground와 decoration/image를 중복 그리지 않아야 한다.

중요한 점은 표준 source-over 두 번으로는 현재 `mix(sharp,blur,t)`가 자동 재현되지 않는다는 것이다.
각 색/alpha에 가중치를 주더라도 두 번째 source-over의 `(1−alpha)` 항이 생긴다.
기존 premultiplied mix와 같게 만들려면 별도 blending/composition 설계가 필요하다.
hard toggle은 pop, overlap은 brightness/coverage 변화가 생길 수 있다. 이번에는 threshold를 튜닝하지 않았다.

PER_LINE은 줄마다 다른 strength와 겹치는 halo가 있으므로 whole renderer 하나를 toggle할 수 없다.
line-local atlas geometry로 sharp를 그릴 수는 있으나 이는 B1과 비슷한 기능/좌표 복제에
추가 draw와 blending 문제가 붙는다. **간단한 renderer 교체 최적화로 권하지 않는다.**

### F5. B3: final composed page만 retain — CLOSED

같은 크기 P페이지에서 Source/H/V scratch 3개 + full-resolution final P개라고 가정한다.

```text
Source → H → V → sharp/blur compose FBO → screen
         3P tasks          +P tasks
```

- FBO수: P+3. tasks: **4P**.
- PERFORMANCE bytes: `[PS + S + Hq + Vq]c`.
- 현재: `[PS + Hq + PVq]c`.
- FHD 6page: FBO13→9지만 tasks18→24, A8 payload6,265,200→6,915,000 bytes
  (약 +10.37%; 정확한 ceil 크기 사용). full-size composition write/read도 추가된다.
- 1page도 tasks/FBO3→4, full-size final 한 장이 추가된다.

아주 많은 page에서는 저장량이 역전될 수 있지만, 지금 문제가 되는 setup/pass 증가 방향이다.
이번 목적에서는 **DROP**. quality-preserving이라는 이유로 추가 pass를 권하지 않는다.

## G. Candidate C — same-Label / capacity reuse

### 현재 무엇이 이미 재사용되는가

- progress 변화만으로는 runtime companion을 재생성하지 않는다. 기존 source/H/V와 constraints를 사용한다.
- 동일 revision과 config의 `SetTextReveal`은 early return한다(TextVisual:2151–2168).
- 같은 companion 내부의 source/H scratch, source atlas TextureSet, shader/kernel caches가 있다.
- image resource ready/replacement binding은 runtime image 갱신 경로가 있다.
- 모든 setter가 항상 전부 재생성된다고 일반화하면 안 된다.

하지만 새로운 text/layout/config가 실제 publication으로 이어지는 경우,
`RemoveRenderer`가 runtime을 retire하고 `PublishPreparedRevealBlur`가 새 candidate를 만든다.
공식적인 old companion FBO/camera/task capacity 이전 경로는 없다.
`runtimeBlur`가 이미 있으면 중복 publication을 거절하며 source/revision/scene/texture identity를 재검증한다.
TextVisual:480/3155/3481 (로컬 자료: `dali-ui-foundation/internal/visuals/text/text-visual.cpp`).

### 재사용 가능한 자원과 경계

| 변경 | 잠재적으로 재사용 가능한 것 | 다시 검증/갱신할 것 |
|---|---|---|
| 같은 dimensions/format, 새 text | FBO/texture storage, 일부 actor/camera/task | foreground/metadata/atlas, geometry, timing, image proxy, 모든 bindings |
| radius 변경 | 우연히 dimensions가 같다면 일부 storage | halo/target size, kernel shader, inverse sizes, prepared crop 및 plan |
| quality 변경 | full Source storage가 맞을 수 있음 | Source sharing ownership, H/V dimensions, output shader/textures/constraints |
| size 축소, 600×400→550×350 | 큰 capacity 자체는 가능 | active rect, viewport/camera, source transform, clamp, clear, output UV |

큰 Texture를 작은 FBO처럼 취급하는 것은 dimension 검사를 한 번 빼는 작업이 아니다.
FULL/Hq/Vq 각각의 capacity/active extent를 분리해야 하며 atlas boundary와 sampling을 일관되게 바꿔야 한다.
남은 영역의 stale pixels와 이전 ImageSpan/gradient LUT가 보이는 문제도 막아야 한다.

현재 lifecycle의 detached construction→revision 검증→own→attach→재검증/rollback을 유지해야 한다.
새 후보가 준비되는 동안 이전 후보의 storage를 덮어쓰면 async old-publication 유지 계약이 깨진다.
old task가 render queue에 남아 있는 시점, scene disconnect, ObjectCreated 재진입,
새 callback이 더 최신 revision을 publish하는 경우도 고려해야 한다.

**판정: same-size private resource transfer는 연구 여지가 있지만 Tier 3.**
actor/task를 통째로 재사용하거나 capacity 기반으로 확대하는 것은 작은 cleanup이 아니다.
progress 재사용은 이미 되고, 데모 장면 교체는 새 Label을 생성하므로 same-Label reuse가
Skeleton→새 Card 생성 비용 전체를 해결하지 못한다. global pool은 권하지 않는다.

## H. Candidate D — GaussianBlurEffect와 실제 비교

GaussianBlurEffectImpl (로컬 자료: `dali-ui-foundation/internal/render-effects/gaussian-blur-effect-impl.cpp`)를
현재 upstream 기반으로 읽었다. 예전의 “항상 quarter 3 pass” 설명과 구분해야 한다.

| 모드 | 실제 offscreen 경로 | tasks/FBO | lifetime |
|---|---|---:|---|
| Strength animation / fractional strength | full Source → full H → full V | 3 / 3 | animation 중 크기 안정 시 유지 |
| static strength1, default downscale .25 | half Source → quarter downsample → quarter H → quarter V | 4 / 4 | static policy에 따라 유지/once 완료 처리 |
| static downscale≥.5 | 해당 scale Source→H→V | 3 / 3 | configured scale |
| strength0 완료 | bypass/deactivate | active blur pipeline 해제 | ordinary output |

`AddBlurStrengthAnimation`: 295–334에서 `ApplyInternalDownscaleFactor(1.0f)`.
샘플 `AnimateBlurEffect`가 실제로 이 API를 호출한다. 따라서 **데모 animation 비교를
“BlurEffect는 quarter니까 당연히 싸다”로 설명하면 틀린다**.

`OnRefresh`:521–609는 resize 시 FrameBuffer들을 다시 만들고, topology가 같으면 task를 재사용해
새 FBO로 rebind한다. strength의 매 frame마다 resize/allocation하지 않는다.
scale policy 전환은 OnDeactivate→shader/renderer 교체→OnActivate가 될 수 있다.
완료 처리 854–872는 zero bypass 또는 strength1 downscale 복귀를 수행한다.
`BlurOnce`는 특별한 completion/task 해제 정책이며 Reveal 재생에 그대로 적용할 수 없다.

### 왜 구조가 단순한가

- 전체 View 한 장을 캡처한다. 각 line의 independent metadata/atlas/output rectangle/schedule을 만들지 않는다.
- final sharp/blur dual read가 아니라 V 한 장을 target renderer가 읽는다.
- Source camera 1개와 H/V가 공유하는 downsample camera 1개를 쓴다.
  Reveal은 task마다 builtin camera가 있어 3개/page다.
- Reveal의 halo, 줄별 독립 blur timing, A8/RGBA/image 분리와 같은 요구사항을 동일하게 제공하지 않는다.
- BlurEffect animation도3 tasks/Label이다. 1page Reveal과 **task수만 비교하면 같을 수 있다**.
- Effect Source는 RGBA와 `Attachment::AUTO`, Reveal은 A8 가능/`Attachment::NONE`이므로
  깊이·스텐실/driver 실제 할당까지 단순 texture byte 비교로 단정하지 않는다.

따라서 BlurEffect가 더 부드럽다는 관찰은 단순한 source graph, 적은 per-line setup,
single-texture Output과 양립한다. 하지만 tasks가 항상 적거나 메모리가 항상 적어서라고는 못 한다.
예전 demo GPU/CPU 수치는 adaptive 포함 등 baseline 차이가 있어 이번 mode 비교 수치로 재포장하지 않았다.

가져올 수 있는 설계 아이디어는 **stable topology에서 task 유지/rebind**,
공통 camera/projection의 제한적 공유, 기존 owner의 foreground를 바로 capture하는 방식이다.
단 현재 Reveal source와 H/V actors는 parent transform/position 및 task exclusive/source-tree 계약이 있어
camera를 단순 handle 교체로 공유할 수 있다고 단정하지 않는다. 이들은 후순위 분석 후보이며,
공통 GaussianBlurEffect나 adaptor를 변경하라는 제안은 아니다.

## I. Combined topology / 실제 Demo eligibility

### I1. page 확대 + B1 이론적 결합

FHD A8 input, compatible pages, B1 exact reconstruction 성공을 가정한 수치다.
**B1은 구현/품질 검증되지 않았다.**

| 경로 | Source/H/V unique | tasks | FBOs | A8 FBO MiB |
|---|---|---:|---:|---:|
| 현재 PERFORMANCE 6page | 6 / 1 / 6 | 18 | 13 | 5.9750 |
| A 1.5×, 4page | 4 / 1 / 4 | 12 | 9 | 5.4114 |
| A 2×, 3page | 3 / 1 / 3 | 9 | 7 | 5.6839 |
| B1만, 6page | 1 / 1 / 6 | 18 | 8 | 1.4660 |
| A1.5× + B1, 4page | 1 / 1 / 4 | 12 | 6 | 1.8042 |
| A2× + B1, 3page | 1 / 1 / 3 | 9 | 5 | 2.3773 |

결합은 현재 대비 좋은 수치지만 **B1만보다 memory가 증가한다**. shared full-size Source scratch가
커지기 때문이다. bytes 최소/objects 최소/tasks 최소를 동시에 달성하는 유일한 plan이 있는 것은 아니다.
또 B1 후에도 Source capture pass/task는 남는다.

참고로 rounding을 무시한 compatible-page 식에서는 PERFORMANCE가 `(17P/16+1/4)S`,
HIGH가 `(P+2)S`다. P=28에서 같고 그보다 많으면 PERFORMANCE bytes가 더 커질 수도 있다.
이는 수학적 경계일 뿐 이번 6page fixture나 실제 allocation 시간의 예측은 아니다.

### I2. Text Effect Demo Card workload

sample:1018–1104 (로컬 자료: `samples/text/text-effect-demo.cpp`)를 직접 확인했다.

- 처음 Card 3개의 day/title/places/subtitle = **12 Labels**.
- 이 중 Card 2 title에 global animated gradient가 있다. 나머지 **11/12**는
  단색 text이고 별도 ImageSpan/emoji/style/cutout을 설정하지 않아 A8 fast-path의 기본 feature 조건에 맞는다.
- 나중에 badge(단색)와 action(global gradient)이 추가되면 **12/14**.
- A8 실제 사용은 backend GLES/GLSL≥300, 실제 foreground L8라는 runtime 조건도 만족해야 한다.
  font fallback/color glyph 결과까지 source만으로 보증하지 않는다.
- Card root background/border는 text foreground decoration이 아니며 이 eligibility를 곧바로 깨지 않는다.
- title는 height54/font24–30 fit, places는64/font18, subtitle는82/font16.
  entrance radius는 Strong24/Medium20/Soft16. Card 최소 width280, text horizontal padding50.
- 이 크기와 짧은 corpus의 각 Label은 통상 **1page**로 예상된다. target의 실제 line metrics/page inventory를
  이번에 새로 측정한 것은 아니므로 정확한 target page count로 보고하지 않는다.
- sample의 기본은 Sync. 새 scene builder가 새 Card/Label을 생성하고, 완료 뒤 `ClearTextReveal`로
  task를 해제한다. 기존 scene resource를 global pool처럼 계속 돌려 쓰는 구조가 아니다.

**Q6 답:** A8 feature coverage는 높지만, retained-Source 공유의 성능 coverage는 낮을 가능성이 높다.
한 Label당1page이면 B1/A 모두 줄일 Source/page가 없기 때문이다.
여러 Label의 Source를 서로 공유하는 것은 전혀 다른 설계이며 이번 추천에 포함하지 않는다.

## J. Risk / candidate comparison / priority

### Candidate E — setup을 여러 frame으로 분산

object 생성량을 줄이지 않고 publication 시점만 바꾸는 방법이다.
대기 중 old publication 유지, progress/layout의 시작 기준, image-ready와 취소 순서가 얽힌다.
blur가 준비되기 전 ordinary text를 잠깐 보이게 하면 이미 해결한 flash 문제가 재발한다.
현재 앱이 callback을 기다리지 않는 계약도 바뀔 수 있다.
**이번 primary optimization으로 권하지 않는다.** 다른 구조 개선이 부족할 때 별도 UX scheduling 의제로만 남긴다.

### 비교표

CPU는 새로 측정하지 않았다. “감소 가능”을 측정된 ms로 읽지 않는다.

| Candidate | Task 감소 | FBO objects | bytes | Setup CPU | Shader/steady cost | Quality risk | Lifecycle risk | 복잡도 |
|---|---|---|---|---|---|---|---|---|
| A:1.5× page, FHD | 18→12 | PERF13→9, HIGH8→6 | PERF−9.43%, HIGH동일 | object 생성 부분 감소 가능; 미측정 | taps/line 그대로, page clear 감소 | 낮음; packing 경계 비교 필요 | 낮음; 기존 ordering 유지 | 낮음 |
| A:2× page, FHD | 18→9 | PERF13→7, HIGH8→5 | PERF−4.87%, HIGH+14.58% | 미측정 | 같은 filtering, 더 큰 scratch | 낮음 | 낮음~중간 | 낮음 |
| B1: A8 input reconstruction | 없음 | compatible P−1 감소 | FHD−75.46%, 1page이득없음 | allocation감소 vs binding증가; 미측정 | Output2→3 또는 더 많은 read | 중간~높음, 보간/양자화 | 중간; source/binding 동기화 | 중간~높음 |
| B2: ordinary/sharp handoff | 기본 없음 | 조건부 Source공유 | 미확정 | draw/state 증가 가능 | extra draw/blend 또는 toggle | 높음 | 중간~높음 | 높음 |
| B3: final composed FBO | **+P** | 6page13→9 | 예시+10.37% | extra task/output 생성 | full-res compose 추가 | exact composition은 가능 | 중간 | 높음 |
| C: same-size local reuse | active수 동일 | active수 동일, 재할당 횟수 감소 | live감소 아님; overlap peak 불명 | republish에만 기대 | steady동일 목표 | sampling scope 유지 시 낮음 | **높음** | 높음 |
| C: capacity reuse | 동일 | capacity 유지 | 오히려 retained bytes 증가 가능 | 미측정 | active-rect/clamp 추가 | 중간 | 높음 | 높음 |
| D: stable tasks/shared camera 아이디어 | tasks동일 | 기본동일 | 큰 변화 기대 없음 | 일부 object/churn만 | 기본동일 | transform 검증 필요 | 중간 | 중간 |
| E: frame 분산 | 총수 동일 | 총수 동일 | overlap에 따라 변화 | 피크를 옮김, 총량감소 아님 | 동일 | 첫 표시/timing 변경 | 높음 | 높음 |

위험 관리 공통 원칙:

- ImageSpan source capture/ready/hidden-original/proxy ownership는 그대로 유지.
- gradient LUT/live property, premultiplied alpha, halo/source-over order 보존.
- async revision/source/layout identity 및 detached candidate rollback 약화 금지.
- 다른 source page가 scratch를 소비하기 전에 overwrite/clear하면 안 됨.
- page 변경의 품질 위험은 sampling/ceil/line boundary 검증으로 한정하고, 새 filter나 timing 식은 도입하지 않음.
  특히 quarter 크기가 ceil되는 홀수 page/line 크기와 fractional scale에서는 page 확대가 sample phase에
  영향을 줄 수 있다. r24의 80px row fixture가 통과한다고 모든 크기의 pixel parity를 보장하지 않는다.
- 논리 FBO payload 외 ordinary/global metadata, atlas, queued uploads, old/new overlap,
  driver caches/MSAA/depth allocations도 실제 peak에 존재한다. 이번 수치로 total VRAM을 보증하지 않음.

우선순위:

- **Tier 1:** A 1.5× page. 가장 작은 변경으로 확인 가능한 multi-page task/object 효과.
- **Tier 2:** B1 plain A8, multi-page 한정. 이후 A와 결합 가능하나 exact sampling 검증이 먼저.
- **Tier 3:** B2, local resource/capacity reuse, task/camera 공유, 별도 UX setup scheduling.
- **DROP:** B3, 추가 filtering pass, prefilter/A-R, 새 sampling approximation, global pool,
  scene/Label 간 scratch 공유를 이번 작은 최적화에 끼워 넣는 방향.

## K. 다음 PoC — 하나만 추천

**기존 page planner의 combined-page pixel budget만 1M→1.5M으로 올리는 bounded A/B PoC.**

정확한 범위:

- pre-A-R baseline에서 수행. 지금 HEAD의 A-R 제거는 별도의 사용자 승인/작업이며 이번에는 하지 않았다.
- page budget 이외 packing 알고리즘, occupancy≥80%, dimension limit, storage ceiling,
  memory admission, format split, scratch ownership, Source/H/V shader와 timing 모두 유지.
- source atlas layout이나 D2, line metadata 생성 범위는 변경하지 않는다.
- current/1.5× 비교만. 2×/4×나 B1을 같이 구현하지 않는다.
- FHD fixture에서 예상값 6→4pages /18→12tasks /PERF FBO13→9를 직접 확인.
  HIGH bytes 증가 여부, mixed image-format pages 및 uneven tail의 ordering/guard가 acceptance point.
- 같은 앱/설정에서 target 비교 가능하도록 작은 국소 변경으로 둔다. API/architecture 확대 없음.
- 1page Label과 demo Card는 negative control: resource topology가 같아야 한다.
  이쪽의 개선이 없으면 실패를 숨기지 않고 **이 PoC는 multi-page 최적화**로만 평가한다.
- 예상 setup-ms/FPS 개선율은 제시하지 않는다. object 감소가 실제 transition 지연을 줄이는지는 다음 PoC의 질문이다.

이 후보는 현재 Card 현상을 반드시 고친다는 약속이 아니라,
가장 적은 품질/lifecycle 위험으로 task/FBO 비용 가설을 검증하는 다음 한 단계다.

## L. 이번에 권하지 않거나 하지 않은 것

- prefilter, A-R, adaptive/reduced-tap Gaussian, reconstruction curve tuning, half-rate.
- Source가 필요 없다는 이유로 Source capture task까지 제거했다고 계산하는 것.
- B1을1page A8에도 무조건 적용하는 것.
- Output samplers 증가만으로 B1을 자동 기각하거나, memory 감소만으로 자동 승인하는 것.
- PERF shader가 가볍다는 이유로 첫-visible latency도 반드시 좋아진다고 단정하는 것.
- target trace 없이 추가 main-thread blocking이나 새 driver bug를 원인으로 확정하는 것.
- 큰 PoC, app실행, production build, full regression, sanitizer, GPU/CPU 광범위 재측정.

## M. Git / 산출물

- UI HEAD `38d7611b6a56270da6ff84a2a412e0a36d3a8cd9`, branch `devel_blur_text` 그대로.
- UI worktree 시작/종료 clean. production source unchanged.
- Core HEAD `f43e95be477ad301f84ecc772c753f9357821ecc`, Adaptor HEAD
  `dcadcfdc3e0d1abdce20767bee2858cb9e1851a4`, 두 worktree도 clean이었다. 변경하지 않았다.
- reset/restore/stash/amend/commit/push 없음. 기존 커밋/remote 상태 보존.
- 새 산출물은 이 repository 밖 디렉터리에만 저장:
  이 보고서, read-only b54bb666 source snapshots, planner script (로컬 자료: `planner_dry_run.py`),
  numeric results (로컬 자료: `planner-results.json`).
- raw 재사용 출처: `reveal-source-production.hWQHqE/dimensions-*.txt`, `matrix.json`, `METRICS.md`.
  WHOLE object 수는 `reveal-ar-production.FY6dsQ/inventory-0-current.log`의 pre-A-R inventory와
  source graph를 대조했다. 역사적 host 측정값을 새 baseline의 측정값으로 주장하지 않는다.
