# Additional command / create-queue attribution

## 범위와 실제 실행 경로

기존 Tizen10.1 ARMv7l 계측본을 확장한다. Shader/FBO/task/geometry/constraint/
blur timing/refresh/cache/reuse 동작은 변경하지 않는다. GPU timing도 추가하지 않는다.
원본 개발 트리와 이전 계측 사본은 보존하고 별도 detached worktree에서 작업했다.

`EglGraphicsController::ProcessCommandBuffer()`는 모든 GL 작업을 직접 실행하지 않는다.
`BIND_TEXTURES`, `BIND_UNIFORM_BUFFER`, `BIND_VERTEX_BUFFERS`, `BIND_PIPELINE`은
주로 Context에 상태를 보관한다. 실제 적용은 DRAW 계열의 `Context::Flush()`에 모인다.
따라서 switch의 각 case에 clock을 넣지 않고 아래 실제 handler/block만 측정한다.

```text
ProcessCommandBuffer (기존 duration)
  BEGIN_RENDERPASS → context 활성화 → BeginRenderPass → FBO bind + clear
  DRAW* → Context::Flush
    pipeline/program + blend/raster state
    ResolveUniformBuffers
    texture/sampler binding + dependency check
    vertex buffer/attribute/VAO 처리
    draw submission
    cached-state 정리
  END_RENDERPASS → unbind + dependency/fence + 기존 glFlush + FBO 정리
  EXECUTE_COMMAND_BUFFERS → 재귀 ProcessCommandBuffer
  PRESENT_RENDER_TARGET → 기존 swap + dependency/sync 정리
```

## Command categories

아래 파일 경로는 `adaptor/dali/internal/graphics/gles-impl/` 기준이다.

| 고정 tag | 실제 계측 범위 | count 의미 |
|---|---|---|
| `RYU - Cmd.RenderPass` | `egl-graphics-controller.cpp`의 BEGIN/END_RENDERPASS case. context 전환, bind/clear, 기존 glFlush/FBO 정리 포함 | begin/end handler 호출 수 합 |
| `RYU - Cmd.PipelineState` | `gles-context.cpp` Flush의 program/pipeline 변경·blend/raster 적용, 마지막 ClearState/pipeline 갱신 block | 실행한 block 횟수. draw당 보통 두 block |
| `RYU - Cmd.BufferUniform` | Flush의 uniform 적용 block과 vertex buffer/attribute block. `gles2/3-graphics-memory.cpp`의 LockRegion/Unlock도 별도로 같은 category | handler/block 호출 수. GL 호출 수 아님 |
| `RYU - Cmd.TextureSampler` | Flush의 sampler reflection/texture binding loop. native texture 준비 포함 | texture loop block 횟수. texture 수 아님 |
| `RYU - Cmd.DependencySync` | Checker의 AddTextures, CheckNeedsSync, CheckFramebufferNeedsSync. controller의 present 후 sync 정리와 PostRender의 reset/aging/discard | handler/block 호출 수. 실제 wait 횟수 아님 |
| `RYU - Cmd.Draw` | Flush의 draw descriptor switch와 관련 index/cache/attribute 처리 | draw submission block 횟수. 실제 GL draw가 생략될 수도 있음 |
| `RYU - Cmd.Other` | parser가 기존 offscreen command CPU union에서 측정 category exclusive CPU를 뺀 나머지 | 직접 계측하지 않으므로 count는 n/a |

Clear는 RenderPass 안에 포함한다. 별도의 GL별 timer를 만들지 않았다.
Other에는 미계측 dispatch/cache/state 처리와 계측 자체 비용도 남을 수 있다.
고비용 handler 구분이 목적이며 모든 command를 완전히 세분화하는 profiler는 아니다.

`CheckNeedsSync`는 TextureSampler의 자식, `CheckFramebufferNeedsSync`/`AddTextures`는
RenderPass의 자식이다. collector는 직접 자식의 inclusive 시간만 부모에서 빼서
**exclusive wall/CPU**를 기록한다. 부모/자식 inclusive 값을 더하면 중복이다.

### Buffer mapping과 sync의 경계

`Memory3::LockRegion`은 GPU buffer에 대해 기존 `glMapBufferRange`를 호출하고,
Unlock은 기존 unmap/flush 처리를 한다. GLES2는 임시 CPU 저장/BufferSubData 경로다.
Map 함수만 따로 측정하거나 GL 호출별 기록을 만들지 않았다.

이 메모리 경로는 ProcessCommandBuffer **밖**에서도 실행되므로, category record에
현재 command scope 안/밖을 별도로 저장한다. command 밖 BufferUniform 비용은
별도 참고 행으로 출력하며 offscreen command table에 더하지 않는다.

DependencySync는 기존 tracking·fence 생성/수명 관리·wait 호출을 감쌀 뿐,
새 wait/polling/glFinish/readback을 수행하지 않는다. CPU 실행/경과시간이며
GPU fence 완료 시간이나 GPU kernel 시간은 아니다.

## Create queues

실제 `ProcessCreateQueues()`의 큐는 아래 세 개뿐이다. 존재하지 않는
Pipeline/RenderPass/Sampler 생성 큐를 새 category로 가정하지 않았다.

| tag | 대상 | count / max 의미 |
|---|---|---|
| `RYU - Create.Texture` | mCreateTextureQueue | items = InitializeResource 호출 시도 수 |
| `RYU - Create.Buffer` | mCreateBufferQueue | 재활용/빠른 skip/실패 후 재시도도 포함 |
| `RYU - Create.Framebuffer` | mCreateFramebufferQueue | 실제 GL object 생성 수와 동일하다고 가정하지 않음 |

비어 있지 않은 queue 처리 한 번을 하나의 duration으로 측정한다.
`calls`는 그 batch 호출 수, `items`는 처리한 object 수다.
per-object 계측 대신, 기존 while loop의 InitializeResource 직전에 **clock 없는 counter**만 추가했다.
`max_wall_ns/max_cpu_ns`는 **가장 긴 queue batch 호출**이지 가장 느린 개별 resource가 아니다.
개별 resource 이름/주소/시간은 기록하지 않는다.

## Collector 확장 / 보관 방식

Core의 기존 private `reveal-attribution.inc`를 확장했다.
새 public API/ABI 또는 trace callback을 만들지 않고 `Trace::LogContext`를 그대로 사용한다.
Adaptor private helper는 고정 literal tag만 전달한다. 신규 ostringstream, tag 동적 생성,
hot-path 콘솔/file 출력, lock, atomic, GPU 동기화는 없다.

- 기존 `RYU - event` v2 duration 형식과 기록은 그대로 유지한다.
- 새 categories는 매 호출을 파일 record로 저장하지 않고 TLS의 작은 category table에 누적한다.
- 다음 frame 시작 직전에 이전 frame의 비어 있지 않은 category totals만 고정 버퍼에 저장한다.
- 정상 종료 때 마지막 합계까지 파일로 출력한다. 덮어쓰기하지 않는 `fopen(...,"wx")`도 그대로다.
- context=other/offscreen/window, commandDepth>0 여부로 bucket을 나눈다.
- bucket = context×2 + insideCommand. 주 비교의 offscreen command bucket은 **3**이다.
- recursive command buffers의 시간은 기존 parser의 interval union으로 중복 제거한다.
- calls/items, inclusive/exclusive CPU·wall, max single-block CPU·wall을 기록한다.
- capacity/drop/stack mismatch/open/clock/allocation 상태를 검증한다. 불완전 trace는 분석을 거부한다.

새 형식:

```text
RYU - categories version=1 enabled=1 records=... dropped=0 unmatched=0 open=0 allocation_failed=0 record_bytes=72 capacity=131072
RYU - category FRAME BUCKET CALLS ITEMS WALL_NS CPU_NS EXCLUSIVE_WALL_NS EXCLUSIVE_CPU_NS MAX_WALL_NS MAX_CPU_NS TAG
```

기존 duration buffer는 traced thread당10MiB다. 상세 aggregate buffer는131072×72bytes,
약9MiB이며 상세 계측이 켜진 render thread에서 할당한다. 이번 Sync의 Event/Render 두
스레드 구성은 기존20MiB + 상세9MiB 정도다. 이것은 **진단 도구 메모리**이며 blur 자체 사용량이 아니다.
최초 할당/초기화는 첫 render iteration timestamp 전에 수행한다.

## Overhead와 OFF

compile gate는 기존 `RYU_REVEAL_ATTRIBUTION && TRACE_ENABLED && __linux__`다.
runtime은 유효한 절대경로 `RYU_REVEAL_TRACE`와 `DALI_TRACE_COMMAND_CATEGORIES=1` 둘 다 필요하다.
새 flag가 없으면 category clock/counter 호출을 하지 않는다. helper의 enabled check는 남는다.

GL 함수마다 측정하지 않지만 draw handler가 많으면 category block clock도 늘어난다.
따라서 **동일 패키지로 OFF/ON target sanity가 필수**다. 로컬 synthetic test나 GBS 성공은
target 오버헤드가 작다는 증거가 아니다. 수치에 문제가 있으면 새로운 최적화가 아니라
계측 밀도부터 재검토해야 한다.

## Parser

`analyze.py`는 이전 `reveal-critical-path.CFOTks/analyse.py`를 그대로 호출해
duration/frame/timeline을 생성하고, 새 aggregate record와 phase 비교를 덧붙인다.

- 진입: CardsStart +1–3초.
- 퇴장: 각 새 CardsReady 뒤 첫 ExitStart → ExitFinished 중 wall이 가장 긴 iteration.
- 이는 자동 선정한 후보 spike다. create count/CPU가 실제로 집중됐는지 함께 확인하며, 가장 긴 frame이라는 이유만으로 생성 병목이라고 확정하지 않는다.
- 퇴장 후반: 같은 ExitStart +300ms → ExitFinished.
- source-only 폴더에 PID가 여러 개면 선택을 요구한다. 조용히 합치지 않는다.
- 상세 계측 누락/disabled/drop은 0ms로 해석하지 않고 실패한다.
- Category count는 block/handler 또는 resource 시도 수다. CPU/count를 driver 고정비용으로 일반화하지 않는다.
- Command table은 exclusive CPU + Other로 additive. Create table은 queue끼리만 additive이며 parent/command 표와 합산하지 않는다.
- GPU time, scheduler 상태, 실제 scanout FPS는 이번에도 unknown이다.

## 동작 보존 검증

수정한 Adaptor 기존 파일 6개에서 새 diagnostic macro/include 줄만 제거하면 이전
성공한 계측 소스와 byte-for-byte 동일함을 확인한다. 기존 렌더링 statement의
순서/분기/인자/값은 변경하지 않았다. 새 helper와 collector만 계측 상태를 보유한다.

로컬 검증은 standalone collector smoke 및 synthetic parser test다.
실제 target 앱 실행/측정, 성능 개선 검증, GPU 측정은 수행하지 않는다.
