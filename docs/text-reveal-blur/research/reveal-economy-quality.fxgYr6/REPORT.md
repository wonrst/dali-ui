# ECONOMY blur — quarter-H/V, quality-first private PoC

## A. Executive Verdict

**ECONOMY QUALITY NOT ACCEPTABLE**

ECONOMY-A의 **maximum blur 자체가 Phase 1을 통과하지 못했다.**
Korean24 / 동일 문장 12줄에서 Soft16은 글자 모양의 얼룩과 줄별 밝기 차이가 뚜렷하다.
Strong24는 더 완화되지만 같은 종류의 불균일한 무늬가 남는다.
읽을 수 없어서 실패한 것이 아니라, 같은 문장이 줄마다 다르게 흐려지는 sampling artifact 때문이다.

따라서 안전한 `Rsafe…Rmax` 구간을 정하지 못했다. **Sharp crossfade로 가리지 않았다.**
ECONOMY-B 역시 “strong blur는 좋다”는 진입 조건을 만족하지 않아 만들지 않았다.
이는 아래 A 구현/fixture의 No-Go이며, 모든 저비용 blur 설계의 불가능성 증명은 아니다.

바로 비교:

```bash
bash /home/bowonryuubuntu/tizen/reveal-economy-quality.fxgYr6/run-viewer.sh
```

- 기본: 왼쪽 CURRENT / 오른쪽 ECONOMY-A, 최대 반경 24, **양쪽 모두 blur-only**.
- `S` Soft16 / `D` Strong24, `[`·`]` effective radius −1/+1px.
- `1` HIGH / `2` CURRENT / `4` ECONOMY / `5` split / `6` triple.
- `Q W E R T Y U I O`: blur amount `.05 .10 .20 .30 .40 .50 .65 .80 1`.
- `Space`: blur amount 0→1→0, 8초. `Esc`: 종료.
- `3` mixed는 Phase 1 실패로 잠겨 있다. 미검증 crossfade를 제공하지 않는다.

## B. Current PERFORMANCE baseline

- 원본 UI HEAD `05087317cac8ea9600bba498f00ccf8086a79d3f`, branch `devel_blur_text`.
- 기존 dirty instrumentation과 sample 변경을 유지했다. BASELINE.json (로컬 자료: `BASELINE.json`).
- 기존 private `reveal-flush-poc.2JyKvS` production objects/Common libraries를 재사용했다.
  CURRENT runtime 코드는 `b54bb666 Batch text reveal blur sources`와 비교 시 비활성 trace scope만 다르다.
  source-only/one-tap/V-only 진단 경로를 baseline으로 사용하지 않았다.
- private runtime object 하나만 다시 빌드/링크했다. install/production build 없음.
  build-record.json (로컬 자료: `build-record.json`), runtime-overlay.diff (로컬 자료: `runtime-overlay.diff`).
- Ubuntu / GTX 1650 / NVIDIA 595.91.07 / GLES / window MSAA 4.
- Label 1920×1080, font size24, 흰 글자/검정 배경, 다음 문장 12회:

  `따뜻한 햇살 아래 아름다운 우리들의 이야기가 시작됩니다`

- Unit::LINE, WHOLE_TEXT, Fade0, Stagger .25, BlurDuration1.
- **Phase 1에서는 Source progress=1 고정**, probe radius를 독립적으로 조절했다.
  Source/H/V는 여전히 매 frame 실행한다. static preblur cache는 만들지 않았다.
  이 단계는 Reveal 타이밍/미공개 glyph 검증이 아니라 blur branch의 품질 분리다.
- 화면은 각 FHD Label의 왼쪽 영역을 원래 픽셀 크기로 clip한다. 글꼴/layout을 축소하지 않는다.

## C. ECONOMY topology

```text
Source full W×H ────────────────────────→ retained sharp input
        │
        ▼
H ceil(W/4) × ceil(H/4): production horizontal Gaussian
        │
        ▼
V same quarter size: quarter-input-domain Gaussian
        │
        ▼
Output (Phase 1: V only; sharp mixing disabled)
```

label마다 **Source/H/V 3 offscreen tasks와 FBO 3개**다. 추가 texture/pass가 없다.
H를 생성 시점부터 작게 할당하므로, 교체 전 full-height H FBO도 남지 않는다.
page planner, Source rendering, kernel radius/halo, camera, refresh rate는 그대로다.
H/V probe shader는 production shared UBO를 변경하지 않고 로컬 상수로 동일 H 계수를 전개했다.
CURRENT와 ECONOMY H shader 내용이 동일함을 audit.py (로컬 자료: `audit.py`)에서 확인했다.

## D. Actual Source/H/V dimensions

RenderTask의 실제 color texture에서 읽었다. capture logs (로컬 자료: `captures/`).

| Case | Path | Source | H | V |
|---|---|---:|---:|---:|
| Soft16 | HIGH | 1956×1116 | 1956×1116 | 1956×1116 |
| Soft16 | CURRENT | 1956×1116 | 489×1116 | 489×279 |
| Soft16 | ECONOMY-A | 1956×1116 | 489×279 | 489×279 |
| Strong24 | HIGH | 1972×1132 | 1972×1132 | 1972×1132 |
| Strong24 | CURRENT | 1972×1132 | 493×1132 | 493×283 |
| Strong24 | ECONOMY-A | 1972×1132 | 493×283 | 493×283 |
| odd Label 1921×1081 / R24 | CURRENT | 1973×1133 | 494×1133 | 494×284 |
| odd Label 1921×1081 / R24 | ECONOMY-A | 1973×1133 | 494×284 | 494×284 |

HIGH comparison: [Soft16](triple/R16-effective16.png), [Strong24](triple/R24-effective24.png).
Odd-size native capture: [Strong24](odd/R24-effective24.png).

## E. Quarter-Y downsample behavior

Source extent `S`, H extent `T=ceil(S/4)`이면 output texel j가 읽는 source pixel 좌표는:

```text
s = S/T
c(j) = (j + 0.5) * s - 0.5
```

정확한 4배 축소에서는 Y 좌표가 `4j+1.5`다. LINEAR 한 번은 source rows
`4j+1`, `4j+2`의 평균이지 **네 행 평균이 아니다.** H의 모든 Gaussian tap은 같은 Y를 읽는다.
따라서 수직 고주파를 먼저 충분히 거르지 않은 상태에서 Y decimation이 일어난다.
이후 V는 이미 줄어든 H를 읽는다. 이는 관측된 획/줄별 무늬의 유력한 구조적 원인이다.
중간 stage readback으로 원인별 기여도를 분리한 실험까지 한 것은 아니다.

- 홀수 `1133→284`: 실제 Y scale `3.98943662`; 첫 source center `1.49471831`,
  마지막 `1130.50528169`. 고정 4나 integer-floor size로 계산하지 않았다.
- 샘플러는 기존 LINEAR / CLAMP_TO_EDGE 유지.
- Source halo는 기존 `R+2` 양쪽: Soft18px, Strong26px.
- H는 source의 inverse extent, V는 **축소 H의 inverse extent**를 사용한다.
- WHOLE_TEXT source rectangle `(0,0,1,1)`과 기존 half-texel clamp 함수를 유지했다.
- full-size camera/quad로 동일 normalized 영역을 작은 FBO에 raster한다.
  viewport를 이전 full-height로 강제하지 않는다.
- private ECONOMY에서는 D2 horizontal-band geometry 선택을 막았다.
  WHOLE_TEXT fixture는 원래 full quad이며, 향후 batched 검증에 full-height band를 재사용하지 않게 했다.
- halo가 잘려 보이는 새 경계는 이 fixture에서 관찰하지 않았다. 모든 배치/edge case 보증은 아니다.
- PER_LINE rectangle/edge-bleed 검증은 Phase 1 실패로 진행하지 않았다.

계산/검사: audit.json (로컬 자료: `audit.json`).

## F. Quarter-domain V Gaussian derivation

production `CalculateBellCurveWidth(R/2)`로 sigma를 얻었다. 임의 radius/4 커널 상수를 쓰지 않았다.

```text
N = R/2                       // production positive packed pair count
sigmaSource = productionBellWidth(N)
sY = actualSourceHeight / actualHHeight
sigmaLow = sigmaSource / sY
supportLow = floor((2*N - 1) / sY)
pairCountLow = ceil((supportLow + 1) / 2)
```

low-domain Gaussian을 이 유한 support에서 normalize하고, production과 같이 center를
양쪽에 반씩 나눠 인접 두 계수를 LINEAR sample 하나로 묶었다.
마지막 pair의 support 밖 항은 0이다. **홀수 크기에서 작은 scale 변화만으로 support/halo를
불필요하게 늘리지 않는다.** V UV step은 `1/actualHHeight`다.

| Max radius | production sigma | low sigma (sY=4) | V positive pairs | V texture reads |
|---|---:|---:|---:|---:|
| 16 | 4.79580498 | 1.19895125 | 2 | 4 |
| 24 | 7.37248039 | 1.84312010 | 3 | 6 |

H는 기존 16/24 reads 그대로다. phase-1 effective radius `r`은 두 축 offset에 `r/R`을 곱한다.
`r`은 source-pixel 반경 parameter이며 sigma 그 자체나 정확한 blur support 측정값이 아니다.

strength=1에서 unpack한 **유한 이산 커널**의 source-space sigma:
Soft `4.76276→4.72875`, Strong `7.31418→7.27670`.
이는 support truncation으로 조금 다르며 pixel-exact 동등성을 주장하지 않는다.
H Y sampling 및 Output reconstruction까지 포함한 최종 PSF sigma도 아니다.
계수 총합은 float 정밀도 내 1이고 음수 계수는 없다. 추가 tap/보정 필터를 넣지 않았다.

## G. Blur-only safe floor

| Case | 확인한 effective radii | Rsafe 판정 |
|---|---|---|
| Soft16 | 16,14,12,10,8,6,4,2 | **확정 불가: max16 자체 FAIL** |
| Strong24 | 24,22,20,18,16,14,12,10,8,6,4,2 | **확정 불가: max24에도 artifact** |

가장 강한 blur에서 실패했으므로 중간의 한 장이 덜 거슬린다는 이유로 safe floor를 고르지 않았다.
유효한 `Rsafe…Rmax` 연속 구간이 필요하다.

전체 sweep: [Soft16](comparisons/sweep-R16.png), [Strong24](comparisons/sweep-R24.png).
원본 native screenshots 20장: captures (로컬 자료: `captures/`). 확대본은 nearest-neighbor이며 명암 보정은 없다.

## H. Sharp↔Blur crossfade formulation

**미실행.** `R=lerp(Rsafe,Rmax,smoothstep(...))`, `mix(Sharp,Blur,blurMix)`는
Phase 1을 통과한 경우에만 구현하기로 했으므로 이번 후보에 적용하지 않았다.
sharpMix=0 / blurMix=1. additive glow나 별도 reconstruction은 없다.
기존 A8 color 재구성 / premultiplied RGBA Output 부분은 보존했다.

## I. Curve candidates

0개. Early/Balanced/Late Ramp sweep으로 maximum artifact를 숨기지 않았다.

## J. Selected visual curve

없음. Rsafe, radiusRampStart, mixStart, mixFull을 채택할 근거가 없다.

## K. Soft16 result

최대 반경에서도 오른쪽에 밝은 획/어두운 부분이 불균일하게 남고, 같은 문장 줄들의 외관이 다르다.
중간/약한 blur에서는 글자 모양 alias가 더 쉽게 보인다.
[native comparison](comparisons/R16-r16-native.png), [2× crop](comparisons/R16-r16-2x.png).

## L. Strong24 result

Soft16보다 무늬가 완화되지만 최대 반경에도 줄별 밝기와 획 형태의 불균일함이 남는다.
HIGH/CURRENT의 더 일정한 반복 문장과 비교하면 단순히 “더 흐리다”는 차이가 아니다.
[native comparison](comparisons/R24-r24-native.png), [2× crop](comparisons/R24-r24-2x.png).

보조 pixel 통계 (ROI x40…660, y48…485; 오른쪽은 x+960, 8-bit):

| Maximum | RMSE | maximum pixel difference | 전체 intensity ratio A/CURRENT |
|---|---:|---:|---:|
| Soft16 | 9.95 | 61 | 0.99993 |
| Strong24 | 7.15 | 43 | 1.00023 |

총 밝기가 거의 같아도 공간적 분포/무늬는 다르다. 이 숫자 자체를 quality score로 사용하지 않았다.
all checkpoints (로컬 자료: `pixel-differences.json`).

## M. Slow animation audit

Soft16 8초 영상 (로컬 자료: `slow-r16.mkv`), Strong24 8초 영상 (로컬 자료: `slow-r24.mkv`).
양쪽 같은 Animation의 amount 0→1→0이다. `[progress=1, b, effective radius, blurMix=1]`을 표시한다.
**이 영상은 blur-only branch sweep이다. 완성된 mixed Reveal 애니메이션이 아니다.**

대표 frame들에서 약/중 blur의 획 모양과 밝기 분포 변화가 보이고, strong 구간에도 불균일함이 남는다.
중간에 safe floor로 제한했다면 숨길 수 있는 weak 영역과, max에도 남는 문제를 구분했다.
[Soft contact](comparisons/slow-contact-r16.png), [Strong contact](comparisons/slow-contact-r24.png).
모든 frame의 shimmer/pop 부재를 보증하는 검증은 아니다.

## N. Forward/reverse

같은 probe amount를 H/V constraint가 읽으며 방향별 상태/캐시/hysteresis를 추가하지 않았다.
영상의 forward/reverse 대표 frame들은 같은 종류의 artifact를 되짚는다.
Source progress는 고정이므로, 실제 Reveal in/out lifecycle이나 glyph 노출 순서를 검증했다고 하지 않는다.
그 검증은 Phase 2 미진입으로 남았다.

## O. PER_LINE result

미실행. WHOLE_TEXT primary가 FAIL이다. private viewer는 WHOLE_TEXT만 지원하며,
PER_LINE/stagger/isolation까지 지원하는 production 후보로 해석하면 안 된다.

## P. RGBA/gradient smoke

미실행. A8 primary FAIL에 따라 GradientSpan/multicolor/ImageSpan으로 확대하지 않았다.

## Q. Optional ECONOMY-B

미구현. “A strong blur가 좋지만 safe floor가 높다”가 아니라 **strong blur도 FAIL**이다.
아래 B 수치는 요청한 topology의 산술 예시일 뿐, 구현/품질/성능 결과가 아니다.

## R. Structural cost comparison — not measured performance

WHOLE_TEXT full quad 기준. B는 이론만. Source 및 Output/clear/submission 비용은 filtering 합계에서 제외.

| R / Path | H pixels | V pixels | H reads/fragment | V reads/fragment | H+V filtering expressions |
|---|---:|---:|---:|---:|---:|
| 16 CURRENT | 545,724 | 136,431 | 16 | 16 | 10,914,480 |
| 16 A | 136,431 | 136,431 | 16 | 4 | 2,728,620 |
| 16 B (theory) | 272,862 | 136,431 | 16 | 8 | 5,457,240 |
| 24 CURRENT | 558,076 | 139,519 | 24 | 24 | 16,742,280 |
| 24 A | 139,519 | 139,519 | 24 | 6 | 4,185,570 |
| 24 B (theory) | 279,038 | 139,519 | 24 | 12 | 8,371,140 |

H+V logical payload bytes:

| R / Path | A8 | RGBA8 (동일 크기의 산술) |
|---|---:|---:|
| 16 CURRENT | 682,155 | 2,728,620 |
| 16 A | 272,862 | 1,091,448 |
| 16 B (theory) | 409,293 | 1,637,172 |
| 24 CURRENT | 697,595 | 2,790,380 |
| 24 A | 279,038 | 1,116,152 |
| 24 B (theory) | 418,557 | 1,674,228 |

- A는 **H+V 면적/bytes −60%, filtering expression 수 −75%**인 구조다.
- full Source까지 포함한 3 FBO 논리 payload는 **−14.29%**에 그친다 (`21Q→18Q`).
- B는 H+V bytes −40%, 같은 방식의 V kernel을 가정한 filtering expressions −50%.
- actual CPU/GPU ms, FPS, bandwidth, driver allocation, peak/RSS/VRAM 결과가 아니다.
- RGBA 열은 같은 크기라면 필요한 bytes 계산이다. RGBA fixture 실행 결과가 아니다.
- D2/PER_LINE packing에는 이 full-quad 산술을 그대로 적용할 수 없다.

structural-cost.json (로컬 자료: `structural-cost.json`).

## S. Remaining Source retention cost

ECONOMY도 full-resolution Source를 유지한다. Source 생성/retention 문제를 제거하는 구조가 아니다.
향후 mixed Output이라면 V와 Source 두 texture read 및 해당 dependency가 다시 필요하다.
**이번 Phase 1 Output은 sharp mixing을 껐으므로 compiler가 사용하지 않는 sharp read를 제거할 수 있다.**
따라서 이 viewer를 그대로 실행해 mixed product 경로의 성능이라고 측정하면 안 된다.

## T. Quality verdict

**ECONOMY QUALITY NOT ACCEPTABLE — stop after phase 1.**

저해상도 blur가 충분히 좋은 strong 영역만 활용한다는 전략을 평가했지만,
이 A topology의 max16/max24에서 그 전제부터 충족되지 않았다.
추가 pass/FBO/tap/filter 또는 넓은 crossfade로 구제하지 않았다.

## U. Recommended next step

위 비교 앱으로 사용자가 native 품질을 확인한다. 이번 A의 product 적용 및 performance screening은 보류한다.
현재 stop rule 아래서는 B/curve tuning으로 계속 확장하지 않는다.
품질 기준이나 비용 제약을 새로 정하지 않는 한 기존 HIGH/PERFORMANCE를 유지한다.

## V. Git state / reproducibility

- 원본 UI/Core/Adaptor의 HEAD·branch·index/worktree diff·dirty/untracked 파일 hash 동일.
  FINAL_STATE.json (로컬 자료: `FINAL_STATE.json`), `python3 baseline.py verify` 통과.
- 세 repo `git diff --check` 통과. audit.json (로컬 자료: `audit.json`)에 full git status와 HEAD를 기록했다.
- commit/push/reset/restore/stash/rebase/amend 없음.
- 타겟/GBS/RPM/성능측정/UTC/sanitizer/production install 없음.
- 변경 및 빌드는 이 외부 디렉터리에만 있다. 기존 보고서/실험 폴더는 수정하지 않았다.
- 캡처는 X11 readback, 영상은 X11grab이다. GL 계측 hook/timer-query/benchmark를 쓰지 않았다.
- 핵심 코드: viewer.cpp (로컬 자료: `viewer.cpp`), candidate.h (로컬 자료: `candidate.h`), build_private.py (로컬 자료: `build_private.py`).
- 재빌드: `python3 build_private.py` 후 `bash build.sh` (이 디렉터리 기준).
- 캡처: `DISPLAY=:1 python3 capture.py`; 영상: `DISPLAY=:1 python3 record.py`.
- 기준 library/objects는 `../reveal-flush-poc.2JyKvS`와 기존 DALi prefix에 의존한다.
  이 폴더 하나만 다른 머신으로 복사해 실행하는 독립 배포본은 아니다.
