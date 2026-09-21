# Bounded glFlush coalescing — PRIVATE PC PoC

2026-09-18. 원본 repository/설치 library 변경 없음. Target 작업 없음.

## A. Executive Verdict

**FLUSH COALESCING NO MEASURABLE BENEFIT**

**Target escalation: NO. 이 후보의 추가 구현/확대는 여기서 중단한다.**

정확히는 “모든 구간에서 이득이 0”이라는 뜻이 아니라, 요청한 **전체 PC 승격 조건을
통과할 만한 반복 가능한 순이득을 확인하지 못했다**는 판정이다.

- Actual glFlush: 진입 **54→2/frame**, 퇴장 **58→2/frame**. 구조적 절감은 확인했다.
- framebuffer: Reveal **96/96 byte-exact**, generic **6/6 byte-exact**.
- fence/wait, pass/draw, FBO/texture 구성은 유지됐다.
- 그러나 진입의 **전체 offscreen command drain CPU는 1.7896→1.7901 ms/frame**으로 동일했다.
- 퇴장 drain은 2.1159→2.0196 ms, 약4.55% 감소했지만 전체 render-thread CPU는 개선되지 않았다.
- 진입 p95/present 대기 지표도 비악화를 입증하지 못했다.

따라서 “flush 호출 96% 감소”를 “CPU/GPU 비용 96% 감소”로 해석하면 안 된다.
현재 결과는 안전하게 좁힌 generic 후보의 host screening 결과이지, TV에서
이 방식이 항상 무효라는 증명도, 모든 GLES 경로의 production correctness 보증도 아니다.

## B. Baseline / repositories

| Repository | HEAD | branch |
|---|---|---|
| Core | `8228720460a4910151f4eb4ad36976816b13a102` | `tizen_10.1` |
| Adaptor | `a3ab9b8db9637fda4c973b5d59c4d4b95a9d5286` | `tizen_10.1` |
| UI | `05087317cac8ea9600bba498f00ccf8086a79d3f` | `devel_blur_text` |

세 repo 모두 기존 trace/diagnostic dirty 상태를 보존했다. 파일 hash, HEAD, branch,
status, 전체 tracked diff hash는 FINAL_GIT_STATE.json (로컬 자료: `FINAL_GIT_STATE.json`)으로 확인했다.
기존 dirty UI를 production baseline이라고 오인하지 않도록, 앞선 계측의 **production
snapshot**을 외부 overlay에 복사해 양쪽에서 공유했다. Source-only는 이번 비교에 쓰지 않았다.

환경: Ubuntu 22.04 계열 / x86_64 / GTX 1650 / NVIDIA 595.91.07 / GLES 3.2,
GCC 11.4.0. Demo 1280×720, MSAA 4, 기본 Sync, Strong, PERFORMANCE.

Core 329 / Adaptor 380 / Foundation 471 objects를 기존 CMake source list로 구성했다.
프로젝트 `RelWithDebInfo`의 `-O2 -g -DNDEBUG`를 사용하되 기존 DEBUG_ENABLED/trace를
유지한 **optimized diagnostic build**다. 완전한 Release 수치라고 부르지 않는다.
기존 glyphy 한 파일의 optimized `-Werror=maybe-uninitialized` 때문에 그 object만
이전 no-O object를 양쪽에서 동일하게 공유했다. 설치된 Components도 양쪽이 공유했다.
build-commands.json (로컬 자료: `build-commands.json`), [재현/빌드 설명](REPRODUCTION.md).

## C. Existing flush / fence sequence

구현 전 [STATIC_AUDIT.md](STATIC_AUDIT.md)와 기존 [상세 backend audit](../reveal-backend-predictability.z457tX/BACKEND_AUDIT.md)를 재확인했다.

```text
Core RenderManager: instruction별 command buffer 작성
  → SubmitCommandBuffers(FLUSH)
  → Controller::Flush                 ← CPU queue drain, GL glFlush가 아님
      create / texture-update / mipmap queues
      ProcessCommandQueues
        ProcessCommandBuffer (secondary 포함)
          BeginRenderPass: 이전 surface read dependency → FBO bind/clear
          Context::Flush: draw state / texture binding / forward dependency
          draw
          EndRenderPass: unbind → AddTextures → producer fence → glFlush(R)
                         → invalidate / FBO cleanup
      discard queues
  → window context에서 Source/V consume
  → present → backward/native fence → 필요 시 glFlush(W)
```

주요 원본 위치:

- Context::BeginRenderPass (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:1092`), EndRenderPass (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-context.cpp:1190`).
- Controller::Flush (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/egl-graphics-controller.h:525`), ProcessCommandBuffer (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/egl-graphics-controller.cpp:674`).
- TextureDependencyChecker (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-texture-dependency-checker.cpp:219`), SyncPool allocation (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles-impl/gles-sync-pool.cpp:346`).
- resource/surface switching (로컬 자료: `../dali/dali-adaptor/dali/internal/graphics/gles/egl-graphics.cpp:68`), RenderTracker/readback 명령 생성 (로컬 자료: `../dali/dali-core/dali/internal/render/common/render-manager.cpp:1286`).

실제 GL flush 호출 위치를 다시 검색했다:

| 위치 | 이유 / context | 이번 변경 |
|---|---|---|
| `Context::EndRenderPass` | FBO producer/fence 제출, ordinary R | 적격 경로만 defer |
| `ResolvePresentRenderTarget` | surface backward/native fence 제출, W | 그대로 |
| `OffscreenRenderSurfaceEgl::PostRender` 216/234/240행 | 비동기 export 실패/성공 또는 callback 없는 경로 | 그대로 |
| X11 `PixmapRenderSurfaceX::PostRender` 235행 | pixmap 외부 사용 전 제출 | 그대로 |
| ubuntu-x11 `PixmapRenderSurfaceEcoreX::PostRender` 242행 | 대체 플랫폼의 같은 handoff | 그대로 |
| `GlImplementation::Flush` 538행 | 실제 `glFlush()` wrapper | 그대로, 외부 counter로 관찰 |

`Memory2/3::Flush`, controller Flush, XFlush는 위 API glFlush와 구별했다.

## D. Mandatory flush boundaries

Fence는 producer context stream에 들어가며, unsubmitted fence는 진행되지 않을 수 있다.
consumer context에서 기다리거나 flush하는 것을 producer 제출의 대체로 보지 않았다.
[Khronos EGL fence sync](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_fence_sync.txt),
[server wait](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_wait_sync.txt).

| 경계 | PoC 처리 |
|---|---|
| R→surface/다른 context | surface/unknown buffer 실행 **전**, R이 current일 때 pending 제출 |
| outer command drain 종료 | pending이면 반드시 flush. 다음 window 작업을 기다리지 않음 |
| FBO-only drain | 같은 종료 규칙. window 유무에 조건을 걸지 않음 |
| REFRESH_ONCE / RenderTracker / completion | syncObject 있는 buffer는 기존 즉시 flush |
| readback | READ_PIXELS buffer 전체를 제외, 기존 finish/readback 유지 |
| native/external / own-context callback | buffer 사전 검사에서 제외; inherited native texture도 사용 전에 제출+defer 종료 |
| explicit FLUSH / 알 수 없는 command | 기존 경로로 fallback |
| multi-window | surface context가 2개 이상이면 coalescing 비활성 |
| unsupported resource context | 기존 경로로 fallback |
| disconnect / discard / context destruction / shutdown | pending은 synchronous command drain 밖으로 나가지 않음. 기존 discard/shutdown 순서 유지 |

`glFlush`를 `glFinish`로 바꾸거나 새로운 CPU/GPU wait를 넣지 않았다.

## E. Candidate design

이 controller의 **유일한 resource Context**에만 두 bool을 둔다:
`mOrdinaryFlushEligible`, `mOrdinaryFlushPending`. process-global/static pending은 없다.

각 top-level command buffer와 그 secondary를 미리 읽는다. ordinary FBO command whitelist만
허용하고, recursive depth가 4를 넘거나 모르는 명령/외부 경계가 있으면 buffer 전체가 fallback한다.
queue 복사·heap allocation·명령 재정렬은 없다. 일반 FBO pass end의 GL flush만 pending으로
바꾸고, 부적격 buffer 진입 전 또는 outer drain 끝에서 한 번 제출한다.

```text
기존: Source/fence/flush → H/fence/flush → V/fence/flush
후보: Source/fence       → H/fence       → V/fence → drain-end flush
```

실제 stream에서 여러 Label/pass가 같은 drain에 들어오면 같이 묶인다.
Reveal/task 이름/radius 조건은 전혀 없다. fence 생성과 ownership은 그대로다.

## F. Context ownership matrix

| 경로 | ownership / 후보 적용 |
|---|---|
| ordinary offscreen | 실제 surfaceless R / 적격 buffer만 적용 |
| 화면 output | W / 미적용; R pending을 먼저 제출 |
| R→W1→R→W2 | window별 기존 흐름 유지, multi-window에서 미적용 |
| native/external / callback-owned context | 미적용 |
| resource context 미지원 | distinct R/W를 가정하지 않고 미적용 |
| 낯선 surface / export | 그 surface 경로의 기존 flush 유지 |

실제 GL interposer는 EGLContext/current surface를 함께 기록했다.
모든 후보 실행에서 mandatory flush의 context 불일치 **0**이었다.

## G. Correctness matrix

| 항목 | 결과 / 범위 |
|---|---|
| Reveal A8 / RGBA-gradient × WHOLE_TEXT / PER_LINE × radius16 / 48 | 8 configurations 모두 실제 runtime blur 활성 |
| 각 configuration의 forward/reverse 6 checkpoints | 총96 screen framebuffers byte-exact |
| 같은 progress의 forward vs reverse | 각 arm 48쌍도 exact |
| task/FBO/format/dimensions | configuration별 동일, inventory (로컬 자료: `CORRECTNESS.json`) |
| ordinary 3-FBO chain | non-black framebuffer exact |
| GaussianBlurEffect | framebuffer exact |
| REFRESH_ONCE / completion | 두 arm 모두 3개의 FinishedSignal 수신 |
| actual RenderTask readback | 65,536 bytes, 동일 hash |
| disconnect/reconnect / resize / task recreation | output exact, task 회수 확인 |
| FBO-only draw interval | resource flush 진행 확인. headless 범위 제한은 K 참고 |
| multi-window | 정상 종료, coalesced-boundary count 0: fallback 확인 |
| lifecycle | arm별200 cycles 정상 완료 |
| native/external / resource-context unsupported | host 실행 미검증, candidate 제외로 제한 |

정적 framebuffer exactness와 focused lifecycle 결과를 모든 동적 프레임/플랫폼의 무결성 보증으로
확대하지 않는다. 이 범위에서 candidate 고유 stale/flicker/hang은 확인되지 않았다.

## H. Exact code changes in private PoC

Adaptor diff (로컬 자료: `private-poc.patch`), 공통 collector tag diff (로컬 자료: `private-collector-tag.patch`).

- `egl-graphics-controller.h`: resource-context-local 상태/내부 helper 선언.
- `egl-graphics-controller.cpp`: ordinary buffer 검사, pending flush, outer queue 경계.
- `gles-context.cpp`: native binding safety guard와 pass-end flush 선택.
- 양쪽 공통: CommandDrain trace scope 및 collector 허용 목록의 **맨 뒤**에 tag 추가.
  기존 tag index 기반 category 분류는 이동시키지 않았다.

`ENABLE_BOUNDED_GL_FLUSH_COALESCING=0/1`은 private compile switch다.
Linux weak counter hook과 LD_PRELOAD meter도 진단 전용이며 production API가 아니다.
source/dependency/fence/shared resource 구현에는 diff가 없다.

## I. Flush count

최종 5회 warm-run 평균들의 median, 단위 calls/presented frame.

| Scenario / counter | Baseline | Candidate | Delta |
|---|---:|---:|---:|
| 진입 total glFlush | 54 | 2 | −96.30% |
| 진입 resource-context glFlush | 53 | 1 | −98.11% |
| 진입 coalesced mandatory-boundary flush | 0 | 1 | +1 |
| 퇴장 total glFlush | 58 | 2 | −96.55% |
| 퇴장 resource-context glFlush | 57 | 1 | −98.25% |
| 퇴장 coalesced mandatory-boundary flush | 0 | 1 | +1 |
| 진입 actual fence calls | 54 | 54 | 0 |
| 진입 actual server wait calls | 36.325 | 36.325 | 0 |
| 진입 offscreen draw blocks | 53 | 53 | 0 |
| 진입 offscreen RenderPass blocks(begin+end) | 106 | 106 | 0 |
| 진입 DependencySync blocks | 177.667 | 177.667 | 0 |
| 퇴장 생성 frame의 FBO initialization | 57 | 57 | 0 |
| 퇴장 생성 frame의 texture initialization | 97 | 97 | 0 |

mandatory counter는 **새 pending flush 실행 위치**의 count다. Baseline의 0은 producer
submission이 없다는 뜻이 아니라, 그 arm은 pass마다 이미 제출한다는 뜻이다.
post-present backward/native flush는 그대로이며 total에는 들어간다.
post-present call은 meter의 다음 swap row에 들어갈 수 있으므로 steady 구간을 주 비교로 사용했다.
API call 감소가 driver 내부 submit 횟수 감소와 일대일로 같다고 주장하지 않는다.

## J. Framebuffer / image correctness

quality-results.json (로컬 자료: `quality-results.json`): **96/96 exact**, 최대 byte 차이 0.

640×480 window, Label 550×300, mixed Latin/Korean 6줄, font24,
PERFORMANCE / PIXEL / Fade .1 / Stagger .25 / BlurDuration 1.
Soft screening radius16, Strong radius48. A8 white와 RGBA gradient를 실제 format으로 확인했다.
각각 p=0,.2,.5,.75,.9,1 및 역순. 각 이동은 실제 Animation을 사용한 뒤 동일한 settled
progress로 고정해서 캡처했다. p0과 p1이 다른 것도 확인해 빈 화면 false pass를 방지했다.

MSAA window의 **swap 전 실제 framebuffer readback**이며 PNG/사진 비교가 아니다.
GPU wait를 유발하는 이 readback은 correctness 전용이다. 성능 실행에서는 하지 않았다.
애니메이션 중간의 모든 프레임을 캡처한 결과는 아니다.

## K. Generic regression

generic-results.json (로컬 자료: `generic-results.json`): 다음6 output이 byte-exact이며 모두 non-black이다:
ordinary chain, REFRESH_ONCE, FBO-only 이후 복원, 재연결, resize/recreate, GaussianBlurEffect.

Readback은 실제 `KeepRenderResult` + FinishedSignal 뒤 `GetRenderResult`를 사용했다.
양쪽 모두 65,536 bytes, FNV hash `9587087419016490847`.

FBO-only 구간은 화면 output actor를 제거하고 default task도 offscreen으로 돌렸다.
안정된 약0.5초 동안 실제 draw는 프레임당3개의 FBO draw였고, resource flush는 **90→30회**였다.
단, 이 Window 기반 framework는 그 구간에도 present를30회 수행했다.
따라서 **“ordinary FBO-only command drain이 종료 시 제출됨”은 확인했지만,
native window 자체가 없는 완전한 headless 실행을 host-validated라고 하지 않는다.**
안전성은 다음 window pass와 관계없이 수행하는 outer-drain-end flush 규칙으로 제한했다.

Multi-window는 실제 두 window를 생성했다. 안정 구간의 coalesced-boundary count는 두 arm
모두0이었으며 정상 표시/종료했다. multi-window 성능 개선은 시도하지 않았다.

Lifecycle은 arm별200회: 100 Reveal blur Label + 100 ordinary FBO chain, connect/animate/
disconnect/reconnect/destroy/recreate. 회수 후 task count는 매번 기본1로 복귀했고,
ordinary FBO texture의 WeakHandle이 다음 cycle에 비어 있음을 검사했다.
heap은25~200 cycle에 지속 증가하지 않았다(B:68.23→66.74 MB, C:14.55→13.09 MB).
이 **서로 다른 절대 heap 값은 최적화 메모리 이득으로 쓰지 않는다**. driver/compiler/cache
상태가 섞이며, 이 후보는 FBO나 texture를 줄이지 않는다. host의 bounded lifecycle 관찰이지
GPU leak sanitizer 또는 모든 자원 leak 부재 증명은 아니다.

Fixture 작성 중 발견한 DSL/readback 호출 순서 오류의 무효 실행은 별도 보존했으며
통과 수에서 제외했다. 자세한 구분은 [REPRODUCTION.md](REPRODUCTION.md)에 있다.

## L. PC performance

최종 **5 independent processes/arm**, B C / C B / B C / C B / B C 순서.
각 process에서 동일 demo cycle을2회 수행하고 **두 번째만** 분석했다.
진입은 CardsStart+1~3초(약120 frames/run), 퇴장 tail은 ExitStart+300ms~ExitFinished
(6~7 frames/run), exit setup은 실제57 FBO가 생성된 frame을 별도로 매칭했다.

primary sample은 각 process의 구간 평균이다. 수백 frame을 독립 반복으로 취급하지 않는다.
아래는5개 run 평균의 median. p95 행은 **각 run의 frame p95를 구한 뒤 그5개의 median**이다.
전체 median/p25/p75/p95는 [METRICS.md](METRICS.md), 원자료는 RESULTS.json (로컬 자료: `RESULTS.json`).

| Metric (ms/frame) | Baseline | Candidate | Delta |
|---|---:|---:|---:|
| 진입 **offscreen command drain CPU** | 1.7896 | 1.7901 | +0.03% |
| 진입 command-buffer body CPU | 1.7146 | 1.7021 | −0.73% |
| 진입 RenderPass exclusive CPU | .6802 | .6652 | −2.21% |
| 진입 전체 offscreen CPU | 2.0492 | 2.0806 | +1.53% |
| 진입 render-thread CPU | 3.4491 | 3.5276 | +2.28% |
| 진입 iteration wall p95 | 14.4841 | 16.2852 | +12.43% |
| 진입 present API wall p95 | 12.0309 | 13.3141 | +10.67% |
| 진입 swap-to-swap interval p95 | 17.4741 | 17.5281 | +0.31% |
| 퇴장 **offscreen command drain CPU** | 2.1159 | 2.0196 | −4.55% |
| 퇴장 전체 offscreen CPU | 2.9242 | 2.5044 | −14.35% |
| 퇴장 render-thread CPU | 5.1327 | 5.3955 | +5.12% |
| 퇴장 iteration wall p95 | 17.2577 | 17.1657 | −0.53% |
| 퇴장 생성 frame iteration CPU | 6.6428 | 6.4857 | −2.36% |
| 퇴장 생성 frame wall | 16.2429 | 17.1540 | +5.61% |
| 퇴장 FBO create CPU | 2.1790 | 2.1959 | +0.77% |

핵심5-run 분포 (median / p25 / p75 / p95):

| Metric | Baseline | Candidate |
|---|---|---|
| 진입 drain CPU | 1.7896 / 1.7814 / 1.9421 / 2.0091 | 1.7901 / 1.7651 / 1.8647 / 1.9138 |
| 진입 thread CPU | 3.4491 / 3.4477 / 4.0568 / 4.3297 | 3.5276 / 3.5221 / 4.0397 / 4.3236 |
| 퇴장 drain CPU | 2.1159 / 2.0887 / 2.1393 / 2.3076 | 2.0196 / 2.0028 / 2.0762 / 2.0825 |
| 퇴장 thread CPU | 5.1327 / 4.7887 / 5.5278 / 5.6206 | 5.3955 / 5.2022 / 5.4990 / 5.5296 |

CommandDrain은 preflight와 마지막 deferred glFlush를 **모두 포함**한다.
RenderPass/body만 보면 옮겨진 flush 비용을 절감으로 오인할 수 있어 primary로 삼지 않았다.
thread CPU는 같은 render thread의 첫/마지막 frame thread-clock 차이를 frame 수로 나눈 값으로,
그 사이 inter-frame CPU도 포함한다. 전체 process/다른 thread CPU나 GPU 시간은 아니다.

진입 drain은 paired run 3/5에서만 감소했다. 퇴장 drain은5/5 감소했지만 thread CPU는
4/5에서 증가했고 구간 길이도 짧다. 따라서 일부 퇴장 개선을 전체 승격 근거로 쓰지 않았다.

기존 detailed trace와 cheap GL counters를 양쪽에 동일 적용했다. per-call 로그/GPU timer/
forced wait/readback은 performance run에 없다. collector dropped/unmatched/open/error와
meter overflow/context mismatch는 모두0이었다. 정확한 라이브러리는 각 capture의 maps 기록으로 확인했다.

## M. Frame-tail / overlap assessment

진입 iteration/present wall p95 median은 악화됐고 실제 swap cadence p95는 거의 같았다.
이는 화면 주기/FPS가 크게 악화됐다는 뜻과 같지 않다. swap wall에는 vsync/compositor/driver
대기가 섞이므로, 이 값만으로 “GPU overlap이 확실히 나빠졌다”고 원인을 단정하지도 않는다.

그러나 요청한 gate는 **CPU 개선 + thread CPU 비악화 + tail 비악화**다.
진입의 CPU 순이득이 없고 tail 비악화도 입증하지 못했으므로 이 gate는 실패다.
퇴장 setup도 CPU는 조금 감소했지만 wall이 증가했고 create 비용은 사실상 그대로다.
이 후보가 FBO initialization spike를 해결했다는 근거는 없다.

## N. Risks / unsupported paths

- command whitelist preflight를 한 번 더 수행하는 비용이 있다. allocation은 없지만 공짜가 아니다.
- deferred submission은 GPU 시작을 늦출 수 있다. 이 host 결과로 모든 driver의 overlap을 보증할 수 없다.
- native/external, own-context callback, unsupported resource context는 실행 검증하지 않았고 적용에서 제외했다.
- 완전한 headless/native-export 경로, context loss, platform-wide shutdown matrix는 host 미검증이다.
- lifecycle200회와 byte exact fixtures는 sanitizer/full UTC/global regression의 대체가 아니다.
- 계측을 켠5회 결과의 작은 percentage를 통계적으로 확정된 일반 성능 차이로 과장하지 않는다.

성능 gate가 실패했으므로 이를 통과시키기 위한 예외 확장·다른 coalescing 규칙·추가 tuning은 하지 않았다.

## O. PC→TV interpretation

**Direction only. No expected TV speedup percentage.**

이전 분석의 Update≈1.2×, RenderPass≈4~5×, Dependency≈6~7×,
Draw≈17~19×, FBO creation≈12~20×는 category마다 전혀 다른 배율이었다.
이전 A-R도 PC GPU 결과가 좋아도 TV에서는 악화됐다. 이 과거 숫자는 새 측정값이 아니며
이번 후보의 TV CPU/FPS를 계산하는 계수로 쓰지 않았다.

이번에는 extra pass/FBO/dependency 없이 API count를 줄이는 structural gate는 통과했지만,
PC CPU/tail gate를 통과하지 못했다. 이것으로 TV에서의 효과를 긍정 또는 부정으로 확정하지 않는다.

## P. Target escalation decision

**NO.** 후보를 production repo에 적용하거나 target용 package로 만들지 않는다.
private 사본과 결과만 보존한다. Original pass-local flush 동작은 그대로다.

## Q. If YES

해당 없음. 이번에는 target 재측정 요청/GBS build/RPM/install을 하지 않았다.
향후 별도 후보도 static audit → private correctness → optimized repeated CPU/count/tail
screening을 거쳐, 통과한 소수 후보만 target 검증으로 올리는 원칙을 유지한다.

## R. Explicitly not changed

Fence 생성/수명/free/count, dependency map, forward/backward sync와 previous/current frame
bookkeeping, RenderTracker, RenderTask/FBO/attachment/texture dimensions, source capture,
Source/H/V/Output topology, Gaussian shader/taps, geometry, constraints, Reveal timing,
page planner, Source/Output batching, blur quality, public API는 변경하지 않았다.

Fence coalescing, SyncObject 축소, FBO pooling/cache, feature-specific backend 조건은 없다.
GPU texture payload 절감도 없다. driver/cache 포함 실제 VRAM 절감 측정이라고 주장하지 않는다.

## S. Git state

FINAL_GIT_STATE.json (로컬 자료: `FINAL_GIT_STATE.json`): 원본3개 repo의 HEAD/branch/status/기존 dirty 파일
SHA-256/전체 diff SHA-256가 시작 상태와 동일하다. 각 repo `git diff --check`도 통과했다.

reset / restore / stash / rebase / commit / amend / push 없음.
원본 source 편집, install, target build/측정 없음. 이번 파일은 전부 이 외부 진단 디렉터리에만 있다.
