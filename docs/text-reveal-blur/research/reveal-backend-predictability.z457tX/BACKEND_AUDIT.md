# GLES dependency / RenderPass / FBO audit

2026-09-18. 분석만 수행했다. 아래 후보는 구현·성능 검증·상품화 승인된 변경이 아니다.
대상은 이전 TV 상세 계측 사본의 GLES 경로다. 주요 파일은
target Adaptor (로컬 자료: `../reveal-command-attribution.O3eOCq/adaptor/dali/internal/graphics/gles-impl/`),
target Core (로컬 자료: `../reveal-command-attribution.O3eOCq/core/`)에 보존되어 있다.
현재 PC Adaptor의 해당 GLES 소스와 target 기준의 차이는 계측 블록을 제외하고 없음을 대조했다.
선택한7파일의 Git base 비교는 BACKEND_BASE_EQUALITY.json (로컬 자료: `BACKEND_BASE_EQUALITY.json`)에 있다.

## 1. 실제 실행 순서

```text
CombinedUpdateRenderController::UpdateRenderThread
  ActivateResourceContext + Core::ResourceUpload
  각 window/scene에 대해:
    Core::RenderScene(scene, offscreen=true)
      RenderManager::RenderScene
        각 render instruction: BeginPass / Execute(secondary draws) / EndPass
        SubmitCommandBuffers(FLUSH)
          EglGraphicsController::Flush   [CPU command queue drain; glFlush와 다름]
            ProcessCreateQueues → ProcessTextureUpdateQueue → ProcessCommandQueues
              Source: ActivateResourceContext(R)
                BeginRenderPass: CheckFramebufferNeedsSync(S), Bind(S), clear
                Context::Flush: pipeline / uniforms / textures / dependency checks
                Draw → S
                EndRenderPass: UnbindCachedTextures, AddTextures(S), fence, glFlush,
                               invalidate unused depth/stencil, BindFramebuffer(0)
              H: 같은 R / 같은 흐름, S read → H write
              V: 같은 R / 같은 흐름, H read → V write
    Core::RenderScene(scene, offscreen=false)
      SubmitCommandBuffers(FLUSH)
        ActivateSurfaceContext(W_i)
        Output: CheckNeedsSync(V), CheckNeedsSync(S), bind/sample/draw
      PresentRenderTarget → ResolvePresentRenderTarget
        W_i::PostRender → eglSwapBuffers[WithDamage]
        MarkFramebufferTextureRead(W_i): backward fence
        CreateNativeTextureSync; 필요 시 glFlush(W_i)
  Controller::PostRender: DependencyChecker::Reset + SyncPool age/discard
```

위 그림은 **surfaceless/resource context를 지원하는 분기**다.
여러 window에서는 `R→W_1→R→W_2`가 될 수 있다. 모든 window의 offscreen을 한꺼번에
처리한 뒤 모든 onscreen을 처리한다고 가정하면 안 된다. RenderTask의 정렬 결과를 실행하는
것이지 backend가 이름을 보고 Source/H/V 순서를 만드는 것도 아니다.

읽은 위치:

- CombinedUpdateRenderController (로컬 자료: `../dali/dali-adaptor/dali/internal/adaptor/common/combined-update-render-controller.cpp:894`): resource upload, 각 scene offscreen/onscreen loop.
- RenderManager (로컬 자료: `../dali/dali-core/dali/internal/render/common/render-manager.cpp:1286`): instruction별 command buffer 작성, submit/Present.
- Controller::Flush (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/egl-graphics-controller.h:525`): resource와 command queue 처리.
- ProcessCommandBuffer (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/egl-graphics-controller.cpp:674`): BEGIN/END, native draw, readback, present dispatch.
- Context::Flush (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:489`), Begin/End (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:1092`): deferred GL 상태와 실제 draw.

## 2. Context ownership

| 경우 | 실제 context / 예외 |
|---|---|
| ordinary Source/H/V FBO | shared **resource context R**. 각각 다른 FBO이지 다른 context가 아님 |
| window output | 해당 surface의 **W_i**. EGL window context는 R과 share group을 구성하지만 동일 context는 아님 |
| texture/resource upload | R. queue가 남으면 Controller::Flush도 R을 활성화 |
| native image / external texture | R 또는 W_i에서 sample 가능; native producer/release fence 계약도 존재. 단순 ordinary FBO 규칙으로 대체 불가 |
| multi-window / across surfaces | ordinary FBO는 R, surface별 W_i로 전환. per-window 제출 경계 필요 |
| next frame | R의 다음 write와 이전 W_i의 sample이 겹치지 않도록 backward dependency 필요 |
| native draw / own-context callback | 일반 command chain의 외부 경계. 현재 context와 상태를 보존한다고 가정하지 않음 |
| resource context 미지원 fallback | InitializeGles는 surface에서 시작하며 ActivateResourceContext의 EGL 전환을 생략. ActivateSurfaceContext도 별도 Context 선택을 하지 않음. 위 distinct R/W 그림을 무조건 적용하지 않음 |

EglGraphics::ActivateResourceContext/ActivateSurfaceContext (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles/egl-graphics.cpp:68`),
EGL context 생성/공유 (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles/egl-implementation.cpp:286`)를 확인했다.
`MakeContextCurrent`는 동일 EGL context이면 이미 반환한다. H/V마다 무조건 비싼
`eglMakeCurrent`가 실행된다는 설명은 잘못이다.

정확한 근거 한계: 전달된 target trace에는 실제 EGLContext handle/extension capability가 없다.
bucket3/5는 offscreen/window **scope 분류**이지 EGLContext identity 기록이 아니다.
따라서 S/H/V의 동일 backend resource-context 분기와 cross-context 지원 경로는 소스로 확인했지만,
TV runtime의 실제 handle 값까지 측정했다고 주장하지 않는다. 첫 후보는
`IsResourceContextSupported()`가 확인되는 ordinary 경로에 한정하고 미지원/embedded 경로는
그대로 두는 것이 안전하다. 다음 PC 검증에서 실제 context identity도 확인해야 한다.

## 3. API contract와 edge별 판단

Ordinary framebuffer write 뒤 다른 FBO를 대상으로 texture sample하는 동일 context 명령은
순서 계약을 따른다. 동일 texture를 현재 draw의 attachment이자 sample로 쓰는 feedback loop는
별개이며 이 논리로 합법화되지 않는다. Shared texture를 다른 context에서 소비할 때는
producer 완료/consumer visibility가 필요하고 FBO container 자체는 shareable texture와 다르다.
[OpenGL ES 3.2 §2.1, §5.1/5.3, §9.3](https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf).

Fence는 생성한 context의 앞선 작업 완료를 표시한다. 다른 context의 wait만으로 아직
제출되지 않은 producer 명령을 진행시킬 수 없다. 특히 consumer에서 flush하는 것으로
producer flush를 대체해서는 안 된다.
[EGL_KHR_fence_sync](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_fence_sync.txt).
Server wait는 CPU completion wait와 다르지만 API/driver CPU 비용이 0임을 뜻하지 않는다.
[EGL_KHR_wait_sync](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_wait_sync.txt).

| Edge | Producer → consumer | explicit completion sync | 현재 구현 |
|---|---|---|---|
| S→H | R→R | 이 edge만을 위한 cross-context fence/wait는 불필요 | S end에서 fence 생성; H read는 context 비교 후 wait 생략 |
| H→V | R→R | 동일 | H end에서 fence 생성; V read는 wait 생략 |
| V→Output | R→W_i | producer 제출 + cross-context ordering 필요 | V fence에 server wait, backward read 기록 |
| S→Output (Late Smooth) | R→W_i | 위와 동일. **S fence 전체 삭제 불가** | S fence에 server wait, backward read 기록 |
| Output(N)→S/V overwrite(N+1) | W_i→R | 이전 read 완료 필요 | post-present fence + 다음 BeginPass의 backward check/wait |
| H write/read/overwrite | R→R | ordinary 동일 stream order | generic check는 남지만 cross-context wait 없음 |

중요: `AddTextures` 시점에는 뒤에서 다른 window/native 소비자가 생길지 아직 모른다.
“다음 H가 same-context”만으로 Source fence를 제거할 수 없다.

## 4. 함수별 감사

| 함수 | same-context / same frame | cross-context / same frame | previous frame / 수명 |
|---|---|---|---|
| `AddTextures` | FBO end마다 attachment index·producer·FBO 기록, **fence allocation 수행** | 같은 기록을 나중 cross-read에 사용 | native preparation release도 호출; 단순 삭제 불가 |
| `CheckNeedsSync` | lookup 후 context 동일하면 wait 및 backward read 기록 생략 | `syncing`/ID 검사 후 `Wait`; `cpu=true`는 `ClientWait` | 기본 forward index는 현재 publication bookkeeping; generic map reset과 함께 이해해야 함 |
| `CheckFramebufferNeedsSync` | attachment별 backward map 조회; 같은 context면 wait 생략 | 앞선 다른 surface read가 있으면 wait | **current/previous 양쪽 map** 검사. 한쪽만 생략 불가 |
| `MarkFramebufferTextureRead` | same-context read만 있으면 목록 비어 fence 없음 | surface read 목록을 dedup, 한 fence 공유 | 다음 R overwrite를 보호; `ResolvePresentRenderTarget`에서 flush |
| `SyncPool::AllocateSyncObject` | 호출된 경우 heap/map + GL/EGL fence 생성 | 동일 | 이름은 Pool이지만 signaled fence를 reset해서 재활용하는 방식이 아님 |
| `Wait` / `ClientWait` | 직접 필요 없음 | server wait / CPU completion wait 구분 | synced guard 및 ID 수명 관리 유지 |
| `FreeSyncObject` / age/discard | 즉시 모든 GL 객체 삭제와 동의어가 아님 | 올바른 context에서 discard되도록 보관 | Reset, texture discard, shutdown guard와 결합 |

코드: DependencyChecker (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-texture-dependency-checker.cpp:172`),
backward helper (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-texture-dependency-checker.cpp:82`),
SyncPool (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-sync-pool.cpp:235`).
`CheckNeedsSync`의 forward `syncing`은 dependency당 상태이지 window별 새 상태가 아니다.
따라서 향후 fence 공유 범위를 늘릴 때 multi-window 순서/수명 검증을 생략하면 안 된다.
이번에는 기존 동작을 바꾸거나 새로운 multi-window bug로 단정하지 않았다.

## 5. Flush 후보: 무엇을 합칠 수 있는가

`EndRenderPass`는 dependency publish 뒤 **매 FBO pass에서 glFlush**한다.
각 S/H/V 사이의 ordinary same-context edge만 보면 매번 제출할 필수 이유는 없다.
그러나 마지막 producer 제출 자체는 필요하다. window의 `eglSwapBuffers`가 다른 context인
R의 pending stream까지 안전하게 대신 flush한다고 가정하면 안 된다.

가장 작은 검토 후보는 **bounded command-drain 내 ordinary resource-context flush coalescing**:

1. pass별 fence 생성·attachment tracking·texture unbind·FBO state reset은 모두 유지한다.
2. 검증된 ordinary R chain에서만 pass-end flush를 pending으로 표시한다.
3. **R이 current인 동안** 다음 경계 전에 flush한다: surface/다른 context 전환,
   native/external release 또는 callback, explicit completion/readback 경로.
4. command drain 끝에도 반드시 flush한다. window draw가 없는 offscreen-only 실행도 진행해야 한다.
5. 불명확한 path는 기존 pass-end flush를 그대로 사용한다. nested secondary buffer 끝을
   무조건 boundary로 잡으면 page별 flush가 되므로 실제 outer drain 범위를 구분한다.
6. END_RENDERPASS에 RenderTracker sync가 추가되는 경로도 검사한다. fence 생성 이후의
   제출을 보장해야 하며, fence보다 먼저 한 flush를 completion 보장으로 잘못 사용하지 않는다.

수정 위치 제안(구현 안 함): `Context::EndRenderPass`의 flush 한 곳과
`EglGraphicsController::ProcessCommandBuffer/ProcessCommandQueues`의 명시적 경계.
상태는 controller/resource-context에 국한된 pending-submission이어야 한다.
텍스트 이름/Reveal 특수 조건을 backend에 넣지 않는다.

| 시나리오 | 기존 | 후보의 필수 조건 | 주요 위험 |
|---|---|---|---|
| same-context chain | pass별 flush | fence/tracking 유지, chain 마지막 flush | 제출 지연이 GPU overlap을 줄일 수 있음; 성능 이득 미확정 |
| cross-context read | producer pass flush | **전환 전 producer context flush** | 미제출 fence를 consumer가 기다려 stall |
| next-frame read/write | backward fence/check | backward path 변경 없음 | 이전 화면 read를 무시한 overwrite 금지 |
| multi-window | surface별 present와 sync | 각 R→W_i 경계마다 flush | 마지막 window 하나에서만 flush하면 안 됨 |
| native image | ResetPrepare/release 계약 | native pending이면 기존 경로 또는 release 전 flush | 외부 producer/consumer 진행 보장 |
| external texture/native callback | 별도 resource 계약 | opaque 경계는 coalescing 제외 | 외부 queue/own context 의존성 미관측 |
| scene disconnect | drain/discard/reset | pending chain drain, 기존 수명 그대로 | 완료되지 않은 handle/ID 조기 폐기 |
| task reorder/scratch reuse | 정렬된 commands 실행 | 실제 command 순서 기준, 알려진 task 이름 금지 | 같은 texture generation을 뒤바꾸면 안 됨 |
| FBO-only/REFRESH_ONCE | pass flush로 제출 | outer drain 종료 flush 필수 | 후속 window가 없을 때 영구 미제출 |
| shutdown/context loss | 기존 guard | 살아 있는 context에서만 처리, 원래 shutdown 계약 유지 | 파괴 후 GL 호출 금지 |

## 6. Structural savings: 측정치와 추론 구분

TV 상세 trace의 `RenderPass calls`는 begin+end 블록 수다. `Draw calls`는 timed draw descriptor
블록이다. **실제 glFlush/fence 함수를 세는 counter가 아니다.** 아래 환산은 ordinary,
nonempty, one-draw-per-pass 조건에서 코드와 trace를 연결한 *잠재 절감 상한*이다.

| 항목 | 기존 steady entrance | bounded flush 후보 | 감소 |
|---|---:|---:|---:|
| offscreen pass 수 추정 | 102.42/2 = 51.21/frame | 동일 | 0 |
| pass-end explicit glFlush | 약51.21/frame | eligible 연속 drain당1 | 단일 drain이면 약50.21/frame |
| forward fence allocation | 약51.21/frame | 동일 | **0** |
| DependencySync 계측 블록 | 171.70/frame | 동일 | **0** |
| RenderPass 계측 블록 | 102.42/frame | 동일 | **0** |
| FBO init, texture bytes, RenderTask | 기존과 동일 | 동일 | **0** |

exit의57 FBO initialization은 신규 자원 수이지 모든 exit frame의 pass 수 측정이 아니다.
그57개가 한 drain에서 각각 한 번 그려지면 해당 drain의 flush57→1 가능하다는 **조건부**
예시만 가능하다. post-present backward/native flush는 여기에 합치지 않는다.

후속 후보인 boundary fence publication은 forward fence를 chain당1개로 공유할 잠재력이 있다.
그러나 마지막-write generation, 여러 소비 context, native release, shared ID 수명, scratch
overwrite를 다시 설계해야 한다. 단순 allocation guard 수준이 아니므로 **현재 1순위가 아니다**.
H만 확실히 private이라는 일반 정보도 현재 backend API에 없으므로 이름 기반 생략은 거절한다.

**제거 count × 현재 category ms로 절감 시간/FPS를 계산하지 않는다.** flush CPU 비용이
DependencySync category가 아니라 부모 RenderPass exclusive에 남는 부분도 있고,
현재 detailed trace에는 clock/collector overhead가 포함된다.

## 7. Framebuffer initialization / recycling

Framebuffer::InitializeResource (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-graphics-framebuffer.cpp:118`):
`!mInitialized` guard → glGen → bind → color attachments → DrawBuffers → optional depth/stencil
texture attachment → bind0 → framebuffer state-cache creation/draw-state update.
별도 `glCheckFramebufferStatus`를 이 함수에서 호출하지 않는다. driver validation은 내부에
발생할 수 있지만 trace만으로 비용을 분리하지 않았다. depth/stencil renderbuffer의 lazy
`PrepareRenderBuffer`는 이후 Bind에서 호출되므로 초기화 비용과 전부 같다고 쓰지 않는다.

| 단계 | 단순 FBO name recycle | 같은 owner, 동일 attachment의 complete object reuse |
|---|---|---|
| Gen/Delete name | 회피 가능 | 회피 가능 |
| Bind/Unbind init | attachment를 바꾸면 대체로 필요 | init 부분은 회피 가능; 실제 draw Bind는 유지 |
| AttachTexture/depth/stencil | 새 texture이면 다시 필요; obsolete attachment detach도 필요 | identity/level/layer까지 같으면 회피 가능 |
| DrawBuffers | layout 동일하면 유지 가능; state-cache 일치 필요 | 유지 가능 |
| driver attachment validation | 회피 보장 없음 | 변경이 없으면 유리할 가능성; 측정 전 미확정 |
| cache registration/reset | 재사용 lifecycle에 맞는 처리 필요 | 일반 draw 상태는 계속 갱신 |
| texture storage allocation | **그대로** | texture까지 보유해야만 회피; retained memory와 trade-off |

Generic resource `TryRecycle` 기본 구현은 false이며 Framebuffer의 override가 없다.
old pointer를 CreateFramebuffer에 넘기는 것만으로 recycle되는 경로가 아니다.
FBO name만 재사용해도44.36ms가 사라진다고 할 수 없다. init batch에서
Gen/Attach/driver validation 중 무엇이 지배적인지 현재 계측으로는 알 수 없다.

| 위험 | bounded reuse에 필요한 조건 |
|---|---|
| GPU/command queue in flight | logical owner 해제만으로 재사용하지 않음; pending command references와 discard 경계 확인 |
| attachment lifetime/replacement | old texture strong ownership, detach 또는 정확한 identity 일치; raw pointer 재사용 금지 |
| format/dimensions/MSAA | color format, size, mip/layer, attachment layout, sample count, depth/stencil contract 모두 일치 |
| context | FBO container는 resource context 소유. shareable texture처럼 다른 context에서 이름 공유 금지 |
| async publication / rapid recreation | 이전 accepted result와 새 result overlap 고려; stale publication이 재활용 자원을 바꾸면 안 됨 |
| disconnect/shutdown | owner-bound 해제, context loss/driver shutdown 이후 GL 호출 금지 |
| retained memory | owner당 제한과 byte budget 필요; global unbounded pool 제외 |

등장radius24→퇴장48처럼 attachment 크기가 달라질 수 있는 실제 UX에서는 same-owner reuse
hit rate도 입증되지 않았다. **이 단계에서는 FBO pool 구현을 권하지 않는다.**
