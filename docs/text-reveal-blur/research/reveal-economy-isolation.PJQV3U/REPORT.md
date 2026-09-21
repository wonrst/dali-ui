# Text::Reveal ECONOMY — Gaussian 격리 및 history 최종 정리

## A. Executive Verdict

**TARGET-READY** — 2026-09-21, PC 빌드·회귀 검증 기준.

- Generic Gaussian 구현은 pre-perf production base와 **byte-for-byte 동일**하다.
- 축소 V 커널은 Text Reveal 전용 `.cpp` 내부로 격리했다.
- `devel_blur_text`에서 지정한 실험/cleanup 커밋 4개를 실제로 제거했다.
- production base 이후에는 서명된 ECONOMY / sample 커밋 **2개만** 존재한다.
- 전체 UTC **3613/3613**, focused ASAN/LSAN **7/7**, HIGH/PERFORMANCE 화면 비교 **10/10 identical**.
- 이전 검증 결과를 재사용하여 PASS로 간주하지 않고, 변경 후 빌드와 검증을 다시 수행했다.

이번 TARGET-READY는 타겟 검증에 넘길 수 있는 소스 상태를 의미한다.
GBS/Windows 빌드·실제 TV 실행·성능 재측정은 이번에 수행하지 않았다.

## B. History before

실제 시작 상태는 BASELINE.json (로컬 자료: `BASELINE.json`)에 기록했다. UI는 clean이었다.

```text
b54bb666 Batch text reveal blur sources
  511fef03 perf test
  035e79f3 perf test 2
  05087317 perf test 3                         (origin/devel_blur_text)
  41faff78 Remove reveal performance diagnostics
  37534795 Add economy text reveal blur
  6eaab62a Add economy blur sample controls    (이전 local HEAD)
```

`git rev-parse 511fef03^` 결과로 다음 production base를 확인했다.

`b54bb666dbc197c0d290fc9cffeba37bca042274`

Core/Adaptor는 이전 보고서 당시와 달리 작업 시작 시 이미 `devel/master`였다.
이를 되돌리거나 업데이트하지 않았다.

- Core: `f43e95be477ad301f84ecc772c753f9357821ecc`, clean.
- Adaptor: `dcadcfdc3e0d1abdce20767bee2858cb9e1851a4`, clean.

확인한 Core/Adaptor 이력에는 제거 대상 UI perf 커밋이 없으며, 두 저장소는 rewrite하지 않았다.

## C. History after / 제거 증명

```text
b54bb666 Batch text reveal blur sources
  6c11f0ec Add economy text reveal blur
  da82f963 Add economy blur sample controls   (devel_blur_text HEAD)
```

`git rev-list --count b54bb666..HEAD` = **2**.
새 HEAD와 backup HEAD의 merge-base = **b54bb666**.

| 제거 대상 | 새 HEAD의 ancestor 여부 |
|---|---|
| `511fef03` perf test | NO |
| `035e79f3` perf test 2 | NO |
| `05087317` perf test 3 | NO |
| `41faff78` Remove reveal performance diagnostics | NO |

각 `git merge-base --is-ancestor <hash> HEAD`는 exit 1이었다.
현재 production branch 전체 subject에서 해당 네 제목을 검색한 결과도 **0**이다.
revert로 상쇄한 형태가 아니라 새 production ancestry에서 커밋 자체가 제외되었다.
remote-tracking ref와 backup에는 이전 이력이 남아 있다. 이는 삭제 대상 production branch와 구분한다.

원본 command 결과는 FINAL-PROOF.json (로컬 자료: `FINAL-PROOF.json`)에 있다.

## D. Backup / recovery point

rewrite 전에 생성한 local 복구 브랜치:

`backup/reveal-economy-before-history-cleanup-20260921`

가리키는 원래 HEAD:

`6eaab62afd71e4a991ec5a2001f8a55c3fed6809`

base에서 임시 reconstruction branch를 만들고 production 변경만 적용했다.
generic refactor를 가져오지 않고 helper를 격리한 뒤 검증·commit했다.
검증 후 `devel_blur_text`를 새 HEAD로 옮겼다. 임시 reconstruction branch만 삭제했고 backup은 유지한다.
`reset --hard`, stash, remote push는 사용하지 않았다.

## E. Gaussian isolation

| 파일 | 결과 |
|---|---|
| `internal/render-effects/gaussian-blur-algorithm.cpp` | **UNCHANGED** |
| `internal/render-effects/gaussian-blur-algorithm.h` | **UNCHANGED** |
| `internal/render-effects/gaussian-blur-kernel.h` | **NOT PRESENT** |

`git diff b54bb666..HEAD -- dali-ui-foundation/internal/render-effects/` 결과 전체가 EMPTY다.
**Generic Gaussian blur files changed = 0.**

base 및 최종 파일의 SHA-256도 각각 동일하다.

```text
cpp c189c34fdac2db55983f28a9631595ed188d5e55ee2663815d396101e023605d
h   574c42926a6e3b2a7cb4ddedf53fcb3274d7bb06073aa2717004858ba2e4933c
```

기존 static 계산 함수, cache, factory, UniformBlock 소유 모델은 원본 그대로다.
GaussianBlurAlgorithm에 ECONOMY용 API를 추가하지 않았다.

## F. ECONOMY private kernel location

새 private 구현:

`dali-ui-foundation/internal/visuals/text/text-reveal-blur-economy.cpp`

선언/기존 curve:

`dali-ui-foundation/internal/visuals/text/text-reveal-blur-economy.h`

좁은 진입점은 다음과 같다.

```cpp
bool CalculateVerticalKernel(uint32_t blurRadius,
                             float sourceToReducedScale,
                             std::vector<float>& weights,
                             std::vector<float>& offsets);
```

Gaussian weight/bell-width 함수는 `.cpp`의 anonymous namespace에 있다.
HIGH/PERFORMANCE용 CalculateGaussianConstants나 generic factory를 복제하지 않았다.
커널 계산 순서/상수/수렴 조건은 기존 검증된 식을 유지한다.

실제 source-height / ceil(source-height/4)를 사용하며, 검증된 짝수 kernel radius와
reduction ratio `(1,4]`만 받는다. radius/scale가 유효하지 않으면 vectors를 비우고 false를 반환한다.
기존 runtime은 양수 blur를 최소 4px 짝수 kernel로 보정하고 halo를 포함한 Source를 사용한다.
scale==1의 generic full-resolution 계수 보존 분기는 만들지 않았다.

최소 두 pair의 uniform-array padding은 유지한다. zero-weight padding offset만 앞 offset을
유지하도록 하여 전체 offsets도 monotonic하게 만들었다. zero weight이므로 filter 기여는 없다.

## G. Why local duplication was chosen

Text Reveal만 필요한 scaled V 계산을 위해 검증된 generic Gaussian 구현을 바꾸지 않기 위한
의도적인 격리 선택이다. 작은 weight/bell-width 계산만 feature 내부에 중복하고,
generic cache/factory/renderer 전체는 복제하지 않는다.

외부 검증 프로그램으로 이전 ECONOMY와 **1000개 입력 조합**을 비교했다.
모든 weight와 실제 사용되는 offset은 byte-identical이다.
17개의 zero-weight padding offset만 위의 monotonic 정리로 달라졌다.
결과 (로컬 자료: `kernel-equivalence.log`), 검증 프로그램 (로컬 자료: `kernel-equivalence.cpp`).
과거 helper 사본은 외부 검증 디렉터리에만 있으며 production source에는 없다.

## H. ECONOMY runtime behavior

이전 후보 대비 다음 파일은 내용 변화가 없다.

- public Reveal API/enum/docs
- runtime path 및 Source/H/V/Output 조립
- sample 두 앱의 quality controls와 기존 preset

유지되는 contract:

- Source full → H quarter X/Y → V quarter X/Y, 페이지당 3 offscreen stages.
- current Source + V의 두 texture 합성, Source retention.
- authored radius, gamma=1/native timing, 기존 Fade/Stagger/BlurDurationRatio.
- floor `min(R, clamp(R*.5,10,12))` 및 기존 BALANCED curve 계수.
- ECONOMY의 full padded H geometry, 기존 line-local clamp와 odd-size actual ratio.
- extra pass/FBO/texture/prefilter 없음.

`Create()`/`CreateBatch()`는 기존 Gaussian을 그대로 사용하고,
ECONOMY V의 `CreateReducedVertical()`만 private helper를 호출한다.
invalid helper 입력은 renderer 생성 실패로 반환하며 부분 kernel을 publish하지 않는다.

## I. HIGH / PERFORMANCE regression

이번에도 b54bb666의 generic Gaussian/runtime/renderer로 별도 reference library를 새로 구성하고,
동일한 현재 플랫폼 환경에서 actual progress `.20/.50/.75/.90/1.0`를 비교했다.
각 화면에는 WHOLE_TEXT/PER_LINE × A8/gradient/ImageSpan이 함께 포함된다.

| 품질 | 비교 화면 | 최대 channel 차이 |
|---|---:|---:|
| HIGH | 5/5 identical | 0 |
| PERFORMANCE | 5/5 identical | 0 |

visual-regression.json (로컬 자료: `visual-regression.json`), 실행 로그 (로컬 자료: `visual-regression.log`).
HIGH/PERFORMANCE shader/kernel/cache 동작을 추가 변경하지 않았다.

새 ECONOMY 5개 화면도 이전 검증의 ECONOMY reference와 모두 pixel-identical이었다.
이 결과는 해당 fixture에 대한 증거이며, 모든 입력/드라이버의 동등성을 보증하는 표현은 아니다.

## J. ECONOMY UTC

`UtcDaliTextRevealEconomyKernelP`는 이제 private `CalculateVerticalKernel()`만 호출한다.

- finite/nonnegative/normalized weights
- finite/nonnegative/ordered offsets
- supported even radius 및 잘못된 radius 거부
- odd dimensions와 non-integer actual scale
- source-space support 제한
- invalid/NaN/infinite/범위 밖 scale 거부 및 출력 초기화

기존 curve, dimensions/lifecycle, kernel ownership, ImageSpan 5개 focused UTC를 다시 실행해 **5/5** 통과했다.
일반 Gaussian source를 변경해 계수 identity test를 지원하는 구조는 없다.

## K. Lifecycle / ownership

`CreateReducedVertical()`의 private UniformBlock은 기존처럼 shader가 strong ownership으로 보유한다.
float-keyed global cache를 새로 만들지 않았다.

- 서로 다른 actual scale의 renderer가 독립된 계수를 보유.
- 하나를 해제해도 다른 shader/block은 유효.
- renderer 해제 후 private block 반환.
- quality 교체, async publication, reverse, scene disconnect/reconnect, None/파괴 검증.
- animation 실행 중 scene removal 검증.
- 155 기존 + 25 ECONOMY = 180-cycle ownership stress 통과.

stress 종료 시 mock textures **0→0**, buffers **5→5**, tracked weak objects **0**,
scene 기본 task만 **1**로 돌아왔다. 이는 GPU driver VRAM/RSS 실측 수치는 아니다.

## L. Sample behavior

두 정상 샘플을 다시 빌드하고 실제 GLES로 실행했다.

- Text Effect Demo: quality button/3키로 HIGH/PERFORMANCE/ECONOMY를 순환하고,
  0 재시작과 다음 화면 전환을 확인했다. Strong entrance24/exit48을 유지한다.
- text-reveal-example: 세 품질로 Preset 2를 실행했다. PER_LINE 입장24/blur-time1과
  WHOLE_TEXT 퇴장48, forward/reverse 시퀀스를 기존대로 사용한다.

R32 override, gamma1.5, 별도 timing API를 추가하지 않았다.
[ECONOMY Preset 화면](reveal-preset-economy.png), 샘플 로그 (로컬 자료: `reveal-preset-result.log`).

기존 benchmark-only `text-reveal-perf-example.cpp`와 target 제거는 sample commit에 포함했다.
앞 단계의 별도 cleanup commit을 되살리지 않는다. 삭제 파일은 backup branch에서 복구 가능하다.

## M. Builds / tests / sanitizer

모든 아래 결과는 **이번 helper 격리 및 clean reconstruction 후 재실행**한 결과다.

| 항목 | 결과 |
|---|---:|
| full foundation/components build/install | PASS |
| public foundation UTC | 2434/2434 |
| internal foundation UTC | 715/715 |
| components UTC | 464/464 |
| 전체 UTC | **3613/3613** |
| 별도 ECONOMY focused UTC | 5/5 |
| focused ASAN/LSAN | **7/7** |
| 두 일반 sample build / real GLES smoke | PASS |
| HIGH/PERFORMANCE 새 reference capture | **10/10 identical** |
| git diff --check | PASS |

전체 suite 결과 (로컬 자료: `utc/summary.xml`), focused 결과 (로컬 자료: `summary.xml`), sanitizer logs (로컬 자료: `asan`).

ASAN/LSAN은 새 economy `.cpp`, renderer, runtime, runtime UTC를 직접 다시 instrument한 focused build다.
private kernel/다른 scale ownership/renderer release/lifecycle stress를 포함하며 오류 보고가 없다.
나머지 UI/Core/Adaptor objects까지 전부 instrument한 full-platform sanitizer는 아니다.

Source batching 이후의 shader 문자열에 맞춘 기존 두 UTC 기대값 수정도 보존했다.
Core/Adaptor는 소스·HEAD를 변경하지 않아 이번에는 따로 rebuild/UTC하지 않았다.
새 성능 계측이나 benchmark는 수행하지 않았다.

## N. Final commits

| Commit | Title |
|---|---|
| `6c11f0ec51a0cbd884758a889daa30f9d9c48b41` | Add economy text reveal blur |
| `da82f963f7feaea56520e530fa1edc927e9b34d0` | Add economy blur sample controls |

두 커밋 모두 `Signed-off-by: Bowon Ryu <bowon.ryu@samsung.com>` 포함.
production base 이후 commit count는 정확히 2개다.

## O. Final git status

- UI: `devel_blur_text`, HEAD `da82f963`, **clean**.
- Core: 시작과 동일한 `f43e95be4`, **clean**.
- Adaptor: 시작과 동일한 `dcadcfdc3`, **clean**.
- generic Gaussian/helper residue 검증 통과.
- Reveal 관련 source의 RYU/POC/SOURCE_ONLY/ONE_TAP/V_ONLY/attribution 심볼 검색 **0 matches**.
- 제거 대상 commit subject 검색 **0**, 네 commit 모두 새 HEAD에서 **not reachable**.
- backup branch는 원본 복구용으로 유지.

## P. Push status

**NOT PUSHED.** remote-tracking `origin/devel_blur_text`는 기존 `05087317` 그대로다.
local history만 재구성했으며 원격 저장소는 변경하지 않았다.
