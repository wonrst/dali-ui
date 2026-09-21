# Text::Reveal PERFORMANCE A-R — production candidate

2026-09-16. Repository: `dali/dali-ui`, branch: `devel_blur_text`.

**A-R PRODUCTION CANDIDATE READY FOR TARGET VALIDATION**

Native A-R은 대표 FHD 8초 왕복 애니메이션에서 draw GPU 시간을
**0.716 → 0.464 ms/frame, 35.2%** 줄였다. CPU는 이번 반복 측정의 편차 안에서
비슷하다. WHOLE_TEXT 논리 FBO 저장량은 **+4.76%**, 페이지당 offscreen task는
**3 → 4**다. 기존 full-height H를 만든 뒤 교체하는 PoC 방식은 사용하지 않는다.

HIGH와 public API/timing 계약은 유지했다. A-R PoC의 p=.75 부근 추가 softness도
남는다. 이 결과는 host에서 검증한 **타겟 검증 후보**이며, 모든 GPU/backend의
최종 품질·성능 또는 merge 승인을 대신하지 않는다.

## A. Git / Prototype preservation

- 시작 HEAD: `5aae0d2b389bb9811bed6e21b754cee629f4ecfc`
  — `Prototype adaptive sampling for whole-text reveal blur`.
- 보존 브랜치: `prototype/reveal-adaptive-sampling`, 위 HEAD와 동일.
- adaptive는 clean UI worktree의 tip 단일 commit이었다. 보존 브랜치를 확인한 뒤
  `git reset --keep b7b0b712`로 해당 tip만 제거했다. 그 이후 commit은 없었다.
- 정리된 baseline: `b7b0b712fd47c6a2582b6245294c99bb415d89ee`
  — `Batch text reveal blur sources`.
- output/Source batching, memory admission, samples 및 앞선 production fixes는 유지했다.
- adaptive 전용 shader/eligibility/coefficients는 current branch에서 제거했다.
  보존 브랜치에는 원본과 dependency stack이 남아 있다.
- exact adaptive tip은 remote ref에 없었다. 더 오래된 remote adaptive commit은 건드리지 않았다.
- 기존 adaptor `gles-texture-dependency-checker.cpp`의 +13줄 사용자 변경을 보존했다.
  core/adaptor 수정, push, 기존 커밋 amend, stash는 하지 않았다.
- 기존 quality/feasibility 디렉터리는 읽기만 했다. 이 디렉터리는 새 외부 진단 결과다.

코드 변경 전 설계 기록: [AUDIT.md](AUDIT.md).

## B. Architecture

```text
HIGH (unchanged)
  Source W×H → H W×H → V W×H → Output
  compatible pages: Source/H shared, V retained

CURRENT PERFORMANCE (frozen b7b0b712)
  Source W×H → H ceil(W/4)×H → V ceil(W/4)×ceil(H/4) → Output
      └────────────────────── sharp Source / Late Smooth ────┘
  compatible pages: H shared, Source/V retained

NEW PERFORMANCE A-R
  Source W×H
     │
     ▼
  Prefilter W×ceil(H/4)       phase-correct pixel-area Y integration
     │
     ▼
  H ceil(W/4)×ceil(H/4)       original horizontal Gaussian
     │
     ▼
  V ceil(W/4)×ceil(H/4)       reduced calibrated vertical Gaussian
     │
     ▼
  Output                     V at Y ± 0.5 low texel + existing Late Smooth
     ▲
     └────────────────────── retained full-resolution sharp Source
```

대표값은 Source 2020×1180, Prefilter 2020×295, H/V 505×295다.
구형 PERFORMANCE H 505×1180은 새 경로에서 생성하지 않는다.

## C. Production implementation

### Prefilter / 임의 크기

기존 private PERFORMANCE branch를 확장했다. 새 public enum/property는 없다.
private `AXIS_AWARE_QUARTER`의 설명을 새 topology에 맞췄다.

```text
outH = ceil(sourceH / 4)
r = sourceH / outH
row j footprint = [j*r, (j+1)*r]
weight(k) = overlap([k,k+1], footprint) / r
```

최대 5개의 source pixel cell을 인접 셀의 LINEAR pairing 3회로 읽는다.
fixed 4-tap/tent가 아니며, 양 끝의 fractional overlap도 계산한다. X는 1:1이다.
새 `text-reveal-prefilter.frag`와 기존 scalar quad를 이용한다.
A8/RGBA 색 변환이나 timing 처리는 추가하지 않았고 FP16도 사용하지 않는다.

### H / V

- H kernel과 X의 source-coordinate offset은 기존과 같다.
  H input Y clamp는 실제 Prefilter height에 맞춘다.
- D2 coverage band에 1 low-Y texel의 보수적인 여유를 추가한다.
  prefilter footprint 때문에 인접 low row에 생기는 coverage를 자르지 않기 위해서다.
- V positive pair count는 `max(2, ceil((normalizedRadius/2)/r))`다.
  단순 radius/4 truncation이나 radius12 고정이 아니다.
- 최소 2 pairs는 기존의 길이 1 uniform-array reflection 회피 방침과 일치한다.
- 기존 `GaussianBlurAlgorithm`의 bell width 계산을 숫자만 반환하는
  `GetKernelSigma()`로 재사용한다. 기존 계수 생성과 shader factory 동작은 변경하지 않는다.
  shared UBO 노출 API나 text 의존성은 추가하지 않았다.

```text
lowStrength = sqrt(max(0, sigmaOriginal² * originalStrength² - preVariance))
              / (sigmaLow * r)
```

`preVariance`는 shader와 동일한 overlap weights의 이산 중심 분산을 output rows에
걸쳐 평균한 값이다. 정렬된 4행에서는 1.25로 PoC와 동일하다.
odd-height에서는 실제 phase를 사용한다. 다만 phase마다 달라지는 분산을 평균하므로,
임의 크기에서도 완전히 동일한 공간 불변 convolution이라는 뜻은 아니다.
선택한 odd/scale fixture의 이미지로 일반화 결과를 확인했다.
계산은 setup 시에만 수행하며 texture height로 제한된 row loop다. 새 cache는 없다.

V offset은 low input height 기준이며, batch에서는 기존 line inverse height에 r을 곱한다.
기존 radius normalization, progress/start guard, transparent/copy endpoint는 유지한다.
HIGH에는 reduced-V define이 없고 기존 kernel/strength가 적용된다.

### Output / ownership

PERFORMANCE의 V read만 Y ± `0.5 / actualVHeight`의 동일 가중치 2샘플로 바꾼다.
scalar는 texture 내부, batch는 각 padded line rectangle 내부로 clamp한다.
sharp Source read, Late Smooth의 2/8 thresholds와 곡선, Reveal timeline은 변경하지 않는다.

Prefilter Actor/FBO는 기존 companion의 Pass가 소유한다. task 배열은 최대 4개이며
HIGH에서는 4번째를 사용하지 않는다. 생성 완료 플래그로 초기화 중 return한 candidate를
정상 publication으로 반환하지 않도록 한다. 새 생성 위치에도 Adaptor 종료 검사를 적용했다.

기존 dimension/radius/texture-limit validation, backend RGBA fallback,
publication cancellation, borrow/restore를 활용한다.
GPU OOM이나 driver FBO allocation failure를 새로 주입해 검증하지는 않았다.
render-thread 실패 통지를 포함한 범용 복구 구조를 새로 만들었다는 의미는 아니다.

## D. PER_LINE / batching

```text
page 0: Source0 → shared Pref → shared H → V0
page 1: Source1 → shared Pref → shared H → V1
page 2: Source2 → shared Pref → shared H → V2

Output: retained V0/V1/V2 + retained Source0/Source1/Source2
```

- **같은 page dimensions/format이며 같은 companion 내부인 경우에만** Pref/H를 공유한다.
- 각 page의 V가 완료된 뒤 다음 page가 scratch를 덮어쓰는 task order를 유지한다.
- Source/output batching과 64-line draw binding 경계는 기존 구현을 활용한다.
  Prefilter는 page당 scalar quad 하나이며 line별 Actor/Renderer를 추가하지 않는다.
- mixed A8/RGBA page 순서와 ImageSpan proxy/capture/retention ownership을 변경하지 않는다.
- decoration은 기존 별도 composition에 남기고 foreground prefilter에 섞지 않는다.
- sync/async는 동일한 runtime path를 사용한다. prepared payload/revision 구조는 무변경이다.
- sample, public header, ImageVisual, InlineReplacementManager는 수정하지 않았다.

## E. Resource inventory

native scene에서 unique handle 수를 확인했다. Actor는 Label과 camera를 포함한다.
RenderTask는 default window task를 제외한다. textures는 FBO 외의 text/metadata도 포함한다.
저장량은 **unique FBO color texture의 논리 texel bytes**이며 실측 VRAM이 아니다.

WHOLE_TEXT: FHD Korean24 A8, radius48, Source 2020×1180.

| Resource | CURRENT | A-R | HIGH |
|---|---:|---:|---:|
| Offscreen tasks | 3 | 4 | 3 |
| Actors (camera 포함) | 10 | 12 | 10 |
| Cameras | 3 | 4 | 3 |
| Renderers | 4 | 5 | 4 |
| Unique geometry | 2 | 2 | 2 |
| Unique FBO | 3 | 4 | 3 |
| Unique textures | 5 | 6 | 5 |
| FBO bytes | 3,128,475 | 3,277,450 | 7,150,800 |
| FBO MiB | 2.9835 | 3.1256 | 6.8195 |

PER_LINE: gradient32, 12줄, RGBA page 858×540 3개, radius48.

| Resource | CURRENT | A-R | HIGH |
|---|---:|---:|---:|
| Offscreen tasks | 9 | 12 | 9 |
| Actors (camera 포함) | 27 | 33 | 27 |
| Cameras | 9 | 12 | 9 |
| Renderers | 13 | 16 | 13 |
| Unique geometry | 13 | 14 | 13 |
| Unique FBO | 7 | 8 | 5 |
| Unique textures | 12 | 13 | 10 |
| Live FBO bytes | 6,372,540 | 6,487,560 | 9,266,400 |
| Live FBO MiB | 6.0773 | 6.1870 | 8.8371 |

A-R은 Source3 + Pref1 + H1 + V3 = **8 FBO**다.
Pref858×135, H/V215×135이며, 같은 scratch를 task마다 중복 집계하지 않았다.
로그의 `TOPOLOGY` byte 열은 task별 합계이므로 multi-page에서는
중복을 제거한 `LIVE_PAYLOAD`를 사용했다.

WHOLE의 active/live FBO는 모두 4개다. PoC의 active4/live5 형태인 구형 H는 없다.
PER_LINE에서도 구형 H215×540은 만들지 않고 저해상도 H215×135 하나를 공유한다.

기록: WHOLE A-R (로컬 자료: `inventory-0-ar.log`), PER_LINE A-R (로컬 자료: `inventory-1-ar.log`),
PER_LINE CURRENT (로컬 자료: `inventory-1-current.log`), PER_LINE HIGH (로컬 자료: `inventory-1-high.log`).

## F. Correctness

- foundation build PASS: build-final.log (로컬 자료: `build-final.log`).
- 새로 빌드한 internal RuntimeBlur + AsyncPublication focused UTC:
  **118/118 PASS** (목록 (로컬 자료: `utc-summary.tsv`), 실행 (로컬 자료: `run-focused.sh`)).
- 새 100-cycle prefilter lifecycle test와 sampling dimensions/dependencies,
  scratch order, 64/65 경계, 최소 kernel, quality switching 검증을 추가/수정했다.
- 기존 tests를 통해 WHOLE/PER_LINE, A8/RGBA, gradient, ImageSpan 혼합,
  scale/render-scale, decoration, tiling fallback, seek/reverse/None,
  source/config 교체를 확인했다.
- 보조 public Label Reveal: **16/16 PASS** (목록 (로컬 자료: `public-summary.tsv`)).
  public test source/API는 그대로다. 기존 public driver를 이번 candidate foundation에
  동적 링크하여 실행했으며, 이번 turn에 public driver를 새로 빌드한 결과는 아니다.
  public build directory는 미구성, 기존 UTC build는 internal module 설정이어서
  public rebuild 시도는 구성 단계에서 종료했다. 빌드 조건을 변경하지 않았다.
- `git diff --check` PASS. 전체 3000+ regression은 이번 범위에서 실행하지 않았다.

### Native framebuffer quality

Ubuntu / GTX1650 / NVIDIA595.91.07 / GLES / window MSAA4.
1920×1080 Label, Korean24 12줄, radius48, WHOLE_TEXT, Unit LINE, Fade0,
Stagger.25, BlurDuration1, progress .20/.50/.75/.90.

native A-R과 보존된 기존 PoC A-R 이미지를 비교했다.
화면 y<900 영역으로 HUD를 제외한 결과다.

| p | max 8-bit channel difference | RGB RMSE (8-bit 단위) |
|---|---:|---:|
| .20 | 1 | .01186 |
| .50 | 1 | .01245 |
| .75 | 1 | .02368 |
| .90 | 0 | 0 |

primary fixture에서 PoC와 양자화 수준의 차이였고 .90은 동일했다.
HIGH는 frozen CURRENT와 동일한 4 progress 모두 pixel-identical했다.

primary 확인 후 Korean32, Latin/thin, gradient, colored StyledText, color emoji,
local ImageSpan을 동일한 4 progress로 확대했다. PER_LINE에서는
odd1917×1077 / UI scale1.25 / renderScale1.5, compressed lineHeight24 / stagger0,
async ImageSpan / radius24도 확인했다. 선택한 이미지에서 새로 눈에 띄는
page seam이나 이미지 누락은 관찰하지 못했다. 모든 font/size/phase에서 artifact가
없다는 보증은 아니다.

[추가 비교 contact sheet](quality-contact.png)는 CURRENT 왼쪽 / A-R 오른쪽이다.
quality matrix 기록 (로컬 자료: `quality-matrix.log`),
[primary native 이미지](captures/korean24-current-p0.75.png),
8초 native reverse 영상 (로컬 자료: `slow-native.mkv`)을 보존했다.
파일명의 `current`는 viewer의 native path 이름이므로 `captures/` 안은 새 A-R이다.
`quality/*/current`만 frozen 구형 CURRENT다.
영상은 저장했지만 모든 프레임의 미세 shimmer를 기계적으로 검출한 것은 아니다.

## G. Lifecycle

focused **ASan/LSan 35/35 PASS**, `detect_leaks=1:halt_on_error=1`.
목록 (로컬 자료: `asan-summary.tsv`), 실행 (로컬 자료: `run-asan.sh`), 빌드 (로컬 자료: `asan-build.log`).

- 새 Prefilter test 100 cycles: radii4/48/200, odd dimensions, A8/RGBA 교체.
  per-instance Actor/Renderer/FBO/Texture/Task의 WeakHandle 소멸을 확인한다.
- 기존 100 A8 + 50 format/quality + 5 line/page 경계 cycles:
  textures **0→0**, graphics buffers **5→5**, weak objects0, scene task1.
- ImageSpan 100 lifecycle cycles 역시 textures0→0, buffers5→5,
  tracked objects0, task1이다.
- sync/async, stale, valid-invalid-valid, pending None/config/source 교체,
  reconnect, destruction, capture replacement/failure, reentry, Adaptor Stop을 포함한다.
- scalar quad / immutable shader cache는 기존과 같은 의도적인 cache다.
  instance teardown마다 사라져야 한다고 검증하지 않는다. Prefilter도 scalar quad를 공유한다.

ASan은 instrumented CPU code + test graphics stub의 검증이다.
NVIDIA driver 자체를 instrument하거나 실제 GPU OOM을 주입한 것은 아니다.
native teardown 뒤 RSS에는 cache/allocator가 남으므로 원래 RSS로 돌아오는 것을
자원 소멸의 유일한 기준으로 사용하지 않는다.

## H. Performance

### 방법 / 반복 / 한계

동일한 외부 native bench를 frozen CURRENT와 candidate library로 교차 실행했다.
PoC installer는 호출하지 않는다. primary 조건은 F절과 동일하다.
각 mode warm process 1회를 제외한 뒤 **독립 process 각 3회** 측정했다.
고정 p=.20을 약 3초, 이후 8초 linear 0→1→0을 각 4초씩 실행했다.
animation window에는 약 482 frames가 포함된다.

GPU는 각 draw에 `EXT_disjoint_timer_query`를 적용하고, 동일한 window frame 수로
Source/Pref/H/V/Output을 나눠 집계했다. drop/disjoint는 0이다.
**clear/upload/resolve/present/wait는 제외**한다. FPS나 전체 frame GPU 시간과 다르다.
CPU는 GPU query 없는 별도 process 각 3회로 측정했다.

### GPU: primary animation mean ms/frame

| Stage | CURRENT | A-R | delta |
|---|---:|---:|---:|
| Source | .057017 | .065690 | +.008673 |
| Prefilter | 0 | .054125 | +.054125 |
| H | .429846 | .153205 | −64.4% |
| V | .122240 | .043805 | −64.2% |
| Output | .106965 | .147447 | +37.8% |
| **Total draw GPU** | **.716068** | **.464271** | **−35.2%** |

CURRENT runs: .712909 / .727150 / .708144.
A-R runs: .445265 / .471666 / .475884.
고정 p=.20은 .758898→.428496 ms/frame, **−43.5%**다.

추가 Prefilter/Output read 비용을 포함해도 목표인 30% 이상의 이득이 유지된다.
PoC의 약45.9% 감소는 참고값이며 이번 결과와 섞어 평균하지 않는다.

### CPU: active animation, process CPU ms / wall second

| | mean | independent runs | sample SD |
|---|---:|---|---:|
| CURRENT | 207.70 | 197.54 / 215.71 / 209.85 | 9.28 |
| A-R | 203.86 | 211.46 / 200.45 / 199.67 | 6.60 |

1 core를 100% 사용하면 1000 ms/s다. 모든 process thread의 누적 CPU이며 main-thread
시간이 아니다. 차이가 run variation보다 작으므로 **CPU neutral**로 판단한다.
안정적인 CPU 개선이라고 주장하지 않는다.

### Setup: event-side 관측

native backend의 frame-rendered fence callback을 사용할 수 없었으므로 엄밀한
첫 GPU frame 완료 시간은 보고하지 않는다. 별도 setup app에서 SetTextReveal부터
40ms timer로 새 task topology를 처음 확인할 때까지 wall/process CPU를 측정했다.
event-side publication 관측값으로 poll 지연과 동시에 실행되는 작업을 포함한다.

| | wall ms (3 runs) | process CPU ms (3 runs) |
|---|---|---|
| CURRENT | 51.529 / 51.413 / 51.631 | 44.994 / 28.816 / 27.307 |
| A-R | 52.071 / 52.225 / 52.814 | 42.468 / 29.498 / 29.732 |

CPU 평균은33.706→33.899ms로 이 관측 정밀도에서는 비슷하다.
benchmark의3.76초 settling interval은 연속 렌더링을 포함하므로 순수 setup 시간이 아니다.
로그의 `INSTALL_CALL`은 native 경로에서 빈 작업이므로 setup 비교에 사용하지 않았다.

### Optional RGBA sanity

Gradient32, 동일 FHD/radius48/8초로 각1회만 측정했다.
total draw **.758442→.422326 ms/frame, −44.3%**다.
1회 sanity이므로 RGBA 전체의 보장이나 통계적 유의성을 의미하지 않는다.

데이터: summary.json (로컬 자료: `summary.json`), analyze.py (로컬 자료: `analyze.py`),
bench.cpp (로컬 자료: `bench.cpp`), meter.cpp (로컬 자료: `meter.cpp`), run.py (로컬 자료: `run.py`),
setup.cpp (로컬 자료: `setup.cpp`), setup-runs.log (로컬 자료: `setup-runs.log`).

## I. Memory

### 논리 FBO 저장량과 실제 process memory

WHOLE FHD A8: Source + 5Q → Source + 6Q.
**2.9835→3.1256 MiB, +0.1421 MiB / +4.76%**다.
RGBA라면11.9342→12.5025 MiB다.
PER_LINE 3page는 Pref/H 공유 덕분에6.0773→6.1870 MiB, **+1.80%**다.

이는 texel bytes이며 driver allocation/alignment, MSAA window, ordinary text/metadata,
CPU buffers는 제외한다. 전용 VRAM을 측정한 수치가 아니다.
기존256MiB admission은3 full RGBA targets 등을 포함하는 보수적인 추산으로,
새 축소4-target 구성도 해당 범위 안이다. admission policy는 변경하지 않았다.

GPU timer 없는 primary 각3process의 평균:

| RSS MiB | CURRENT | A-R |
|---|---:|---:|
| Ordinary Label settled | 123.953 | 123.219 |
| Blur prepared | 138.186 | 138.147 |
| Active animation | 138.190 | 138.147 |
| 각 process의 ordinary 대비 증가 | 14.233 | 14.928 |
| VmHWM mean | 140.072 | 138.967 |

prepared RSS 절대값은 비슷하지만 각 process의 ordinary 대비 증가분은 A-R이 약0.70MiB 크다.
추가 Actor/Renderer/task, shader/kernel cache, allocator/driver를 포함하는 지표다.
작은 차이를 개별 object에 엄밀하게 배분한 것은 아니다.

### Cold first-use: 약197 MiB 재현

warm/첫 RGBA A-R에서 약197MiB가 다시 나타났다.
새 setup executable로 fresh process를 양쪽 각3회 추가했으며 cache는 삭제하지 않았다.

| Fresh-process VmHWM MiB | first | second | third |
|---|---:|---:|---:|
| CURRENT | 197.75 | 139.43 | 139.45 |
| A-R | 196.86 | 141.16 | 139.30 |

따라서 **197MiB를 A-R에만 존재하는 정적 texture 증가로 볼 수 없다**.
모두 새 process이지만 driver cache를 purge한 완전 cold 측정은 아니다.

추가 제한 진단에서 기존 `DALI_SHADER_USE_PROGRAM_BINARY=0`을 해당 child process에만
적용하고 `glCompileShader`/`glLinkProgram` 전후 RSS를 기록했다.

- CURRENT도 compile121004→142248KiB, link142248→163856KiB로 크게 증가했다.
- A-R도 compile140480→161204KiB, link161204→182244KiB 증가가 있었다.
- 이 실행의 prepared RSS는 CURRENT 약177.91MiB, A-R 약179.28MiB였다.

양쪽 shader compile/link 구간의 약40MiB 증가를 직접 관측한 것이다.
**197MiB 전체 원인을 모두 분해한 것은 아니다**. compiler/driver 관련 비용을 FBO 저장량이나
instance leak과 구분하는 근거이며, cold spike가 해결되었다는 뜻은 아니다.
shader cache 경로는 기존 공용 DALi cache다. cache 삭제나 driver/adaptor 변경은 하지 않았다.
CURRENT compile (로컬 자료: `compile-memory-0.log`), A-R compile (로컬 자료: `compile-memory-1.log`).

### None / destroy / recreate

CPU runs에서 None/destroy 뒤 A-R RSS는 약135.8–136.0MiB다.
ordinary 수준으로 완전히 복귀하지는 않지만, G절 반복 검증에서 per-instance weak handle과
task는 소멸하고 graphics texture/buffer count는 baseline으로 돌아온다.
남은 RSS를 곧바로 leak이라고 판단하지 않는다.
반대로 GPU driver heap의 장기 retention을 이번 짧은 확인만으로 완전히 배제하지도 않는다.

## J. Remaining visual trade-off

- strong blur의 수직 reconstruction은 앞선 Y-only 후보의 band를 완화한다.
- **p=.75 부근은 CURRENT보다 조금 더 soft**하다. 확인된 A-R PoC의 특성을 유지한다.
- primary p=.90은 sharp handoff 뒤 PoC와 동일하다.
- compressed line/fractional phase/color image에서는 CURRENT와 blur profile이 다르다.
  HIGH 비교와 일부 sharp 상태의 동일성을 PERFORMANCE 전체의 동일성으로 확대하지 않는다.
- threshold/curve tuning, adaptive sampling, idle suspension은 추가하지 않았다.

**native A-R만** 직접 보는 명령:

```bash
MODE_INDEX=1 SLOW=1 bash /home/bowonryuubuntu/tizen/reveal-ar-production.FY6dsQ/run-viewer.sh
```

viewer의 CURRENT 표시는 native library를 뜻하므로 이 명령에서는 새 A-R이다.
기존 PoC 비교 모드는 선택하지 않는다. 기존 sample 코드는 그대로지만 candidate library를
사용하면 PERFORMANCE가 A-R로 실행된다.

## K. Follow-up optimization candidates

1. **Reconstruction gating 품질 PoC**:
   strong에서2 reads, near-sharp에서1 read를 검토한다.
   p=.75 softness와 Output 비용 개선 가능성이 있으나 별도 curve/threshold 검증이 필요하다.
2. **Output exact fetch branch**:
   이번 Output은.147ms로 total draw의 약32%다.
   exact하게 불필요 read를 생략할 조건을 별도로 측정할 가치가 커졌다. 실제 이득은 미측정이다.
3. **Adaptive-on-A-R**:
   마지막 순서다. 보존된 old-topology coefficients/eligibility를 그대로 이식하지 않는다.
   별도 quality/performance 검증이 필요하다.

어느 것도 이번 후보에 포함하지 않았다. 먼저 TV target에서 대표 퇴장/PER_LINE의
품질, GPU/CPU, cold setup을 확인한다.

## L. Verdict

**A-R PRODUCTION CANDIDATE READY FOR TARGET VALIDATION**

primary GPU −35.2%, CPU neutral, native resource4/live4, scratch 공유 유지,
PoC framebuffer parity, HIGH parity, focused UTC/ASan/ownership PASS가 근거다.
새 public API나 큰 구조 변경은 없다.

남은 확인은 p=.75 softness의 UX 수용 여부, 추가 task의 target 비용,
driver 의존 cold peak다. TV/Windows, 전체 regression, GPU fault injection은 미실행이다.

## M. Final git state

- 새 후보: `96bf7828866c021606b1009c4940f84294325f8d`
  — `Optimize performance reveal blur filtering`.
- `Signed-off-by: Bowon Ryu <bowon.ryu@samsung.com>`.
- `devel_blur_text` UI worktree clean, `git diff HEAD^ --check` PASS.
- 보존 브랜치: `prototype/reveal-adaptive-sampling`
  → `5aae0d2b389bb9811bed6e21b754cee629f4ecfc`.
- 기존 production commit은 amend하지 않았다. 이번에 만든 새 후보만 메시지의
  줄바꿈 오류를 바로잡았으며 code tree는 동일하다.
- pre-commit hook이 정렬/namespace 주석을 정리했다. 검증한 코드와 동작 차이는 없다.
- push 없음. 기존 adaptor +13줄 변경 유지.
- 보고서/bench/captures는 repo 외부이며 commit에 포함하지 않았다.
