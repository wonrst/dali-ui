# Adaptive Gaussian — production-path target PoC

2026-09-15

## A. Initial Repository State

| Item | Value |
|---|---|
| Repository | `/home/bowonryuubuntu/tizen/dali/dali-ui` |
| Branch | `devel_blur_text` |
| Initial HEAD | `46d0ea182d77af03b8b75ab6e5480bf5d591624c` — Batch text reveal blur sources |
| Initial worktree / index | Clean |
| Backup | `backup/reveal-before-adaptive-poc-20260915` → same initial HEAD |

Initial state record and Phase A audit (로컬 자료: `/home/bowonryuubuntu/tizen/reveal-adaptive-integration-audit.BfX6fU/INITIAL_STATE.txt`). 사용자가 PARTIAL ELIGIBILITY를 확인한 뒤, sample을 유지하고 조건에 맞는 퇴장에만 적용하도록 승인했다. 이전 commit을 rewrite/amend/squash하지 않는다.

기존 exact native library를 source 변경 전에 repository 밖 `parent/`에 복사했다. SHA256은 `aaee5bd2599f297e88a9098f70b09a5b01ec67cfbd4daa0f21d6f46328811f43`이며 이전 PoC에서 기록한 baseline과 같다. Source revision도 같은 `46d0ea18`이다. Snapshot은 framebuffer A/B에만 사용했다.

## B. text-effect-demo Audit

Sample은 변경하지 않았다. [전체 39 Label slot / 상태 문구 / 예외 경로 표](../reveal-adaptive-integration-audit.BfX6fU/REPORT.md)는 이전 Phase A 감사 결과 그대로다.

일반적인 완료 후 퇴장의 공통 recipe:

```text
Blur ON + Strong + Performance
WHOLE_TEXT / Fade 1 / Stagger 0 / radius 48 / BlurTime 1
progress 1 → 0
Scene exit: .40s LINEAR
Status string replacement: .28s LINEAR
```

최종 대응표는 다음과 같다. `적용`은 아래 D의 scale/resource 조건까지 만족하는 publication을 뜻한다. TV에서 실제 FPS나 모든 Label의 frame을 관측했다는 의미는 아니다.

| Text / effect | Adaptive eligibility | 이유 |
|---|---|---|
| Intro: brand, hero, JEJU, conditions, Create My Trip, footnote | 완료 후 퇴장에 적용 | PIXEL, 공통 exit recipe; JEJU는 gradient |
| Generating: brand, trip, generating title, DAY 1/2/3 | 완료 후 퇴장에 적용 | PIXEL, 공통 exit recipe; title shimmer는 RGBA path |
| Generating status 4문구 | 완료 후 교체/퇴장에 적용 | None 정리 후 PIXEL 또는 유지된 CHARACTER; Fade1에서 동일 uniform fade |
| Results: brand, trip, heading, subheading, hint | 완료 후 퇴장에 적용 | PIXEL, 공통 exit recipe |
| Cards 1/2/3: 각각 day/title/places/subtitle | 완료 후 퇴장에 적용 | PIXEL, 공통 exit recipe; Card 2 title은 gradient |
| Card 2: BEST MATCH, View day plan → | 자체 entrance 완료 후 퇴장에 적용 | PIXEL, 공통 exit recipe |
| Detail: Back, brand, day, hero, route, scroll hint | 완료 후 퇴장에 적용 | PIXEL, 공통 exit recipe; hero는 gradient |
| Detail: Planning / Writing / Completed status | 완료 후 교체/퇴장에 적용 | PIXEL / CHARACTER / WORD 중 무엇이든 Fade1 whole-text 수식 동일 |
| 등장 중인 위 Label에서 퇴장 | 기존 exact | ConfigureExitReveal의 보호 분기가 PER_LINE/Fade0/radius24를 유지 |
| RESULTS_READY 직후 등장 중인 badge/action에서 상세 이동 | 기존 exact | 상태 이름과 달리 badge/action은 약 .66초 entrance가 별도로 남아 있음 |
| 로딩 문구, Markdown, immediate restart/removal | 해당 없음 | Reveal blur exit가 아니거나 traversal에서 제외 |
| HIGH / Medium·Soft / 다른 Fade·BlurTime / scale 변경 / ImageSpan | 기존 exact | 검증된 좁은 recipe 밖 |

원래 sample의 중단된 entrance 보호를 제거하지 않았다. 부모 scene opacity fade와 gradient animation도 그대로다. Unit/recipe를 공유하는 하나의 mutable instance나 앱 callback 동기화를 추가하지 않았다.

## C. Audit Verdict

**PARTIAL ELIGIBILITY — accepted by user for narrow integration.**

완료된 일반 exit는 Adaptive 수혜 대상이고, 미완료 entrance의 reverse 및 제외 경로는 exact를 유지한다. Renderer는 animation의 방향을 판별하지 않는다. 검증된 recipe의 progress domain에 따라 tier가 결정되므로 같은 recipe의 seek/forward도 같은 함수를 사용하며, 별도 animation clock이나 direction state는 없다.

## D. Adaptive Integration

변경 파일:

| File | Change |
|---|---|
| `internal/visuals/text/text-visual.cpp` | 검증된 publication snapshot에서 eligibility 결정 |
| `internal/visuals/text/text-reveal-runtime-blur.h` | 기본 false인 internal creation setting 하나 |
| `internal/visuals/text/text-reveal-runtime-blur.cpp` | Scalar H/V factory 호출에 해당 setting 전달; resolved radius/timing 재확인 |
| `internal/visuals/text/text-reveal-blur-renderer.h/.cpp` | Scalar-only private shader cache; 다른 radius 및 batch는 exact |
| `internal/graphics/shaders/text-reveal-blur.frag` | Adaptive compile-time branch; exact loop/UBO/endpoint 코드는 유지 |
| `internal/graphics/shaders/text-reveal-blur-adaptive.frag` | 검증된 immutable 2/12/16/20/24-pair functions |
| `internal/graphics/builtin-shader-extern-gen.h` | 새 shader fragment의 generated extern 선언 |
| `utc-Dali-TextRevealRuntimeBlur-internal.cpp` | Eligibility와 lifecycle UTC 2개 추가 |

상기 경로 중 production 파일은 `dali-ui-foundation/` 아래, UTC는 `automated-tests/src/dali-ui-foundation-internal/` 아래다. CMake/build 조건은 수정하지 않았다. 기존 generator를 다시 configure/build하여 새 fragment와 dependency가 포함되는 것을 확인했다.

### Exact eligibility

Publication 시 다음 모두 만족해야 한다:

- quality = PERFORMANCE, sequence = WHOLE_TEXT
- authored FadeDurationRatio = 1.0, BlurRadius = 48.0, BlurDurationRatio = 1.0
- UI scale = 1, effective scale = 1, render scale = 1
- prepared options가 perLine이 아니고 resolved radius = 48
- prepared fadeDuration = 1, blurDuration = 1
- prepared ImageSpan 목록이 비어 있음

Scalar runtime은 quarter path / display radius48 / blur duration1 / sequence start0도 재확인한다. Renderer factory에서 radius48 및 원래 `radius >> 1 == 24`를 확인한다. Batch factory는 adaptive option을 받지 않는다.

Authored 1/48은 setter에 정확히 저장되는 float 상수이므로 exact comparison을 사용했다. 47.9가 kernel rounding으로 48이 되더라도 제외됨을 UTC로 확인했다. .99 Fade/BlurTime도 제외한다. Broad epsilon이나 임의 quality 확대는 없다.

### Coefficients / tiers

기준: 검증된 외부 adaptive.inc (로컬 자료: `/home/bowonryuubuntu/tizen/reveal-adaptive-study.8A0C8e/adaptive.inc`). Float 값 74 pairs 모두 원본과 bit-identical인지 확인했다. 재계산/재튜닝 및 18/22-pair middle-range 후보 추가 없음.

| Progress | Positive pairs |
|---|---:|
| .86 ≤ p < 1 | 2 |
| .82 ≤ p < .86 | 12 |
| .60 < p < .82 | 20 |
| .20 < p ≤ .60 | 24 exact |
| 0 < p ≤ .20 | 16 |

원래 `p<=0` hidden return과 strength=0 copy return이 먼저 실행된다. Coherent uniform branch 하나이며 runtime forced override는 없다. 24도 외부 PoC와 동일하게 복사한 private constants다.

### Cache / isolation

기존 exact cache 및 `GaussianBlurAlgorithm`은 수정하지 않았다. 별도 thread-local Shader 하나를 H/V와 여러 Label이 공유한다. File-cache name도 `GaussianBlurShader_24_TextReveal_Adaptive`로 분리했다. Shared Gaussian UBO는 이 shader에 연결하지도, 수정하지도 않는다.

추가 per-frame CPU kernel 선택, Shader/Renderer 교체, constraint/property 생성 없음. Internal bool은 publication snapshot에만 저장되며 public API/환경변수/property로 노출하지 않는다.

## E. Unchanged Architecture

```text
Source → H → V → Output
         ↑   ↑
       eligible recipe의 sampling 함수만 변경
```

Source, Source batching, H/V FBO 크기/format, Output/Late Smooth, task refresh, actor/renderer topology, HIGH, PER_LINE, lifecycle, async publication/ownership, ImageSpan 구현, public API, sample 전부 유지했다.

공통 shader를 exact 모드로 전처리한 token stream은 parent와 동일하다. Scalar / PER_LINE 모두 비교했다. 기존 GaussianEffect 및 Source batching 관련 파일에는 diff가 없다.

Texture 저장량을 줄이는 패치는 아니다. 기존 texture/FBO/task 수는 동일하며, 추가 저장 항목은 shader code/cache와 작은 creation setting이다. Constants 원자료는 74×2 float = 592 bytes이나 compiler/program cache 전체 크기나 driver VRAM 증가량을 이 숫자로 표현하지 않는다.

## F. Focused Validation

### Builds

- Foundation/components: `cmake --build build/tizen --target dali2-ui-foundation dali2-ui-components -j6` 통과
- Internal UTC executable build 통과
- Builtin shader generator와 extern 생성 확인
- `git diff --check` 통과

로그: build.log (로컬 자료: `build.log`), utc-build-final.log (로컬 자료: `utc-build-final.log`).

### Targeted UTC: 14/14

추가한 2개:

1. `UtcDaliTextRevealRuntimeGaussianAdaptiveEligibilityP`
   - Sync/Async × plain/gradient × CHARACTER/WORD/LINE/PIXEL positive cases
   - HIGH, PER_LINE, Fade .99, radius40, BlurTime .99, authored radius47.9, render scale1.25, UI scale1.25 negative cases
   - 각 negative의 exact Shader handle identity와 positive 복원
   - local ready ImageSpan은 exact, H/V 및 여러 Label의 adaptive shader cache 공유
   - 일반 Gaussian shader cache와 다른 radius의 factory fallback 확인
2. `UtcDaliTextRevealRuntimeGaussianAdaptiveLifecycleP`
   - Sync/Async에서 enable, tier를 가로지르는 seek/reverse, actual animation, None, re-enable, disconnect/reconnect, direct destruction
   - offscreen tasks가 3개에서 baseline으로 회수되고 companion/Label weak handle이 만료됨
   - progress property index와 private shader identity 유지

기존 12개:

- RuntimeGaussianShaderIsolationP
- RuntimeGaussianFactoryValidationP
- RuntimeGaussianQualityPublicationP
- RuntimeGaussianPublicLifecycleP
- RuntimeGaussianAsyncLifecycleP
- RuntimeGaussianPublicImageTimingP
- RuntimeGaussianKernelSharingP
- SourceAtlasPackingP
- SourceAtlasGroupingP
- SourceAtlasLifetimeP
- SourceAtlasImageBoundaryP
- RuntimeGaussianScaleP

모두 `UtcDaliTextReveal` prefix의 기존 UTC다. 결과 로그는 이 디렉터리에 전체 함수 이름으로 보존했다. Test corpus/기존 assertion을 변경하지 않았다. Full regression/sanitizer/Windows/TV 실행은 이번 focused PoC에서 수행하지 않았다.

개발 중 새 UTC의 WeakHandle에 temporary handle을 넘긴 compile 오류를 lvalue handle로 수정했다. Production 로직 변경을 요구한 실패는 없었다. 최초 실행의 기존 gcov timestamp 충돌 경고는 coverage 산출물의 문제였으며 UTC exit status는 0이었다. 새 shader cache miss는 첫 native compile에서만 관찰했으며 shader compile/link 실패는 없었다.

### Native framebuffer: 24/24 ≤1 LSB

실제 GLES renderer로 parent library와 integrated candidate library를 각각 로드했다. 외부 앱에서 shader를 교체하지 않는다. Native log에서 parent `ADAPTIVE,0`, candidate `ADAPTIVE,1`을 확인했다.

| Fixture | Value |
|---|---|
| Label/window | 1920×1080, 기존 FHD Korean corpus, font24 |
| Source | 2020×1180 |
| H | 505×1180 |
| V | 505×295 |
| Formats | Plain A8 / color span + gradient RGBA |
| Progress | 1, .9, .86, .84, .82, .8, .7, .6, .5, .2, .1, 0 |
| Path | Native Source/H/V/Output; 2 rendered frames 후 readback |
| Host | GTX1650 / NVIDIA595.91.07 / GLES, window MSAA4 |

- **Final RGB max error ≤1 LSB: 24/24**
- `.5` exact24: A8/RGBA 모두 final RGBA byte-identical
- Endpoint 0/1: A8/RGBA 모두 byte-identical
- 전체 10/24 points byte-identical (`1, .9, .6, .5, 0`, 각 format)
- **Source 캡처: 24/24 byte-identical**
- Source/H/V 크기 및 format 동일, offscreen task/FBO 각각 3개

결과 JSON (로컬 자료: `spot-check-results.json`), 분석/coefficients 검증 (로컬 자료: `analyze.py`), 외부 native fixture (로컬 자료: `spot-check.cpp`). 이 검증은 통합 실수를 찾는 대표 checkpoint 비교이며 모든 font/좌표/타겟/animation frame에 대한 수학적 품질 보장은 아니다. CPU/GPU 성능은 새로 측정하지 않았다. 외부 readback 도구의 GPU instrumentation은 FPS 비교에 사용하지 않았다.

## G. Demo Build

`cmake --build samples/text --target text-effect-demo.example -j4` 통과. Sample build log (로컬 자료: `sample-build.log`).

Sample source와 버튼/텍스트/recipe/대기/animation duration은 변경 없음. 이 빌드는 실제 기존 sample target의 build 확인이다. 이번 native readback은 별도 외부 fixture이며, TV demo 전체를 실행해 관찰했다고 주장하지 않는다.

## H. Target Test Procedure

1. Parent `46d0ea182d77af03b8b75ab6e5480bf5d591624c`와 아래 새 commit을 같은 TV SDK/build/package 설정으로 빌드한다.
2. 각 빌드에서 `text-effect-demo.example`을 실행하고 **Blur ON / Strong / Performance**를 사용한다. UI/render scale은 1인 상태에서 비교한다.
3. 등장 완료 후 동일한 장면 이동을 반복한다. Results에서는 badge/action까지 끝난 뒤 이동하는 주 측정과, 즉시 이동하는 exact-fallback 확인을 구분한다.
4. 같은 display/target 상태에서 실행 순서를 교차해 평균 FPS, 제공되는 경우 min-like FPS, 눈에 띄는 frame drop을 기록한다. 사용 중인 target FPS 수집 도구를 사용하며 지원 시 `DALI_FPS_TRACKING=1`도 사용할 수 있다.
5. 특히 WHOLE_TEXT 퇴장의 `.2 < p <= .6`는 exact24다. 시작/끝만 개선되고 중간 frame drop은 남을 수 있다. 평균 개선을 최저 FPS 해결로 해석하지 않는다.
6. Grid / brightness pop / halo jump가 보이면 reject. Target 이득이 거의 없으면 이 commit만 drop/revert할 수 있다.

로컬 checkout을 건드리지 않는 별도 parent build tree가 필요하면:

```bash
git worktree add --detach ../dali-ui-adaptive-parent 46d0ea182d77af03b8b75ab6e5480bf5d591624c
```

이 명령은 안내이며 이번 작업에서 실행하지 않았다. 기존 SDK의 out-of-tree build 설정이 source path를 올바르게 가리키는지도 확인한다. Production env switch는 추가하지 않았다.

이전 외부 PoC의 GPU draw 합계 감소 A8 13.1% / RGBA 18.1%는 역사적 host 결과다. 이번 commit의 새로운 GPU 측정이나 TV FPS 개선율이 아니다.

## I. Commit

| Item | Value |
|---|---|
| New commit | `cd9567612780b6fb46ea0ef829acb1732a0c8d52` |
| Title | Prototype adaptive sampling for whole-text reveal blur |
| Parent | `46d0ea182d77af03b8b75ab6e5480bf5d591624c` |
| Sign-off | `Signed-off-by: Bowon Ryu <bowon.ryu@samsung.com>` |
| Backup | `backup/reveal-before-adaptive-poc-20260915` |

기존 HEAD 위에 독립 commit **1개**만 추가했다. Amend/rebase/squash/push는 하지 않았다. DALi UI worktree/index는 clean이며 `git show --check HEAD`도 통과했다. Commit hook의 formatting 변경은 공백 정렬과 namespace 종료 주석이며 로직 변경은 없다. 커밋 후 foundation/components 및 demo 재빌드도 통과했다: build-post-commit.log (로컬 자료: `build-post-commit.log`).

커밋된 소스 기준으로 internal UTC executable을 다시 빌드하고 위 targeted UTC **14/14를 재실행해 모두 exit 0**을 확인했다. UTC 재빌드 로그 (로컬 자료: `utc-build-post-commit.log`)와 `post-commit-UtcDaliTextReveal*.log`에 결과를 보존했다. 최종 `git diff HEAD^ --check`도 통과했다. Native framebuffer 결과는 동일 로직의 commit 전 결과이며, hook의 formatting만을 이유로 재측정하지 않았다.

별도 dali-adaptor worktree에 있던 `gles-texture-dependency-checker.cpp`의 기존 13-line 변경은 그대로 보존했고, 이 commit에는 포함되지 않는다.

## J. Revert

이 후보만 되돌릴 때, 보존할 작업을 정리한 clean worktree에서:

```bash
git revert cd9567612780b6fb46ea0ef829acb1732a0c8d52
```

기존 Source batching commit을 되돌릴 필요는 없다. 위 명령은 안내이며 실행하지 않았다.

## K. Final Verdict

**POC COMMIT READY FOR TARGET TEXT-EFFECT-DEMO TEST**

**PRODUCTION ADOPTION PENDING TARGET RESULT**

조건에 맞는 publication만 적용하는 PoC이며, 모든 PERFORMANCE blur로 일반화하지 않았다. 보고서/native 진단/캡처는 repository 밖에 두고 commit에는 포함하지 않는다.
