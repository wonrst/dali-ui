# Text::Reveal Blur — target evidence 기반 production 방향 재평가

## A. Executive verdict

**MORE ISOLATION NEEDED BEFORE ARCHITECTURE CHANGE**

단, 여기서 필요한 isolation은 또 다른 렌더링 변형이 아니다.
**2-stage PoC는 지금 하지 않는다. Gaussian tap/작은 shader 최적화도 중단한다.**
다음 하나만 권한다: **동작을 바꾸지 않는 target frame critical-path attribution**.
즉, 늦은 frame이 setup/event, update, render submission/driver, GPU 중 어디에서 늦어지는지
같은 시간축에서 구분하는 제한된 계측이다. 이번에는 수행하거나 구현하지 않았다.

현재 증거로 확정할 수 있는 결론:

1. 이 target/workload에서 **H/V pipeline bundle 전체를 제거하는 효과는 매우 크다**.
2. 같은 bundle을 유지한 채 Gaussian fetch/ALU만 대폭 줄인 효과는 작았다.
3. 그러나 task/FBO/constraint 수 중 하나를 단독 주원인으로 확정할 수 없다.
4. BlurEffect도 task가 많고 frame drop이 크다. **production-perfect reference가 아니다**.
5. 현재 작은 진단 변형의 정보 이득은 줄어들었다. 그렇다고 separable Gaussian 자체가
   잘못된 선택이거나, 현재 PER_LINE contract가 원천적으로 불가능하다는 증거도 없다.

따라서 “지금 구조를 조금씩 줄이면 반드시 상품화된다”도, “새 blur 알고리즘으로 바꾸면
해결된다”도 승인할 근거가 부족하다. 다음 최적화의 **종류를 결정할 정보**가 먼저 필요하다.

## B. Evidence summary / 기준점

### Git 상태

repository: `/home/bowonryuubuntu/tizen/dali/dali-ui`, branch `devel_blur_text`.
시작/종료 worktree clean. 현재 Source-only는 unstaged가 아니라 아래 HEAD에 commit되어 있다.

| 순서 | commit | 의미 |
|---|---|---|
| production | `b54bb666dbc197c0d290fc9cffeba37bca042274` | Batch text reveal blur sources |
| V-only | `511fef039841068eae38337e48b39b6f4d786134` | final Source read/Late Smooth 제외 |
| H/V 1-tap | `035e79f352454bdd2fb2cc5f044ce579ed8af2f6` | V-only 위에서 H/V fragment만 center sample |
| 현재 HEAD | `05087317cac8ea9600bba498f00ccf8086a79d3f` | Source-only; H/V 자원 생성 제거 |

현재 HEAD의 활성 PERFORMANCE를 production PERFORMANCE로 잘못 읽지 않았다.
production 구조 검토에는 이전에 `git show b54bb666`로 추출/대조한
production-runtime.cpp (로컬 자료: `../reveal-pipeline-count.a9GqcW/production-runtime.cpp`)를 사용했다.
이번에는 private build조차 새로 하지 않았다.

### Target 관찰 — 사용자 제공, 이번 측정 아님

| 단계 | 실제로 바꾼 것 | 사용자가 확인한 target 결과 |
|---|---|---|
| Reveal-only | offscreen blur 없음 | 복잡한 전환도 거의60 FPS |
| production PERFORMANCE | S→H→V, Output(V+S), Late Smooth | 큰 frame drop; Strong>Soft 비용; 상품화 기준 미달 |
| V-only | Output의 Source binding/read/Late Smooth만 제외 | 소폭 개선, 여전히 부족 |
| H/V1tap | V-only 상태에서 Gaussian fragment→center1 sample | 거의 비슷하게 느림; Strong/Soft 차이도 일부 남음 |
| Source-only | H/V task/FBO/camera/renderer/constraint 등을 실제로 생성하지 않음 | 대체로60, 때때로55–57, 동시 퇴장 약52 |
| BlurEffect reference | Label마다 일반 GaussianBlurEffect strength animation | Reveal보다 부드럽지만 `60 44 38 40 44 55 43 57 60` 관찰 |

1-tap의 직접 대조군은 **V-only Gaussian**이다. production→V-only→1tap을 구분해야 한다.
이 관찰은 정밀 frame-time A/B 통계나 CPU/GPU attribution이 아니다. FPS를 합산/보간해서
stage당 ms나 예상 FPS를 계산하지 않는다. 60 FPS ceiling도 숨은 headroom을 알려주지 않는다.

### Native inventory — 기존 Ubuntu 데이터 재사용

1280×720 / SYNC / Strong / Cards12 Label20줄. TV의 font/layout이 같은지는 별도 문제다.
actor/constraint 수는 Label subtree 기준이며 blur 순수 증가분이나 GPU draw-call 수가 아니다.

| Cards 등장 | BlurEffect | Production PERF | H/V1tap | Source-only |
|---|---:|---:|---:|---:|
| owner Labels | 12 | 12 | 12 | 12 |
| pipelines/pages | 12 | 15 | 15 | 15 |
| offscreen tasks/FBO | 36/36 | 45/45 | 45/45 | 15/15 |
| cameras | 24 | 45 | 45 | 15 |
| actors(camera 포함) | 84 | 156 | 156 | 96 |
| renderers | 60 | 72 | 72 | 42 |
| unique geometries | 2 | 54 | 54 | 24 |
| applied constraints | 60 | 386 | 386 | **316** |
| color FBO logical bytes | 2,806,920 | 603,494 | 603,494 | 459,231 |

Sources:
[inventory 비교](../reveal-pipeline-count.a9GqcW/REPORT.md),
[1tap](../reveal-onetap-local.xAtm4Y/REPORT.md),
[Source-only](../reveal-sourceonly-local.l6cJM9/REPORT.md),
[V-only](../reveal-vonly-local.WmHrK8/REPORT.md).
이전 보고서의 “target 미측정”은 작성 당시 상태이며, 이번 판정에는 이후 사용자 관찰을 반영했다.

## C. What has been ruled down — 가설 A~I 재분류

분류는 해당 가설이 **현재 target 지연의 주요 설명인가**를 기준으로 한다.
코드상 비용이 존재한다는 것과 그 비용이 병목이라는 것은 다르다.

| 가설 | 분류 | 근거 / 한계 |
|---|---|---|
| A. Gaussian sampling/ALU가 주병목 | **WEAK** | 극단적인1tap에서도 큰 개선 없음. 모든 환경에서 비용0이라는 뜻은 아님 |
| B. H/V offscreen stages의 전체 비용이 큼 | **STRONGLY SUPPORTED** | H/V bundle을 실제 제거한 Source-only가 크게 개선. 내부 항목별 기여는 미분리 |
| C. FBO object count 자체가 주병목 | **UNKNOWN** | allocation 횟수/attachment 면적/clear/write/graph와 함께 바뀜. 독립 실험 아님 |
| D. RenderTask count 자체가 주병목 | **WEAK** | 등장36vs45 차이는 있으나 정상 퇴장36vs36. count 하나로 상대 차이를 설명 못함 |
| E. processed pixels/halo area가 큰 비용 | **PARTIALLY SUPPORTED** | 1tap에서도 Strong/Soft 차이, 퇴장 Source 면적 차이. 그러나 total pixels만으로 등장 결과는 설명 안 됨 |
| F. per-line/page CPU/update machinery가 큰 비용 | **PARTIALLY SUPPORTED** | 실제 반복 실행/복제 구조 존재. 단 Source-only에도316 constraints가 남아 크게 빨라짐; 총수 중심 설명은 약함 |
| G. camera/actor/geometry/render-graph complexity가 큰 비용 | **PARTIALLY SUPPORTED** | bundle 제거 시 객체/의존성이 함께 감소. geometry는 퇴장에서 이미 양쪽2개; 독립 attribution 없음 |
| H. Source capture 자체가 주병목 | **WEAK** | 동일 Source 크기/포맷/metadata/계획을 유지한 Source-only가 대부분60. 잔여 비용이 없다는 뜻은 아님 |
| I. setup/publication peak가 steady보다 더 큰 문제 | **UNKNOWN** | 전환에서 두 비용이 겹침. 1초 FPS와 live inventory는 시간적 원인 분리가 안 됨 |

별도로 **“Reveal이 BlurEffect보다 훨씬 많은 tasks를 쓰기 때문에 느리다”라는 단일 설명은
CONTRADICTED**다. 그렇다고 render-task 처리 비용의 존재가 반박된 것은 아니다.

### 1-tap이 실제로 말하는 것

eligible H/V fragment의 Gaussian multi-read/weight accumulation을 center1 read로 줄였다.
하지만 다음은 그대로였다:

- H/V의 geometry coverage와 target dimensions, clear, attachment write;
- task processing, cameras, renderer traversal, intermediate texture dependency;
- H/V timing/progress 및 Output strength constraints;
- Source capture/metadata, page planner, allocation/lifecycle.

따라서 **Gaussian 연산을 줄여도 남는 bundle이 크다**는 해석은 유효하다.
“H/V passes도 중요하지 않다”는 해석은 정반대다.
원래 shader도 progress0/sequence 시작 전에는 texture를 읽지 않고, strength0이면1tap copy다.
따라서45 tasks가 모두 매frame multi-tap을 수행했다고 가정하면 안 된다.
타겟의 늦은 frame에서 실제 Gaussian-active fragment 비율은 측정하지 않았다.
cache/bandwidth/driver/compiler 때문에 texture 명령 감소율과 실제 시간 감소율은 같지 않다.
기존 diagnostic의 unused-attribute cache-miss 로그도 있었으므로 타겟 관찰을
zero-overhead 정밀 GPU 측정처럼 취급하지 않는다.

### Source-only가 실제로 말하는 것

H/V의 task/FBO/camera/renderer/geometry, clear/write/dependency, timing constraints70개가
함께 사라졌다. Output 입력도 reduced V가 아닌 full Source가 되었다.
따라서 task/FBO/constraints 중 하나를 고르는 실험이 아니다.

중요한 반증 단서:

- Source capture의 halo/bytes는 그대로다.
- per-line Source batching/metadata/clone machinery 대부분도 그대로다.
- constraints는 **386→316**, 약18%만 감소했다. Output strength20개도 유지됐다.
- unique geometry도24개 남았다.

즉 “많은 Source-side constraints 때문에 무조건 느리다” 또는 “Source가 크면 그것만으로
이 정도 stall이 난다”는 설명은 약해진다. 반면 제거된70개와 H/V renderer의 dirty 처리,
task 준비/driver/GPU 비용의 **조합**이 중요할 가능성은 남는다.

## D. What remains plausible — ranked suspects / 실제 source 감사

순위는 **조사 우선순위**이지 측정된 ms 순위가 아니다.

1. **H/V pass 실행에 붙은 전체 graph/submission/intermediate 처리 비용.** 가장 강한 bundle 증거.
2. **전환 때의 생성·publication·update/render 준비가 겹치는 critical path.** steady 비용과 미분리.
3. **radius-dependent 영역, 특히 WHOLE_TEXT 퇴장의 full Label+halo.** tap 이외의 구체적인 크기 차이.
4. **H/V timing 갱신과 camera/renderer update 구조.** 실제 반복 비용, 다만 총수만으로는 설명 불가.
5. **PERFORMANCE Output의 Source 추가 read/Late Smooth/dependency.** V-only의 소폭 개선과 일치.

이 목록에서 GPU와 CPU 중 어느 쪽이1위인지는 아직 정할 수 없다.

### D1. Constraint/update-side

| Cards 등장 constraint 종류 | Production/1tap | Source-only | 실행 내용 |
|---|---:|---:|---|
| H/V line strength/state | 40 | 0 | progress sanitize/clamp, 정규화, cubic smoothstep; batch는 start와 Vector2 구성 |
| H/V progress mirror | 30 | 0 | owner float 복사 |
| Output line strength | 20 | 20 | 같은 scalar strength 식 |
| 기타 Source/Label/transform/color/gradient | 296 | 296 | scalar/vector mirror, 일부 extent/offset/gradient rect 변환 |
| 합계 | 386 | 316 | |

functor 자체는 glyph 수를 순회하거나 raster하는 함수가 아니다. strength는 소수의
산술/분기이고, mirror는 대부분 float/vector copy다. setup의 property 목록 조회/clone 생성과
매 update의 functor 실행은 분리해서 봐야 한다.

core에서 직접 확인:

- ConstraintBase 기본값 (로컬 자료: `../dali/dali-core/dali/internal/event/animation/constraint-base.cpp:72`)은 APPLY_ALWAYS.
- ConstraintContainer::Apply (로컬 자료: `../dali/dali-core/dali/internal/update/animation/scene-graph-constraint-container.cpp:91`)는 active constraint를 순회한다.
- scene-graph-constraint.h (로컬 자료: `../dali/dali-core/dali/internal/update/animation/scene-graph-constraint.h:79`)는
  initialized/connected 상태와 apply rate를 확인한 뒤 **functor 실행 후** old/current를 비교한다.
  입력이 같다는 이유로 functor 호출 자체를 생략하는 구조가 아니다.
- UpdateRenderers (로컬 자료: `../dali/dali-core/dali/internal/update/manager/update-manager.cpp:1124`)에서 활성 renderer의
  constraints/PrepareRender 등이 수행된다. 값 변경은 property-owner updated 상태에 영향을 준다.

따라서 안정된 copy에도 update가 실행되는 동안 함수/입력/비교 비용은 있다.
하지만 앱이 idle이어도 무조건 계속 frame을 생성한다는 뜻은 아니다.
386개가 모두 동일한 비용도 아니며, 값이 바뀌는 timing과 정적인 style copy는 downstream 비용도 다르다.

BlurEffect의60 constraints는 주로 일반 Reveal/color/opacity와 corner bindings다.
H/V strength는 **Animation animator**로 구동하므로 constraint count에 들어가지 않는다.
“386/60=CPU6.43배”는 잘못된 비교다.

**constraints-off가2-stage보다 informative한가?**
CPU 가설에는 더 직접적일 수 있지만, 현재 형태로는 다음 PoC로 권하지 않는다.
모두 freeze하면 progress/strength/transform/출력 내용이 바뀌고 render dirty/명령 제출도 줄어들 수 있다.
개선되어도 functor 산술인지 renderer dirty/driver 준비인지 구분되지 않는다.
특히 이미316개를 남긴 Source-only가 빠르므로 총수만 겨냥한 실험의 정보 가치는 낮아졌다.
같은 uniform 값의 시간 변화를 다른 producer로 재현해야 깨끗한 isolation이 되는데,
그렇게 만들면 더 이상 작은 “constraints만 OFF” 실험이 아니다.

production 감축 가능성은 있다. 예를 들어 base vector/component 중복 mirror 감사,
동일 owner progress의 반복 전달, timing 계산의 renderer-local 정리가 후보가 될 수 있다.
그러나 live color/gradient/transform animation, seek/reverse, async 교체를 유지해야 하며
현재 데이터를 근거로 몇 ms를 절약한다고 예측할 수 없다.

### D2. Camera / geometry / graph

Reveal은 OnSceneConnection (로컬 자료: `../reveal-pipeline-count.a9GqcW/production-runtime.cpp:1689`)에서
각 S/H/V task에 `SetBuiltinCameraActor(ATTACHED_TO_SOURCE_ACTOR, pass.size, …)`를 호출한다.
core 구현 (로컬 자료: `../dali/dali-core/dali/internal/event/render-tasks/render-task-impl.cpp:238`)은 task-local
camera를 생성하고 해당 source actor 아래에 붙인다. 이것이3 cameras/page의 이유다.
필터의 수학적 요구로 H/V마다 반드시 camera가 달라야 하는 것은 아니다.

BlurEffect는 같은 internal root 좌표계의 H/V actor에 대해 projection과 extent가 같아서
H/V camera 하나를 명시적으로 공유한다. Source용 camera를 더해2개/effect다.

Reveal에서도 **동일 page의 H/V camera 공유 가능성은 있다**. quarter H/V의 FBO 해상도가
달라도 현재 logical camera/quads는 모두 full `pass.size`이므로 해상도 차이만으로 불가능하지 않다.
다만 다음을 입증해야 한다:

- source actor의 parent/position/scale 및 projection, invert-Y가 실제로 동등함;
- scalar/packed path와 owner layout transition에서 캡처 좌표가 같음;
- builtin camera를 소유한 task 제거가 다른 task의 camera를 끊지 않음;
- partial construction 취소/disconnect/reconnect/async replace 시 수명과 scene membership 보존.

companion/page가 명시적으로 camera를 소유하는 국소 방식이 cross-Label 공유보다 검증 범위가 작다.
H/V만 공유하면 Cards cameras45→30이라는 **구조적 상한 예시**가 되지만,
tasks45/clear/write/Source/H/V pixels는 그대로다. 실제 성능 효과는 미측정이다.

Geometry의54vs2도 과장하면 안 된다.

- PER_LINE의 H/D2, V, Output, Source batch마다 다른 rect/offset/line-index/UV vertex를 가진다.
- 각 line은 기본4 vertices/6 indices의 quad다.54개의 복잡한 tessellation이라는 뜻이 아니다.
- geometry는 publication 시 만들고 animation 중 매번 새로 만드는 것이 아니다.
- scalar path는 **이미 cached quad**를 쓴다. 정상 퇴장에서는 양쪽 geometry가2개로 같다.
- static index/vertex-format 공유나 동일 immutable geometry 재사용 여지는 있지만,
  “shape가 quad니까 모두 같은 geometry”로 바꾸면 per-line atlas/halo/order/D2 의미를 잃을 수 있다.
- geometry identity가 pipeline-cache 준비에 영향을 줄 가능성과 per-frame vertex 비용은 별개다.

내부156 actors는156 Labels/Views가 아니다. runtime은 core actors와 camera를 쓰며,
각 actor가 text layout/raster를 다시 수행하는 구조가 아니다.
그렇지만 Node transform, camera update, renderer/task traversal 등의 비용은 남는다.

**camera/국소 geometry 정리는 새 필터나 pass fusion보다 correctness 검증을 한정하기 쉽다.**
그러나 지금 병목을 해결할 만큼 이득이 큰지까지 확인된 것은 아니다.

### D3. Source halo / processed area

현재 구조를 정확히 구분해야 한다:

- scalar/WHOLE_TEXT target: `ceil(contentSize) + 2*(kernelRadius+2)`.
- PER_LINE은 실제 foreground/mask의 nonzero coverage를 읽고,
  display transform으로 매핑한 뒤 `radius+4` guard로 넓혀 full target 안에 제한한다.
- CPU raster/metadata crop의2px/ownership guard는 Gaussian FBO halo와 별개다.
- page planner는 padded line rectangle을 쌓으며, 새 combined page에1M-pixel 및
  page area≤occupied padded rectangles×1.25 제한을 둔다.

즉 PER_LINE은 이미 **glyph coverage 기반 cropping**을 한다. page의80% occupancy는
“실제 잉크80%”가 아니라 “halo 포함 line rectangle80%”다.
잉크 대비 낭비율을 이번 자료만으로 수치화할 수는 없다.
halo 자체, glyph 내부/사이 빈 공간, max-width packing slack을 구분해야 한다.

기존 Cards Source 면적:

| 구간 | Effect Source | Reveal Source | Reveal / Effect |
|---|---:|---:|---:|
| 등장 | 233,910 | 399,327 | 1.71× |
| 정상 퇴장 | 230,610 | 817,300 | **3.54×** |

퇴장의 C1 day만 보아도 Label339.333×30에 대해 Effect Source339×30,
Reveal radius48 Source는440×130이었다. 이는 기존 inventory의 값이지 새 측정이 아니다.

**보수적인 부분은 있는가?** 있다. WHOLE_TEXT가 ink union 대신 content rectangle 전체를
pad하는 것은 sparse text/여백 많은 Label에 보수적이다. 반면 radius48의 support까지
그냥 불필요한 padding이라고 할 수는 없다. blur spill을 Label 밖으로 자연스럽게 표시하는 기능이다.

안전성 관점의 우선 구분:

1. **실제로 비어 있는 바깥 영역을 crop하고 필요한 blur support는 그대로 보존:** 검토 가치 있음.
2. **Gaussian tail/guard를 임의로 줄임:** 품질 변화 가능성이 크므로 권하지 않음.
3. **진행 중 strength에 맞춰 FBO를 계속 줄임/늘림:** 새 allocation/UV/camera/lifetime 문제를 만들므로 작은 개선이 아님.

crop도 단순 좌표 빼기만으로 끝나지 않는다. fractional UI/render scale, quarter ceil/sample phase,
premultiplied filtering, ImageSpan의 reserved-but-not-ready coverage, decorations와
live transform 변화까지 반영한 bounds가 필요하다. shader에서 transparent라고 보인다고
해당 texel을 아무 guard 없이 잘라도 되는 것은 아니다.

D2는 H의 Y draw support만 줄인다. full H clear, Source/V attachment 크기는 유지한다.
clear를 줄이려면 shared scratch와 미기록 texel이 downstream에서 절대 읽히지 않는다는
증명이 필요하다. D2가 통과했다는 이유로 같은 bounds를 모든 FBO에 적용할 수 없다.

영역 최적화의 가능성은 특히 정상 퇴장에 남아 있다. 다만 같은 Source를 유지한 Source-only가
빠르므로 **Source crop 하나면 전체 문제 해결**이라고 기대해서도 안 된다.

## E. BlurEffect comparison — 세 가지 질문

### E1. 왜 같은3-stage인데 더 잘 버티는가?

**정확한 시간 원인은 아직 모른다.** 다음 구조 차이는 확인됐다.

| 차이 | 현재 해석 |
|---|---|
| whole Label vs line/page | Reveal은 독립 sequence timing/line isolation/halo packing을 보존. Effect는 전체 Label strength 하나 |
| cameras | Effect H/V 공유. Reveal task-local camera |
| constraints/actors | Reveal의 복제/타이밍/update graph가 더 큼. count를 ms로 환산할 수는 없음 |
| geometry | 등장에서는 큰 개수 차이. 정상 퇴장에서는2vs2이므로 공통 해답 아님 |
| Source halo | 퇴장 Reveal Source3.54배. Effect의 Label rectangle과 capture 범위가 다름 |
| A8/RGBA | Reveal은 대부분 A8라 논리 bytes가 훨씬 작음. 특정 TV의 format별 실행 효율은 미확인 |
| Output | Effect는 V 출력, PERFORMANCE는 V+Source/Late Smooth |
| hierarchy/transform | Effect는 owner Label capture/cache output; Reveal은 foreground/page-local composition. 둘 다 owner transition과 상호작용 |
| creation/reuse | 둘 다 active animation 중 자원 identity 유지. Effect도 새 exit instance를 만들며 무상 reuse가 아님 |
| page fragmentation | 등장15vs12 pipelines의 일부 차이. 정상 퇴장은 양쪽12 |

Effect는 strength animation 중 **full-resolution S/H/V**다. static .25 경로를 이 비교에 넣지 않는다.

### E2. 실제 의미가 큰 차이의 우선순위

정상 퇴장에는 halo/Source 면적과 Output/update graph 차이를 우선 본다.
등장에는 page 수와 per-line object/update/creation 복잡도가 추가된다.
cached geometry나 camera 공유는 국소 후보이지 충분한 해법으로 확인된 것이 아니다.
특히 A8가 논리 메모리를 절약한다는 사실만으로 TV 실행시간도 반드시 작다고 가정하지 않는다.

### E3. BlurEffect도 frame drop이 크다는 의미

그 구조를 복사해도60 FPS가 보장되지 않는다. 여러 Label의 full-res capture/filter를
동시에 수행하는 비용이 이미 크거나, 그 전환의 CPU/driver 비용이 클 수 있다.
이 결과는 **“현재 workload의 blur 비용을 더 명확한 budget으로 다뤄야 한다”**는 근거이지,
모든3-stage Gaussian이 low-end에서 불가능하다는 증명은 아니다.

카드 퇴장은 tasks/FBO36vs36, geometry2vs2인데도 상대 동작 차이가 있으므로
“작은 target 수십 개가 무조건 나쁘다”는 설명은 성립하지 않는다.
전체 화면은 등장 Effect51/Reveal60, 퇴장57/57이다. Cards45를 앱 전체 수로 쓰지 않는다.

| Cards 기준 | Effect | Reveal PERFORMANCE |
|---|---:|---:|
| 등장 S+H+V target pixels | 701,730 | 524,774 |
| 퇴장 S+H+V target pixels | 691,830 | 1,073,269 |
| 퇴장 cameras | 24 | 36 |
| 퇴장 constraints | 62 | 109 |

등장에서는 Effect의 총 target pixels가 더 큰데 상대적으로 잘 버틴다. 퇴장에서는
Reveal이 더 크다. 따라서 total pixels 하나도 양쪽 구간을 설명하는 단일 변수는 아니다.
이 표는 논리 target 면적이지 실제 shaded fragments/GPU bandwidth/VRAM 측정이 아니다.

## F. 2-stage diagnostic value — **DON'T DO NOW**

여기서 stage는 **offscreen pass**다. 화면 Output은 어느 경우에도 별도다.
제안된 `Source→H1tap→Output(H)`는 품질 있는 blur가 아니라 V bundle 제거 진단이다.

### 얻을 수 있는 정보

- 기존1tap과 대조하면 V task/FBO/camera/renderer/timing/dependency 묶음의 marginal 영향.
- Source-only와 대조하면 H1tap bundle을 남긴 잔여 비용.
- V target은 작으므로, 큰 개선이 있다면 raw pixel 면적보다 stage-associated 비용/graph depth가
  중요할 가능성을 더 지지한다. 그래서 완전히 무의미한 선형 보간만은 아니다.

예를 들어 기존 Cards inventory에서는 V가25,362 pixels로 전체524,774의 약4.83%다.
V를 제거하면 tasks45→30이지만 target면적은 약4.83%만 줄어든다.
동시에 final Output은 V 대신4배 안팎 더 큰 H texture를 읽고, V timing/progress constraints도
사라진다. 이 조건 역시 task count 하나의 isolation은 아니다.

### 결과별 production 결정력

| 결과 | 말할 수 있는 것 | 말할 수 없는 것 |
|---|---|---|
| 크게 개선 | V를 포함한 추가 stage bundle이 비용/critical path에 중요 | 실제 Gaussian 품질을 가진2-stage가 빠르거나 구현 완료됐다는 주장 |
| 거의 미개선 | V만 없애서는 부족; H/Source/remaining graph 또는 다른 구간이 중요 | task 비용0, GPU 원인 배제, Source가 단독 원인이라는 주장 |
| 중간 | 이 진단 workload가15/45-task 사례 사이의 어느 성능 구간에 놓임 | 일반적인30-task budget, 선형 stage당 ms |

### 현실적인2-stage blur architecture는 있는가?

가능한 구조 자체는 있다. 그러나 **검증된 저비용 production 후보는 아직 없다**.

- `Source→one-pass 2D blur→Output`: sparse12tap은 기존 Vogel 실험에서 Soft16 한글부터
  복제 획 패턴으로 실패했다. 모든2D 접근의 불가능성 증명은 아니지만, 새 근거 없이 재개할 이유도 없다.
  dense2D filtering은 정확도에 필요한 sampling 비용이 훨씬 커질 수 있다.
- `Source→H→화면에서 V+handoff`: separable Gaussian을 유지하면서 V offscreen target을
  없애는 구조는 개념적으로 가능하다. 그러나 V 계산을 **제거하는 것이 아니라 화면 draw로 이동**한다.
  현재 quarter V의 계산/양자화/확대와 같지 않고, 화면 크기에서 훨씬 많은 V fragments를
  평가할 수 있다. output clipping/overlap/line clamp/Sharp blend까지 검증해야 한다.
- Source 생성과H를 합치는 방식은 text Reveal/gradient/mask/ImageSpan evaluation을 filter의
  여러 sample과 결합해야 하며, Late Smooth용 sharp Source 요구도 남는다. 작은 merge가 아니다.

따라서 H1tap 결과가 좋아도 위 구조의 품질/비용을 승인할 수 없다.
나빠도 현 architecture의 camera/update/area 개선을 포기할 수 없다.
**결과 양쪽에서 다음 production 결정이 충분히 좁혀지지 않으므로 지금 우선순위는 낮다.**

## G. Production candidate ranking

Upside는 가능성/상한이며 성능 예측치가 아니다. 모든 후보는 이번에 구현하지 않았다.
Correctness는 timing/좌표/기존 동작, Quality는 눈에 보이는 filtering 품질을 뜻한다.

| Candidate | Upside | Evidence | Correctness / regression | Quality risk | Lifecycle risk | Complexity | Verdict |
|---|---|---|---|---|---|---|---|
| A.2-stage diagnostic / future architecture | bundle 감소는 클 수 있으나 품질 후보 미확정 | Source-only 개선은 있음; 1tap 2-stage 결과로 실제 blur를 검증 못함 | 실제 fusion은 매우 넓음 | 높음 | 중간~높음 | 진단 작음 / 제품 큼 | **LOW**: 현재 진단 보류 |
| B.constraint/update-side reduction | update-bound이면 의미 있음; 총수 설명은 약함 | ALWAYS 실행,386→316만으로 큰 개선된 반증도 있음 | live property/seek/reverse/async 입력 보존 필요 | 낮음~중간 | 중간 | 중간 | **MEDIUM**: CPU attribution 조건부 |
| C.camera sharing | 일부 객체/transform/setup 감소, GPU passes 그대로 |45vs24, task-local 생성 이유 확인 | 공통좌표/scale/projection 검증 범위 | 조건 충족 시 낮음 | 중간 | 작음~중간 | **MEDIUM**: 상대적으로 국소적 |
| D.geometry reuse/simplification | setup/cache 개선 가능; per-frame 상한 작을 수 있음 | scalar는 이미 cache, exit 2vs2 | UV/D2/order/format key 검증 | 중간 | 국소면 낮음 / global cache면 중간 | 중간 | **LOW**: 현 문제의 공통 해법 아님 |
| E.Source/halo area reduction | sparse WHOLE_TEXT에서는 잠재적으로 큼 | exit Source 3.54배; line은 이미 crop | scale/ImageSpan/mask/bounds 전반 | exact crop 중간 / halo 축소 높음 | 중간 | 중간~큼 | **MEDIUM**: 빈 영역 crop과 tail 삭감 구분 |
| F.page fragmentation reduction | 이 Cards는 최대 15→12 pages; exit 이득 없음 | 12 owners, exit 이미 1page/owner | sample phase/format/line order | 중간 | 낮음~중간 | 중간 | **LOW**: 이 workload 기준 |
| G.Source/V ping-pong FBO3→2 | 저장/생성량 일부; 3 passes는 남음 | PERF는 final Source도 필요, Source/V 크기도 다름 | overwrite/dependency contract 충돌 | 높음 | 높음 | 큼 | **DROP**: 현재 PERF의 작은 최적화로 부적합 |
| H.BlurEffect-style whole-Label approximation | per-line graph 단순화 가능; 60 보장은 없음 | Effect 상대 개선, 하지만 최저38; exit 이미 WHOLE_TEXT | public sequence semantics 변경 | **UX 의미 변경** | 중간 | 큼 | **MEDIUM**: 별도 합의된 design option만 |
| I.현 architecture 유지+micro optimization | 작은 국소 이득 가능 | tap/V-only 개선 작음 | 변경별 상이 | 변경별 상이 | 변경별 상이 | 작음, 누적 복잡도 증가 | **LOW**: 주전략으로 중단 |

G는 stage3→2와 다르다. S와V가 같은 저장소를 쓰려면 S를 덮기 전에 소비가 끝나야 하는데,
PERFORMANCE Output은 V와S를 둘 다 읽는다. Source가 full이고 V가quarter인 크기 차이도 있다.
별도 sharp copy를 보관하면 절감이 사라지거나 작업이 늘어난다. 현재 문제에 맞는 작은 해법이 아니다.

F에서 page를 더 합치면 max-width rectangle의 빈 공간과 clear/write 면적이 커질 수 있다.
“45→36 tasks이므로 이득”을 보장할 수 없다. cross-Label 통합은 이 항목의 국소 튜닝 범위를 넘는다.

## H. Semantic feasibility

**현재 PER_LINE semantics를 유지하는 bounded workload의 상품화 가능성은 열려 있다.**
하지만 현재처럼 여러 Label을 동시에 나타내고 큰 radius로 동시 퇴장시키는 target workload에서
그 semantics를 그대로 유지하면서60 FPS를 달성할 수 있다고 약속할 근거는 없다.

유지해야 하는 비용은 단순한 line count가 아니다:

- 독립 sequence 시작/strength, Reveal unit/fade와의 시간 정합성;
- line-local source isolation, halo와 overlap source-over order;
- quality handoff, color/gradient/ImageSpan/scale, 교체와 reverse의 lifecycle.

이 중 어떤 것이 bottleneck인지 모른 채 하나를 완화하면 UX만 바뀌고 성능은 못 얻을 수 있다.
특히 **현재 정상 퇴장은 이미 WHOLE_TEXT**다. PERFORMANCE를 whole-Label approximation으로
바꾸는 것만으로 퇴장 문제가 해결된다는 주장은 성립하지 않는다.

향후 논의 가능한 trade-off는 다음과 같다. 이번의 추천 다음 작업은 아니며 옵션 목록이다.

- whole-Label blur envelope를 별도 opt-in policy로 제공하되 Reveal의 unit progression과 구분;
- blur 동시 적용 범위/visible sequence 수/큰 radius 영역에 명시적인 budget;
- budget 초과 시 ordinary Reveal 유지, 혹은 합의된 blur fallback;
- exact 모드의 현재 contract는 유지하고 approximate mode를 명시적으로 선택.

어떤 경우에도 기존 `PER_LINE` 요청을 조용히 whole-Label blur로 바꾸거나 timing을 숨겨서
변경하면 안 된다. budget enforcement도 source capture를 매frame rebuild하는 형태이면 역효과가 날 수 있다.

## I. Recommended next action — 딱 하나

**동작 무변경 target critical-path attribution diagnostic을 한 차례 수행한다.**

새 shader/2-stage/constraints-off/camera-sharing PoC는 만들지 않는다.
렌더링 알고리즘을 바꾸는 실험 대신 기존 production과 Source-only의 **동일 전환에서
늦은 frame의 시간이 어디에서 소모되는지** 구분한다. 이것을 다음 PoC 목록의 “다른 후보”로 추천한다.
지금은 계획만 작성했으며 측정/계측 patch는 없다.

### 최소 범위

- same target/build flags/SYNC/Strong/window/font. production `b54bb666` vs Source-only `05087317`.
- warm Skeleton→Results와 완료 후 정상 퇴장, 두 구간만. 첫 shader compile과 steady를 섞지 않는다.
- frame timestamp와 event/publication, update, render-task preparation, render submission,
  wait/present 및 가능하면 GPU timeline을 연결한다.
- 기존 target profiler/trace를 먼저 사용한다. repo에는 `DALI_UPDATE_RENDERERS`,
  `DALI_ANIMATION_ANIMATE`, `DALI_PROCESS_RENDER_TASK`, `DALI_EGL_SWAP_BUFFERS` 등의 trace 지점이 있다.
  **타겟 build에서 활성화/수집 가능한지는 아직 확인하지 않았다.**
- trace 때문에 상세 로그를 매draw 콘솔에 출력하거나 강제 GPU 동기화를 넣지 않는다.
- process CPU 누적시간이나 SwapBuffers wall time만으로 GPU ms를 추정하지 않는다.
  present 대기는 vsync/GPU/driver 등을 포함할 수 있다. thread running/waiting을 구분해야 한다.
- GPU 시간 수단이 없으면 CPU측으로 확인된 범위까지만 결론낸다. 미분리 GPU 항목을 임의로 채우지 않는다.

### 결과가 바꾸는 production 결정

| 관측 결과 | 이후 선택 / 중단할 방향 |
|---|---|
| late frame이 주로 event/setup/publication에 집중하고 steady는 budget 내 | 재publication/생성 burst 최소화와 국소 reuse 검토. pass/blur 알고리즘 재설계는 후순위 |
| active update/render preparation CPU가 critical path, GPU는 여유 | B/C 국소 graph·timing 갱신 축소 우선. 2D blur 연구/halo 희생은 중단 |
| CPU preparation은 작고 H/V 실행·dependency/driver/GPU 구간이 critical | camera/constraint 미세 정리 기대를 낮춤. 실제 실행 영역/bundle 수·동시성 budget 및 semantics 논의로 전환 |
| 양쪽 비용이 겹치고 작은 감축으로 요구 budget을 맞출 근거 없음 | exact PER_LINE 무제한 사용을 전제로 한 미세 최적화 중단, 명시적인 UX/budget trade-off 설계 |
| trace가 불충분해 분리가 안 됨 | 2-stage 같은 새 micro 진단으로 확정을 가장하지 않음. 미확정임을 유지하고 예산/UX 결정을 요청 |

이 측정은 하나의 병목 이름을 찍기 위한 것이 아니라 **서로 다른 production 작업(B/C vs E/semantic vs
setup reuse) 중 어떤 것에 투자할지 결정**한다. 결과가 좋아도/나빠도 같은 다음 실험으로 가는 2-stage와 다르다.

### Stop rule

한정된 attribution 이후에도 “조금 줄일 수 있을 것 같다”만 남으면 micro PoC를 계속 나열하지 않는다.
필요한 개선폭과 현실적인 감축 범위를 대조해, current semantics를 지킬지 budget/UX를 명시적으로
바꿀지 결정한다. 이번 자료에는 그 개선폭을 정확히 계산할 frame-time breakdown이 없다.

## J. Stop list / 이전 권고의 변경

- Gaussian tap/weight ALU의 추가 미세 최적화, adaptive taps.
- 새 근거 없는 prefilter/A-R/Vogel/Poisson/sparse1pass/half-rate 재개.
- 3→2 stages 진단을 “다음 순서니까” 수행하는 것.
- FBO count와 pass count를 같다고 보고 ping-pong부터 구현하는 것.
- constraints 총수나 geometry 총수만 줄이면 해결된다는 가정.
- scope를 섞은45tasks→15tasks와 전체 화면 FPS의 선형 환산.
- BlurEffect 성능을 곧바로 상품화 합격선/동일 UX 동등 비교로 사용하는 것.
- geometry/draw bounds를 줄였으니 FBO clear/bandwidth도 같이 줄었다고 보고하는 것.

이전 pipeline-count 보고서의9/18/27/45 sweep은 **practical budget용 정보 가치 자체는 남는다**.
다만 이번 root-cause/architecture 선택의 다음 작업으로는 보류한다. pixels/format/labels/timing도
함께 바뀌는 실험이라 CPU graph와 GPU pass 중 어디를 수정할지 결정하지 못한다.
예전 1-page Output timing 이동 제안 역시, 이후 V-only/1tap/Source-only 증거를 반영해 주전략에서 내린다.

## K. Git state / 수행 범위

- UI: `05087317cac8ea9600bba498f00ccf8086a79d3f`, `devel_blur_text`, clean 유지.
- core: `f43e95be477ad301f84ecc772c753f9357821ecc`, clean 유지.
- adaptor: `dcadcfdc3e0d1abdce20767bee2858cb9e1851a4`, clean 유지.
- source/sample/test/build 설정 수정 없음. 새 PoC/patch 없음.
- commit/push/amend/rebase/reset/restore/stash 없음.
- 새 build/실행/benchmark/UTC/sanitizer 없음.
- 기존 보고서와 JSON inventory를 재사용하고 실제 source를 read-only로 확인했다.
- 새 산출물은 repository 밖의 이 분석 보고서 하나다. 기존 보고서는 수정하지 않았다.

추가 참조:
[Vogel 품질 실패](../reveal-vogel12-quality.0TCvfD/REPORT.md),
[기존1-page 감사](../reveal-onepage-analysis.RepMKt/REPORT.md),
[기존topology 분석](../reveal-topology-analysis.Thk4x4/REPORT.md).
