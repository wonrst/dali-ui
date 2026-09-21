# Text Effect Demo: BlurEffect vs production Reveal PERFORMANCE pipeline inventory

## A. Executive verdict

**LOWER PIPELINE MULTIPLICITY ALONE DOES NOT EXPLAIN THE BLUREFFECT ADVANTAGE.**

아래 숫자는 실행시간 추정이 아니라 실제 앱에서 수집한 DALi object inventory다.

BlurEffect는 Card 단위 3개가 아니라 **Label마다 붙는다**.

| 시점 / 범위 | BlurEffect offscreen tasks | Reveal PERFORMANCE offscreen tasks | Reveal / Effect |
|---|---:|---:|---:|
| 등장, Cards의 12 Label | **36** | **45** | 1.25× |
| 등장, 화면 전체 | **51** | **60** | 1.176× |
| 정상 퇴장, Cards의 12 Label | **36** | **36** | 1.00× |
| 정상 퇴장, 화면 전체 | **57** | **57** | 1.00× |

각 행에서 unique color FBO/attachment texture 수도 task 수와 같았다.
window/default task **1개는 위 표에서 제외**했다.

따라서 “BlurEffect 9 tasks vs Reveal 45 tasks” 가설은 이 앱에서 틀리다.
등장에서는 Reveal에 page 3개(9 tasks)가 더 있지만, 정상 퇴장은 같은 수다.
실제 차이는 task 수 외에도 Source 영역/halo, A8·RGBA, camera/constraint,
Output 및 animation 방식에 있다. 어느 항목이 타겟 시간의 얼마인지는 이번에 측정하지 않았다.

**pipeline-count sweep은 practical budget을 찾는 용도로는 가치가 있다.** 다만 요청의
조건부 구현 기준인 “BlurEffect가 훨씬 적은 pipeline을 쓴다”는 결과가 아니므로,
이번에는 정확한 [sample-only patch plan](PIPELINE_SWEEP_PLAN.md)까지 준비하고
sweep variant 코드는 추가하지 않았다. production/source-only/sample 모두 변경하지 않았다.

## B. Baseline / Git / 측정 방법

- repository: `/home/bowonryuubuntu/tizen/dali/dali-ui`
- branch: `devel_blur_text`
- 현재 HEAD: `05087317cac8ea9600bba498f00ccf8086a79d3f` (`perf test 3`, Source-only)
- 시작 worktree: **clean**. Source-only는 unstaged가 아니라 이미 HEAD에 commit되어 있었다.
- 진단 전 production: **`b54bb666dbc197c0d290fc9cffeba37bca042274`** (`Batch text reveal blur sources`).
- 이후 `511fef03` V-only → `035e79f3` H/V 1-tap → `05087317` Source-only.
- production~현재의 tracked 차이는 runtime cpp와 renderer h의 namespace comment뿐이다.
  sample/BlurEffect cpp/renderer cpp/shader는 이 진단 history에서 바뀌지 않았다.

BASELINE.txt (로컬 자료: `BASELINE.txt`)에 원본 상태/로그를 보존했다.
production-runtime.cpp (로컬 자료: `production-runtime.cpp`)는 `git show b54bb666:<runtime path>`에서
추출했다. build-private.py (로컬 자료: `build-private.py`)는 이를 byte 비교한 후 private runtime.o로
컴파일하고, 현재 CMake objects1.rsp의 다른 foundation object와 relink한다.
현재 source-only runtime.o는 포함하지 않는다. 기존 installed/build library는 덮어쓰지 않았다.
이 mixed-object private build는 **native inventory용이지 성능 비교 build가 아니다**.

probe.cpp (로컬 자료: `probe.cpp`)는 현재 sample을 변경 없이 include하고, 외부에서만 private state를
읽어 실제 sample 함수를 호출한다. `Constraint::Apply` pass-through로 weak handle을 기록한다.
effect의 내부 객체는 read-only로 접근한다. rendering 값/constraint 인수는 변경하지 않는다.

환경/fixture:

- Ubuntu, DISPLAY `:1`, window **1280×720**, MSAA4, SYNC, Strong.
- entrance radius24 / exit radius48; sample의 원래 duration/alpha/delay 유지.
- GENERATING(skeleton) → 실제 transition → REVEAL_RESULTS(cards).
- 등장 시작 약 .20/.55초 후 inventory; 완료 후 정상 DETAIL_STREAMING 퇴장을 요청,
  퇴장 시작 약 .15/.35초 후 inventory. 시간은 checkpoint 선택용이며 latency 측정이 아니다.
- 양쪽 mode에서 각 Card Label의 **layout size와 line count가 동일**함을 대조했다.
- 각 mode의 완주한 run 1회. 초기 probe는 완료된 `Reveal::None()`에서 GetBlurRadius를
  호출하는 진단 코드 실수로 abort했다. None guard를 넣고 양쪽을 다시 완주했다.
  첫 실패 로그 `effect.log`, `performance.log`는 보존하되 최종 통계에는 쓰지 않았다.
- 최종 로그: effect-complete.log (로컬 자료: `effect-complete.log`), performance-complete.log (로컬 자료: `performance-complete.log`).
- 양쪽 모두 마지막 Shutdown 이후 **window task 1개만 남았다**.

## C. Sample의 실제 BlurEffect 설정 / instance 수

주요 source:

- AnimateBlurEffect (로컬 자료: `../dali/dali-ui/samples/text/text-effect-demo.cpp:1351`):
  `GaussianBlurEffect::New(radius)` → **`label.SetRenderEffect(effect)`** →
  `AddBlurStrengthAnimation` → Label/effect handle 보관.
- AnimateTextEntrance (로컬 자료: `../dali/dali-ui/samples/text/text-effect-demo.cpp:1363`):
  원래 Reveal progress 0→1과 같은 animation/time period, EASE_OUT_SQUARE로 strength1→0.
- AnimateTextExit (로컬 자료: `../dali/dali-ui/samples/text/text-effect-demo.cpp:1382`):
  정상 완료 후 퇴장은 새 effect(radius48), progress1→0/strength0→1 LINEAR.
  미완료 entrance의 역재생은 기존 effect/radius를 유지한다.
- StartResultReveal (로컬 자료: `../dali/dali-ui/samples/text/text-effect-demo.cpp:1660`):
  Card 3개의 day/title/places/subtitle **12 Label에 effect를 한 번에 생성**한 후
  같은 animation의 개별 delay로 순서대로 나타낸다.
  delay 전 Label도 resource/task는 이미 있다.
- `StartSceneTextEntrance`가 다른 scene Label에도 effect를 붙인다.

| checkpoint | Cards effect handles | 다른 scene effect handles | 총 effect handles |
|---|---:|---:|---:|
| 등장 | 12 | 5 | **17** |
| Cards 완료 직후 | 0 | 2 | **2** |
| 정상 퇴장 | 12 | 7 | **19** |

완료 직후의 2개는 후속 badge/action 애니메이션이다. Cards의 12개 effect는 해제된다.
퇴장 전에 `RefreshSceneLabels`가 후속 Label까지 포함하므로 19개가 된다.
completed checkpoint의 기존 `mSceneLabels` vector는 아직 17개라 부분 scope로 전체를
계산하면 누락된다. 이 시점의 전체 수는 **WINDOW inventory**를 사용했다.
등장/퇴장의 primary 비교에서는 scene Label vector와 실제 대상이 일치한다.

BlurEffect mode도 ordinary Reveal은 실행한다. 다만 Reveal 자체의 BlurRadius는0이며,
별도의 BlurEffect가 **Label 전체**를 blur한다. PER_LINE sequence별 blur와 같은 의미가 아니다.

## D. GaussianBlurEffect runtime topology

Source:
GaussianBlurEffectImpl (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/render-effects/gaussian-blur-effect-impl.cpp:287`),
RenderEffectImpl::UpdateTargetSize (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/render-effects/render-effect-impl.cpp:329`).

sample의 **strength animation 중** effect 1개:

| 항목 | 실제 구성 |
|---|---|
| Source | owner Label capture 1 task/FBO |
| H | 1 task/FBO |
| V | 1 task/FBO |
| 별도 downsample/upscale task | **0** |
| color textures | 3, 모두 RGBA8888 |
| 크기 | S/H/V 모두 Label target의 **full resolution** |
| cameras | **2**: Source 1개, H/V가 공유하는 camera 1개 |
| 내부 일반 actors | root/downsample/H/V =4개; downsample actor는 animation 중 비활성 |
| effect가 보유한 renderers | cache Output + H + V + 사용하지 않는 downsample =4개 |
| 기존 Label renderer 포함 | 이번 fixture에서5개/Label |
| refresh | S/H/V 모두 REFRESH_ALWAYS(1) |
| clear | 모두 enabled / transparent |

`AddBlurStrengthAnimation`은 `mInternalDownscaleFactor=1.0f`를 적용한다.
실제 inventory에서도 모든 animation effect가 internalScale1, strengthAnimation=true,
downsampleTask=false였다.

**static 기본 .25 경로와 다르다.** static은 Source .5 + downsample .25 + H .25 + V .25로
4 tasks가 될 수 있다. 이 static 수/크기를 sample animation 비용으로 쓰면 안 된다.

크기는 owner Label SIZE(없으면 natural size)에서 결정하며 정수 변환한다.
Reveal의 radius halo 확장과 같은 영역이 아니다. 예를 들어 카드 day는339×30,
title은339×54, places339×64, subtitle339×82였다.
Source FBO의 attachment policy는 AUTO, H/V는 NONE이다. 아래 byte 표는 **color attachment만** 센다.

## E. Production Reveal PERFORMANCE topology

추출한 runtime source (로컬 자료: `production-runtime.cpp:1170`)와 actual inventory가 일치한다.

- 12 Labels → **20 visible lines → 15 pages**.
- 한 page마다 Source/H/V 3 tasks, 각 task에 builtin camera1개.
- Source는 full page, H는 `ceil(W/4)×H`, V는 `ceil(W/4)×ceil(H/4)`.
- H는 D2 horizontal coverage geometry를 사용하며, V/output은 원래 경로.
- Source/H/V 모두 REFRESH_ALWAYS. half-rate/V-only/1tap/Source-only 없음.
- Output은 production **V + full Source의 Late Smooth**.
- 이번 Cards에서는 A8 14 pages + RGBA 1 page(C2 title). 15 pages의 FBO는 모두 distinct.
- 일반적으로 같은 companion의 동일 크기/format page끼리는 H scratch를 공유할 수 있지만,
  이번 fixture의 companion 내부에는 해당 조건이 없어 task45/FBO45였다.
- 원래 text/metadata texture, Source renderer clone/batch, timing/property constraints가 유지된다.

페이지 수(등장):

| Card | day | title | places | subtitle | 합계 |
|---|---:|---:|---:|---:|---:|
| C1 | 1 | 1 | 2 | 1 | 5 |
| C2 | 1 | 1 | 1 | 1 | 4 |
| C3 | 1 | 1 | 2 | 2 | 6 |

정상 퇴장의 `WHOLE_TEXT / Fade1 / Stagger0`에서는 **각 Card Label당1 page**가 되어
12 pages/36 tasks로 바뀐다. 등장45를 퇴장에도 그대로 적용하면 잘못된 계산이다.

## F. Direct comparison — 동일 native fixture

### 등장, Cards 12 Label

수치는 각 Label subtree + reachable effect cache renderer + task camera의 unique objects다.
ordinary Label renderer/property constraints도 포함하며 **순수 blur 추가분이나 draw-call 수가 아니다**.

| Metric | BlurEffect | Reveal PERFORMANCE | Reveal / Effect |
|---|---:|---:|---:|
| owner Labels | 12 | 12 | 1.00× |
| effect / blur companion | 12 | 12 | 1.00× |
| blur units | whole Label12 | line20 | 의미 다름 |
| pages/pipelines | 12 | 15 | 1.25× |
| S / H / V tasks | 12 / 12 / 12 | 15 / 15 / 15 | 1.25× |
| tasks / FBO / attachments | 36 / 36 / 36 | 45 / 45 / 45 | 1.25× |
| cameras | 24 | 45 | 1.875× |
| actors, cameras 포함 | 84 | 156 | 1.857× |
| renderers | 60 | 72 | 1.20× |
| unique geometries | 2 | 54 | 27× |
| applied constraints | 60 | 386 | 6.433× |
| color FBO pixels | 701,730 | 524,774 | 0.748× |
| color FBO bytes | 2,806,920 | 603,494 | 0.215× |
| color FBO MiB | **2.6769** | **0.5755** | |

Effect의60 renderers에는 사용하지 않는 downsample renderer12개가 들어 있다.
Reveal의72에도 hidden ordinary renderer가 포함된다. allocated renderer 수를 GPU draw 수로 읽지 않는다.
geometry54는 unique handles이고 모두 큰 mesh라는 뜻이 아니다. Effect는 cached quad를 공유한다.

constraint도 연산비에 비례하지 않는다. Effect는 H/V strength를 Animation으로 구동하지만,
Reveal은 progress-derived constraints로 구동한다. Effect의 animation animator 개수를
이 constraint 표가 포함하는 것은 아니다.

Cards Reveal 등장 constraints386 중 timing은 H/V state40 + progress mirror30 + Output strength20;
나머지296은 기존/복제 text, transform/color/gradient 등의 constraints다.
Effect60은 rendererOpacity/uTextColorAnimatable/uTextRevealProgress 각12와
effect cornerRadius/cornerSquareness 각12다.

### 정상 퇴장, Cards 12 Label

| Metric | BlurEffect | Reveal PERFORMANCE |
|---|---:|---:|
| pipelines | 12 | 12 |
| S/H/V tasks | 12/12/12 | 12/12/12 |
| FBOs / attachments | 36 / 36 | 36 / 36 |
| cameras | 24 | 36 |
| actors(camera 포함) | 84 | 120 |
| renderers | 60 | 48 |
| unique geometries | 2 | 2 |
| applied constraints | 62 | 109 |
| color FBO pixels | 691,830 | 1,073,269 |
| color FBO bytes | 2,767,320 | 1,340,239 |
| color FBO MiB | **2.6391** | **1.2782** |

### 화면 전체 — Cards 외부 포함

| Metric | Effect 등장 | Reveal 등장 | Effect 퇴장 | Reveal 퇴장 |
|---|---:|---:|---:|---:|
| blur-enabled Labels | 17 | 17 | 19 | 19 |
| pipelines | 17 | 20 | 19 | 19 |
| offscreen tasks / FBOs | 51 | 60 | 57 | 57 |
| window/default task | 1 | 1 | 1 | 1 |
| offscreen cameras | 34 | 60 | 38 | 57 |
| window camera 포함 cameras | 35 | 61 | 39 | 58 |
| all reachable renderers | 107 | 119 | 118 | 101 |
| all applied constraints | 128 | 534 | 141 | 249 |
| color FBO pixels | 1,292,706 | 738,144 | 1,324,530 | 2,090,399 |
| color FBO bytes | 5,170,824 | 816,864 | 5,298,120 | 2,410,442 |
| color FBO MiB | 4.9313 | 0.7790 | 5.0527 | 2.2988 |

“45 vs15”라는 기존 diagnostic 수는 Cards 내부 scope였다. 그 값을 전체 화면 offscreen
수라고 부르면 다른 scene Labels의 부하를 놓친다.

## G. Per-pass pixels / logical clear-write footprint

| 시점 / 범위 / mode | ΣSource pixels | ΣH pixels | ΣV pixels | 합계 |
|---|---:|---:|---:|---:|
| 등장 Cards Effect | 233,910 | 233,910 | 233,910 | 701,730 |
| 등장 Cards Reveal | 399,327 | 100,085 | 25,362 | 524,774 |
| 등장 전체 Effect | 430,902 | 430,902 | 430,902 | 1,292,706 |
| 등장 전체 Reveal | 561,629 | 140,684 | 35,831 | 738,144 |
| 퇴장 Cards Effect | 230,610 | 230,610 | 230,610 | 691,830 |
| 퇴장 Cards Reveal | 817,300 | 204,390 | 51,579 | 1,073,269 |
| 퇴장 전체 Effect | 441,510 | 441,510 | 441,510 | 1,324,530 |
| 퇴장 전체 Reveal | 1,591,761 | 398,091 | 100,547 | 2,090,399 |

Effect는 모두 full-res, Reveal에서는 Source 열만 full-res이며 H/V가 reduced-res다.
모든 관측 task가 clear enabled이므로 이 합계는 refresh 시 **color target clear의 논리 면적**이다.
같은 수를 attachment write 대상 면적으로 볼 수 있지만, 실제 fragment writes와 같지는 않다.
Reveal H의 D2 band, Source의 glyph coverage, viewport/scissor/visibility, driver clear 처리,
tile memory/compression 및 미실행 task 등 때문에 **GPU bandwidth나 실행 pixels/ms가 아니다**.

등장에서는 Effect가 더 큰 누적 target 면적을 쓰면서 task가 조금 적다.
그러나 Source만 보면 Reveal이 더 크다. 퇴장에서는 halo48로 인해 **Reveal의 Source가
Effect의 약3.54배(Cards)**이며, 총 target 면적도 약1.55배다.
이는 “Effect는 큰 것 몇 개, Reveal은 작은 것 수십 개”로만 설명되지 않는다.

MiB는 width×height×pixel-format bytes의 합이다. driver allocation/alignment, depth/stencil,
window MSAA, original text/metadata, CPU buffers, deferred GPU resource lifetime은 제외했다.
**RSS/실제 VRAM/peak GPU allocation 측정이 아니다.**

## H. Creation / reuse / lifecycle

### 관측한 reuse

identity-checks.json (로컬 자료: `identity-checks.json`)의 native handles 비교:

- 등장 .20→.55 checkpoint: 양쪽 모두 Cards/전체의 **task/FBO/color texture/camera identity 동일**.
- 퇴장 .15→.35 checkpoint도 동일.
- 매 animation frame마다 task/FBO를 재생성하는 구조는 아니며, 두 경로 모두 steady phase는 유지한다.
- Cards 완료 후12 Label의 blur task는 양쪽 모두0. 후속 badge/action용6 tasks는 따로 남는다.
- Shutdown 이후 전체 window task1만 남음. 반복 leak/stress 검증을 했다는 의미는 아니다.

### source상 lifecycle

Effect:

- completed entrance에서 `ClearTextReveal`→`ClearBlurEffect`가 effect를 제거한다.
- 정상 exit는 **새 effect instance**를 만든다. 이전 Cards effect를 재사용하는 것이 아니다.
- interrupted entrance는 기존 effect를 유지해 역재생한다.
- activation이 S/H/V texture/FBO/task를 생성하고 deactivation이 제거한다.
- `OnRefresh`는 FBO를 새로 만든다. 기존 downsample topology와 같으면 **task는 유지하고
  새 FBO로 rebind**, topology가 바뀌면 tasks도 다시 만든다.
- `ApplyInternalDownscaleFactor`의 static→animation 전환은 활성 상태이면 deactivate/reactivate한다.
  sample은 attach 후 strength animation을 요청하므로 조건에 따라 준비 중의 static resources도
  잠깐 생성될 수 있다. “Effect는 생성비가 아예 없다”는 주장은 틀리다.
- internal actors/cameras는 동일 effect object가 살아 있으면 보존될 수 있지만,
  별도 effect instance들 사이의 FBO/task pool은 없다.

Reveal:

- publication에서 companion/page/FBO/renderer/constraint를 구성하고 scene connection에서
  page마다3 tasks와 builtin cameras를 연결한다.
- 기존 source 재사용 가능한 경로가 있더라도, 완료 후 None으로 제거된 companion을
  새 exit configuration에 그대로 재활용하는 것은 아니다.
- 이번 정상 exit는17 Labels가 새로 구성되고, 미완료인 후속2 Labels는 기존 경로를 유지했다.
- 동일 companion 내부의 조건부 scratch reuse는 있으나 cross-Label shared pool은 아니다.

### 제한된 create burst counter

ObjectRegistry에서 관측 가능한 actor/camera/renderer 생성만 셌다. incoming scene install 직전
counter를 초기화했으며, entire scene 범위(12 Cards만 아님)다. camera는 actor 열에서 제외했다.

| 구간 | mode | 일반 actor 생성 | camera 생성 | renderer 생성 |
|---|---|---:|---:|---:|
| incoming scene→등장 checkpoint | Effect | 106 | 34 | 141 |
| incoming scene→등장 checkpoint | Reveal | 172 | 60 | 119 |
| 정상 exit 요청→exit checkpoint | Effect | 68 | 34 | 102 |
| 정상 exit 요청→exit checkpoint | Reveal | 102 | 51 | 51 |

Effect는 초기 H/V renderer 후 strength-animation용 renderer 교체가 있어서 cumulative renderer
수가 live renderer보다 많을 수 있다. Reveal은 cameras/actors가 더 많지만 이 데이터로
**전체 setup time이 몇 배다**라고 말할 수 없다.

이 core의 ObjectRegistry callback으로 Texture/FBO/Task/Geometry 생성은 집계되지 않았다.
로그에 항목이 없다는 것은0회가 아니다. **누적 CreateTask/New 횟수를 실제 측정했다고
주장하지 않는다.** live counts, stable identities, source상 allocation/rebind policy를 근거로 삼았다.

## I. Target 관찰과의 해석 — 증거 / 가설 분리

아래는 **사용자가 이미 전달한 TV 결과**이며 이번 host에서 재측정한 값이 아니다.

| target 관찰 | 이번 inventory와 함께 해석할 수 있는 범위 |
|---|---|
| Reveal-only ≈60 FPS | offscreen blur 자체의 비용이 중요하다는 방향과 일치 |
| Source-only 대부분60, 때때로55–57, 동시 exit≈52 | H/V stage 제거가 유효. 단 task뿐 아니라 중간 clear/write와 Output도 같이 줄어든 실험 |
| H/V1tap, Cards45 tasks에서도 크게 개선되지 않음 | Gaussian tap 산술만이 주원인이라는 설명은 약함 |
| V-only 소폭 개선 | Source 추가 read/Late Smooth도 기여할 여지. 그 하나가 전부는 아님 |
| BlurEffect `60 44 38 40 44 55 43 57 60` | Effect도51/57 offscreen tasks를 쓰므로 상당한 부하가 있는 것과 일치 |
| Strong이 Soft보다 무거움 | radius는 tap 수뿐 아니라 Reveal halo/source 영역도 바꾸므로 양쪽을 분리해야 함 |

가능한 차이를 요청한 항목별로 분리하면:

| 가설 | 확인 결과 |
|---|---|
| A fewer blur instances | owner/effect/companion Label 수는 같음. cards12/화면17(등장) |
| B fewer RenderTasks | 등장36vs45로 일부 차이, 정상 퇴장36vs36으로 없음 |
| C fewer FBO objects | 이번 fixture는 task 차이와 동일. 퇴장은 동일 |
| D larger but fewer targets | 등장 누적면적 Effect가 큼. 그러나 Reveal Source는 크고 exit 총면적도 Reveal이 큼 |
| E task/FBO reuse | 둘 다 active phase reuse. Effect만 지속 pool을 쓰는 차이 아님 |
| F fewer per-line constraints/metadata | Effect에 유리한 실제 구조 차이. CPU 시간 기여는 미측정 |
| G simpler Output | Effect는 cache V 출력, PERFORMANCE는 V+Source Late Smooth. 비용 기여 미확정 |
| H target granularity | Effect는 Label rectangle, Reveal은 foreground/line/halo/page. semantics도 다름 |
| I animation/lifecycle | whole-Label strength vs per-sequence normalized blur, delayed Labels 선생성, shared animation 종료 시 해제 등 차이가 있음. 공정한 pixel-identical 비교는 아님 |

task multiplicity가 중요하지 않다는 뜻은 아니다. **Effect도 많은 tasks를 감당한다는 새 근거로
count 하나만을 root cause로 확정할 수 없다는 뜻**이다. 특히 정상 퇴장은 Source 면적과
Output/update-side 차이를 함께 봐야 한다. driver bug나 CPU/GPU 단독 병목도 확정하지 않는다.

## J. Pipeline-count sweep recommendation

**실용 budget 탐색으로는 진행 가치 있음. 원인 하나를 증명하는 실험으로는 부족함.**

이번에는 conditional sweep 구현 대신 [정확한 변경 계획](PIPELINE_SWEEP_PLAN.md)을 제공한다.
public radius0으로 ordinary Reveal을 남기고 offscreen blur만 끌 수 있다.
단 blur on/off는 internal plan의 duration normalization 유무도 바꾼다.
progress/Animation duration/alpha/Fade/Stagger 값을 유지할 수 있지만 **unit별 실제 출현시각까지
완전히 동일하다고 보장하지 않는다**. 이를 억지로 맞추는 production 변경은 하지 않는다.

## K. Diagnostic levels — 실제 per-Label inventory 기반 산출

non-card blur를 모든 level에서 끈 controlled sweep 권장. level명 HIGH는 quality HIGH가 아니며,
모든 level에서 quality는 PERFORMANCE다.

| level | blur Labels | 등장 pages/tasks/FBO | 퇴장 pages/tasks/FBO | 등장 color FBO bytes |
|---|---:|---:|---:|---:|
| LOW: C1 day,title + C2 day | 3 | 3 / 9 / 9 | 3 / 9 / 9 | 46,298 |
| MID: C1 전체 + C2 day | 5 | 6 / 18 / 18 | 5 / 15 / 15 | 196,116 |
| HIGH: C1+C2 전체 | 8 | 9 / 27 / 27 | 8 / 24 / 24 | 429,007 |
| FULL: Cards 전체 | 12 | 15 / 45 / 45 | 12 / 36 / 36 | 603,494 |

이 숫자는 native inventory (로컬 자료: `inventory.json`)에서 더한 **planned values**다.
variant를 build/run 검증한 값은 아니다. 그 점은 sweep-plan.json (로컬 자료: `sweep-plan.json`)에도 표시했다.
원본 화면 전체 FULL은60/57 tasks이므로 controlled FULL45/36과 구분한다.

## L. Target test procedure

- 계획대로 production baseline의 sample-only variants를 만든 뒤 같은 toolchain/options로 빌드한다.
- TV에서 해상도/font/SYNC/Strong/PERFORMANCE를 동일하게 하고 variant별 동일 장면 순서로 실행한다.
- 같은 FPS logger로 Skeleton→Results→Cards와 정상 exit를 각각3회 정도 비교한다.
- raw1초 FPS sequence, 최소값, transition 체감, Card root animation이 처음부터 보이는지,
  반복 일관성을 기록한다. 현재 native count와 TV count가 같은지도 확인한다.
- BlurEffect도 동일 조건 reference. 기존 FPS sequence는 historical user observation으로 남긴다.
- 이번 host에서는 FPS/GPU timer/CPU latency/bandwidth를 측정하지 않았다.

host inventory를 재현하려면(화면을 띄우며 자동으로 두 transition 후 종료):

```bash
bash /home/bowonryuubuntu/tizen/reveal-pipeline-count.a9GqcW/run-inventory.sh effect
bash /home/bowonryuubuntu/tizen/reveal-pipeline-count.a9GqcW/run-inventory.sh performance
```

이 스크립트는 **inventory 전용**이며 target용 sweep 실행기는 아니다.

## M. Decision rule

- 예를 들어 등장18 tasks는 안정적이고27부터 부족하다면 해당 fixture의 practical budget≈18로 본다.
- 27~45도 충분하면 더 작은 변경으로 해결할 여지가 있다.
- 9에서도 부족하거나 면적/quality mix에 강하게 의존하면 task-count만 줄이는 방향을 재검토한다.
- 이후에만 grouping/granularity/pass 수/semantics trade-off를 검토한다.
- 이번 단계에서는 target sweep이 없으므로 **허용 임계값을 아직 알 수 없다**.

## N. Explicitly not concluded / Q1–Q9

1. **Q1:** 등장 Cards12 / 화면17 GaussianBlurEffect, 정상 퇴장 Cards12 / 화면19.
2. **Q2:** 등장36/51, 퇴장36/57 offscreen task/FBO. window task1 별도.
3. **Q3:** Cards 등장 Reveal45는 Effect36의1.25배. 정상 퇴장은1배.
4. **Q4:** 등장 Effect의 총 FBO pixels가 더 큼. 퇴장은 Reveal이 더 큼. G표 참조.
5. **Q5:** “Card3개에 큰 blur3개” 구조가 아님. 양쪽 모두 최소Label 단위.
6. **Q6:** active animation/size 고정 동안 task/FBO identity 유지. 완료·정상 재시작은 새 구성.
7. **Q7:** offscreen 부하 중요성에는 일관적. 단 BlurEffect 우위를 multiplicity만으로 설명하지 못함.
8. **Q8:** practical budget 확인에는 가치 있음. 단일 병목 증명용 실험은 아님.
9. **Q9:** controlled Cards-only 등장9/18/27/45; 같은 선택의 정상 퇴장은9/15/24/36.

미결론: task 하나당Xms, 특정 driver 버그, target CPU/GPU 시간 분해, 보편적인 task limit,
정확한 GPU resident/peak memory, image/font/해상도 전체 일반화.
level간 total pixels/A8·RGBA/constraints/Label 수와 timing normalization도 변한다.

## O. 보호 상태 / 작업 범위

- UI HEAD/branch/Source-only 유지; 종료 worktree clean.
- core/adaptor 기존 상태 유지; 둘 다 이 작업 시작/종료 clean.
- repo의 production/sample/test/CMake 수정0.
- 모든 새 파일은 이 external diagnostic 폴더 안에만 있다.
- commit/amend/push/reset/restore/stash/rebase 없음.
- full build/full UTC/sanitizer/대형 host performance benchmark 없음.
- private foundation relink + external probe build, 제한된 native inventory만 수행.
- Gaussian/atlas/shared-pool/prefilter/page-budget 등 production optimization 없음.

기계 판독 자료: inventory.json (로컬 자료: `inventory.json`), identity-checks.json (로컬 자료: `identity-checks.json`),
sweep-plan.json (로컬 자료: `sweep-plan.json`), parser summarize.py (로컬 자료: `summarize.py`).
