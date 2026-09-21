# Text Effect Demo: pre-A-R PERFORMANCE의 1-page 비용 감사

## A. Executive verdict

**SAME TOPOLOGY; EXTRA OUTPUT TIMING + SOURCE COMPOSITION — TARGET BOTTLENECK NOT YET PROVEN.**

동일 radius에서 PERFORMANCE가 Task/FBO/Actor를 더 만드는 것은 아니다.
확인된 추가 비용은 **Output의 줄별 strength 계산·property 갱신과 full-resolution Source 재샘플링**이다.
이것이 타겟의 transition 저하 중 얼마를 차지하는지는 이번 조사로 확정할 수 없다.

- 실제 1280×720 데모의 초기 Card Label은 12개, 20줄이다. **9개는 1페이지, 3개는 2페이지**, 합계 15페이지다.
- HIGH/PERFORMANCE 모두 12 companions, 45 offscreen tasks, 45 FBOs, 같은 Actor/Renderer 수다.
- PERFORMANCE는 Output strength constraint가 **20개 추가**된다. 활성 renderer의 update에서 계속 실행되는 계산이지, 생성 때만 발생하는 작업이 아니다.
- Soft→Strong은 같은 quality에서 FBO attachment 저장량을 약 **36% 증가**시킨다. 그렇지만 PERFORMANCE Strong의 저장량은 HIGH Soft보다 **40.4% 작다**.
- host에서 실제 Intro/Skeleton을 거친 첫 Card 진입에는 새 Shader 객체·GL compile·link가 모두 **0회**였다. warm 재생성도 0회다. shader prewarm은 현재 증거로 우선순위가 낮다.
- 다음 PoC는 **PERFORMANCE의 batched Output timing만 vertex로 옮기는 작은 비교본** 하나를 추천한다. H/V, Late Smooth fragment 식, Source read, task/FBO는 유지한다. 이 workload에서는 20 constraints가 통째로 없어지는 것이 아니라, progress 전달 15개가 필요하므로 **순감소는 5개**다. 기대치를 과장하면 안 된다.

### 기준 및 조사 방법

요청과 달리 시작 HEAD에는 A-R이 남아 있었다.

```text
branch: devel_blur_text
HEAD:   38d7611b6a56270da6ff84a2a412e0a36d3a8cd9
        Optimize performance reveal blur filtering
baseline: b54bb666 Batch text reveal blur sources
```

따라서 production tree는 그대로 두고 **b54bb666의 pre-A-R runtime/factory/shader/sample**을 기준으로 분석했다.
Source/Output batching, D2, lifecycle hardening, admission은 유지되며 adaptive/prefilter는 조사 경로에 없다.

호스트 진단은 실제 baseline sample을 include한 외부 probe.cpp (로컬 자료: `probe.cpp`)에서 수행했다.
Ubuntu / GTX 1650 / NVIDIA 595.91.07 / GLES / MSAA 4 / 1280×720 / Sync 조건이다.
HIGH Soft, PERFORMANCE Soft, HIGH Strong, PERFORMANCE Strong, Blur OFF 각 1 process에서
**첫 Card 구성 1회 + warm 재구성 1회**, 총 2개 inventory snapshot만 수집했다.
Blur OFF도 일반 Reveal은 사용한다. 일반 Label-only control이 아니다.

기존 현재 build object를 재사용하되, b54bb666의 runtime·blur-renderer 두 TU와 원래 blur fragment 상수를
외부에서 빌드/링크한 private foundation library를 사용했다. 설치나 production build는 하지 않았다.
HEAD의 미사용 GetKernelSigma 메서드/미사용 prefilter shader 상수는 linked objects에 남지만,
이 diagnostic의 runtime에는 그 호출 경로가 없다. H가 full Y, V가 quarter Y이고 3 stages인 것도 native inventory로 확인했다.
재현 구성은 build_diagnostic.py (로컬 자료: `build_diagnostic.py`)에 있다.

원본 sample의 동작은 보존했다. 진단 wrapper가 quality/radius를 선택하고,
Intro 후 Generating 상태로 이동하며, 원래 scene-finished callback을 그대로 호출하면서 측정 phase만 표시했다.
실제 timer가 Skeleton→Cards를 실행한다. 모든 12 companions가 준비된 초기 진입과,
완료 후 같은 Cards를 재생성한 시점에서 객체를 조회했다.
Constraint Apply/GL 호출은 인자를 바꾸지 않는 pass-through 계수이며 constraint는 WeakHandle만 기록한다.

**측정하지 않은 것:** CPU ms, GPU ms, FPS, 타겟 성능, RSS/VRAM, 전체 앱 peak.
아래 bytes는 관측한 texture dimensions × bytes-per-pixel의 논리 저장량이다.

## B. Text Effect Demo actual workload

설정 근거: baseline sample (로컬 자료: `samples/text/text-effect-demo.cpp`)의 `NewItineraryCard`,
`BuildResultsScene`, `ConfigureEntranceReveal`, `StartSceneTextEntrance`, `StartResultReveal`.

### Label 수와 공통 설정

- 초기 scene에 mount되는 Label은 **17개 = Card 12개 + 외부 5개**다.
- Card 2의 badge/action Label 2개는 미리 생성하지만 처음에는 detached 상태다. 초기 blur inventory에서 제외한다.
  reveal 완료 뒤 mount되어 총 19개가 된다.
- 하단 버튼, loading Label, 이전 Skeleton scene, Markdown은 아래 Card resource 합계에 포함하지 않는다.
- 초기 Card 전부 `PIXEL / PER_LINE / Fade=0`, 기본 Sync다.
- 짧은 copy는 `Stagger=0 / BlurDurationRatio=1`; subtitle은 `Stagger=.25 / BlurDurationRatio=.5`다.
  분류는 실제 줄 수가 아니라 UTF-8 continuation byte를 제외한 문자 수 **64 이상**이다.
- preset은 source 그대로 **Soft: entrance16/exit32, Medium:20/40, Strong:24/48**이다.
  이번 진입 비교 radius는 16과 24다. 퇴장 radius48과 섞지 않았다.
- quality는 네 비교별 HIGH/PERFORMANCE로 고정했다. 앱 기본값은 PERFORMANCE다.
- Card 2 title만 text gradient이고 RGBA capture다. 나머지 11개는 이번 GLES 환경에서 A8 capture다.
  Cards에는 ImageSpan/emoji/underline/shadow가 없다.

### 각 Card의 실제 copy

| Card | day | title | places | subtitle |
|---|---|---|---|---|
| 1 | DAY 1 | Forest & Oreum | Bijarim Forest · Abu Oreum · Local Cafe | Walk beneath ancient cedars, climb a quiet oreum, and end with coffee among Jeju's green landscapes. |
| 2 | DAY 2 | Sea & Sunset | Woljeongri · Sehwa · Hamdeok | Follow the eastern coastline slowly, leaving time for ocean views, village walks, and an unhurried sunset. |
| 3 | DAY 3 | Market & Old Town | Dongmun Market · Old Jeju · Local Dessert | Browse the morning market, discover old alleyways, and finish the trip with Jeju's local flavors. |

### Layout / font / animation

Card root: minimum 280×379, flex-grow 1, 내부 padding 좌우25/상하22, vertical gap12.
Label은 `StackLayoutParams::FILL`이며 명시적인 고정 width를 주지 않는다.
이 호스트 창에서는 각 Label의 실제 layout width가 **339.333px**였다.

| 타입 | requested height/배치 | 실제 size | authored font / family | 실제 lines (Card1/2/3) | pages (1/2/3) | Reveal duration |
|---|---|---|---|---|---|---|
| day ×3 | 30px eyebrow row에서 weight1/FILL | 339.333×30 | 14 / SamsungOneUI_700 | 1 / 1 / 1 | 1 / 1 / 1 | .42s |
| title ×3 | height54, FILL | 339.333×54 | 31 / SamsungOneUI_700, Fit Range24–30 step1 | 1 / 1 / 1 | 1 / 1 / 1 | .88s |
| places ×3 | height64, FILL | 339.333×64 | 18 / SamsungOneUI_500 | 2 / 1 / 2 | 2 / 1 / 2 | 1.18s |
| subtitle ×3 | height82, FILL | 339.333×82 | 16 / SamsungOneUI_400 | 3 / 3 / 3 | 1 / 1 / 2 | 2s |

font31은 authored `GetFontSize()` 값이지 실제 fitting 후 glyph 크기가 31이라는 뜻이 아니다.
title/places/subtitle은 multiline, line height는 각각 1.02/1.22/1.25다.

Card별 delay는 0/.48/.96s, day/title/places/subtitle offset은 0/.10/.30/.42s이며,
Reveal 시작에는 추가 .06s lead가 있다. **Reveal과 12px 아래에서 올라오는 .60s text slide 모두 EASE_OUT_SQUARE**다.
Card root에도 별도의 .36s entrance transition이 있다. 따라서 Reveal timing과 layout/opacity animation이 겹치는 실제 workload다.

외부 5개 Label도 같은 entrance 설정을 받는다. 아래 resource 합계에는 포함하지 않지만 무시할 수 없는 동시 workload다.

| copy | requested layout | font/family | timing |
|---|---|---|---|
| DALI UI AI TRAVEL CONCIERGE | height30 header, weight1/FILL | 14 / 700 | short |
| JEJU  ·  3 DAYS  ·  RELAXED | height30 header, END align | 14 / 500 | short |
| Your personalized escape | height58, FILL | 39 / 700 | short |
| A slower route through forests, coastlines and local neighborhoods. | height38, FILL | 17 / 400 | long |
| Select the best match to open your detailed AI itinerary | height36, FILL, centered | 14 / 400 | short |

family suffix는 모두 SamsungOneUI이다. 이 5개도 PIXEL/PER_LINE/Fade0, 동일 radius/quality/Sync이며 text gradient는 없다.
short는 duration1s/stagger0/blurTime1, long은 2s/.25/.5다.

### 실제 page 판정

**대부분 1페이지지만 전부는 아니다.** 1-page 9개, 2-page 3개다. 두 radius와 두 quality에서 동일했다.
Card1 places, Card3 places, Card3 subtitle의 짧은 마지막 줄은
whole-page의 occupied-area 조건(최소80%)을 만족하지 못한다.
예를 들어 Soft Card1 places는 346×58 + 80×54를 346×112로 합치면 점유율이 약63%다.
1M pixel 한도에 걸린 것이 아니다.

각 2-page Label의 두 페이지 크기는 서로 다르다. 같은 Label의 equal-size Source/H scratch 공유가
가능한 형태가 아니므로 **HIGH/PERFORMANCE 간 scratch 공유 차이도 이 workload에는 없다**.
page count 자체는 양쪽의 공통 비용이다. page planner/multi-page Source retention을
동일 radius의 quality 차이에 대한 primary 원인에서 제외한다. planner 변경은 수행하지 않았다.

### Source/H/V 실제 크기

아래는 페이지별 Source `W×H`다. HIGH는 Source=H=V이다.
PERFORMANCE는 모든 행에서 `Source=W×H`, `H=ceil(W/4)×H`, `V=ceil(W/4)×ceil(H/4)`다.
전체 stage 치수는 inventory.json (로컬 자료: `inventory.json`)에 있으며, A8는 1B/px, RGBA는 4B/px다.

| Label | format | Soft Source | Strong Source |
|---|---|---|---|
| day ×3 | A8 | 79×50 | 95×66 |
| Card1 title | A8 | 274×62 | 290×78 |
| Card2 title | RGBA | 240×62 | 256×78 |
| Card3 title | A8 | 318×63 | 334×79 |
| Card1 places | A8 | 346×58 + 80×54 | 362×74 + 96×70 |
| Card2 places | A8 | 315×58 | 331×74 |
| Card3 places | A8 | 344×58 + 107×53 | 360×74 + 123×69 |
| Card1 subtitle | A8 | 364×164 | 380×212 |
| Card2 subtitle | A8 | 328×164 | 344×212 |
| Card3 subtitle | A8 | 367×110 + 139×52 | 383×142 + 155×68 |

예를 들어 Card1 subtitle은 HIGH Soft의 S/H/V가 모두364×164인 데 비해,
PERFORMANCE Strong은 **S380×212 / H95×212 / V95×53**이다.
이 치수는 host의 실제 layout/font 결과이며, TV에서도 동일한 line/page 수라는 보장은 없다.

## C. HIGH vs PERFORMANCE — same radius

### 1-page PER_LINE Label

N줄이1 draw batch에 들어가고, Source/Output 각각1 draw로 표현되는 이번 단일-format Label 기준이다.

| resource/work | HIGH | PERFORMANCE |
|---|---:|---:|
| Source/H/V task | 1 / 1 / 1 | 1 / 1 / 1 |
| total offscreen tasks / cameras / FBOs | 3 / 3 / 3 | 3 / 3 / 3 |
| FBO attachment texture objects | 3 | 3 |
| Source | W×H | W×H |
| H | W×H | ceil(W/4)×H |
| V | W×H | ceil(W/4)×ceil(H/4) |
| Label subtree Actors (camera 포함) | 11 | 11 |
| Label subtree Renderers (ordinary 포함) | 5 | 5 |
| bound texture objects (이번 A8 / gradient) | 7 / 8 | 7 / 8 |
| Output texture bindings | V:1 | V+Source:2 |
| Output strength constraints | 0 | N |
| H/V strength evaluators | 2N | 2N |
| H/V common-progress mirrors | 2 | 2 |
| blur timing constraints 합계 | 2N+2 | 3N+2 |
| H/V renderer property count (각각) | 37+N | 37+N |
| Output property count（A8 / RGBA） | 34 / 33 | 35+N / 34+N |
| Gaussian shader | radius+batch별 공통 cache | 같은 shader/kernel |
| Output shader | full/A8 또는 RGBA | quarter/Late Smooth variant |

property count는 renderer API가 열거하는 built-in/custom 합계이며 UBO bytes가 아니다.
Output 차이는 authored radius property 1개와 line strength N개다.
복수 format/64줄 초과/그리기 순서에 따른 split에는 이1-draw 식을 그대로 적용하지 않는다.

### 초기3 Cards (12 Label)의 구성 완료 시점 live inventory

| 항목 | HIGH Soft | PERF Soft | HIGH Strong | PERF Strong | Blur OFF |
|---|---:|---:|---:|---:|---:|
| blur companions | 12 | 12 | 12 | 12 | 0 |
| lines / pages | 20 / 15 | 20 / 15 | 20 / 15 | 20 / 15 | blur page 없음 |
| Actors | 156 | 156 | 156 | 156 | 12 |
| Cameras (Actors에 포함) | 45 | 45 | 45 | 45 | 0 |
| Renderers | 72 | 72 | 72 | 72 | 12 |
| Geometries（unique） | 54 | 54 | 54 | 54 | 1 |
| offscreen RenderTasks | 45 | 45 | 45 | 45 | 0 |
| FBOs | 45 | 45 | 45 | 45 | 0 |
| unique bound textures | 94 | 94 | 94 | 94 | 25 |
| live applied constraints | 366 | 386 | 366 | 386 | 36 |
| 그중 H/V strength | 40 | 40 | 40 | 40 | 0 |
| 그중 Output strength | 0 | 20 | 0 | 20 | 0 |
| 그중 progress mirrors | 30 | 30 | 30 | 30 | 0 |
| 나머지 constraints | 296 | 296 | 296 | 296 | 36 |

각 subtree는 owning Label Actor/ordinary renderer도 포함한다. 따라서 blur가 추가한 Actor는144개,
renderer는60개이며, Cameras45는 Actor156에 중복 포함된 내수다.
기본 window task, Card root/scroll/layout 컨테이너, 외부5 Label 등은 제외했다.
위 수치는 생성 후 살아 있는 객체를 중복 제거한 inventory다. 생성 중 폐기된 transient 객체까지 센 cumulative allocation count가 아니다.
초기/재생성 두 snapshot에서 위 값은 모두 같았다.

## D. Soft vs Strong — radius effect

### 실제 FBO attachment payload

| 구성 | Source bytes | H bytes | V bytes | 합계 bytes | MiB |
|---|---:|---:|---:|---:|---:|
| HIGH Soft | 337,759 | 337,759 | 337,759 | 1,013,277 | .9663 |
| PERF Soft | 337,759 | 84,637 | 21,566 | 443,962 | .4234 |
| HIGH Strong | 459,231 | 459,231 | 459,231 | 1,377,693 | 1.3139 |
| PERF Strong | 459,231 | 115,061 | 29,202 | 603,494 | .5755 |

- 동일 quality에서 radius16→24: HIGH **+364,416B / +35.96%**, PERF **+159,532B / +35.93%**.
- 동일 radius에서 PERFORMANCE: HIGH보다 attachment payload **약56.2% 감소**.
- HIGH Soft→PERF Strong: Source는 **+121,472B / +35.96%**지만 S/H/V 합계는 **−409,783B / −40.44%**.
- bound texture 전체 (ordinary/atlas/metadata/LUT/FBO 포함, unique):
  HIGH Soft 2.2039MiB / PERF Soft 1.6609MiB / HIGH Strong 2.5514MiB / PERF Strong 1.8131MiB。
  FBO 외 합계는4조건 모두 **1,297,636B**로 같았다.

driver allocation/alignment, deferred deletion, window MSAA, CPU copy는 포함하지 않는다.
**실측 VRAM이나 전체 화면 peak가 아니다.**

### radius가 바꾸는 부분 / 바꾸지 않는 부분

`ResolveRuntimeRevealBlurTarget`의 full extent는 `ceil(control)+2*(radius+2)`다.
line coverage를 crop하는 runtime guard는 별도로 `radius+4`이며 full extent로 clamp한다.
따라서 “모든 경우 단순히 radius+2만큼 line margin”이라고 표현하면 부정확하다.
이번 실제 결과에서는 radius8 증가에 따라 각 line target의 width/height가16씩 증가했고,
3줄을 세로로 pack한 page 높이는48 증가했다.

CPU foreground/metadata의 crop guard는 coverage 주변2px이며 blur radius만큼 CPU bitmap을 pad하지 않는다.
radius는 admission/target halo/geometry에 영향을 주지만, 이번 corpus의 원본 raster/atlas 크기는 같았다.
즉 큰 radius가 CPU crop bytes를 같은 비율로 늘린다는 근거는 없다.
prepare 코드: text-reveal-blur-preparation.cpp (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp:355`).

Gaussian `NUM_SAMPLES=radius>>1`은 8→12이고 각 sample에 ±두 reads가 있으므로,
실제 filtering branch의 fragment당 texture 표현식 수는16→24다.
이는 **+50%의 shader sampling 명령 수**이지 GPU time +50%라는 뜻은 아니다.
D2는 H의 실제 Y coverage만 그리고 전체 FBO는 clear하므로, FBO 면적을 그대로 H filtering fragment 수로 세면 안 된다.
PERFORMANCE에서도 kernel tap 수 자체는 HIGH와 같다.

결론적으로 HIGH Soft vs PERFORMANCE Strong은 quality 차이뿐 아니라
**Source/Output halo와 kernel 길이 차이**가 섞여 있다.
그럼에도 총 FBO bytes는 PERFORMANCE Strong이 작으므로 “더 많이 할당해서 더 느리다”로 결론낼 수 없다.

## E. PERFORMANCE-only CPU work

### 정확한 timing 식과 개수

baseline runtime의 `BlurStrength::Evaluate`:

```text
p = ResolveRenderProgress(progress)
q = p >= 1 ? 1 : clamp((p - sequenceStart) / blurDuration, 0, 1)
strength = 1 - q*q*(3 - 2*q)
```

여기 strength는 **cubic smoothstep의 역**이다.
Output의 Late Smooth에서 사용하는 **quintic smootherstep과 다른 식**이다.

N=전체 line 수, D=H 또는 V의 draw batch 수, O=Output draw 수라면:

```text
HIGH:        2N strength + 2D progress copies
PERFORMANCE: 3N strength + 2D progress copies
```

이번 N20/D15/O15에서는 **70→90 blur-timing constraints**, 그중 strength functor40→60이다.
다른 constraint296개는 동일하다. 따라서 subtree 전체에서는366→386, 약5.5%의 개수 증가다.
constraint마다 비용이 다르므로 CPU time +5.5%로 해석하지 않는다.

PERFORMANCE는 추가20개의 float property, Constraint/source 연결 및 event→update 생성 메시지가 필요하다.
update에서는 input progress read, clamp/division/cubic 계산, old/current 비교,
변경 시 target updated 표시 및 uniform 전달이 추가된다.
handoff geometry는 full Output vertex보다 line-index float가 하나 많다.
전체20 line×4 vertices 기준 raw attribute payload는320B 증가하므로, 그 자체를 주요 메모리 원인으로 보지 않는다.

### Core update까지 확인한 실행 경로

```text
Animation update
 → Node/layout/opacity constraints
 → active renderer constraints
 → ConstraintContainer::Apply
 → SceneGraph::Constraint::Apply
 → BlurStrength::Evaluate
 → old/current 비교, 필요하면 bake/updated
```

근거:

- constraint-base.cpp (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-core/dali/internal/event/animation/constraint-base.cpp:80`): 기본 apply rate는 APPLY_ALWAYS.
- update-manager.cpp (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-core/dali/internal/update/manager/update-manager.cpp:1124`): active renderer 순회에서 constraints 실행.
- scene-graph-constraint-container.cpp (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-core/dali/internal/update/animation/scene-graph-constraint-container.cpp:91`): active constraints Apply.
- scene-graph-constraint.h (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-core/dali/internal/update/animation/scene-graph-constraint.h:79`): functor를 호출한 **후** old/current를 비교.

native inventory도 timing constraint의 rate=1/source=1을 확인했다.
따라서 연결되어 있고 input이 초기화된 **활성 renderer에서는 매 update 계산**한다.
progress가 같은 값이어도 먼저 functor가 실행된다. 생성 때만 평가하거나 input변화 때만 평가하는 코드가 아니다.
물론 renderer/node가 deactivate되거나 scene이 update를 멈추면 이 설명을 무조건 적용할 수 없다.
20이라는 수는 live evaluator 수이며, 매 vblank에 정확히20회 실행된다는 실측 횟수는 아니다.

layout transition과 Reveal animation이 함께 진행될 때 동일 update-thread budget을 추가로 사용한다는 것은 확인된다.
**이 20개 때문에 타겟에서 frame drop이 발생했다고는 아직 입증하지 않았다.**
render-side의 추가 uniform/texture binding/dependency 처리도 있어 전체 차이가 이 functor뿐인 것은 아니다.

### Output timing을 vertex로 옮기는 경우

가능하다. static sequenceStart/blurDuration와 shared progress로 동일 cubic을 vertex에서 계산하고,
기존 strength varying을 fragment에 전달할 수 있다. 하지만 progress는 renderer에 공급해야 한다.
부모 Label의 custom uniform은 자식 Output Actor에 자동 상속되지 않는다.
Core의 BuildUniformIndexMap (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-core/dali/internal/render/renderers/render-renderer.cpp:767`)은
해당 draw의 Node map과 renderer/shader map을 결합한다. 임의의 ancestor custom property를 읽지 않는다.

**새 공유 UBO/Actor 재설계 없이 작은 변경을 한다면:**

```text
기존:  Output strength constraints N개
후보:  Output progress mirror O개 + static timing
순감소: N - O
```

이번은20→15로 **5개 순감소**다. 전체386→381, timing90→85가 된다.
20개의 cubic functor를 없애고15개의 단순 float copy로 바꾼다.
1line/1page Label은 constraint 개수 감소가 없고 functor가 가벼워지는 효과만 있다.
3line/1page subtitle은3→1로2개 줄어든다.

quad당4 vertex에서 clamp/division/cubic/endpoint 처리가 추가된다. 이번20줄이면 Output 기준80 vertex invocation에 해당하는
작은 산술이지만 GPU timing 이득/손해는 측정하지 않았다.
Late Smooth quintic/mix는 fragment에 그대로 두면 현재 품질과 비교하기 쉽다.
그 quintic까지 vertex로 옮기는 추가 변경은 이번 추천 PoC에 포함하지 않는다.
static timing 데이터가 추가되므로 uniform 저장량까지 자동으로 감소하는 것은 아니다.
driver가 uniform block 전체를 올리는 경우도 있어, 동적 property 감소를 그대로 UBO upload bytes 감소로 간주하지 않는다.
후보 Output shader의 최초 compile 비용도 기존과 분리해서 warm 상태로 비교해야 한다.

보존해야 하는 correctness 조건:

- `ResolveRenderProgress`: `!(p>0)`은 NaN/음수도0으로 만든다. `p>=1-float_epsilon`은1로 보정한다.
  단순 GLSL clamp만으로 대체하지 않는다.
- p=1일 때 q=1 강제, reverse/seek에서 state 없는 동일 함수 평가.
- sequenceStart/duration은 animation seconds를 추정하지 않고 validated publication의 값을 사용한다.
- H/V의 `start - 1/65535` 투명도 gate는 그대로 둔다. Output timing 최적화가 별도 fade gate를 만들지 않는다.
- static duration은 현재 runtime validation을 유지하고, highp/CPU-GPU rounding 및 threshold 주변을 비교해야 한다.
- async 새 publication과 static timing/progress binding의 owner 수명은 기존 companion 단위로 함께 바뀌어야 한다.
- MAX_LINES_PER_DRAW=64, mixed-format output split의 index는 Output segment 시작이 아니라 **원래 H/V draw의 local index**다.
- shared cached shader에 Label별 mutable progress를 직접 기록하면 다른 Label과 섞일 수 있다. renderer-local 전달을 유지한다.

H/V까지 같은 방식으로 확장하면 산술상 더 줄어들 여지는 있지만,
Gaussian strength 계산의 CPU→GPU 이동까지 검증 범위가 넓어진다. **이번 다음 PoC는 Output만**으로 제한한다.

## F. PERFORMANCE-only GPU work

### Output 차이

baseline CreateBlurOutput (로컬 자료: `dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`):

| 항목 | HIGH | PERFORMANCE |
|---|---|---|
| texture expressions | V 1회 | V + full Source 각1회 |
| sampler/texture slots | 1 | 2（기존 sampler handle 재사용） |
| strength uniform/varying | 없음 | line별 float array / line-index / strength varying |
| effective radius | 없음 | authoredRadius × strength |
| handoff | 없음 | 아래 quintic + premultiplied mix |
| A8 색 복원 | 동일 | 동일 |
| owner opacity/color | 동일, Output에서 적용 | 동일, Output에서 적용 |

```text
t = (authoredRadius*strength - 2) / (8 - 2)
u = clamp(t, 0, 1)
blend = u³ * (u * (6u - 15) + 10)
color = t <= 0 ? sharpSource : mix(sharpSource, V, blend)
```

source code에는 양쪽 texture read가 handoff 판단 전에 있다.
driver가 endpoint에서 어떤 expression을 실제 최적화하는지는 별도 shader/하드웨어 관측 없이 보장하지 않는다.
또 “texture expression 1회”는 bilinear texel4개가 항상 DRAM에서4번 읽힌다는 뜻이 아니다.

### dependency / lifetime

```text
HIGH:        Source → H → V → Output(V)
PERFORMANCE: Source → H → V → Output(V + Source)
                └───────────────────────↑
```

**1페이지에서는 Source texture object lifetime이 더 길어지는 것이 아니다.**
HIGH도 pass.buffers에 Source를 보유하며 companion 해제 전까지 살아 있다.
달라지는 것은 frame 내부의 마지막 read 위치와 fan-out이다.
PERFORMANCE는 Source가 최종 화면 draw까지 GPU read dependency에 참여한다.
equal-size multipage에서 Source scratch 재사용이 제한되는 경우와 이 1page 문제를 구분해야 한다.

현재 GLES adaptor도 추가 바인딩 texture에 대해 dependency를 추적한다.
CheckNeedsSync/MarkFramebufferTextureRead (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-texture-dependency-checker.cpp:219`)는
다른 context의 FBO texture를 읽을 때 forward/backward 관계를 관리한다.
Source가 surface Output에서도 사용되면 V-only보다 tracking 대상이 하나 늘 수 있다.
하지만 sync 객체는 공유/중복 제거되며, “line마다 fence 하나 추가”, “CPU blocking wait가 반드시 추가”라고 단정할 수 없다.

backend-neutral하게 확정되는 것은 **추가 full-resolution 샘플링, texture slot, dependency와 fragment 산술**이다.
tile GPU의 store/reload/cache residency, memory bandwidth 경쟁, driver submission/동기화 비용의 실제 증가는 vendor/context에 달려 있다.
HIGH도 H에서 Source를 읽으므로 Source store가 PERFORMANCE에만 처음 생긴다고 표현해서도 안 된다.

이 final Source read는 같은 radius의 1page에서 **가장 눈에 띄는 추가 GPU 구성 차이**다.
그러나 H/V가 줄어든 효과를 상쇄하는 최대 병목인지, 실제 bytes/frame이나 ms가 얼마인지는 알 수 없다.
작은 Card에서도 무조건 PERFORMANCE가 빠르거나 느리다고 이 구조만으로 정할 수 없다.

### reduced H/V의 backend realization

양쪽 모두 같은 Texture/FrameBuffer 생성 API, format, single-sample attachment, Source→H→V 순서,
REFRESH_ALWAYS, D2 geometry/clear 정책을 사용한다.
PERFORMANCE는 H width와 V width/height, input inverse-size/clamp만 바꾼다. 별도 resize task는 없다.
현재 GLES의 `Texture::InitializeResource`/`Framebuffer::InitializeResource`에서도
quality별 특수 backend 분기는 없고 받은 dimension/format으로 texture storage와 attachment를 만든다.
작아진 FBO가 cache/tile 효율이나 driver allocation latency를 어떻게 바꾸는지는 이 소스 확인으로 정량화하지 않았다.

### Source final read 없이 Late Smooth를 유지할 수 있는가?

| 방향 | WHOLE_TEXT | PER_LINE |
|---|---|---|
| A. 현재 captured Source + V | 현재 정확한 두 입력 | line별 reveal·halo·premultiplied 합성 보존 |
| B. ordinary/line-atlas sharp handoff | 단색·단순 조건은 후보가 될 수 있지만 원래 Reveal foreground를 다시 평가/합성해야 함 | line별 coverage/UV/색/gradient/mask/Reveal metadata 및 overlap 순서를 맞춰야 함 |
| C. threshold 후 blur Output 숨기고 ordinary authority | 한 sequence라 gate는 비교적 단순하지만 현재 smooth mix와 달라짐 | line별 strength가 달라 whole renderer toggle 불가; 일부만 ordinary로 복원할 새 ownership/masking 필요 |

원래 atlas가 current captured Source와 동일한 결과를 담는 것은 아니다.
Source에는 현재 progress의 Reveal 적용, 색/gradient, line isolation, ImageSpan 등이 이미 반영된다.
ordinary foreground로 갈아타는 것은 동등한 sharp read 주소를 바꾸는 한 줄 수정이 아니다.
crossfade를 유지하면 두 draw/authority를 조율해야 하고, hard threshold면 pop/밝기/overlap 위험이 있다.
따라서 **현재 기능·품질을 보존하면서 captured Source final read를 없애는 작은 공통 후보는 확인하지 못했다.**

## G. First-use shader/setup

### factory/cache 감사

| stage | cache/key | first-use 성격 |
|---|---|---|
| A8 Source | runtime의 thread_local alpha shader, atlas/non-atlas | quality-independent |
| RGBA/gradient Source | TextVisualShaderFactory의 feature 조합/atlas cache | quality-independent, text style에 따라 variant |
| H/V | TextRevealBlurRenderer thread_local `[batch][radius>>1]` | **HIGH/PERF 공통 shader**, radius 바뀌면 다른 kernel variant |
| HIGH Output | whole-alpha / batch A8·RGBA | quality-specific full Output |
| PERF Output | quarterShaders[batch][alphaOnly] | Late Smooth Output, radius는 uniform이므로 radius별 Output compile 없음 |

Gaussian factory에는 FILE_CACHE_SUPPORT가 있고 Output은 Hint::NONE이다.
따라서 새 process에서 compile 필요성이 모두 같지는 않다. Shader handle cache와 driver program/pipeline cache도 구분해야 한다.
Intro/Skeleton에 이미 A8/gradient와 PER_LINE이 있어 Card가 같은 variant를 재사용할 가능성이 높으며 native에서도 확인했다.

| 실제 구간 | HIGH Soft | PERF Soft | HIGH Strong | PERF Strong | OFF |
|---|---|---|---|---|---|
| 첫 Card 진입 early snapshot: Shader object / glCompileShader / glLinkProgram | 0/0/0 | 0/0/0 | 0/0/0 | 0/0/0 |
| warm Card 재생성: 같은 세 counters | 0/0/0 | 0/0/0 | 0/0/0 | 0/0/0 | 0/0/0 |

이 구간 glProgramBinary도0이다. 앞선 Intro/Generating 구간에는 실제 nonzero counters가 있어 hook이 동작함을 확인했다.
Card 이후 affordance/cleanup까지 포함한 긴 phase에는 공통으로 Shader object1/program binary1이 추가됐지만,
이를 초기 Card compile로 계산하지 않았다.

ObjectRegistry의 Shader 생성 수는 새 Shader 객체 수이며 factory getter 호출 수가 아니다.
disk cache는 삭제하지 않았다. **process의 첫 Card vs warm**이지, 모든 disk/driver cache가 cold인 측정은 아니다.
GPU pipeline realization/state validation까지 zero라는 의미도 아니다.
그럼에도 이 host 경로에서는 “PERFORMANCE만 Card 진입 때 shader compile”이라는 가설을 뒷받침하지 않는다.
target 첫 진입/다른 radius 선택의 shader stall까지 배제할 수는 없다.

이번에는 CPU micro-attribution/settled-layout negative-control을 추가 실행하지 않았다.
구조적으로 존재하는 update 추가 작업과 Output read가 확인됐고,
그보다 넓은 profiling 없이도 다음의 작은 isolation PoC를 정의할 수 있기 때문이다.
이 선택 때문에 실제 setup CPU와 GPU 병목의 상대 순위는 **미확정**으로 남긴다.

## H. Candidate comparison

| Candidate | Setup CPU | Per-frame CPU | GPU bandwidth | Task/FBO | Quality risk | Lifecycle risk | Complexity |
|---|---|---|---|---|---|---|---|
| A. Output timing→vertex | constraint 연결 N→O; static timing 필요 | N cubic→O float copy; 이번 순−5개 | 동일 | 동일 | 낮음, rounding/endpoint 검증 필요 | 낮음, renderer-local owner binding 유지 | 작음 |
| B. captured Source 제거/ordinary handoff | 새 authority/routing 비용 가능 | 반드시 줄어들지 않음 | Source read 감소 가능, ordinary draw 추가 가능 | 설계에 따라 다름 | 높음: line overlap/색/mask/threshold | 높음: None/async/교체/reverse | 큼 |
| C. shader prewarm | 비용을 앞 구간으로 이동 | 정상 재생 unchanged | 동일 | 동일 | 낮음 | cache/scene 준비 수명 고려 | 중간, 이번 Card compile0 |
| D. radius/setup attribution만 추가 확인 | 변경 없음, 비용 위치만 측정 | 변경 없음 | 변경 없음 | 동일 | 없음 | 없음 | diagnostic만 필요 |

H/V까지 vertex timing으로 확장하는 큰 consolidation은 이번 추천에 포함하지 않는다.
현재 후보A의 이득이 작을 수 있다는 점을 그대로 드러내고, quality를 흔드는 handoff 변경을 먼저 하지 않는다.

## I. Recommended next PoC — 딱 하나

**PERFORMANCE PER_LINE batched Output의 strength만 static timing + renderer-local progress로 vertex에서 계산한다.**

변경 경계:

1. Output line strength constraint를 제거하고 static sequenceStart/blurDuration를 전달한다.
2. Output renderer당 progress mirror1개로 현재 owner progress를 받는다.
3. vertex에서 기존 CPU cubic/endpoint 처리와 동등한 strength를 계산해 기존 varying으로 전달한다.
4. fragment Late Smooth/Source+V read/H/V/타이밍 스케줄/geometry 배치/Task/FBO/format/refresh는 바꾸지 않는다.
5. HIGH와 WHOLE_TEXT 경로는 그대로 둔다. shared shader에 Label별 mutable 값을 넣지 않는다.

예상되는 것은 **setup/update의 작은 직접 절감**이지, 큰 GPU 개선이 아니다.
이번 workload에서 추가20 evaluator를 제거하지만 진행값 copy15개가 생기므로 순감소5개,
추가 Source read는 그대로다. 따라서 이 후보만으로 transition 문제가 해결될 것이라고 약속하지 않는다.

다음 단계의 판정은 same-radius HIGH/PERF와 PERF PoC를 동일 sample에서 비교해야 한다.
품질/timing의 reverse·seek·endpoint·async 및64-line/split index가 먼저 같아야 한다.
target transition이 그대로라면 “20개의 Output cubic이 주요 원인”이라는 가설의 우선순위를 낮추고,
PoC를 억지로 확대하지 않는다. **이번 작업에서는 PoC를 구현하지 않았다.**

## J. Explicitly deprioritized / 답변 요약

page budget/planner, prefilter/A-R, adaptive, reduced taps, half-rate, endpoint suspension,
global pool은 분석/변경 대상으로 확대하지 않았다. full regression/sanitizer/GPU benchmark도 수행하지 않았다.

| 질문 | 답 |
|---|---|
| Q1 대부분 1page? | 예, host 12개 중 9개. 3개만 2page. |
| Q2 same radius 객체 수 동일? | 예, 이번 native 구성의 Actor/Renderer/Camera/Task/FBO 모두 동일. |
| Q3 추가 per-frame CPU? | 활성 Output strength functor/property/uniform 처리, 추가 texture binding/dependency 처리. |
| Q4 추가 constraints? | 초기 Cards 20개. 외부 5 Label은 이 숫자에 미포함. |
| Q5 매 update 실행? | 활성 renderer/초기화된 inputs/APPLY_ALWAYS에서 예. frame당 횟수를 별도 실측한 것은 아님. |
| Q6 radius만의 resource 차이? | Soft→Strong FBO 약 +36%, Source +121,472B. 객체 수는 동일. |
| Q7 Source read가 주요 구조 차이? | 추가 GPU 작업 중 가장 명확한 차이. 실제 최대 병목인지는 미확정. |
| Q8 vertex 이동 시 몇 개 제거? | 20 strength 제거−15 progress 추가=**순 5개**, cubic 20회는 copy 15회로. |
| Q9 Late Smooth 유지+Source 제거 작은 후보? | 전체 기능을 보존하는 작은 공통 후보는 없음. |
| Q10 다음 PoC? | Output-only timing consolidation 1개. |

## K. Git state / 결과 자료

- UI HEAD/branch 유지, production source unchanged, UI worktree clean.
- core HEAD `f43e95be477ad301f84ecc772c753f9357821ecc`, adaptor HEAD `dcadcfdc3e0d1abdce20767bee2858cb9e1851a4` 유지, 모두 clean.
- commit/amend/push/reset/restore/stash/rebase 없음. 기존 보고서/진단 폴더도 변경하지 않았다.
- 신규 파일은 이 외부 diagnostic directory에만 작성했다.
- 원자료: HIGH Soft (로컬 자료: `high-soft.log`), PERF Soft (로컬 자료: `perf-soft.log`), HIGH Strong (로컬 자료: `high-strong.log`), PERF Strong (로컬 자료: `perf-strong.log`), OFF (로컬 자료: `off.log`).
- 정리된 상세 inventory: inventory.json (로컬 자료: `inventory.json`), 집계 코드: summarize.py (로컬 자료: `summarize.py`).
- frozen baseline runtime/sample은 이 폴더의 `dali-ui-foundation/`, `samples/`에 있다.

**결론:** 타겟 체감 차이의 원인을 확정한 보고서가 아니라,
동일 topology에서 남는 두 축인 **Output update 계산 / Source+V 최종 합성**으로 범위를 좁힌 보고서다.
작은 CPU isolation부터 확인하고, 효과가 없으면 큰 구조 변경의 근거로 확대 해석하지 않는 것이 적절하다.
