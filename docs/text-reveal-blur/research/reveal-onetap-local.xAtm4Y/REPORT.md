# Text::Reveal PERFORMANCE — H/V center 1-tap target diagnostic

## A. Executive summary

**ONE-TAP FILTER TARGET DIAGNOSTIC READY**

현재 로컬 `devel_blur_text`의 PERFORMANCE는 별도 설정 없이 다음 경로다.

```text
Full-resolution Source
  → H: center 1-tap, 기존 reduced-X FBO
  → V: center 1-tap, 기존 reduced-XY FBO
  → V-only Output
```

기존 V-only Gaussian과 비교하여 **filter fragment만 변경**했다.
Strong Cards 기준 12 companions / 20 lines / 15 pages / 45 tasks / 45 FBOs /
386 constraints 및 모든 Source/H/V 크기·포맷이 동일하다.
HIGH는 기존 Gaussian이다. 품질 개선이나 production 채택 판단이 아니다.
호스트 CPU/GPU 시간·FPS는 측정하지 않았다. 타겟 결과는 아직 없다.

## B. Baseline

- Repository: `/home/bowonryuubuntu/tizen/dali/dali-ui`
- Branch: `devel_blur_text`
- HEAD: `511fef039841068eae38337e48b39b6f4d786134` — `perf test`
- 시작 시 UI worktree는 **clean**이었다.
- 요청서의 예상과 달리 V-only는 이미 HEAD에 커밋되어 있었다.
  이 커밋과 함께 들어 있던 formatting을 그대로 보존했다.
- `USE_PERFORMANCE_V_ONLY_POC = true` 유지.
- A-R/prefilter/Vogel 및 reduced-rate task를 추가하지 않았다.
- 초기 확인 기록: BASELINE.txt (로컬 자료: `BASELINE.txt`).

비교 OFF/ON은 **새 ONE_TAP flag만** 다르다. 둘 모두 V-only ON이다.
PRIVATE OFF 라이브러리는 외부 디렉터리에서 runtime.cpp 복사본의 상수만 false로
컴파일했다. 레포에서 flag를 바꾸거나 다른 worktree/commit을 만들지 않았다.

## C. Exact shader change

레포 변경은 다음 3개 파일뿐이다.

1. `text-reveal-runtime-blur.cpp`
   - `USE_PERFORMANCE_ONE_TAP_FILTER_POC = true` 추가.
   - scalar/batch H/V의 renderer 생성 선택 두 곳만 변경.
   - PERFORMANCE만 새 diagnostic renderer 선택.
2. `text-reveal-blur-renderer.cpp`
   - 별도 center-sample fragment, 별도 shader cache/name.
   - 기존 vertex/geometry 및 renderer 초기 property 등록은 그대로 재사용.
   - 기존 kernel factory를 같은 radius로 호출한다. CPU kernel 생성·UniformBlock
     ownership을 제거하지 않았다. GPU compiler의 unused block 제거는 허용한다.
3. `text-reveal-blur-renderer.h`
   - text 내부의 `CreateOneTapDiagnostic()` 선언만 추가. public API 아님.

기존 `.frag/.vert`, GaussianBlurAlgorithm factory/구현은 변경하지 않았다.
원래 Gaussian fragment/cache와 production Late Smooth source는 남아 있다.

center coordinate는 기존 `ReadRevealBlurTexture(vTexCoord)`와 같다.

```glsl
point = uRevealBatchRect.xy + vTexCoord * uRevealBatchRect.zw;
low   = uRevealBatchRect.xy + uRevealBatchInvSize * 0.5;
high  = uRevealBatchRect.xy + uRevealBatchRect.zw
        - uRevealBatchInvSize * 0.5;
color = TEXTURE(sTexture, clamp(point, low, high)) * uOpacity;
```

batch rect는 기존 vertex의 line-local rect/atlas offset을 그대로 받는다.
시퀀스 시작 전 투명 처리와 `1/65535` 허용오차도 그대로다.
따라서 시작 전에는 0 read, eligible fragment는 H/V 각각 1 read다.

실제 사용된 batch shader를 renderer에서 꺼내 전처리해 확인했다:
Strong fragment (로컬 자료: `perf-strong-on/GaussianBlurShader_12_TextReveal_Batch_OneTapDiagnostic.preprocessed.txt`),
Soft fragment (로컬 자료: `perf-soft-on/GaussianBlurShader_8_TextReveal_Batch_OneTapDiagnostic.preprocessed.txt`).
main에 TEXTURE 호출 1개, Gaussian loop/paired reads/weights accumulation 없음.
이는 shader texture 명령 수 확인이지 실제 메모리 transaction 수 측정은 아니다.

## D. Preserved topology

1280×720 창의 실제 Text Effect Demo Cards 3개 내부 Label 12개를 조사했다.
표의 actors/renderers/textures는 **그 Label subtree 전체**를 센 값이다.
카드 외 화면 전체 inventory나 순수 blur 증가분으로 해석하지 않는다.

| 항목 — Strong entrance radius24 | V-only Gaussian | V-only 1-tap |
|---|---:|---:|
| Blur companions / visible lines / pages | 12 / 20 / 15 | 12 / 20 / 15 |
| Source / H / V tasks | 15 / 15 / 15 | 15 / 15 / 15 |
| Total offscreen tasks / FBOs | 45 / 45 | 45 / 45 |
| Cameras | 45 | 45 |
| Actors | 156 | 156 |
| Renderers / geometries | 72 / 54 | 72 / 54 |
| Distinct shaders in this inventory | 7 | 7 |
| Texture objects | 94 | 94 |
| Logical FBO bytes | 603,494 | 603,494 |
| All bound texture logical bytes | 1,901,130 | 1,901,130 |
| Constraints | 386 | 386 |
| Final Output binding | V only | V only |
| H/V eligible read count when blur strength > 0 | 24 each | 1 each |

Gaussian도 strength=0에서는 원래 1-tap copy다. 위 multi-tap 수치는 blur 활성 구간만 뜻한다.

Source→H→V→Output texture identity, task 순서, REFRESH_ALWAYS를 page마다 확인했다.
각 Output에는 direct Source binding이 없다. viewport, clear enable/color, attachment
bytes-per-pixel, renderer property/binding 개수도 각각 동일하다.
page planner, Source/Output batching, D2 geometry, admission/lifecycle은 수정하지 않았다.

Logical bytes = width × height × format bytes이며 RSS/VRAM 실측이 아니다.
driver alignment, window MSAA, CPU allocation은 포함하지 않는다.

## E. Dimensions — Soft16 / Strong24

아래 각 행의 크기는 ONE_TAP OFF(Gaussian) / ON(1-tap) 사이에 완전히 같다.
12 labels, 15 pages의 전체 크기는 inventory.json (로컬 자료: `inventory.json`)에 기록했다.

| Preset / Label | Format | Source | H | V |
|---|---|---:|---:|---:|
| Strong / Card1 day | A8 | 95×66 | 24×66 | 24×17 |
| Strong / Card1 title | A8 | 290×78 | 73×78 | 73×20 |
| Strong / Card2 gradient title | RGBA8 | 256×78 | 64×78 | 64×20 |
| Strong / Card1 subtitle | A8 | 380×212 | 95×212 | 95×53 |
| Soft / Card1 day | A8 | 79×50 | 20×50 | 20×13 |
| Soft / Card1 title | A8 | 274×62 | 69×62 | 69×16 |
| Soft / Card2 gradient title | RGBA8 | 240×62 | 60×62 | 60×16 |
| Soft / Card1 subtitle | A8 | 364×164 | 91×164 | 91×41 |

Soft에서도 동일한 15 pages / 45 FBOs / 94 textures.
Soft FBO payload는 양쪽 모두 **443,962 bytes**이고 Strong은 **603,494 bytes**다.
radius를 0/1로 바꾸지 않았다. halo와 allocation의 radius 영향은 유지한다.

## F. Constraint invariant

PERFORMANCE Soft/Strong 모두 OFF/ON 동일:

- H/V timing constraints: 40
- Output strength constraints: 20
- H/V progress mirrors: 30
- 기타 Label subtree constraints: 296
- 합계: **386**

renderer의 property count, state/strength slot 개수, constraint target name,
apply rate/source count를 비교했다. 생성 코드 자체도 변경하지 않았다.
1-tap shader가 strength를 소비하지 않더라도 update-side 계산은 남겨 둔다.
Output strength constraints 역시 V-only에서 사용하지 않지만 제거하지 않았다.

## G. HIGH invariant

HIGH+Soft OFF/ON 두 실행에서:

- Source/H/V/Output shader name, vertex/fragment source hash 모두 동일.
- textures/bindings/properties/constraints 및 Source/H/V 크기 모두 동일.
- 45 tasks/FBOs, 156 actors, 72 renderers, 54 geometries 유지.
- constraints 366, FBO payload 1,013,277 bytes로 양쪽 동일.
- V-only/one-tap marker 모두 없음, unused attribute warning 없음.

HIGH는 full-resolution Gaussian 그대로다. PERFORMANCE와 HIGH 사이의 constraint
차이는 기존 구현의 차이이며 이번 변경으로 생긴 것이 아니다.

## H. Host smoke / warning audit

환경: Ubuntu / NVIDIA GTX1650 / driver 595.91.07 / GLES / MSAA4.

- foundation/components build: PASS — build-main.log (로컬 자료: `build-main.log`)
- 비교 실행 6개: PERFORMANCE Strong/Soft와 HIGH Soft의 ONE_TAP OFF/ON.
- 실제 demo source를 수정 없이 포함한 외부 inventory probe 사용.
  Intro → Skeleton → Cards, Cards 재생성, Shutdown을 실행했다.
- 각 실행에서 initial/recreated resource inventory 동일.
- A8 및 RGBA gradient title 표시와 PER_LINE 경로 확인.
  1-tap 특유의 aliasing/저해상도 품질은 예상된 진단 결과이며 평가 대상 아님.
- snapshot에서 추적한 event-side actors/renderers/tasks/FBOs/FBO textures/
  constraints의 weak handles는 recreate와 shutdown 뒤 모두 소멸.
  종료 직전 window 기본 task 1개만 남았다. GPU driver allocation 회수 측정은 아님.
- 별도 unmodified demo executable도 현재 main-build library로 실행하고 정상 종료(0).
- `git diff --check`: PASS.
- 결과/비교 코드: smoke-summary.log (로컬 자료: `smoke-summary.log`),
  compare_inventory.py (로컬 자료: `compare_inventory.py`), probe.cpp (로컬 자료: `probe.cpp`).

대표 화면: [1-tap Strong](perf-strong-on/perf-strong.png),
[Gaussian Strong](perf-strong-off/perf-strong.png).
애니메이션 시간으로 캡처한 smoke 이미지이며 정확한 동일 progress 품질 비교는 아니다.

### Warnings

각 프로세스는 intro/전환/Cards 재생성을 포함한다. 아래는 그 실행 전체의 개수다.

| Warning | Gaussian PERFORMANCE | 1-tap PERFORMANCE | HIGH |
|---|---:|---:|---:|
| `aRevealLineIndex` | 59 | 59 | 0 |
| `aRevealInverseSize` | 0 | 118 | 0 |

`aRevealLineIndex`는 기존 V-only Output 경고다. 새 H/V에서는 시작 시각 조회에
line index가 실제 사용된다. 새 `aRevealInverseSize` 경고는 offset direction이
사용되지 않아 compiler가 해당 attribute를 제거하기 때문이다.

이 로그는 core `PipelineCache::GetPipelineCacheL0()`의 **cache miss에서 새 항목을
만들 때만** 출력된다. 프로세스당 1회라는 뜻은 아니며 geometry/pipeline 재생성 시
다시 나온다. 일정한 per-frame flooding은 관찰되지 않았다.
첫 snapshot 이후 약 4초 구간에도 Gaussian은 기존 경고 2개, 1-tap은 기존 2개+
새 경고 4개가 있었다. 따라서 steady 구간 경고가 완전히 0이라고 보고하지 않는다.

진단 marker는 프로세스당 각각 1회다. attribute 경고를 없애려고 geometry를
바꾸거나 인위적인 keep-alive 연산을 추가하지 않았다. 타겟 console/file logging
비용은 측정하지 않았으므로 0이라고 보증할 수 없다. 첫 shader compilation 및
위 cache-miss 로그와 반복 전환 결과를 구분해서 기록할 필요가 있다.

처음 생성한 두 diagnostic shader는 shader binary cache file-not-found 로그 뒤
정상 소스 compile로 이어졌다. shader compile/link 오류나 실행 실패는 없었다.

Host timing/GPU timer/FPS benchmark, full UTC/regression, sanitizer는 실행하지 않았다.
비교용 library를 처음 만들 때 오래된 object 파일까지 모아 duplicate link 오류가
있었고, 외부 build 도구만 CMake의 실제 objects response list 사용으로 고쳤다.
본 foundation/components build나 레포 build 설정 문제는 아니었다.

## I. Target instructions

평소대로 현재 working tree를 build/install하고 `text-effect-demo.example`을 실행한다.
새 환경변수·argument·sample option은 없다. 기존 버튼으로만 선택한다.

1. **PERFORMANCE + Strong**부터 Skeleton → Results를 여러 번 확인.
2. **PERFORMANCE + Soft**로 같은 전환 확인.
3. **HIGH + Soft** reference 확인.
4. 기존 **Blur Effect** mode로 같은 전환 reference 확인.

현재 로컬 host 빌드를 install 없이 직접 볼 경우에만 아래 launcher를 사용할 수 있다.
이는 build library 경로를 연결할 뿐, PoC mode를 선택하지 않는다.

```bash
bash /home/bowonryuubuntu/tizen/reveal-onetap-local.xAtm4Y/run-demo.sh
```

봐야 할 것: Skeleton→Results stall, Cards root transition이 처음부터 보이는지,
중간 위치에서 갑자기 나타나는 현상, FPS, 반복 전환의 일관성.
**Blur/글자 품질은 무시한다.** center resampling이므로 일반 Gaussian처럼 보이지 않는다.

원복 비교는 `text-reveal-runtime-blur.cpp`의
`USE_PERFORMANCE_ONE_TAP_FILTER_POC`만 false로 바꿔 재빌드한다.
V-only flag는 true로 유지하면 직전 V-only Gaussian 조건이 된다.
이번 작업에서는 현재 레포의 flag를 true로 남겼다.

## J. Target decision table

| 타겟 결과 | 이번 실험으로 지지되는 해석 | 다음 단계 |
|---|---|---|
| 매우 큰 개선, BlurEffect 수준 접근 | H/V filtering workload가 주요 병목 중 하나라는 강한 근거 | 품질을 지키는 sampling 감소 연구 후보 |
| 조금만 개선 | filtering 외에 offscreen scheduling/clear/write/CPU 등의 잔여 비용도 큼 | topology 포함 추가 분리 필요 |
| 거의 변화 없음 | 이 workload에서는 Gaussian read/ALU 감소 효과가 지배적이지 않음 | tap 미세 최적화보다 Source/45-pass pipeline 조사 우선 |

미개선 결과만으로 **45 tasks가 유일한 원인이라고 확정하지 않는다**.
Source raster/CPU constraints/driver scheduling 등은 이번에 그대로 남아 있다.
타겟 결과 전에는 다른 최적화를 추가하지 않는다.

## K. Strong vs Soft interpretation

둘 다 active H/V fragment는 1 read지만 radius/halo/FBO 크기는 그대로 다르다.

- 기존 Strong/Soft 차이가 크게 줄면 tap/kernel workload 차이의 영향이라는 근거.
- Strong이 여전히 느리면 큰 halo/FBO의 fragment 수·clear/write/bandwidth 및
  관련 setup 비용이 잔여 원인일 가능성. 이것만으로 한 항목을 확정하지 않는다.
- 샘플 Strong은 **입장24 / 퇴장48**, Soft는 **입장16 / 퇴장32**다.
  위 inventory table은 Cards 입장 상태이며 퇴장48/32의 peak 표가 아니다.

## L. BlurEffect reference

질문은 **“동일한 45-task topology를 남겨도 H/V filtering을 거의 없애면
BlurEffect의 transition smoothness에 접근하는가?”**다.
이번 타겟에서 YES/NO 및 반복 시 일관성을 기록한다.
BlurEffect는 PER_LINE Reveal과 동일한 UX/topology가 아니므로 품질 동등 비교나
동일한 일을 더 싸게 한다는 production 결론으로 확장하지 않는다.

## M. Git state / protected scope

- HEAD/branch 변경 없음. 기존 V-only 커밋 `511fef03` 보존.
- 새 1-tap 변경은 앞서 나열한 3개 파일에 **unstaged**로만 남겼다.
- `git diff`는 이번 1-tap 변경만 보여 준다. V-only는
  `git show 511fef03 -- dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`로
  별도 확인할 수 있다. 이미 커밋된 V-only를 다시 unstaged로 옮기지 않았다.
- sample/public API/common Gaussian shader·factory 수정 없음.
- core/adaptor worktree clean 상태 유지. 설치 라이브러리 교체 없음.
- commit/amend/push/reset/restore/stash/rebase/git add 없음.
- 외부 보고서·probe·private OFF/ON libraries는 이 폴더에만 저장했다.
