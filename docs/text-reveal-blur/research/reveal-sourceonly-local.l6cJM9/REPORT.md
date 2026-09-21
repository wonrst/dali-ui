# Text::Reveal PERFORMANCE — Source-only offscreen diagnostic

## A. Executive summary

**SOURCE-ONLY TARGET DIAGNOSTIC READY**

현재 로컬 PERFORMANCE는 자동으로 다음 경로를 사용한다.

```text
기존: Source FBO → H FBO → V FBO → Output(V)
이번: Source FBO ────────────────→ Output(Source)
```

H/V를 실제로 생성하지 않는다. ordinary Reveal foreground를 화면에 바로 그리는
0-pass 경로가 아니다. page마다 **full-resolution Source capture 1회**가 남는다.

같은 Cards workload에서 15 pages를 유지하면서:

- offscreen tasks / FBOs / cameras: **45 → 15**
- renderers: **72 → 42**
- constraints: **386 → 316**; Output strength 20개 유지
- Source dimensions / formats / bytes / Source shader는 동일
- HIGH는 기존 Gaussian 그대로

호스트는 빌드·자원·의존성·표시·lifecycle만 확인했다.
CPU/GPU 시간·FPS 성능 측정은 하지 않았다. **타겟 성능 결과는 아직 없다.**
Source-only는 blur가 없으므로 production 후보나 품질 후보가 아니다.

## B. Baseline / 1-tap cleanup

- Repository: `/home/bowonryuubuntu/tizen/dali/dali-ui`
- Branch: `devel_blur_text`
- HEAD: `035e79f352454bdd2fb2cc5f044ce579ed8af2f6` — `perf test 2`
- 시작 상태: **worktree clean**, HEAD와 origin/devel_blur_text 동일.
- V-only는 `511fef03 perf test`, 1-tap은 `035e79f3 perf test 2`에 이미 커밋되어 있었다.
- 초기 명령 결과: BASELINE.txt (로컬 자료: `BASELINE.txt`).

커밋을 되돌리거나 재작성하지 않았다. HEAD diff와 실제 코드를 읽은 뒤
1-tap 전용 flag/renderer entry/fragment/cache/name/logging을 **working-tree 편집으로 제거**했다.
renderer.cpp는 1-tap 이전 구현과 동일하다. header의 기존 namespace 주석 개선은 유지했다.

이전 [1-tap 보고서](../reveal-onetap-local.xAtm4Y/REPORT.md), 결과·probe·비교 라이브러리와
더 이전 실험 디렉터리는 수정/삭제하지 않았다. 이전 1-tap inventory를 재사용했으며
같은 baseline을 새로 측정하지 않았다.

## C. Exact Source-only topology

`USE_PERFORMANCE_SOURCE_ONLY_POC = true`인 PERFORMANCE에서:

1. 기존 page planner로 동일한 Source page를 정한다.
2. 기존 Source actor/renderer/shader/metadata/textures로 Source FBO를 capture한다.
3. 전용 Output shader가 그 Source attachment 하나를 읽어 화면에 합성한다.

`USE_PERFORMANCE_V_ONLY_POC = true`는 그대로 남아 있다.
새 Source-only flag를 false로 바꾸면 **기존 V-only + Gaussian H/V**로 돌아간다.
두 diagnostic flag를 모두 false로 바꾸면 기존 Source+V/Late Smooth 구현이 선택된다.
1-tap 코드는 요청대로 제거했으므로 새 flag를 false로 해도 1-tap으로 돌아가지는 않는다.

Output은 V texture를 무작정 Source로 교체한 것이 아니다.
별도 `TEXT_REVEAL_SOURCE_ONLY_DIAGNOSTIC[_BATCH]_OUTPUT` shader/cache를 사용한다.
실제 sampler는 `sSharpSource` 하나이고, V sampler/읽기/Late Smooth/blur ALU가 없다.

## D. Removed pipeline bundle

Source-only에서 생성하지 않는 것:

- H/V actors, renderers, GPU geometries
- H/V FrameBuffer, attachment Texture, RenderTask, builtin Camera
- 그 renderer의 strength constraints와 progress mirrors
- 해당 clear/write, pass transition, intermediate texture dependency

기존 Pass의 고정 배열은 보존하지만 H/V slot은 빈 handle이다.
OnSceneConnection은 Source task만 생성한다.
기존 GetOffScreenRenderTasks/teardown은 빈 slot을 건너뛰므로 lifecycle 구조를 추가하지 않았다.
H/V를 잠깐 생성했다가 숨기거나 제거하는 방식이 아니다.

기존 H/V 코드는 flag가 꺼진 경로와 HIGH를 위해 그대로 남겨 두었다.
Source planning 중의 CPU bounds/vertex 계산까지 별도로 최적화하지는 않았다.
기존 decoration composition용 별도 pass도 재설계하지 않았다. 이 Cards fixture에는
그 pass가 없으며, decoration이 필요한 다른 구성의 전체 task 수까지 P개라고 단정하지 않는다.

## E. Preserved behavior / sampling

변경하지 않은 것:

- page planner, occupancy, budget, tail/page 분할, memory admission
- authored radius16/24 및 Source halo/크기/포맷
- Source batching, foreground atlas, metadata, capture shader
- progress/Fade/Stagger/BlurDuration/sequence 수식
- Source까지의 gradient/color glyph/ImageSpan capture 코드
- Output placement/geometry와 owner color/opacity, premultiplied-alpha 설정

Source sampling은 기존 PERFORMANCE의 sharp branch와 같은 식이다.

```glsl
// Scalar / whole output
color = TEXTURE(sSharpSource, vTexCoord);

// Batched output
color = TEXTURE(sSharpSource,
                vRevealRectangle.xy + vTexCoord * vRevealRectangle.zw);
```

기존 sharp branch에는 별도 half-texel shift/clamp 연산이 없다.
이번에도 추가하지 않았다. 기존 padded page rect, output geometry 및
`BindTexture()`의 LINEAR / CLAMP_TO_EDGE sampler를 그대로 사용한다.

A8는 기존처럼 `uTextColorAnimatable`의 unpremultiplied RGB와 Source red coverage로
`vec4(rgb * coverage, coverage)`를 복원한다. RGBA는 premultiplied capture 값을 사용한다.
마지막 `color * uColor`와 blend 설정도 기존 sharp output과 같다.

기존 production/V-only GLSL body는 source에서 원문 그대로 보존했다.
Source-only fragment만 별도로 선택하므로 HIGH의 shader source도 바뀌지 않는다.

## F. Inventory comparison

창 1280×720, Text Effect Demo Cards 3개 내부 Label 12개, visible lines 20개.
actor/renderer/texture 개수는 **선택한 Label subtree 전체**이며 앱 전체 수치나
순수 blur 추가분만의 수치가 아니다. tasks/FBOs는 해당 subtree의 offscreen 자원이다.

| 항목 — Strong entrance radius24 | 이전 V-only + 1-tap | Source-only |
|---|---:|---:|
| Blur companions / lines | 12 / 20 | 12 / 20 |
| Pages | 15 | 15 |
| Source / H / V tasks | 15 / 15 / 15 | 15 / 0 / 0 |
| Total offscreen tasks | 45 | 15 |
| Source / H / V FBOs | 15 / 15 / 15 | 15 / 0 / 0 |
| Total FBOs / cameras | 45 / 45 | 15 / 15 |
| Actors | 156 | 96 |
| Renderers / geometries | 72 / 54 | 42 / 24 |
| Texture objects | 94 | 64 |
| Shader objects in this inventory | 7 | 6 |
| Constraints | 386 | 316 |
| Source logical bytes | 459,231 | 459,231 |
| H logical bytes | 115,061 | 0 |
| V logical bytes | 29,202 | 0 |
| Total blur FBO logical bytes | 603,494 | 459,231 |
| All bound texture logical bytes | 1,901,130 | 1,756,867 |
| Final Output texture | V | Source |

Constraint 내역:

| 분류 | 이전 1-tap | Source-only |
|---|---:|---:|
| H/V strength/state | 40 | 0 |
| H/V progress mirrors | 30 | 0 |
| Output strength | 20 | 20 |
| 기타 Source/Label subtree constraints | 296 | 296 |

Soft도 같은 15 pages와 동일한 object/constraint 변화다.

| FBO bytes — Soft entrance radius16 | 이전 1-tap | Source-only |
|---|---:|---:|
| Source | 337,759 | 337,759 |
| H | 84,637 | 0 |
| V | 21,566 | 0 |
| 합계 | 443,962 | 337,759 |

Source 크기 대표 예시 — **이전/현재 완전히 동일**:

| Label | Format | Strong24 Source | Soft16 Source |
|---|---|---:|---:|
| Card1 day | A8 | 95×66 | 79×50 |
| Card1 title | A8 | 290×78 | 274×62 |
| Card2 gradient title | RGBA8 | 256×78 | 240×62 |
| Card1 subtitle | A8 | 380×212 | 364×164 |

모든 15 pages의 정확한 값: inventory.json (로컬 자료: `inventory.json`).
논리 bytes는 width×height×format bytes 합계다. RSS/VRAM 실측, driver alignment,
window MSAA 또는 CPU allocation을 포함하지 않는다. 이번에는 그것들을 측정하지 않았다.

## G. Dependency / ordering

모든 Source-only page에서 다음을 실제 handle로 확인했다.

- Source task의 SourceActor가 기존 Source actor.
- Output TextureSet은 1개이고 **Source FBO attachment와 같은 Texture handle**.
- H/V actor/renderer/task/attachment 없음.
- Source task는 FBO target, window task는 onscreen target.
- REFRESH_ALWAYS, viewport, clear enable/color는 baseline의 Source task와 동일.
- GetOffScreenRenderTasks는 생성된 Source task만 반환하고 기존 reorder 요청을 유지.

주의: window task와 offscreen task의 `OrderIndex` 숫자를 직접 비교하면 안 된다.
window의 index는 여기서 INT_MIN이었다. DALi는 두 phase로 렌더링한다.

- CombinedUpdateRenderController (로컬 자료: `../dali/dali-adaptor/dali/internal/adaptor/common/combined-update-render-controller.cpp`):
  `RenderScene(..., true)`로 FBO를 먼저 처리한 뒤 `RenderScene(..., false, ...)`로 window 처리.
- RenderManager (로컬 자료: `../dali/dali-core/dali/internal/render/common/render-manager.cpp`):
  해당 phase와 framebuffer 유무가 다른 render instruction은 건너뛴다.

따라서 기존 frame의 offscreen→onscreen ordering 및 backend texture dependency
처리 경로를 그대로 따른다. Source를 REFRESH_ONCE로 캐싱하거나 임의 지연시키지 않았다.
이 확인은 실제 attachment/phase 구성과 core/adaptor 코드 근거이며 GPU timestamp 측정은 아니다.

외부 probe의 첫 실행은 이 cross-phase index 비교를 잘못하여 assertion이 실패했다.
제품 코드에는 문제가 없었고, probe만 위 계약에 맞게 고쳐 다시 확인했다.
초기 로그도 `perf-strong-initial-probe/`에 보존했다.

## H. A8 / RGBA / PER_LINE

- A8 text와 RGBA gradient title이 정상 색상·배치로 표시됨.
- PER_LINE metadata/progress를 통해 아직 진행 중인 줄은 부분적으로 드러남.
- 기존 1-tap의 downsampled 화면과 달리 full-resolution Source를 읽어 선명하게 표시됨.
- Source shader name 및 vertex/fragment hash는 baseline과 label별로 동일.
- A8/RGBA Source-only fragment를 실제 renderer에서 추출하고 전처리했다.
  sampler 1개 / TEXTURE 호출 1개, Gaussian/strength/Late Smooth 없음.

대표 화면: [Strong](perf-strong/perf-strong.png), [Soft](perf-soft/perf-soft.png).
실제 애니메이션 중 캡처한 smoke 화면이며 품질 점수나 동일 progress A/B 비교는 아니다.

ImageSpan/emoji/async capture 코드는 변경하지 않았지만 이번 focused smoke에서
별도의 전체 조합 회귀를 새로 수행한 것은 아니다.

## I. HIGH invariant

HIGH+Soft를 실행하고 이전 1-tap 실험의 HIGH+Soft 결과와 비교했다.

- 전체 Source/H/V/Output shader name 및 vertex/fragment hash 동일.
- 모든 target dimensions/formats/viewport/clear/refresh 동일.
- renderer properties, texture binding 개수, constraint target/rate/source count 동일.
- actors 156 / cameras 45 / renderers 72 / geometries 54.
- tasks/FBOs 45 / textures 94 / constraints 366.
- FBO logical bytes 1,013,277로 동일.
- Source-only marker 및 unused attribute 경고 없음.

HIGH는 계속 Source→H Gaussian→V Gaussian→Output(V)다.

## J. Host smoke / warnings

Ubuntu / GTX1650 / NVIDIA 595.91.07 / GLES / MSAA4 / 1280×720.

- foundation/components build PASS: build-main-final.log (로컬 자료: `build-main-final.log`).
- PERFORMANCE Strong/Soft, HIGH Soft의 **3개 실행**.
  이전 baseline matrix는 다시 실행하지 않고 기존 결과를 사용했다.
- 실제 demo source를 수정 없이 포함한 외부 inventory probe로
  Intro→Skeleton→Cards, Cards 재생성, Shutdown을 확인.
- 각 실행의 최초/재생성 inventory 동일.
- 추적한 event-side actors/renderers/tasks/FBOs/FBO textures/constraints weak handles는
  재생성·Shutdown 이후 모두 소멸. 종료 전 기본 window task 1개만 남음.
- 별도 unmodified demo executable도 현재 build library로 실행 후 정상 종료(0).
- `git diff --check` PASS.
- 최종 검증: verification.log (로컬 자료: `verification.log`), compare_inventory.py (로컬 자료: `compare_inventory.py`).

PERFORMANCE 각 실행 전체의 경고:

| 경고 | 이전 1-tap | Source-only |
|---|---:|---:|
| `aRevealLineIndex` | 59 | 59 |
| `aRevealInverseSize` | 118 | 0 |

Output vertex format과 strength constraints를 유지했기 때문에 기존 V-only의
unused line-index 경고는 남았다. 새로운 attribute 경고는 추가되지 않았다.
core pipeline-cache의 cache miss 시 발생하며 매 프레임 반복하는 로그가 아니다.
첫 snapshot 이후 구간에도 cache 생성에 따른 기존 경고 2개가 있어 완전한 0회로
보고하지 않는다. shader compile/link 오류 및 ERROR 로그는 세 최종 실행에서 없었다.
새 diagnostic marker는 PERFORMANCE 프로세스당 1회, HIGH에서는 0회다.

성능 benchmark/GPU timer/FPS 측정/full regression/sanitizer는 수행하지 않았다.
초기 probe ordering 검사와 보고서 파일 glob의 오탐은 외부 도구만 수정했다.
최종 raw 로그를 verification.log (로컬 자료: `verification.log`)로 다시 검증한 결과가 모두 PASS다.

## K. Target instructions

현재 working tree를 평소대로 build/install 후 `text-effect-demo.example`을 실행한다.
새 환경변수/argument/public property/sample toggle은 없다.

1. **PERFORMANCE + Strong**으로 Skeleton→Results 반복.
2. **PERFORMANCE + Soft**로 같은 전환 반복.
3. Reference: **Blur Effect**, **HIGH + Soft**, **Blur OFF / Reveal-only**.

확인할 것: FPS, stall, Card layout transition이 시작 위치부터 보이는지,
중간 위치에서 갑자기 나타나는 현상, 반복 시 일관성.
Source-only에서 **blur가 없는 것이 정상**이며 품질은 평가하지 않는다.

로컬 install 없이 현재 build를 확인하려면:

```bash
bash /home/bowonryuubuntu/tizen/reveal-sourceonly-local.l6cJM9/run-demo.sh
```

이 launcher는 local library 경로만 연결하며 diagnostic mode 선택 옵션이 아니다.

## L. Target result decision tree

사용자가 확인한 기존 타겟 결과는 이번 측정 결과와 구분한다.

| 비교점 | 해당 Cards offscreen passes | 현재 알려진 타겟 결과 |
|---|---:|---|
| Reveal-only | 0 | 사용자 확인: 복잡한 전환에서도 60 FPS |
| Source-only | 15 | **이번 타겟 확인 필요** |
| V-only + H/V 1-tap | 45 | 사용자 확인: 기존 대비 거의 비슷하게 느림 |

| Source-only 결과 | 해석 / 다음 조사 방향 |
|---|---|
| 매우 크게 개선, BlurEffect/60 FPS에 근접 | Gaussian ALU보다 H/V pipeline bundle의 비용이 크다는 강한 근거. pass 감소의 가치 조사 |
| 크게 개선하지만 60 FPS 미달 | H/V bundle 비용은 크지만 Source capture 및 남은 CPU/driver 비용도 존재할 가능성 |
| 조금만 개선 | Source capture/page multiplicity 또는 기타 잔여 비용 비중이 클 가능성 |
| 거의 차이 없음 | H/V 제거만으로 설명되지 않음. Source preparation, event/update, 다른 scheduling 재검토 |

미개선을 곧바로 “FBO가 원인이 아니다”로 확장하지 않는다.
개선되더라도 blur가 없으므로 곧바로 production 구현하지 않는다.
이번 타겟 결과 전에는 추가 알고리즘/2-FBO/배칭 최적화를 넣지 않는다.

## M. Strong vs Soft

둘 다 Source capture 1 pass/page지만 Source 영역은 다르다.
Strong Source payload 459,231 bytes, Soft 337,759 bytes를 그대로 유지했다.

- 차이가 계속 남으면 Source extent/capture pixel area/halo 및 setup 비용 가능성이 올라간다.
- 차이가 줄면 기존 차이에 H/V 영역·pipeline의 영향이 컸다는 근거가 된다.

샘플 Strong은 입장24/퇴장48, Soft는 입장16/퇴장32다.
위 inventory는 **Cards 입장 상태**이며 퇴장48/32의 peak 값은 아니다.

## N. Important caveat

**이번 비교는 FBO 개수만의 isolation이 아니다.**
H/V task/FBO/camera/renderer/geometry/clear/write/dependency와 renderer-side
constraints 70개가 함께 제거됐다. Output도 reduced V가 아니라 full Source를 읽는다.

따라서 결과는 “H/V offscreen pipeline bundle을 제거한 영향”이다.
task traversal, driver scheduling, bandwidth, CPU constraint 비용 각각의 기여를
이 실험 하나로 분해하거나 확정할 수 없다. Source-only가 빠르더라도 blur 품질을
유지하는 production 해결이 이미 있다는 뜻은 아니다.

## O. Git state / scope

- HEAD `035e79f3`, branch `devel_blur_text` 그대로.
- 변경 파일 3개: runtime blur, blur renderer cpp/h.
- **1-tap 제거 + Source-only 추가가 unstaged**.
- 기존 committed V-only 및 production Gaussian/Late Smooth 구현 보존.
- sample/public API/core/adaptor/common Gaussian factory 수정 없음.
- core/adaptor worktree clean 유지. 설치 라이브러리 교체 없음.
- 이전 report/results 보존. 새 진단 결과는 이 외부 폴더에 저장.
- git add/commit/amend/push/reset/restore/stash/rebase 없음.
