# Text::Reveal Blur — Lifecycle / Ownership Audit

2026-09-14 · devel_blur_text · HEAD `70fe035551deed33fed02078c9354edfcb06adc2`

## A. Ownership Model

코드 수정 전에 작성한 [OWNERSHIP.md](OWNERSHIP.md)에 전체 graph와 teardown authority를 기록했다.

- Label의 등록 visual이 TextVisual을, TextVisual의 attachment가 runtime companion을 소유한다. 연결 중에는 Label의 child 목록도 companion을 소유한다. Companion → Label은 **WeakHandle**이다.
- Companion은 Source/H/V/output Actor, Renderer, Geometry/VertexBuffer, TextureSet, FrameBuffer, pass task handle을 소유한다. 연결된 scene의 RenderTaskList도 task를 소유한다. Task의 source/camera는 observer reference이며, task는 FBO를 소유한다. Builtin camera는 source Actor에 붙는다.
- Renderer는 Geometry/VertexBuffer와 TextureSet을, FrameBuffer는 color Texture를 유지한다. 적용된 Constraint는 target Object가 소유하고 source/target을 관찰한다. Blur functor는 scalar timing/geometry 값만 보관하므로 Label로 돌아가는 strong cycle을 만들지 않는다.
- ImageSpan manager는 Label attachment가 소유한다. Entry/등록 visual은 ImageVisual을 유지하고, capture state는 원본 image Renderer를 일시적으로 유지한다. Client/retention link는 weak이다. Companion이 retention Actor와 capture proxy를 소유한다.
- `RemoveRuntimeRevealBlur()`가 authoritative companion handle을 먼저 비우고, `ReleaseForeground()`가 borrow를 한 번만 해제한다. `Unparent()` → `OnSceneDisconnection()`이 task-list에서 모든 pass/decoration task를 제거한다. 마지막 companion handle 해제 시 내부 Actor/Renderer/FBO/Texture/Geometry/Constraint도 은퇴한다.

## B. Lifecycle Audit Findings

검증 범위에서 추가 production correctness issue는 발견하지 못했다.

| 영역 | 결과 | 확인한 핵심 |
|---|---|---|
| Runtime blur | PASS | Prepare는 detached candidate를 생성한다. Activate 시 foreground borrow 및 scene 연결; released/borrowed flag로 중복 activate/restore 차단 |
| Output batching | PASS | line-local property/constraint는 renderer 소유. 1/6/64/65줄, 여러 page, mixed-format split에서 제거 확인 |
| Async stale publication | PASS | reveal/render/source/layout revision 및 mode/owner/renderer identity를 publication 전후 확인. 취소된 observer/task entry는 completion을 전달하지 않음 |
| Reentry | PASS | ObjectCreated, renderer property, resource-ready, async completion 경계의 None/config/text/style/mode/disconnect/shutdown 취소 검증 |
| Scene reconnect | PASS | disconnect 시 task 제거, reconnect 시 새 publication. 이전 companion/proxy/page의 중복 연결 없음 |
| Owner destruction | PASS | None을 먼저 호출하지 않는 plain text 및 ImageSpan의 sync/async 직접 파괴 검증 |
| ImageSpan capture | PASS | borrow/restore, READY/PREPARING/FAILED, descriptor/renderer 교체, occurrence 제거, owner 파괴 검증 |
| Decoration | PASS | sync/async × whole/per-line에서 shadow/underline/별도 composition task 생성·제거 |
| Format transition | PASS | A8/gradient/overlay/colored StyledText 및 HIGH/PERFORMANCE 교체 후 이전 자원 해제 |
| Shutdown | PASS | active/pending/construction 중 Stop; 추가 publication 차단, task 및 event-side 객체 해제에 idle 불필요 |

### ImageSpan ownership 상세

`CaptureUpdateScope`는 manager mutation 전에 기존 borrow를 해제하고 depth로 재귀 capture를 막는다. `UpdateGuard`는 manager나 Label이 아니라 작은 cancellation state만 공유한다. generation/alive 검사는 visual unregister/discard, property 설정 및 생성처럼 실제 callback이 가능한 경계 뒤에 있다.

`CaptureBlurRenderer()`는 source revision, occurrence, current renderer, client를 확인하고 owner draw list에서 제거한 원본을 retention에 연결한다. 실제 capture는 proxy가 수행한다. 반복 READY는 같은 borrow를 중복 추가하지 않는다. PREPARING/FAILED는 proxy를 제외하며, 정상 READY 복귀는 기존 page/FBO/geometry를 재사용한다.

`ReleaseBlurCapture()`는 state를 먼저 move-out하고 retention에서 원본을 제거한다. 현재 entry/renderer가 여전히 일치하고 owner가 살아 있을 때만 owner draw list로 복구한다. 교체된 old renderer는 복구하지 않는다. `ReleaseEntryVisual()`은 matching retained source를 unregister보다 먼저 제거한다.

Label 파괴에서는 `PrepareOwnerDestruction()`이 retention/constraints를 정리하고 `Self()`를 요구하는 일반 unregister를 피한다. 등록 visual의 최종 discard는 ViewDataImpl에 맡긴다. `RemoveInlineReplacementData()`는 old attachment identity를 먼저 제거하여 reentrant new attachment가 old cleanup에 지워지지 않게 한다.

### Async / callback 경계

TextVisual candidate를 attachment에 기록한 후 Activate하므로 activation 도중의 취소가 후보를 찾고 정리할 수 있다. Rollback은 여전히 자기 candidate/current publication일 때만 수행한다. 다른 callback이 설치한 foreground/texture를 old rollback이 덮지 않는다.

AsyncTextManager는 observer를 strong-own하지 않는다. RequestCancel/ObserverDestroyed는 waiting/running lookup을 제거한다. 이미 실행 중인 worker는 완료할 수 있지만 lookup에서 제거된 요청은 former observer에 publication하지 않는다. 메인 스레드가 worker completion을 기다리는 동작을 추가하지 않았다.

Late HIGH → PERFORMANCE는 quality-publication UTC의 coalescing과 공통 stale-revision rejection 경로를 함께 확인했다. Stale completion, None 후 완료, valid→invalid→valid, 업로드 실패 및 ImageSpan restore는 기존 targeted publication UTC를 재사용했다.

### `wholeAlphaShader` invariant

Scalar output 호출은 `alphaOnly || quarterBlur` 조건에서만 `CreateBlurOutput()`을 사용한다. 따라서 `!quarterSource && !batched`이면 alpha-only이다. HIGH scalar RGBA는 `CreatePlainOutput()`이다. 구조로 확인 가능한 조건이므로 추가 assert나 cache 일반화는 하지 않았다.

## C. Leak Analysis

- **Per-Label/per-publication:** 반복 종료 후 tracked Actor/Renderer/Geometry/VertexBuffer/Constraint/FrameBuffer 및 추적한 FBO color Texture weak handle이 만료되었다. 테스트 GL texture와 graphics-buffer 수가 warm baseline으로 복귀했다.
- **Shared caches:** thread_local Shader, cached quad Geometry, Gaussian kernel/UBO, VisualFactory 공유 자원은 warm-up 후 구분했다. Label 수에 비례하는 누수로 취급하지 않았다.
- **Framework/driver:** render/update queue의 deferred destruction, allocator RSS high-water, driver allocator cache는 event-side strong ownership과 다르다. RSS 감소나 물리 VRAM 반환을 요구하는 잘못된 assertion은 추가하지 않았다.
- 이 감사는 반복 가능한 소유권·자원 수와 sanitizer 결과를 근거로 한다. 실제 타겟 driver 내부 메모리까지 완전히 검증했다는 의미는 아니다.

## D. Fixes

**No production lifecycle change required.**

이번 감사에서는 production/API/shader/topology/성능 로직을 변경하지 않았다. 시작 시 존재했던 unstaged production cleanup도 그대로 보존했다.

`utc-Dali-TextRevealRuntimeBlur-internal.cpp`만 검증 보강:

1. `UtcDaliTextRevealRuntimeGaussianOwnershipStressP` 추가. A8 반복, format/quality 교체, 64/65줄 및 multi-page teardown의 weak ownership/count 검사.
2. 기존 `CheckImageBlurDirectDestruction()` 재사용. Geometry/VertexBuffer/Constraint 추적, 같은 application에서 async ImageSpan 100회 파괴, plain sync/async 직접 파괴, baseline 검사 추가. Shutdown은 기존처럼 idle을 돌리지 않는다.
3. 기존 `UtcDaliTextRevealRuntimeOutputBatchImageOrderP`의 nonzero strength-slot split output에 renderer/geometry weak-expiration 검사를 추가했다.

새 stress fixture의 초기 360px × 65줄은 기존 준비 단계의 보수적 256MiB 추정 한도에 걸려 일반 Reveal로 fallback했다. 64px 폭의 짧은 ASCII fixture로 바꿔 실제 blur 경로를 검증했다. production limit은 변경하지 않았다. 이는 테스트 fixture 수정이며 발견된 lifecycle bug가 아니다.

## E. Focused Tests

| 실제 실행 범위 | 결과 |
|---|---:|
| Runtime Blur internal 파일의 UTC | 79/79 PASS |
| TextVisual async publication UTC | 32/32 PASS |
| InlineReplacementManager ownership/binding/lifecycle UTC | 3/3 PASS |
| 합계 | **114/114 PASS** |

전량 UI regression 대신 ownership surface에 국한했다. 정확한 실행 이름/exit status는 normal-summary.tsv (로컬 자료: `./normal-summary.tsv`), 개별 로그는 `normal/`에 있다.

마지막으로 nonzero-slot weak 검증과 colored-format PERFORMANCE stress 배치를 다듬은 뒤, 해당 두 UTC는 각각 normal 및 ASan/LSan으로 다시 실행했다. `output-ownership-{normal,asan}.log`, `ownership-final-{normal,asan}.log`가 최종본 결과다. 같은 UTC 재실행을 별도의 unique test 수로 중복 계산하지 않았다.

## F. Stress

각 warm-up은 아래 반복 수에서 제외했다.

- PERFORMANCE/PER_LINE/A8/6줄 enable→progress 변경→None→파괴 **100회**.
- A8→gradient→A8→overlay→A8→colored StyledText→A8 및 quality/sequence 교체 **50회**. Colored format은 PERFORMANCE, 중간 A8은 HIGH로 교체한다. 같은 Label에서 2줄 및 65줄 변경도 포함한다.
- 1/2/64/65줄 및 multi-page **5회** 추가. 전체 tracked resource 해제 확인.
- Pending replacement request를 남긴 **async ImageSpan 직접 파괴 100회**, plain async 직접 파괴 1회.
- 기존 ImageSpan lifecycle stress **100회** 재사용: sync/async, WHOLE_TEXT/PER_LINE, READY/PREPARING/FAILED, disconnect/reconnect, None.
- 기존 sync/async shutdown 및 replacement ownership, 재진입 테스트 재사용. 원격 서버·sleep·random timing·새 Cartesian product 없음.

## G. Sanitizer

- **ASan: PASS. LSan: PASS.** Targeted unique UTC **34/34**, 추가 최종 targeted rerun 포함 오류 보고 없음.
- `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`; 별도 suppression을 추가하지 않았다.
- 별도 `asan-build`에서 foundation/internal UTC와 test utility 소스를 `-fsanitize=address -fno-omit-frame-pointer`로 빌드했다. libasan 연결을 확인했다.
- **한계:** 설치된 core/adaptor 공유 라이브러리 자체는 ASan으로 재빌드하지 않았다. Foundation 쪽 UAF/OOB 및 프로세스 leak 검사는 수행했지만, core/adaptor 내부 접근 전체에 대한 sanitizer 보증은 아니다.
- **UBSan: NOT RUN.** 전체 DALi UI sanitizer suite도 실행하지 않았다.
- 실행 목록: asan-summary.tsv (로컬 자료: `./asan-summary.tsv`); 개별 결과: `asan/`.

## H. Resource Baseline

정상 실행 중 teardown 후 idles/render를 처리한 결과:

| Case | GL textures | Graphics buffers | Scene tasks | 잔존 tracked weak objects |
|---|---:|---:|---:|---:|
| A8 100 + churn 50 + boundaries 5 | 0 → 0 | 5 → 5 | 1 → 1 | 0 |
| Async direct: plain 1 + ImageSpan 100 | 0 → 0 | 3 → 3 | 1 → 1 | 0 |
| Sync direct: plain 1 + ImageSpan 1 | 0 → 0 | 3 → 3 | 1 → 1 | 0 |
| 기존 ImageSpan lifecycle 100 | 0 → 0 | 5 → 5 | 1 → 1 | 0 |

`GetNumGeneratedTextures()`는 GenTextures에서 증가하고 DeleteTextures에서 감소하는 현재 테스트 GL 이름 수다. 누적 생성 횟수가 아니다. `mAllocatedBuffers`도 생성/파괴 시 insert/erase되는 현재 buffer 목록이다. 따라서 warm baseline 비교가 유효하다.

**Shutdown 해석:** Stop 이후 추가 Render/idle 없이 task=1 및 tracked event-side object=0을 확인한다. 이 시점의 mock graphics count는 texture 12 / buffer 15가 남아 있다. 아직 소비하지 않은 graphics teardown queue까지 강제로 flush하지 않았기 때문이다. 이것을 정상 실행 중의 baseline 복귀와 혼동하지 않았다. 해당 test process 종료 시 ASan/LSan도 통과했다.

## I. Performance Sanity

기존 probe를 재사용해 대표 한 case만 측정했다. 8 Labels × 550×300 / A8 / 6줄 / PIXEL / PER_LINE / stagger .25 / radius40 / PERFORMANCE / progress .5. 기존 환경과 동일한 X11 GLES/MSAA4 및 빌드 라이브러리 사용.

첫 생성 + warm 재생성 2회, .5초 settle 후 약 2초 측정. CPU/GPU는 각각 한 번, 별도 실행했다. 비교값은 이전 production cleanup 보고서의 수치를 재사용했다.

| 8 Labels 합계 | 이전 cleanup | 이번 smoke |
|---|---:|---:|
| Process CPU ms/s | 267.292 | **260.850** |
| GPU draw 합계 ms/frame | .631654 | **.627742** |
| Output draws/frame | 8 | **8** |
| 추가 RenderTasks | 24 | **24** |
| Actors / Renderers | 88 / 80 | **88 / 80** |
| Unique FBO color texture bytes | 3,038,728 | **3,038,728** |
| 전체 unique texture payload bytes | 10,497,088 | **10,497,088** |

Label당 Source/H/V는 **434×666 / 109×666 / 109×167**, A8로 동일하다. 전체 8개의 FBO payload는 2.898MiB, 전체 texture payload는 10.011MiB다. Width×height×format을 중복 제거한 저장량이며, 물리 driver VRAM 사용량은 아니다.

이번 GPU 분해: Source .240651 / H .121634 / V .114647 / Output .150810 ms/frame. Output 960 draws / 120 frames = 8. Dropped/disjoint query는 0. 종료 시 추가 task=0.

CPU는 전체 process CPU time/wall time이다. GPU는 clear/upload/swap/compositor를 제외한 elapsed-query draw 합계이며 전체 frame latency가 아니다. GPU 계측 run의 CPU 306.439ms/s는 계측 오버헤드가 있어 CPU 비교값으로 사용하지 않았다.

단회 smoke이므로 개선 주장이나 통계적 보증은 하지 않는다. **리소스 크기와 출력 구조가 동일하고 큰 성능 회귀는 관찰되지 않았다.** Production 소스도 감사 시작 시와 byte-identical한 diff를 유지한다.

## J. Remaining Gates

- 실제 Tizen target: driver/GPU 자원 수명, 장시간 scene/async/ImageSpan 교체, 메모리 안정성.
- 실제 GLES2 backend: 제한·fallback 및 shader/resource lifetime. Mock fallback UTC 통과와 실제 기기 검증은 구분한다.
- Final merge/build-server full integration regression, Windows build/UTC.
- 필요 시 core/adaptor까지 포함한 통합 sanitizer. 이번 감사가 플랫폼 전체를 대체하지 않는다.

## K. Working Tree

- **HEAD unchanged:** `70fe035551deed33fed02078c9354edfcb06adc2`.
- 시작 전 `git status`, diff, cached diff, HEAD를 `initial-*` 및 `initial.diff`에 기록했다.
- 기존 runtime production cpp 및 public reveal.h diff가 initial.diff와 동일함을 비교 확인했다. 이번 추가 변경은 internal UTC 파일 하나뿐이다.
- UI의 세 dirty 파일은 모두 unstaged. Index는 비어 있다. 기존 adaptor 변경도 건드리지 않았다.
- Commit/amend/add/reset/restore/checkout/stash/rebase 없음.
- `git diff --check`: PASS. 보고서·빌드·측정 로그는 UI repository 밖의 이 audit 디렉터리에 보관했다.

## L. Verdict

**LIFECYCLE / LEAK AUDIT PASS — 검증한 범위에서.**

추가 production lifecycle 변경 없이 target validation으로 진행할 수 있다. 반복 후 Label별 runtime 자원이 은퇴하고, late publication/reentry/shutdown이 이를 다시 살리거나 유지하는 문제는 이번 소스 감사·집중 UTC·ASan/LSan에서 발견하지 못했다. 남은 target/GLES2/integration gate는 유지한다.
