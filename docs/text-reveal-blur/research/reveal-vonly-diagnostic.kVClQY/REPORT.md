# PERFORMANCE V-only diagnostic PoC

## A. Baseline

**요청한 Output-only PoC를 구현했고, 작은 host 검증을 통과했다. 타겟 성능은 아직 측정하지 않았다.**

원본 repository는 변경하지 않았다. 시작 시 A-R/prefilter가 남아 있어 별도 worktree를 사용했다.

| 구분 | branch | HEAD | 상태 |
|---|---|---|---|
| 원본 | devel_blur_text | 38d7611b6a56270da6ff84a2a412e0a36d3a8cd9 | A-R 포함, clean 유지 |
| 진단 | diagnostic/reveal-performance-v-only-20260917 | b54bb666 — Batch text reveal blur sources | pre-A-R, PoC 1파일만 unstaged |

진단 소스 경로:

```text
/home/bowonryuubuntu/tizen/reveal-vonly-diagnostic.kVClQY/dali-ui
```

**PoC는 commit하지 않았다.** 위 diagnostic branch 이름만 다른 곳으로 옮겨서는 PoC 변경이 전달되지 않는다.
타겟 빌드에는 이 worktree의 미커밋 변경을 포함하거나, 정확한 pre-A-R baseline에
performance-v-only.patch (로컬 자료: `performance-v-only.patch`)를 적용해야 한다.
현재 원본 HEAD에 patch만 적용하면 A-R이 남아 있으므로 이번 비교 조건과 다르다.

## B. Exact diagnostic change

수정 파일은 text-reveal-runtime-blur.cpp (로컬 자료: `dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`) 하나다.
sample, 공통 Gaussian, core/adaptor, public API는 변경하지 않았다.

```text
baseline PERFORMANCE: Source → H → V → Output(V + Source + Late Smooth)
diagnostic V-only:    Source → H → V → Output(V)
```

- `DALI_REVEAL_PERFORMANCE_V_ONLY_POC=1`인 경우에만 활성화한다.
- 프로젝트의 `Dali::EnvironmentVariable::GetEnvironmentVariable()`을 사용한다.
  최초 PERFORMANCE Output 구성 시 static 값으로 한 번 읽으며 per-frame getenv는 없다.
- absent/`0`이면 기존 shader 문자열·shader 이름·cache 선택·binding 경로를 사용한다.
- HIGH에서는 진단 경로를 선택하지 않는다.
- 별도 `vOnlyShaders[batched][alphaOnly]` cache와 별도 shader 이름을 사용한다.
- 진단 fragment에는 `TEXT_REVEAL_V_ONLY_DIAGNOSTIC`를 정의하고,
  `QUARTER_BLUR_SHARP_TAKEOVER`는 정의하지 않는다.
  기존 conditional shader body에서 Source sampler/read와 effective-radius/quintic/mix가 모두 빠진다.
- Output TextureSet에 Source texture/sampler의 slot1을 추가하지 않는다. slot0의 V만 사용한다.
- Source texture 자체는 기존 pass.buffers/H input이 계속 소유한다. allocation/task/scratch 정책은 그대로다.
- A8 색 복원, RGBA premultiplied color, owner color/opacity는 기존 코드 그대로다.
- scalar WHOLE_TEXT와 batched PER_LINE Output 모두 같은 진단 선택을 지원한다.
  demo의 Skeleton 퇴장 등에서도 scalar 경로가 실행됐다.

**의도적으로 유지한 것:** authored-radius property, Output line-strength properties/constraints,
H/V strength/progress constraints, vertex shader와 Output geometry, Source capture, metadata,
async publication/lifecycle, refresh rate, 모든 Source/H/V 크기와 개수.
사용하지 않는 uniform/attribute를 GPU compiler가 제거하는 것은 허용된 차이이며,
event/update-side constraint는 제거하지 않았다.

## C. Invariants

이전 분석과 같은 1280×720, Sync, Strong entrance radius24, 초기3 Cards/12 Label 조건이다.
실제 줄20개, 9 Label은1페이지/3 Label은2페이지로 총15페이지다.
각 mode는 별도 process에서 **첫 구성1회 + 재구성1회**만 확인했다.

| 항목 | PERFORMANCE OFF | PERFORMANCE V-only |
|---|---:|---:|
| companions | 12 | 12 |
| lines / pages | 20 / 15 | 20 / 15 |
| offscreen RenderTasks | 45 | 45 |
| FBOs / attachment textures | 45 / 45 | 45 / 45 |
| Actors (camera/owning Label 포함) | 156 | 156 |
| Cameras (Actor 내수) | 45 | 45 |
| Renderers (ordinary 포함) | 72 | 72 |
| unique Geometries | 54 | 54 |
| unique bound textures 전체 | 94 | 94 |
| live applied constraints | 386 | 386 |
| H/V strength constraints | 40 | 40 |
| Output strength constraints | 20 | 20 |
| H/V progress mirrors | 30 | 30 |
| Source/H/V dimensions | 모든 Label/page별 동일 | 동일 |
| FBO attachment payload | 603,494B | 603,494B |
| 모든 bound texture payload | 1,901,130B | 1,901,130B |
| Output texture slots (draw당) | **2: V + Source** | **1: V** |

이 합계는 Card Label subtree만 센다. 외부5 Label, 하단 버튼, Skeleton, window 기본 task는 제외한다.
bytes는 관측한 dimension×BPP의 논리 저장량이며 RSS/실제 VRAM 측정이 아니다.
전체 texture 수가94로 같은 이유는 Output에서 뺀 Source가 H 입력과 pass.buffers에 그대로 남기 때문이다.

실제 치수 예:

| Label | format | Source | H | V |
|---|---|---|---|---|
| day ×3 | A8 | 95×66 | 24×66 | 24×17 |
| Card1 title | A8 | 290×78 | 73×78 | 73×20 |
| Card2 gradient title | RGBA | 256×78 | 64×78 | 64×20 |
| Card1 subtitle | A8 | 380×212 | 95×212 | 95×53 |

두 mode의 모든 실제 page 치수와 renderer property count를 자동 비교했다.
H/V shader 문자열 hash와 Output vertex 문자열 hash도 동일했다.
바뀐 것은 Output fragment variant/hash와 Output texture slot 수다.
결과 전체: inventory.json (로컬 자료: `inventory.json`), 검증 코드: compare_inventory.py (로컬 자료: `compare_inventory.py`).

### HIGH control

HIGH Strong도 switch0/1 각1 process에서 동일한 첫 구성/재구성을 확인했다.

- Actor156 / Camera45 / Renderer72 / Geometry54 / Task45 / FBO45 / Texture94 모두 동일.
- constraints366, FBO payload1,377,693B 동일.
- **H/V와 Output shader 이름·vertex/fragment hash까지 동일**, Output texture slots1 유지.
- HIGH에서는 V-only 진단 로그가 발생하지 않았다.

## D. Host smoke

환경: Ubuntu, GTX1650, NVIDIA595.91.07, GLES, MSAA4.

- 별도 worktree의 foundation/components **정상 빌드**. 기존 설치 library는 덮어쓰지 않았다.
- 원본 `text-effect-demo.cpp`를 수정 없이 별도 executable로 빌드했다.
- 동일 library/executable에서 환경변수로 OFF/ON 선택 가능하다.
- 실제 sample class를 include한 외부 diagnostic driver로 Intro→Skeleton→Cards,
  Cards 재구성과 Shutdown을 실행했다. sample scene/timing/문구/Reveal 설정은 바꾸지 않았다.
- standalone demo도 별도 실행해 시작/로딩/Intro를 확인했다. 짧은 smoke 종료는 timeout으로 요청했다.
- PERFORMANCE ON/OFF, HIGH ON/OFF 총4 process 모두 정상 종료했고 crash/assert/shader-link failure는 관찰하지 않았다.
- A8와 gradient RGBA가 함께 보이는 중간 화면을 캡처했다.
  [OFF](perf-strong-0.png), [V-only](perf-strong-1.png).
  애니메이션의 정밀한 동일 progress 비교용 이미지는 아니며, blank/깨짐 여부를 보는 smoke다.

### 작은 lifecycle 확인

첫 구성 자원은 WeakHandle로만 추적했다. 다음 Cards 재구성 후 이전 자원의 live 수:

```text
Actor 0 / Renderer 0 / RenderTask 0 / FBO 0 / FBO texture 0 / Constraint 0
```

두 번째 구성도 Shutdown 후 같은 결과였으며, 최종 window task는1개만 남았다.
PERFORMANCE OFF/ON, HIGH OFF/ON 모두 같다.
이는 이번2회 구성에서 event-side 자원 잔존이 없다는 확인이다.
**장시간 leak test나 driver/GPU allocation 회수 전체를 입증한 것은 아니다.**

### 예상된 unused-attribute 경고

V-only에서는 strength varying을 fragment가 읽지 않아 compiler가
`aRevealLineIndex`를 최적화로 제거한다. geometry는 요구사항대로 유지했으므로 Core에서 다음 경고가 나왔다.

```text
Attribute not found in the shader: aRevealLineIndex
```

Core의 `PipelineCache::GetPipelineCacheL0()`에서 **새 program/geometry cache 항목을 만드는 시점**의 경고다.
그 후 cache에 들어가므로 매 draw/frame에 무조건 반복하는 코드가 아니다.
이번 ON process 전체(Intro/Skeleton/첫 Cards/재구성)에서59회, OFF에서는0회였다.
추가하는 PoC 자체 로그는 최초1회뿐이다.

경고를 없애려고 geometry를 바꾸거나 가짜 strength 의존식을 추가하지 않았다.
타겟 측정 때 로그 출력이 과도하면 startup/setup 비교를 오염시킬 수 있으므로 이 차이도 확인해야 한다.
GPU compiler가 unused strength uniform을 지우는 차이도 이 진단의 일부다.
V-only가 같거나 느릴 때 이 경고/새 shader 첫 사용 비용을 무시하고 원인을 단정하지 않는다.

full regression, sanitizer, CPU/GPU/FPS 성능 matrix는 수행하지 않았다.
host에서 타겟 성능 향상을 추정하지 않는다.

## E. Expected quality difference

**V-only는 품질 후보가 아니다.** near-sharp에서도 quarter V를 확대해 쓰므로 흐릿하거나
작은 글자 획이 뭉개지는 것이 예상된다. 캡처에서도 확인됐다.
Reveal 제거/ordinary 복원 때 선명도가 바뀌어 보일 수도 있다.

이번 판단은 그 품질 차이가 아니라 **Card layout transition과 화면 전환이 가벼워지는지**다.
Source/H/V refresh나 strength/fade/stagger/animation은 그대로 유지한다.

## F. 실행 방법

### 로컬: 준비된 동일 executable/library로 비교

```bash
# baseline pre-A-R PERFORMANCE (default)
DALI_REVEAL_PERFORMANCE_V_ONLY_POC=0 bash /home/bowonryuubuntu/tizen/reveal-vonly-diagnostic.kVClQY/run-demo.sh

# diagnostic V-only
DALI_REVEAL_PERFORMANCE_V_ONLY_POC=1 bash /home/bowonryuubuntu/tizen/reveal-vonly-diagnostic.kVClQY/run-demo.sh
```

runner는 이 worktree의 build library만 우선 로드한다. 원본 설치 library/기본 실행 파일은 변경하지 않는다.
창 크기/MSAA 등 기존 환경변수는 그대로 사용한다.
환경변수는 최초 construction 시 고정하므로 **OFF/ON 전환 때 앱을 종료하고 재실행**한다.

### 타겟

`b54bb666 + performance-v-only.patch`를 타겟용으로 빌드/배포한 **동일 app/library**에서:

```bash
DALI_REVEAL_PERFORMANCE_V_ONLY_POC=0 ./text-effect-demo.example
DALI_REVEAL_PERFORMANCE_V_ONLY_POC=1 ./text-effect-demo.example
```

ON에서는 처음 PERFORMANCE Output을 만들 때 다음 로그가1회 나온다.

```text
[REVEAL-VONLY] enabled=1 quality=PERFORMANCE outputTextures=1
```

1. 우선 **Blur ON / Strong / Performance**로 양쪽을 비교한다.
2. 첫 화면→Skeleton loading→결과 화면→3 Cards 진입을 동일하게 진행한다.
3. Card root slide가 처음부터 보이는지/중간부터 보이는지, 전환 체감, 기존 FPS 표시를 본다.
4. 이후 **Soft / Performance**에서 같은 비교를 한 번 한다. HIGH는 기존 reference로 사용한다.
5. window 크기, Sync/Async, 기타 환경변수를 양쪽에서 같게 유지한다.
6. 새 shader의 첫 사용 비용과 정상 재생을 혼동하지 않도록 재실행/반복 여부도 함께 기록한다.

타겟 결과는 아직 없다. 미커밋 worktree를 포함하지 않은 원본 branch/package에서는 이 스위치가 동작하지 않는다.

## G. Target interpretation

- **V-only가 명확히 빨라짐:** final Source composition/dependency 묶음이 의미 있는 비용이라는 가설이 강해진다.
  이후에만 품질을 유지하는 handoff/Source authority 대안을 별도 연구한다.
- **거의 차이 없음:** 로그/first-use 같은 진단 부수 효과를 확인한 뒤 이 묶음의 우선순위를 낮추고,
  Output timing/update-side CPU를 별도 PoC로 본다.
- 이것만으로 “texture fetch1회가 전부 원인”이라고 결론내리지 않는다.
  함께 제거된 것은 Source sample, slot binding, 최종 read dependency, Late Smooth fragment ALU,
  관련 backend tracking 및 사용되지 않게 된 GPU uniform/attribute다.
- event/update의 Output strength constraints는 그대로 남았으므로 CPU timing consolidation 실험과는 구분된다.

## H. Git state / 자료

- 원본 `devel_blur_text`/HEAD/working tree 유지, clean.
- 진단 worktree: runtime.cpp 한 파일만 unstaged (`+44/-9`).
- sample/public API/공통 Gaussian/core/adaptor 수정 없음.
- commit/amend/push/reset/restore/stash/rebase 없음.
- `git diff --check` 통과.
- patch (로컬 자료: `performance-v-only.patch`), build log (로컬 자료: `build.log`), demo build log (로컬 자료: `demo-build.log`).
- PERF OFF (로컬 자료: `perf-off.log`), PERF ON (로컬 자료: `perf-on.log`), HIGH OFF (로컬 자료: `high-off.log`), HIGH ON (로컬 자료: `high-on.log`).
- 진단 driver (로컬 자료: `probe.cpp`), inventory 검증 (로컬 자료: `compare_inventory.py`), 기계 판독 결과 (로컬 자료: `inventory.json`).

다음 행동은 **타겟에서 OFF/ON 비교**다. 이번 PoC를 production 최적화로 통합하지 않았다.
