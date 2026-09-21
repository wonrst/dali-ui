# Text::Reveal PERFORMANCE — V-only target diagnostic

## A. Executive Summary

**V-ONLY TARGET DIAGNOSTIC READY**

현재 **`devel_blur_text`의 로컬 변경을 빌드하면 PERFORMANCE는 자동으로 V-only**다.
환경변수·argument·hidden property·샘플 UI 추가 없이 평소처럼 build/install/run하면 된다.
기존 PERFORMANCE Late Smooth 구현과 HIGH는 보존했다.

이것은 production optimization이나 quality candidate가 아니다.
Source/H/V task/FBO는 그대로이고, final Output의 Source binding/read와 Late Smooth만 빠진다.
타겟 성능은 아직 측정하지 않았다. host 성능 benchmark도 하지 않았다.

## B. Baseline / Vogel cleanup

```text
repository: /home/bowonryuubuntu/tizen/dali/dali-ui
branch: devel_blur_text
HEAD: b54bb666dbc197c0d290fc9cffeba37bca042274

b54bb666 Batch text reveal blur sources
d95cc4a9 Add samples for testing text reveal blur
3c856fe6 Add blur to text reveal
69030aed Add custom Gaussian blur shader factory
```

시작 시 `text-reveal-runtime-blur.cpp` 하나에 Vogel12 +86줄이 unstaged였다.
전체 diff를 읽어 다른 변경이 없음을 확인한 후, 해당 hunk만 `apply_patch`로 제거했다.
직후 `git status --short`/`git diff`는 빈 결과였고 Vogel identifier 검색도 0건이었다.
그 상태에서 이번 V-only 변경만 추가했다.

- 제거 전 diff: input-working-tree.patch (로컬 자료: `input-working-tree.patch`). 필요시 복구할 수 있도록 보존.
- 이전 외부 Vogel report/viewer 및 V-only worktree는 그대로 유지.
- A-R/prefilter/adaptive Gaussian production 경로가 없는 b54 baseline.
- Source/Output batching, D2, memory admission, lifecycle 수정은 유지.
- HEAD/branch/core/adaptor 기록: BASELINE.txt (로컬 자료: `BASELINE.txt`).
  이 파일의 UI status는 이번 V-only 추가 후의 상태이며, 시작 diff는 위 별도 patch다.

## C. Exact change

변경 파일:
text-reveal-runtime-blur.cpp (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:63`)

```text
기존 PERFORMANCE: Output = V + full Source + Late Smooth
현재 local PoC:   Output = V only
```

private compile-time 상수:

```cpp
constexpr bool USE_PERFORMANCE_V_ONLY_POC = true;
```

false로 변경하고 재빌드하면 기존 PERFORMANCE가 복원된다.
사용자의 본 작업 트리는 계속 true로 유지했다. OFF 비교는 외부의 private source copy에서만 했다.

변경 내용은 다음뿐이다.

1. PERFORMANCE에만 `vOnlyShaders[batched][alphaOnly]` 별도 cache/handle 선택.
2. 기존 GLSL body를 그대로 두고, V-only에서는 `QUARTER_BLUR_SHARP_TAKEOVER`를 정의하지 않음.
   따라서 **전처리된 fragment에는 Source sampler/read, effective-radius, smootherstep, mix가 없다.**
   원래 C++ raw shader 문자열의 조건부 구현 자체는 그대로 보존한다.
3. scalar/batch Output 모두 slot1 Source texture와 sampler를 추가하지 않음. 실제 TextureSet은 slot0 V 하나.
4. 최초 PERFORMANCE Output 구성 시 프로세스당 diagnostic 로그 1회.

별도 shader 이름:

```text
TEXT_REVEAL_V_ONLY_DIAGNOSTIC_OUTPUT
TEXT_REVEAL_V_ONLY_DIAGNOSTIC_BATCH_OUTPUT
```

HIGH shader를 재사용하거나 공유 handle의 program을 변경하지 않는다.
shader source의 모든 `R"SHADER(...)SHADER"` body가 HEAD와 동일한 것도 자동 확인했다.
새 public API/Property/getenv/sample option은 없다.

## D. Preserved topology — 실제 데모 inventory

1280×720 / Sync / MSAA4 / Strong entrance radius24.
실제 `text-effect-demo.cpp`를 수정 없이 include한 외부 driver가 Intro→Skeleton→Cards,
Cards 재생성, Shutdown을 수행했다. 각 mode당 초기/재생성 2개 snapshot만 수집했다.

Card Label 12개, 실제 20줄, 9개 Label은 1페이지/3개는 2페이지: 총 15페이지.
아래 합계는 Card Label subtree이며 외부 Label/버튼/window 기본 task는 제외한다.

| 항목 | 기존 PERFORMANCE | V-only PERFORMANCE |
|---|---:|---:|
| Blur companions | 12 | 12 |
| Lines / Pages | 20 / 15 | 20 / 15 |
| Source tasks | 15 | 15 |
| H tasks | 15 | 15 |
| V tasks | 15 | 15 |
| FBO / attachment textures | 45 / 45 | 45 / 45 |
| Actors (Label/camera 포함) | 156 | 156 |
| Cameras (Actor 내수) | 45 | 45 |
| Renderers | 72 | 72 |
| Unique Geometries | 54 | 54 |
| 전체 참조 texture 수 | 94 | 94 |
| Applied constraints | 386 | 386 |
| H/V strength constraints | 40 | 40 |
| **Output strength constraints** | **20** | **20** |
| H/V progress mirrors | 30 | 30 |
| FBO attachment 논리 payload | 603,494 B | 603,494 B |
| 전체 참조 texture 논리 payload | 1,901,130 B | 1,901,130 B |
| **Output TextureSet / draw** | **2: V + Source** | **1: V** |

bytes는 관측한 texture 크기×BPP 합이다. RSS/VRAM benchmark가 아니다.
Source는 pass.buffers/H 입력에 계속 남으므로 texture 수/저장량은 줄지 않는다.

모든 Label/page의 Source/H/V 치수와 format이 동일했다. 예:

| Label | format | Source | H | V |
|---|---|---|---|---|
| Card1 day | A8 | 95×66 | 24×66 | 24×17 |
| Card1 title | A8 | 290×78 | 73×78 | 73×20 |
| Card2 gradient title | RGBA8 | 256×78 | 64×78 | 64×20 |
| Card1 subtitle | A8 | 380×212 | 95×212 | 95×53 |

property count, constraints, H/V shader hash, Output vertex hash는 모두 동일하다.
페이지 계획, D2 geometry, batching, capture, timing, allocation, refresh rate는 수정하지 않았다.

자료: inventory.json (로컬 자료: `inventory.json`), 검증 코드 (로컬 자료: `compare_inventory.py`), 요약 로그 (로컬 자료: `smoke-summary.log`).

## E. Dependency change / ordering

```text
기존:  Source → H → V → Output
         └───────────────↑

V-only: Source → H → V → Output
```

실제 handle 연결을 page별로 확인했다.

- H input slot0 == Source attachment.
- V input slot0 == H attachment.
- Output slot0 == V attachment.
- PERFORMANCE OFF에만 Output slot1 == Source attachment.
- ON/OFF 모두 S<H<V의 RenderTask order 유지, 세 작업 모두 REFRESH_ALWAYS.
- process당 15 pages×2 snapshots = 30개 page 확인. 모두 통과.

코드 경로도 확인했다.

- Runtime companion의 `GetOffScreenRenderTasks()`는 기존 pass.tasks의 Source/H/V 순서 그대로 반환한다.
- Core `RenderTaskList::ReorderTasks()`는 offscreen actor subtree와 위 task 목록을 사용한다.
  이번 Output TextureSet 변경으로 반환 목록/actor 구조는 바뀌지 않는다.
- GLES `Context`는 실제 binding별 `CheckNeedsSync()`를 호출한다.
- `TextureDependencyChecker::AddTextures()`는 FBO attachment writer를 등록하고,
  `CheckNeedsSync()`는 다른 context에서 읽는 texture를 동기화/추적한다.
  따라서 screen Output의 Source 직접 read/binding이 없어지고 그 직접 dependency 추적도 빠진다.
  Source→H, H→V, V→Output 연결은 유지된다.
- Source는 여전히 매 frame capture되고 H가 읽는다. Source lifetime이나 scratch 정책은 바꾸지 않았다.

이를 확인하기 위해 core/adaptor를 수정하거나 backend hook을 추가하지 않았다.
타겟에서 실제 sync 비용이 얼마나 줄지는 **아직 미측정**이다.

## F. A8 / RGBA / PER_LINE verification

- primary Card 12 Label은 모두 PER_LINE으로 실행했다.
- A8의 red-channel coverage → text RGB/alpha 복원, uColor/owner opacity,
  premultiplied blending은 기존 shader body 그대로다.
- RGBA gradient title은 V 안의 이미 blur된 premultiplied RGBA를 그대로 출력한다.
- batched V UV는 기존 `rectangle.xy + localUV*rectangle.zw` 식 그대로다.
  scalar는 기존 `vTexCoord`다. reduced dimensions/page crop/geometry를 변경하지 않았다.
- H/V의 line-local sampling/clamp도 그대로다. Output은 offset sampling을 추가하지 않는다.
- [V-only 실제 화면](perf-on/perf-strong.png)에서 A8 text와 Card2 gradient title을 확인했다.
- [기존 PERFORMANCE 화면](perf-off/perf-strong.png)도 보존했다.
  smoke capture이므로 정확히 같은 animation progress의 품질 비교 자료는 아니다.
- 데모의 WHOLE_TEXT 구성도 같은 switch를 적용받지만 별도 broad visual matrix는 하지 않았다.
- ImageSpan/emoji/async는 이번에 새로 폭넓게 검증하지 않았다. 해당 처리 코드는 변경하지 않았다.

## G. HIGH invariant

HIGH Strong도 compile-time constant OFF/ON 각 1 process에서 확인했다.

- Actor156 / Camera45 / Renderer72 / Geometry54 / Tasks45 / FBO45 / Texture94 모두 동일.
- constraints366, Output strength0, FBO 논리 payload1,377,693B 동일.
- **H/V 및 Output shader 이름·vertex/fragment hash까지 동일.**
- Output TextureSet 1개 그대로, diagnostic 로그 0회.
- 초기/재생성 snapshot과 정리 결과도 동일.

이는 HIGH 경로/생성물의 동등성 확인이다. 애니메이션 screenshot을 pixel-by-pixel
동기화한 별도의 품질 검증을 했다는 의미는 아니다.

## H. Host smoke / build

1. **현재 main repository foundation/components build 통과.**

   ```bash
   cmake --build build/tizen --target dali2-ui-foundation dali2-ui-components -j8
   ```

   빌드 로그 (로컬 자료: `build-main.log`). 설치된 library는 덮어쓰지 않았다.

2. 현재 소스 true / 상수만 false인 private 비교 library와 실제 sample executable 빌드 통과.
   build-diagnostic.py (로컬 자료: `build-diagnostic.py`), 빌드 로그 (로컬 자료: `build-diagnostic.log`).
   private 비교는 기존 b54 object에서 runtime object만 교체했으며,
   이전 worktree의 getenv 기반 diagnostic runtime object는 제외했다.

3. PERFORMANCE OFF/ON, HIGH OFF/ON 총4개 process의 초기 Cards/재생성/종료 통과.
   실제 sample class를 사용하고 앱 소스는 수정하지 않았다.

4. 매번 이전 자원을 WeakHandle로 확인했다. 재생성과 Shutdown 후:

   ```text
   Actor / Renderer / Task / FBO / FBO texture / Constraint: live=0
   최종 window task: 1
   실패: 0
   ```

   이것은 이번 짧은 event-side lifecycle smoke의 결과다.
   장시간 leak test/GPU driver allocation 회수 전체의 증명은 아니다.

5. 수정하지 않은 standalone demo도 **main build library**로 실행해 초기 화면을 확인하고,
   6초 후 SIGINT로 종료 요청했다. standalone.log (로컬 자료: `standalone.log`)에 정상 종료 lifecycle과
   V-only marker가 있다. `timeout` 도구 결과 124는 이 예정된 종료 요청 때문이다.

6. crash/assert/shader-link failure, 명백한 blank/order failure는 관찰하지 않았다.
   `git diff --check` 통과.

CPU/GPU/FPS/타겟 성능, full regression, sanitizer matrix는 수행하지 않았다.

### 진단 로그와 unused attribute 주의

PERFORMANCE ON process에서 다음 marker가 정확히 1회 나왔다.
HIGH와 OFF에서는 0회다.

```text
[TEXT-REVEAL-VONLY-POC] PERFORMANCE output uses V only; Source/H/V and constraints unchanged
```

**V-only에서는 기존 geometry의 `aRevealLineIndex`를 GPU compiler가 unused로 제거한다.**
geometry/vertex/constraint 유지 조건에 따라 이를 없애려고 구조를 바꾸지 않았다.
그 결과 host ON smoke 전체에서 기존 Core의 다음 경고가 59회 관찰됐다.

```text
Attribute not found in the shader: aRevealLineIndex
```

이것은 새 PoC 로그가 아니라 program/geometry pipeline cache 구성 시의 기존 경고다.
매 frame 강제로 출력하는 코드를 추가한 것은 아니다.
**첫 transition 비교에는 새 shader/cache와 이 로그 출력 비용이 섞일 수 있다.**
첫 실행과 반복 전환을 구분하고, 반복에서도 차이가 있는지 확인해야 한다.
같거나 느리다는 결과만으로 이 오염 가능성을 무시하고 병목을 단정하지 않는다.

## I. Expected visual difference

near-sharp에도 quarter V만 확대하므로 text가 흐릿하거나 pixelated하게 보일 수 있다.
capture에서도 확인했다. 이것은 이번 diagnostic에서 의도한 차이다.
원본 Source로 선명하게 복귀하는 Late Smooth가 없기 때문이다.
Reveal 제거 때 ordinary renderer로 돌아가며 선명도가 달라 보일 수도 있다.

이번 판단 대상은 이 품질이 아니라 **Skeleton→Cards 전환 stall과 layout animation의 가시성**이다.

## J. Target test instructions

**현재 로컬 미커밋 변경을 포함해** 평소처럼 DALi UI target build/install하고 앱을 실행한다.
commit/branch 이름만 다른 checkout으로 옮기면 unstaged PoC는 전달되지 않는 점에 주의한다.

```bash
./text-effect-demo.example
```

추가 env/argument/hidden toggle은 필요 없다.

1. 기존 UI에서 **PERFORMANCE + Strong**.
2. Intro→Skeleton→Results→Cards entrance/layout transition 확인.
3. **PERFORMANCE + Soft** 같은 전환 확인.
4. reference로 **HIGH + Soft** 확인.
5. 첫 실행 및 반복 전환을 구분해서 기록.

관찰 포인트:

- Card가 중간 위치부터 나타나지 않고 처음부터 올라오는가?
- Skeleton→Results 순간 stall/freeze가 줄었는가?
- 이미 사용하는 FPS 표시가 있다면 그 결과도 기록.
- 반복에서도 체감 차이가 일관적인가?

host에서 install 없이 현재 main build를 실행하려면:

```bash
bash /home/bowonryuubuntu/tizen/reveal-vonly-local.WmHrK8/run-demo.sh
```

이 wrapper는 local build library 위치만 지정한다. V-only 선택은 라이브러리 안의 상수이며
환경변수로 mode를 고르지 않는다.

## K. Target interpretation

| 결과 | 해석 / 다음 작업 |
|---|---|
| 명확한 개선 | Source read/binding/dependency + Late Smooth 묶음이 의미 있는 원인이라는 근거가 강해짐. 이후에만 lifetime/FBO 구조 검토 |
| 약간 개선 | 일부 비용은 있으나 주원인을 전부 설명하지 못함. 개선 폭 대비 구조 변경 리스크를 다시 판단 |
| 거의 차이 없음 | 이 비교에서 final Source composition이 주병목이라는 근거가 약함. 3→2 redesign 진행하지 않고 Output update-side 비용 등 다음 가설 검토 |

세부 요소 각각의 비용을 이번 한 비교로 분해할 수는 없다.
특히 첫 shader/cache/log 비용과 반복 구간 차이를 함께 보고 판단한다.

## L. Future FBO 3→2 — 미구현

가능성만 기록하면 A=Source/V, B=H로 Source(A)→H(B)→V(A)→Output을 생각할 수 있다.
하지만 Source/V의 **치수도 다르므로** 단순히 attachment를 바꾸면 끝나는 작업이 아니다.

후속에는 order/read-after-write/write-after-read, size/format, shared scratch,
frame N/N+1, first frame, async/stale publication, reconnect/rapid replacement,
ImageSpan ready, gradient update, multi-page, backend sync, destruction/reentry를 다시 검토해야 한다.
이번에는 ping-pong, FBO 3→2, source/task 제거, lifetime redesign을 **전혀 구현하지 않았다.**

## M. Final Git state

- HEAD/branch unchanged.
- Vogel source/helper/constants/env branch 완전 제거. 외부 기존 자료는 보존.
- UI 변경은 `text-reveal-runtime-blur.cpp` **1파일, +37/-10, unstaged**.
- 기존 HIGH/PERFORMANCE GLSL body 보존; sample/public API/core/adaptor 수정 없음.
- Stage 비어 있음. add/commit/amend/push 없음.
- reset/restore/stash/rebase 없음. 기존 다른 worktree 및 외부 진단 파일 변경 없음.
- `git diff --check` 통과.
