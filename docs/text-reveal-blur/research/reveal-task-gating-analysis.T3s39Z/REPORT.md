# Text::Reveal Blur — H/V RenderTask sleep/wake 분석

2026-09-21. **ANALYSIS ONLY.** Production 변경·빌드·앱 실행·UTC·계측·성능 측정 없음.

## A. Executive Verdict

**SAME-FRAME WAKEUP CANNOT BE GUARANTEED — 현재 UI-only refresh gating은 구현하지 않는다.**

| 대상 | 판정 | 이유 |
|---|---|---|
| 현재 API로 H/V refresh gating | **UNSAFE** | `SetRefreshRate()`는 event→update 메시지이고, animation progress와 같은 update에서 판단하는 constrainable activity property가 없다. |
| 향후 generic update-side task activity capability가 생긴 경우 | **CONDITIONAL** | 원리적으로 가능하지만 현재 제공되는 기능은 아니다. 초기화·refresh state·partial update·same-frame reverse 검증이 필요하다. |
| progress만으로 Source gating | **UNSAFE / NOT RECOMMENDED** | progress가 정지해도 gradient/ImageSpan 등 Source 내용은 변경될 수 있다. |

H/V를 쉬게 할 **수학적 구간과 실제 RenderPass 절감 가능성은 존재**한다. 하지만 지금의 API로 안전하게 연결할 방법이 확인되지 않았다. Actor visibility도 조사했으나 **draw만 제외되고 clear-only RenderPass가 남는다.**

따라서 현재 ECONOMY/PERFORMANCE를 유지한다. 이 결론은 blur curve를 더 바꾸자는 뜻도, H/V gating이 영구적으로 불가능하다는 뜻도 아니다.

요청에 제공된 target 평균 FPS 49.10→48.87, 중앙값 53.23→52.59는 전체 평균 개선 근거가 아니다. below-59 평균 42.29→42.98, 최저 27.92→30.21만으로 병목의 정확한 비중도 확정할 수 없다. 과거 1-tap/glFlush/Source-only 결과와 함께 보면 fixed pipeline overhead를 조사할 이유는 충분하지만, 이번 결과를 CPU/GPU ms나 FPS로 환산하지 않는다.

## B. Current RenderTask lifecycle

`RuntimeBlurActor::Initialize()`가 publication별 page를 구성하고 Source/H/V FBO, actors, renderers, constraints를 만든다. `OnSceneConnection()`에서 page당 task 세 개를 Source→H→V 순서로 연결한다.

```text
TextVisual valid publication
  → RuntimeBlurActor / Pass[] 생성
  → 각 page: Source → H → V
  → onscreen Output
  → None / 교체 / disconnect: task 제거 및 runtime 정리
```

모든 Source/H/V task는 clear enabled, transparent clear, exclusive, REFRESH_ALWAYS다. `GetOffScreenRenderTasks()`가 pass 순서와 내부 Source/H/V 순서를 반환하고 Core reorder가 offscreen 작업을 onscreen보다 먼저 배치한다. 장식이 있으면 별도의 decoration capture task가 추가되며 이번 H/V 분석 대상이 아니다.

중요: REFRESH_ALWAYS는 **scene가 바뀌며 프레임을 생성할 때 매번** 처리한다. 정지한 앱을 무조건 60 FPS로 계속 깨우는 설정은 아니다. 완전히 idle인 화면에 대한 지속 GPU 비용 절감을 주장하지 않는다.

근거: runtime Initialize / pass 생성 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1250`), task 생성·해제 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1789`), 순서 반환 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:955`), refresh API contract (로컬 자료: `../dali/dali-core/dali/public-api/render-tasks/render-task.h:572`).

## C. Current HIGH / PERFORMANCE / ECONOMY activity

W/H는 halo와 page packing을 반영한 크기다. 아래 quarter는 각 축의 크기이며 면적 1/4이라는 뜻이 아니다.

| Quality | Source | H | V | Output | q=1 이후 현재 동작 |
|---|---|---|---|---|---|
| HIGH | W×H | W×H | W×H | V만 | H/V가 최신 Source를 copy |
| PERFORMANCE | W×H | ceil(W/4)×H | ceil(W/4)×ceil(H/4) | Source/V Late Smooth | H/V copy, Output은 Source |
| ECONOMY | W×H | ceil(W/4)×ceil(H/4) | 동일 quarter 크기 | Source×sharpAlpha + V×blurAlpha | H/V copy, Output은 Source |

세 quality 모두 **page당 3 tasks**. 같은 runtime 안의 같은 size/format page는 H scratch를 공유할 수 있다. HIGH만 Source scratch도 공유할 수 있다. PERFORMANCE/ECONOMY Source는 Output이 읽으므로 page별로 유지한다. V는 각 page의 결과를 유지한다.

따라서 sleeping H가 자기 page의 이전 결과를 보존한다고 가정하면 안 된다. 다른 page가 동일 H FBO를 덮어쓸 수 있다.

근거: scratch 및 FBO dimensions (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1290`), Output 구성 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:451`).

## D. Exact blur-unused conditions

현재 `BlurStrength::Evaluate()`:

```text
p = ResolveRenderProgress(authored/animated progress)
q = p >= 1 ? 1 : clamp((p - sequenceStart) / blurDuration, 0, 1)
b = 1 - q*q*(3 - 2*q)
```

`ResolveRenderProgress()`에는 기존 endpoint 보정이 있다: nonpositive/NaN→0, `p >= 1 - float epsilon`→1. 이번 분석에서 새 epsilon을 제안하거나 추가하지 않는다.

| Quality | V의 색 기여가 없어지는 조건 | H/V sleep의 충분조건인가? |
|---|---|---|
| PERFORMANCE | Output의 `t <= 0`, 즉 R×b ≤ 2 | 모든 page consumer가 이 조건이고 초기 유효 texture가 존재할 때 논리적으로 충분 |
| ECONOMY | 실제 `Composition(b).y == 0` | 모든 consumer의 실제 weight가 0일 때 논리적으로 충분 |
| HIGH | 일반적인 sharp endpoint에는 없음 | strength=0도 V를 사용하므로 불충분 |

R은 output에 전달된 **display-pixel authored radius**다. kernel의 even rounding radius와 혼동하지 않는다. UI scale이 달라지면 R도 달라진다.

Output shader는 소스 코드상 V fetch를 먼저 작성한 뒤 Source와 합성한다. weight=0은 **V 색상에 대한 의존성이 0**이라는 뜻이지 texture binding, driver dependency check, texture fetch가 반드시 없어지는 뜻은 아니다. 기존 sampler/유효 texture는 유지해야 한다.

근거: native amount (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:63`), progress endpoint (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal.h:58`), Output 식 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:574`).

## E. PERFORMANCE Late Smooth inactive interval

실제 상수 이름은 `SHARP_ONLY_EFFECTIVE_RADIUS=2`, `BLUR_ONLY_EFFECTIVE_RADIUS=8`이다.

```text
t = (R*b - 2) / 6
blurMix = smootherstep(clamp(t, 0, 1))
color = t <= 0 ? sharpSource : mix(sharpSource, V, blurMix)
```

| R | Source-only native b | local q 범위 | local linear 시간의 sharp-only 꼬리 |
|---|---|---|---|
| 24 | 0 ≤ b ≤ 1/12 | q ≥ 0.822500797 | 약 17.750% |
| 48 | 0 ≤ b ≤ 1/24 | q ≥ 0.876997836 | 약 12.300% |

global progress 경계는 보통 `start + blurDuration*q_threshold`이며 p=1 endpoint 강제 완료도 반영해야 한다. **q를 Label 전체 progress 또는 wall-clock 비율로 바로 읽으면 안 된다.**

start=0, duration=1인 짧은 Label에 EASE_OUT_SQUARE를 적용하면 p=1−(1−u)²다. R24 Source-only는 wall-clock u≈0.578693457부터이므로 뒤의 약 42.131%다. 반면 normal exit는 LINEAR reverse이므로 R48에서는 초반 약 12.300%, 0.4초 중 약 49.201ms만 Source-only다.

이 수치는 식의 해를 계산한 것이지 portable CPU/GPU gate threshold를 새로 확정한 것이 아니다. shader highp 연산과 CPU threshold를 다른 식으로 계산해 경계에서 한 프레임 빨리 끄면 안 된다.

## F. ECONOMY inactive interval

실제 식:

```text
sharp = 1 - smoothstep(0, .60, b)
blur  = min(1 - sharp, .90 * smoothstep(0, .65, b))
```

실수 연산에서는 b>0이면 blur>0, b=0이면 blur=0이다. 즉 **자기 blur 구간 0<q<1에 의도적으로 만든 Source-only 구간은 없다.** 다만 q는 clamp되므로 q=1이 된 뒤에도 Label Reveal은 계속 진행할 수 있다. BDR<1인 긴 텍스트에서는 endpoint 한 프레임만의 문제가 아니다.

실제 float에는 `1 - sharp`의 cancellation이 있다. 계산 스크립트 (로컬 자료: `calculate.py`)는 각 연산을 IEEE binary32 round-to-nearest로 반올림하고 FMA/fast-math 없이 계산한다. production 실행/UTC가 아니라 수치 reference다.

| binary32 reference | 값 |
|---|---:|
| Composition의 blur가 0인 최대 nonnegative amount | 0.00005980398418614641 |
| 다음 representable amount | 0.000059803987824125215 |
| 그때 blurAlpha | 약 2.28544×10⁻⁸ |

native q→b까지 계산하면 경계의 반올림 때문에 단일 매끈한 q threshold도 아니다:

- q=0.9955273270606995부터 잠시 0.
- q=0.9955281615257263부터 다시 작은 양수.
- q=0.995529055595398부터 1까지 0.

이 작은 꼬리는 quality 정책상 새 dead zone이 아니다. compiler contraction/플랫폼 연산에 따라 경계가 달라질 수 있으므로 **이 숫자를 hardcode해서 gate하면 안 된다.** 정확성을 우선하면 실제 Composition 결과를 사용하거나 더 보수적으로 q=1 완료만 사용해야 한다. 아래 구조 계산은 이 rounding 꼬리를 이득으로 세지 않았다.

## G. Before-start opportunity

H/V shader는 현재 `p<=0` 또는 `p < start - 1/65535`에서 transparent를 출력한다. 실제 start 경계에는 기존 metadata 양자화 허용 범위가 있다. 단순 `p<start`만으로 재작성하면 동일 동작이 아니다.

| 상태 | 논리적 내용 | 지금 task를 바로 쉬게 하면? |
|---|---|---|
| 처음 생성된 before-start | 투명 | FBO가 아직 유효하게 초기화되지 않았을 수 있음 |
| reverse/seek로 돌아온 before-start | 투명이어야 함 | 이전 V가 남아 Output에 노출될 수 있음 |
| 완료 후 sharp | 최신 Source | PERFORMANCE/ECONOMY는 V weight가 0이어서 old V의 색은 무관 |

**before-start는 completed-sharp와 다르다.** before-start의 b는 1이며 여기서 분석한 radius24/48의 PERFORMANCE는 V, ECONOMY는 약 .9×V를 사용한다. Output에는 H/V shader와 같은 before-start guard가 없다. 따라서 H/V의 transparent clear/draw 없이 stale V를 남기면, Source가 투명이더라도 잘못 보일 수 있다. 일반식상 PERFORMANCE의 display radius가 2 이하이면 전 구간 Source-only지만, 이 작은-radius 예외로 일반적인 before-start gating을 정당화할 수는 없다.

추가 Output gating 또는 상태 진입 시 clear를 보장하면 가능성을 따로 검토할 수 있지만, 이번 최소 후보에는 포함하지 않는다. HIGH도 before-start라고 무조건 sleep할 수 없다.

근거: blur shader main (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/graphics/shaders/text-reveal-blur.frag:85`), Output main (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:574`).

## H. PER_LINE/page aggregation

`mSequences`를 pack한 `batch.first/count`가 task 3개를 갖는 `Pass`에 대응한다. 현재 Pass에는 size/actors/buffers/tasks만 있고 activity나 consumer timing 범위는 저장하지 않는다.

정확한 필요조건:

```text
page may sleep only if every output consumer of that page
is independent of V in the current frame
AND retained texture bindings are valid.
```

"아직 시작하지 않았으니 invisible"를 무조건 independent로 분류하지 않는다(G 참조). 가장 작은 보수적 조건은 **해당 page의 모든 sequence가 Source-only 완료 영역에 들어간 경우**다. 하나라도 blur active이거나 V에 의존하는 before-start이면 page H/V를 유지한다.

- A8 text와 image-bearing RGBA line은 format별로 나누어 pack한다.
- ImageSpan은 해당 sequence의 Source capture에 포함된다. 별도의 독립 image blur 완료를 text만 보고 판단하면 안 된다.
- 원래 logical order 복원을 위해 한 page에 Output renderer가 여러 개 생길 수 있다.
- `MAX_LINES_PER_DRAW=64`는 draw/UBO split이다. 65줄을 두 draw로 나눠도 같은 page라면 H/V task는 공유한다.
- 첫 output renderer/첫 line만 확인하는 것은 틀리다. 모든 draw/output consumer를 page 단위로 묶어야 한다.
- 다른 page가 공유 H scratch를 덮어쓸 수 있으므로 wake는 **H와 V를 함께** 해야 한다.

근거: mixed format partition (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1128`), draw/output split (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1610`), 64-line limit (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-blur-renderer.h:32`).

## I. REFRESH_ALWAYS / REFRESH_ONCE semantics

Event `RenderTask::SetRefreshRate()`는 cached value를 변경하고 `SetRefreshRateMessage`를 enqueue한다. Update RenderTask가 메시지를 받으면:

```text
rate>0 → RENDER_CONTINUOUSLY, frameCounter=0
rate=0 → RENDER_ONCE_WAITING_FOR_RESOURCES, waiting=true

IsRenderRequired:
  continuous → frameCounter==0
  once-waiting → true
  rendered-once / notified → false
```

`UpdateState()`가 once-waiting→rendered-once로 옮기고 이후 완료 통지를 처리한다. requires-sync 사용 시 완료 통지는 sync 상태도 확인하지만, 다음 draw가 매 프레임 반복되는 것은 아니다.

따라서 **REFRESH_ONCE는 즉시 sleep 명령이 아니라 한 번 더 렌더링을 요청하는 명령**이다. 다시 호출하면 다시 한 번 렌더링한다. 매 frame inactive 상태에서 재설정하면 잠들지 않는다.

`SetRefreshRate(REFRESH_ALWAYS)` 메시지가 update에 도달한 frame에서는 counter=0이므로 바로 render 대상이 된다. 문제는 animation threshold를 event에서 알아낸 후 메시지를 보내는 **도착 시점**이다.

FrameBuffer handle은 SetRefreshRate로 교체·해제되지 않는다. event측 `ClearRenderResult()`는 CPU readback 보관 상태를 정리하는 것으로, GPU color attachment를 clear하거나 없애는 함수가 아니다. scene/graphics context가 살아 있는 동안 retained GPU texture를 유지할 수 있다. context loss는 별도의 재초기화 문제다.

근거: event setter (로컬 자료: `../dali/dali-core/dali/internal/event/render-tasks/render-task-impl.cpp:672`), update state (로컬 자료: `../dali/dali-core/dali/internal/update/render-tasks/scene-graph-render-task.cpp:220`), CPU readback cleanup (로컬 자료: `../dali/dali-core/dali/internal/render/renderers/render-frame-buffer.cpp:134`).

## J. Sleep behavior — 실제 없어지는 작업

Core `ProcessTasks()`는 `IsRenderRequired()==false`면 renderables 순회와 `RenderInstructionProcessor::Prepare()`를 호출하지 않는다. 따라서 해당 task의 RenderInstruction이 frame container에 들어가지 않는다. RenderManager는 instruction별 command buffer와 RenderPass를 생성하므로 해당 H/V pass가 실제로 빠진다.

| 방식 | draw | FBO bind/clear/pass | dependency/submit | 판정 |
|---|---|---|---|---|
| H/V `uOpacity=0` | 그대로 shader 실행 | 그대로 | 그대로 | no gate |
| Renderer opacity=0 | 일반 renderer는 skip 가능하나 현재 H/V는 BlendMode::OFF → OPAQUE | 그대로 | pass 처리 남음 | 효과 보장 없음 |
| H/V source Actor VISIBLE=false / ignored | renderables traversal에서 제외 | **clear-enabled instruction 남음** | Begin/EndRenderPass, FBO sync/flush 남음 | fixed pass 비용 제거 실패 |
| REFRESH_ONCE 완료 후 | 없음 | 없음 | 해당 pass producer 작업 없음 | 실제 task rendering sleep |
| source/camera를 unparent | task inactive | 없음 | 없음 | event/lifecycle mutation이므로 매-frame 제어 대안 아님 |

Actor visibility는 constrainable이므로 same-update draw 제어는 가능하다. 하지만 `ReadyToRender()`의 active 조건은 **source/camera의 scene 연결**이며 visibility가 아니다. invisible node를 제외한 후에도 `mIsClearColorSet`이면 instruction을 push한다. 따라서 visibility만으로 pass sleep이 된다는 결론은 틀리다.

clear를 항상 꺼버리는 것도 답이 아니다. D2의 partial H geometry, page 여백, 공유 scratch를 transparent로 지우는 기존 동작을 바꾸며 stale content 위험이 있다. clear-enabled는 현재 animatable boolean property도 아니다.

실제 refresh sleep 후에도 task container 순회, camera/actor/constraint update, retained resource/handle, Output의 sampler binding은 남는다. Adaptor의 Output texture binding은 zero shader weight를 이해하지 않으므로 `CheckNeedsSync()` 호출 자체까지 0이 된다고 주장하지 않는다. 그러나 H/V가 생산하지 않는 frame에는 해당 pass의 FBO write-check, draw, EndRenderPass의 AddTextures/fence/flush가 없어지고, 그 pass가 생산한 당-frame forward dependency도 없다. Source 및 다른 producer의 sync는 유지된다.

근거: task processor (로컬 자료: `../dali/dali-core/dali/internal/update/manager/render-task-processor.cpp:251`), visibility traversal (로컬 자료: `../dali/dali-core/dali/internal/update/manager/render-task-processor.cpp:91`), clear-only instruction (로컬 자료: `../dali/dali-core/dali/internal/update/manager/render-instruction-processor.cpp:707`), BlendMode::OFF (로컬 자료: `../dali/dali-core/dali/internal/update/rendering/scene-graph-renderer.cpp:735`), RenderPass 생성 (로컬 자료: `../dali/dali-core/dali/internal/render/common/render-manager.cpp:1126`), texture read dependency (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:581`), FBO write dependency (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:1113`), pass-end fence/flush (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:1212`).

## K. Wake-up behavior

기존 order는 필요한 프레임에 H/V가 모두 제출되기만 하면 Source→H→V→Output을 보장하는 기반이다. shared H에서도 각 page의 H 직후 V를 처리하고 다음 page로 간다. 그러나 **order는 제출되지 않은 sleeping task를 자동으로 깨우지 않는다.**

실패 가능한 현재 API 연결:

```text
frame N:   p=1, H/V once-render 완료, Output=Source
frame N+1: update Animation이 p를 blur 영역으로 이동
           Output constraint는 새 p / 새 blur weight
           H/V는 아직 RENDERED_ONCE → instruction 없음
           Output이 old V를 읽음
           notification은 event로 전달되는 중
frame N+2 또는 이후: event에서 보낸 ALWAYS 메시지를 처리, H/V 재개
```

한 프레임 이상 stale 가능성이 있다. 메인 스레드를 기다리지 않게 만든다고 이 순서 문제가 해결되지는 않는다.

## L. Reverse / seek correctness

| Case | 수학적으로 H/V sleep 가능? | wake 조건 / 현재 연결의 위험 |
|---|---|---|
| WHOLE_TEXT before-start | 무조건 불가 | V를 투명하게 만든 사실/Output guard 필요 |
| WHOLE_TEXT blur active | 불가 | 같은 frame H/V 필수 |
| PERFORMANCE complete sharp | 가능 | p가 Source-only 영역을 벗어나는 바로 그 update에서 wake |
| ECONOMY complete sharp | 가능 | actual blurAlpha>0으로 돌아가는 바로 그 update에서 wake |
| HIGH complete | 불가 | 최신 sharp도 V를 경유 |
| PER_LINE 한 줄만 완료 | 나머지 consumer에 따라 불가 | active/before-start consumer가 있으면 page 유지 |
| page all complete | 가능 | 하나라도 V를 다시 필요로 하면 H/V pair wake |
| reverse Animation | 조건부 | event notification으로는 첫 blur frame 보장 불가 |
| seek 1→.2 | 조건부 | next rendered frame에서 최신 H/V가 있어야 함 |
| seek 1→0→1 | 조건부 | 한 update에 collapse되면 최종 상태, 서로 다른 update면 각 상태를 처리. p=0은 stale V를 지워야 함 |
| quality switch/reconnect | 새 runtime 기준 | old sleep bit/cache를 상속하면 안 됨 |

매 frame current progress에서 activity를 계산하는 update-side 방식이라면 crossing event를 기억할 필요가 없다. 이 방식이 있어야 reverse/seek가 동등하게 처리된다. 현재 refresh API에는 그 연결점이 없다.

## M. Event / update / render thread interaction

UpdateManager의 실제 순서:

```text
queued event messages
→ Animate
→ UpdateNodes / node constraints
→ RenderTask constraints
→ Shader / Renderer constraints
→ PropertyNotification check & event queue
→ cameras
→ Process RenderTasks / instructions
→ render thread command submission
```

RenderTask의 현재 기본 property table에는 viewport position/size와 clearColor 등만 있으며 **refreshRate/activity는 없다.** custom boolean을 등록해 constraint를 걸어도 scheduler가 이를 읽지 않으므로 task gate가 되지 않는다. UpdateProxy에도 RenderTask refresh/activity 제어 기능은 없다. update constraint 안에서 event API SetRefreshRate를 호출하는 것은 적절한 thread contract가 아니다.

PropertyNotification은 update→event queue 후 event→update message를 다시 거친다. Animation의 해당 frame Output보다 빨라질 수 없고 event loop 지연도 있을 수 있다. many-page notifications, disconnect 이후 callback 등 수명 관리도 추가된다. Timer/polling과 함께 **NO-GO**다.

향후 generic task-activity property를 만든다면 owner progress를 직접 읽어야 한다. RenderTask constraint는 renderer constraint보다 먼저 적용되므로 Output renderer의 이번 frame weight를 직접 읽겠다는 설계에도 순서상 주의가 필요하다.

근거: update 순서 (로컬 자료: `../dali/dali-core/dali/internal/update/manager/update-manager.cpp:1290`), notification queue (로컬 자료: `../dali/dali-core/dali/internal/update/manager/update-manager.cpp:1112`), task property table (로컬 자료: `../dali/dali-core/dali/internal/event/render-tasks/render-task-impl.cpp:64`), UpdateProxy (로컬 자료: `../dali/dali-core/dali/public-api/update/update-proxy.h:287`).

## N. Source-change-while-sleeping

H/V만 잠들고 Source는 계속 갱신한다는 전제에서:

- A8의 단색 변경은 일부를 Output에서 적용하지만 RGBA/gradient/image Source 내용은 실제로 바뀔 수 있다.
- text/style/layout/source revision 변경은 runtime을 교체할 수도 있다.
- 기존 runtime에서 Source가 바뀌었으면 wake frame의 **최신 Source→H→V**가 필요하다.
- 이전 H를 재사용하면 안 된다. shared scratch일 수도 있다.

현재 always path는 이 순서를 매번 유지한다. 가상의 update-side gate도 같은 순서를 보존해야 한다. event notification wake로는 최신 Source와 과거 V가 섞이는 frame을 막지 못한다.

## O. Source gating separate verdict

**UNSAFE / NOT RECOMMENDED: progress가 완료·정지했다는 사실만으로 Source를 쉬게 하지 않는다.**

progress 외의 foreground property, gradient animation, ImageSpan renderer 교체/readiness, async publication, layout/font/raster invalidation을 모두 포괄하는 Source-stability contract가 현재 runtime에 없다. GPU texture가 그대로 있다는 사실도 그 texture에 찍는 shader uniforms가 그대로라는 뜻은 아니다.

이를 정확히 추적하려고 새 invalidation graph, revision/cache, generic animation observer를 추가하면 이번 최소 H/V 후보를 벗어난다. H/V gating과 묶지 않는다. scene가 완전히 idle이면 Core가 이미 frame 생성을 멈출 수 있어 추가 이득도 자동으로 있는 것이 아니다.

## P. Async / disconnect / quality switch lifecycle

현재 publication은 reveal revision, controller render revision, source identity/layout generation, feature/scale/geometry 등 동일성을 확인한다. runtime을 data에 소유시킨 후 activate하며, reentry/cancel은 현재 candidate가 맞는지 확인하고 제거한다. `RemoveRenderer()`는 runtime을 먼저 제거한다.

가상의 gate는 runtime/Pass가 소유하고 scene-global manager나 callback queue에 별도로 남기지 않아야 한다. 새 quality/async result는 current progress에서 activity를 새로 구성하며 old task의 sleeping state를 이식하면 안 된다. disconnect는 현재처럼 task를 제거하고 reconnect 때 새 task를 구성한다.

BDR=0 또는 radius=0은 기존 eligibility에서 runtime blur를 만들지 않는다. preparation에서 normalized blurDuration이 양수가 아니면 fallback한다. 양수 tiny duration은 기존 q clamp를 그대로 이용하고, threshold crossing 통지를 의존하지 않아야 한다. small/.5/1 모두 통과하지 않은 threshold를 보상하기 위한 timer가 필요 없어야 한다.

근거: eligibility (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:3113`), normalized duration (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp:494`), publication identity (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:3155`), candidate activation/cancellation (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:3480`), removal (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:479`), disconnect (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1852`).

## Q. Text Effect Demo timeline

**이번에는 앱을 실행하지 않았다.** Label 수/delay/duration/cleanup은 현재 sample source에서 읽었고, page 수는 [기존 1280×720 native inventory](../reveal-pipeline-count.a9GqcW/REPORT.md)의 production 측정을 재사용했다. 이 sample과 과거 baseline 사이의 diff는 quality controls 관련이며 아래 타이밍/텍스트는 동일하다. 페이지 packing 구현도 ECONOMY 추가로 바뀌지 않았다. 다만 새 폰트/창 크기의 actual page 수를 재검증했다는 뜻은 아니다.

Cards 3개×day/title/places/subtitle = **12 Labels**. 과거 fixture는 20 visible lines / 15 pages였다.

| Card | day/title/places/subtitle pages | Reveal 시작 초: day/title/places/subtitle | 완료 초: day/title/places/subtitle |
|---|---|---|---|
| 1 | 1 / 1 / 2 / 1 | .06 / .16 / .36 / .48 | .48 / 1.04 / 1.54 / 2.48 |
| 2 | 1 / 1 / 1 / 1 | .54 / .64 / .84 / .96 | .96 / 1.52 / 2.02 / 2.96 |
| 3 | 1 / 1 / 2 / 2 | 1.02 / 1.12 / 1.32 / 1.44 | 1.44 / 2.00 / 2.50 / 3.44 |

duration은 .42 / .88 / 1.18 / 2.00초이며 alpha는 모두 EASE_OUT_SQUARE다. delay에는 기존 .06초 reveal lead를 포함했다.

- 짧은 day/title/places: PER_LINE, Fade0, Stagger0, BDR1, radius24.
- 64문자 이상 subtitle: PER_LINE, Fade0, Stagger.25, BDR.5, radius24.
- Cards의 12 Labels는 공통 animation이 끝나는 **3.44초에 일괄 None** 처리한다. 먼저 끝난 day만 개별 해제하지 않는다.
- scene의 다른 5 Labels는 별도 animation으로 .06초에 시작한다. 짧은 텍스트 1초/긴 텍스트 2초이며 최장2.06초에 일괄 None 처리한다.
- 이후 badge/action 2 Labels가 .06초 lead + .60초 동안 등장한 뒤 None 처리된다.
- 최종 RESULTS_READY에서는 **Reveal blur가 모두 해제된 상태**다. gradient loop가 계속된다는 이유로 H/V가 영구적으로 남는 구조가 아니다.
- normal exit: 최종19 Labels, WHOLE_TEXT/Fade1/Stagger0/BDR1/radius48, LINEAR 역재생 .40초. 과거 fixture는 19 pages였다. 미완료 entrance를 중단한 exit는 기존 schedule을 유지하므로 별도 사례다.

샘플의 정상 퇴장에서는 완료 시 해제했던 blur runtime을 새 퇴장 설정으로 다시 만든다. 따라서 “잠들어 있던19 pages를 그대로 깨운다”와는 다르다.

### Page timeline — 완료 후만 쉬는 보수적 가상 gate

처음부터 모든 page가 준비되었다고 가정한다. before-start는 gate하지 않는다. 긴 텍스트의 actual sequenceStart는 과거 inventory에 없으므로 q가 아닌 **Label p=1까지 active를 유지**한다. PERFORMANCE 열만 짧은 텍스트의 알려진 Source-only 구간도 반영했다.

| t초 | Cards retained pages | p=1 완료 후만: active / sleep | PERFORMANCE 짧은 텍스트도 반영: active / sleep |
|---|---:|---:|---:|
| 0 | 15 | 15 / 0 | 15 / 0 |
| .48 | 15 | 14 / 1 | 14 / 1 |
| 1.0 | 15 | 13 / 2 | 12 / 3 |
| 1.5 | 15 | 11 / 4 | 8 / 7 |
| 2.0 | 15 | 7 / 8 | 6 / 9 |
| 2.5 | 15 | 3 / 12 | 3 / 12 |
| 3.0 | 15 | 2 / 13 | 2 / 13 |
| 3.43 | 15 | 2 / 13 | 2 / 13 |
| 3.44 cleanup 후 | 0 | 0 / 0 | 0 / 0 |

H/V active task 수는 active pages×2다. scene의 다른5 pages는 2.06초까지 별도로 더한다. 위 표는 새 trace가 아니다.

### 긴 텍스트의 sequence normalization

실제 start/duration은 final glyph/pixel plan에 의존한다. 3줄, 첫 gap 없음, Fade0, Stagger.25, 각 줄 pixel span을 최장 줄 대비 w_i≤1로 두면 raw total은 T=max(max(i×.25+w_i), .5+.5)다. T∈[1,1.5], start_i=i×.25/T, blurDuration=.5/T다.

모든 줄이 sharp가 되는 global p의 이론 범위는 PERFORMANCE≈[.6075003,.9112504], ECONOMY=[2/3,1]이다. ECONOMY도 긴 텍스트에서는 fade/reveal 종료 전에 blur가 끝날 수 있다. 실제 w_i나 page별 line 배치를 추정해서 확정하지 않기 위해 이 추가 절감분은 위 표에 넣지 않았다.

근거: schedule normalization (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal.cpp:1281`), blur normalization (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp:494`), entrance spec (로컬 자료: `../dali/dali-ui/samples/text/text-effect-demo.cpp:306`), Cards timing / cleanup (로컬 자료: `../dali/dali-ui/samples/text/text-effect-demo.cpp:1674`), 다른 Labels cleanup (로컬 자료: `../dali/dali-ui/samples/text/text-effect-demo.cpp:1432`).

## R. Structural execution reduction

**실행 가능성을 검증한 측정이 아니라, same-frame gate가 있다고 가정한 산술 모델**이다. 60Hz에서 t=f/60, Cards는 f=0..206의207 logical frames, 첫 layout/async 지연은 없다고 가정한다. 경계 반올림과 실제 dropped frame에 따라 실행 수는 달라진다. 이번에 target을 측정하지 않았다.

| 구간 | baseline H/V executions | 가상 gate | reduction |
|---|---:|---:|---:|
| Cards entrance, p=1 완료만 | 6,210 | 3,596 | 42.09% |
| Cards entrance, PERFORMANCE 짧은 텍스트 sharp-only도 이용 | 6,210 | 3,102 | 50.05% |
| scene의 다른5 Labels, p=1 완료만 | 1,240 | 760 | 38.71% |
| scene의 다른5 Labels, PERFORMANCE 짧은 텍스트도 이용 | 1,240 | 560 | 54.84% |
| 전체 entrance, p=1 완료만 | 7,450 | 4,356 | 41.53% |
| 전체 entrance, PERFORMANCE 짧은 텍스트도 이용 | 7,450 | 3,662 | 50.85% |
| entrance/affordance 종료 후 steady | 0 | 0 | 추가 절감 없음 |

첫 p=1 모델은 ECONOMY/PERFORMANCE 양쪽에 수학적으로 적용할 수 있다. HIGH에는 이 숫자를 적용하지 않는다. Source task 실행 수는 동일하다. Cards만 보면 Source=3,105 logical executions가 남아 전체 offscreen 합계는 9,315→6,701 또는6,207이다. **H/V−42%를 전체 pipeline−42%나 FPS+42%라고 부르면 안 된다.**

normal exit는 24 logical frames×19pages×2=912 H/V executions다. PERFORMANCE R48의 초기 약49.2ms만 Source-only다.

| normal exit .4초 | baseline | 이상적 gate | 첫 유효 결과 seed pass를 1frame 확보할 경우 |
|---|---:|---:|---:|
| PERFORMANCE | 912 | 798 (−12.5%) | 836 (−8.33%) |
| ECONOMY, float의 작은 tail은 제외 | 912 | 874 (t=0 endpoint만 제외) | 912 (절감 없음) |

퇴장 시작이 첫 render sample보다 앞서면 endpoint frame 자체를 관측하지 않을 수 있다. ECONOMY의 binary32 tail도 linear .4초에서는 약1.8ms로, 안정적으로 여러 프레임을 줄일 수 있는 구간은 아니다. 퇴장 대부분은 두 quality 모두 H/V가 필요하다.

현재 API로 안전한 gate를 구현할 수 있다는 결론이 아니다. 숫자는 “실현한다면 어느 구간에 의미가 있는가”를 한정하기 위한 것이다. 전체 계산은 CALCULATIONS.json (로컬 자료: `CALCULATIONS.json`)에 있다.

## S. Setup / FBO limitation

Task/FBO/actors/renderers를 유지하고 실행만 멈추면 allocation 수, GPU texture payload, CPU retained memory는 줄지 않는다. driver의 lazy initialization 시점은 달라질 수 있지만 초기화 총수를 줄이는 설계는 아니다.

특히 이 demo의 normal exit는 runtime을 새로 만들기 때문에 기존에 관찰한 **exit publication/FBO creation spike를 직접 해결하지 않는다.** seed frame이 필요하면 첫 무거운 frame도 남는다. 기존 runtime을 reverse할 때는 재allocation이 필요 없지만, 여러 page 동시 wake의 submission burst는 남는다.

## T. Candidate design — 구현하려면 필요한 최소 구조

1. **현재 REFRESH_ALWAYS↔ONCE: NO-GO.** callback/polling 없이 progress를 scheduler에 연결하지 못한다.
2. **현재 renderer/actor draw gating: 목적에 불충분.** clear-pass/fence/flush를 없애지 못한다.
3. **향후 generic Core task-activity property: 조건부 별도 과제.** Text 전용 branch는 만들지 않는다.

3을 별도로 추진한다면 default=true인 update-side activity를 instruction 생성 전에 판정하는 generic capability가 최소 방향이다. 그러나 property 하나를 추가하면 끝나는 것은 아니다. inactive 중 REFRESH_ONCE state 진행, frame-counter, dirty/render-loop wake, partial update, initialization/context 복구를 정의해야 하므로 Core 변경의 리스크를 높게 본다. Adaptor에 Text 판별은 필요 없다.

UI 쪽 첫 후보를 최소화한다면 PERFORMANCE/ECONOMY만, **completed-sharp 쪽만**, Source는 계속 유지하고 H/V를 pair로 같은 frame에 판정한다. before-start는 현재 transparent 처리를 유지한다.

Ownership은 RuntimeBlurActor::Pass에 한정한다. threshold/마지막 sequence start 등의 immutable metadata를 page 구성 시 저장한다. p>=1만 이용하면 line scan도 필요 없다. 더 세밀한 q=1 조건도 같은 duration의 최신 start를 이용해 보수적으로 판정할 수 있다. floating Composition의 비단조 경계까지 활용하려고 매 frame line scan을 추가하지 않는다.

각 task constraint는 owner progress를 직접 읽는다. 모든 line의 매 frame 순회, allocation, 문자열 property lookup, notification을 피하고 O(page) 계산과 소수의 property writes에 제한한다. 그래도 기존 renderer/strength constraints는 남고 제어 비용 자체도 무료가 아니다.

**이 설계는 미구현이며, 지금 PoC를 진행하라는 추천이 아니다.** 현재 GO 조건3·4가 충족되지 않아 중단한다.

## U. Exact code locations

주요 추적 지점이다. 행 번호는 이번에 확인한 HEAD 기준이다.

| Repository / location | 확인 내용 |
|---|---|
| UI runtime:63 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:63`) | native q/b, zero 시 copy |
| UI runtime:90 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:90`) | Output update constraints |
| UI economy:68 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-blur-economy.h:68`) | Composition |
| UI runtime:574 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:574`) | Output Source/V 의존 |
| UI runtime:1290 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1290`) | shared H scratch / retained Source |
| UI runtime:1789 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1789`) | clear/refresh/task 연결 |
| Core event task:672 (로컬 자료: `../dali/dali-core/dali/internal/event/render-tasks/render-task-impl.cpp:672`) | SetRefreshRate queue |
| Core messages:68 (로컬 자료: `../dali/dali-core/dali/internal/update/render-tasks/scene-graph-render-task-messages.h:68`) | event→update setter message |
| Core update task:220 (로컬 자료: `../dali/dali-core/dali/internal/update/render-tasks/scene-graph-render-task.cpp:220`) | refresh state machine |
| Core update task:393 (로컬 자료: `../dali/dali-core/dali/internal/update/render-tasks/scene-graph-render-task.cpp:393`) | instruction 및 clear/sync |
| Core update task:636 (로컬 자료: `../dali/dali-core/dali/internal/update/render-tasks/scene-graph-render-task.cpp:636`) | active는 scene 연결 조건 |
| Core task processor:272 (로컬 자료: `../dali/dali-core/dali/internal/update/manager/render-task-processor.cpp:272`) | render-required일 때만 Prepare |
| Core instruction processor:707 (로컬 자료: `../dali/dali-core/dali/internal/update/manager/render-instruction-processor.cpp:707`) | clear-only pass가 남는 근거 |
| Core RenderManager:1126 (로컬 자료: `../dali/dali-core/dali/internal/render/common/render-manager.cpp:1126`) | instruction→command buffer/pass |
| Core reorder:261 (로컬 자료: `../dali/dali-core/dali/internal/event/render-tasks/render-task-list-impl.cpp:261`) | offscreen ordering |
| Adaptor context:581 (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:581`) | texture binding 시 dependency |
| Adaptor context:1113 (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:1113`) | FBO write dependency |
| Adaptor context:1212 (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:1212`) | producer 등록/fence/flush |
| Adaptor dependency reset:130 (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-texture-dependency-checker.cpp:130`) | frame dependency 갱신 |

## V. Required UTC / regression tests — 이번에는 실행하지 않음

generic capability를 별도 과제로 구현할 때 필요한 검증이다.

- active frame pixel-identical 비교, PERFORMANCE threshold 전후, ECONOMY exact-zero 전후. quality curve 변경 없음.
- first publication at p=0 / intermediate / 1, 초기 유효FBO 및 Source/V binding.
- update-side Animation reverse, 같은 frame seek1→.2, 서로 다른 frame의1→0→1. old V에 알려진 색을 넣어 stale 노출 검출.
- PER_LINE mixed/all-complete, 여러page/shared H, A8/RGBA 혼합, ImageSpan late load/renderer replacement.
- 64/65 draw split과 logical-order Output split을 포함하는 all-consumer 판정.
- BDR0/tiny/.5/1, radius0, scale/ceil-rounded dimensions, partial update on/off.
- HIGH/ordinary Reveal/blur-off 무영향.
- Source style/gradient/image 갱신 중 sleep→reverse. 새Source→H→V 순서.
- async 이전 결과 폐기/atomic replacement, None, disconnect/reconnect, quality switch, context lifecycle.
- task/FBO 수·retained bytes 증가 없음. sleep frame에 H/V instruction/pass/bind/clear/draw가 없고, Output dependency는 별도로 남는지 구분.
- generic REFRESH_ONCE finished signal, refresh interval, activity 중 counter semantics의 Core 회귀.

## W. Risks

1. event/update round-trip에 의한 stale frame이 가장 큰 blocker.
2. before-start와 Source-only를 혼동하면 이전 blur가 다시 나타남.
3. Source-only라도 Output의 V sampler/texture validity 요건은 남음.
4. shared H 또는 mixed output을 놓치면 다른 page 내용을 읽음.
5. CPU/GPU 경계 연산 차이와 ECONOMY float cancellation을 portable threshold로 오인할 위험.
6. generic activity 추가로 refresh-once 통지와 idle/wake contract를 깨뜨릴 가능성.
7. setup/FBO allocation은 줄지 않아 target 최대 spike에 효과가 있다고 보장 못 함.
8. scene idle 또는 demo가 이미 None으로 해제한 상태에는 steady 절감이 없음.

## X. Recommended next action — 하나

**현재 ECONOMY/PERFORMANCE를 유지하고 UI-only H/V gating은 여기서 중단한다.**

계산상 entrance 절감 여지는 있지만 same-frame wake를 보장하지 못한 채 최적화를 넣지 않는다. generic Core capability는 향후 별도로 범위·contract·회귀 검증을 합의할 과제이며, 이번 작업에 이어 자동으로 구현하지 않는다.

## Y. Git state / artifacts

| Repository | HEAD | working tree |
|---|---|---|
| dali-ui / devel_blur_text | da82f963f7feaea56520e530fa1edc927e9b34d0 | clean, 변경 없음 |
| dali-core | f43e95be477ad301f84ecc772c753f9357821ecc | clean, 변경 없음 |
| dali-adaptor | dcadcfdc3e0d1abdce20767bee2858cb9e1851a4 | clean, 변경 없음 |

새 파일은 repository 밖의 이 report, calculate.py (로컬 자료: `calculate.py`), CALCULATIONS.json (로컬 자료: `CALCULATIONS.json`)뿐이다.
기존 보고서·측정 결과는 덮어쓰지 않았다. production code/shader 변경 없음. commit/amend/rebase/push 없음.
build/UTC/sample 실행/trace instrumentation/target profiling/성능 측정 없음. 실행한 것은 offline 수식·logical frame 수 계산뿐이다.
