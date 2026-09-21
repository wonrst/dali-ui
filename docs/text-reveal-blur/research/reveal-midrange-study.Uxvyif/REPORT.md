# Text::Reveal Blur — Mid-range Adaptive Gaussian Follow-up

2026-09-15 · production baseline `46d0ea182d77af03b8b75ab6e5480bf5d591624c`

## 결론 / Verdict

**MID-RANGE QUALITY LIMIT REACHED — KEEP CURRENT ADAPTIVE.**

이는 기존 외부 Adaptive PoC를 보존한다는 뜻이다. Production은 원래 exact24 그대로이며, 이번 후보를 production이나 이전 PoC에 적용하지 않았다.

핵심 답변은 다음과 같다.

1. **p=.2~.6 전체에서 exact24가 반드시 필요한 것은 아니다.** 시험한 kernel pool 안에서 각 지점마다 다른 kernel을 고를 수 있으면, 0.01 grid의 모든 지점에서 20 pair 이하가 가능했다.
2. 그러나 **소수의 immutable kernels로 연속 애니메이션을 안전하게 덮는 문제는 별개**다. 제한한 후보군/8-set 상한에서 고른 매핑은 최대 22 pairs, 중간 평균 19.65 pairs였다. 이 평균은 sampled-grid 추정치다.
3. 이 매핑은 coarse 123/123 비교를 통과했지만, 경계를 .0025까지 세분하면 **213개 중 10개 비교, 9개 서로 다른 progress에서 2 LSB**가 발생했다. 한글에서 단일 threshold 이동으로 해결되지 않는 통과/실패 역전도 확인했다.
4. 요청한 중단 조건에 따라 추가 kernel/분기/fallback을 계속 붙이지 않았다. **새 runtime shader, GPU/CPU benchmark, slow video, target candidate는 만들지 않았다.** 따라서 이번 후보의 실제 성능 개선율은 없다.

이는 모든 Gaussian 근사의 수학적 불가능성을 증명한 결과가 아니다. **이번 mass-preserving grouping·후보군·FHD corpus·1 LSB 기준에서, 작은 고정 kernel 집합의 실용적인 한계가 확인된 것**이다. 2 LSB를 허용하도록 기준을 낮추지 않았다.

## A. Current limitation / 실험 기준

기존 Adaptive는 20@.50의 고정 kernel이 FHD 중간 구간에서 2 LSB를 내어 .2~.6을 exact24로 보호했다. 과거 p=.295 실패도 [이전 보고서](../reveal-adaptive-study.8A0C8e/REPORT.md)에 기록되어 있다. 이런 실패는 .01 grid만으로는 놓칠 수 있다.

이번의 **exact는 PERFORMANCE 해상도에서 production Gaussian 24 pairs를 사용한다는 뜻**이다. HIGH/full-resolution과의 비교가 아니다.

- PIXEL / WHOLE_TEXT / Fade=1 / BlurTime=1 / radius48 / PERFORMANCE.
- UI scale=1, render scale=1, font-size 설정 24, window/Label 1920×1080.
- Korean/RGBA: 이전 FHD 한글 문단, actual raster 1920×1054, 31줄.
- Latin: AVATAR / ffi / office / IIII / WWWW 및 thin/wide-stroke 문장 반복. Actual raster 1920×616, 22줄. FHD Label/FBO를 사용하며, 아래 빈 공간을 감안해 별도 ROI도 계산했다.
- Source/H/V = **2020×1180 / 505×1180 / 505×295**, 이전 quarter 경로 그대로.
- RGBA는 같은 한글에 color span + 기존 linear gradient를 적용했다.
- GTX 1650, NVIDIA 595.91.07, GLES/X11, window MSAA=4.
- 기존 `kernels.h`의 production kernel 추출 및 EffectiveGroups를 재사용했다. Normalized mass, 양수 weight, monotonic offset, symmetry, positive first moment를 검사했다.
- 실제 DALi Source/H/V/Output을 렌더링하여 GL framebuffer를 읽었다. Source에는 이미 fade가 적용되어 있으므로 결과에 opacity를 다시 곱하지 않았다.

Private24 calibration: 41 points × 3 corpora. A8/Latin은 모두 Output byte-identical, RGBA는 1개 지점에서 최대 1 LSB, 나머지는 동일했다. 모든 candidate acceptance는 private24가 아니라 **원본 production shader Output**을 기준으로 했다.

## B. Fine Progress Envelope

아래 수치는 **시험한 grouping/candidate 안에서의 최소값**이다. 최종 framebuffer를 직접 최소화하는 전역 최적해나 수학적 lower bound는 아니다.

- Own: 해당 p에서 EffectiveGroups를 생성한 결과.
- Pool: 다른 anchor에서 만든 kernel까지 포함하여, **지점마다 자유롭게 선택**한 결과.
- 2 LSB 열은 진단용이며 후보 승인에는 사용하지 않았다.

| p | effective radius | Own ≤1 LSB | Pool ≤1 LSB | Pool ≤2 LSB |
|---:|---:|---:|---:|---:|
| 0.20 | 43.008 | 16 | 16 | 14 |
| 0.21 | 42.539 | 16 | 16 | 14 |
| 0.22 | 42.053 | 18 | 18 | 14 |
| 0.23 | 41.550 | 18 | 18 | 14 |
| 0.24 | 41.033 | 18 | 16 | 14 |
| 0.25 | 40.500 | 18 | 18 | 14 |
| 0.26 | 39.953 | 18 | 18 | 16 |
| 0.27 | 39.392 | 18 | 18 | 14 |
| 0.28 | 38.818 | 18 | 18 | 14 |
| 0.29 | 38.231 | 20 | 20 | 16 |
| 0.30 | 37.632 | 22 | 20 | 16 |
| 0.31 | 37.022 | 20 | 20 | 16 |
| 0.32 | 36.400 | 22 | 20 | 16 |
| 0.33 | 35.768 | 18 | 18 | 14 |
| 0.34 | 35.127 | 20 | 20 | 16 |
| 0.35 | 34.476 | 20 | 20 | 16 |
| 0.36 | 33.817 | 22 | 20 | 14 |
| 0.37 | 33.149 | 24 | 20 | 16 |
| 0.38 | 32.474 | 20 | 20 | 16 |
| 0.39 | 31.792 | 20 | 20 | 16 |
| 0.40 | 31.104 | 20 | 18 | 14 |
| 0.41 | 30.410 | 22 | 20 | 14 |
| 0.42 | 29.711 | 20 | 20 | 14 |
| 0.43 | 29.007 | 20 | 20 | 16 |
| 0.44 | 28.299 | 20 | 20 | 14 |
| 0.45 | 27.588 | 20 | 20 | 16 |
| 0.46 | 26.874 | 20 | 20 | 16 |
| 0.47 | 26.157 | 18 | 18 | 14 |
| 0.48 | 25.439 | 20 | 20 | 14 |
| 0.49 | 24.720 | 20 | 20 | 14 |
| 0.50 | 24.000 | 22 | 20 | 16 |
| 0.51 | 23.280 | 20 | 20 | 16 |
| 0.52 | 22.561 | 18 | 18 | 14 |
| 0.53 | 21.843 | 20 | 20 | 16 |
| 0.54 | 21.126 | 16 | 16 | 16 |
| 0.55 | 20.412 | 18 | 18 | 14 |
| 0.56 | 19.701 | 16 | 16 | 14 |
| 0.57 | 18.993 | 16 | 16 | 14 |
| 0.58 | 18.289 | 16 | 16 | 12 |
| 0.59 | 17.590 | 16 | 16 | 12 |
| 0.60 | 16.896 | 16 | 16 | 12 |

세 corpus가 모두 통과해야 한다. Own의 중간 sampled 평균은 **19.10**, Pool은 **18.65**다. Pool의 최대는 20이지만, 이것이 소수 immutable kernel의 연속 coverage를 뜻하지는 않는다.

생성 objective는 bilinear impulse-response group SSE이며 실제 화면의 max error와 동일하지 않다. 그래서 다른 anchor의 kernel이 own-anchor kernel보다 final max error를 낮추는 경우도 있었다. 예: p=.50의 A8는 20@.50에서 2 LSB, 20@.475에서 1 LSB.

전체 progress/opacity/strength/scaled offsets: trajectory.csv (로컬 자료: `trajectory.csv`). Envelope: fine-envelope.csv (로컬 자료: `fine-envelope.csv`).

## C. Candidate Kernel Library

처음에는 .20/.25/.30/.35/.40/.45/.50/.55/.60에서 12/14/16/18/20/22를 생성하고 주변 ±.05를 실제 GL로 검사했다. 기존 16@.50 / 20@.50은 중간 전체를 검사했다.

실패 지점에는 .275/.325/.375/.425/.475/.525를 추가하고, p=.50에서 이미 생성한 .49/.51 kernels도 재사용했다. 이후 유망한 20@.475, 20@.51, 18@.25, 두 22-pair kernel, 상단 비교용 20@.575의 **6개만** 중간 전체로 검증 범위를 확장했다.

Anchor/count 조합은 97개, 계수가 같은 것을 합치면 **78개 고유 diagnostic sets**다. 이 78개를 runtime shader에 넣은 것이 아니다.

선택된 중간 구간의 6개 sets는 다음과 같다. 바깥 구간용 2/12를 더하면 총 8개다.

| kernel | coefficient floats | .01 grid에서 관찰한 유효 구간 |
|---|---:|---|
| 기존 16@.50 | 32 | .20 |
| 기존 20@.50 | 40 | .20–.31, .39, .46, .53, .55, .57, .59–.60 |
| 18@.20 (=18@.25) | 36 | .20–.28 |
| 22@.20 (=22@.40/.45 등) | 44 | .20–.60 |
| 20@.275 (=20@.51 등) | 40 | .20–.29, .31, .34, .36–.38, .40, .42–.51, .53, .55–.57, .59–.60 |
| 20@.475 | 40 | .20–.28, .30–.31, .33, .35, .37, .39–.41, .43, .45, .47–.51, .53–.58, .60 |

**표의 구간은 연속적인 품질 보증이 아니라 .01 grid의 연속 통과점을 묶은 것**이다. 실제로 더 세밀하게 검사하면 내부에서 실패했다.

전체 aliases/intervals: kernel-library.json (로컬 자료: `kernel-library.json`). 전체 coefficients: kernel-coefficients.csv (로컬 자료: `kernel-coefficients.csv`).

## D. Coverage Search

동일 coefficients를 합친 후, 기존 바깥 구간의 2/12/16/20을 보존하고 추가 sets 0~4개를 조합했다. 미검증 지점은 invalid로 취급했다. 목적 함수는 **peak pairs → sampled 평균 pairs → set 수**이며, 전체 sets를 8개 이내로 제한했다.

| 추가 sets | 전체 sets | middle peak | middle sampled 평균 | full sampled 평균 | exact24 full-time estimate |
|---:|---:|---:|---:|---:|---:|
| 0 | 5 | 24 | 22.15 | 17.22 | 22.0% |
| 1 | 5 | 22 | 21.05 | 16.78 | 0.0% |
| 2 | 6 | 22 | 20.30 | 16.48 | 0.0% |
| 3 | 7 | 22 | 19.90 | 16.32 | 0.0% |
| 4 | 8 | 22 | 19.65 | 16.22 | 0.0% |

이는 **선정한 후보 pool과 측정 grid 안의 조합 검색 결과**다. 모든 가능한 coefficients/anchors에 대한 최적성 증명은 아니다.

- Question A: 이 조건의 작은 set으로 중간 전체 peak≤20인가? **아니오.**
- Question B: exact24 fallback을 15% 이하로 줄이는 sampled cover가 있는가? **예.**
- 하지만 최종 품질 gate에는 boundary 검사도 포함된다. 아래 F의 실패로 runtime PoC 진입은 취소했다.

8-set 후보는 .32/.52의 sample에서 22, 나머지 대부분에서 18/20을 선택했다. 같은 pair count에서는 전환 수를 최소화했지만 **12개 middle thresholds**가 필요했다.

Sampled 평균은 전체 **16.22 pairs**지만, 실제 제안 threshold의 길이를 합하면 중간 **19.80**, 전체 **16.28 pairs**다. 이 둘을 혼동하지 않는다.

| 항목 | exact | 이전 Adaptive | 이번 제안 매핑 — 미승인 |
|---|---:|---:|---:|
| 전체 평균 pairs | 24 | 17.96 | 16.28 |
| active Gaussian 명목 reads/pass | 48 | 35.92 | 32.56 |
| exact 대비 명목 fetch 감소 | 0% | 25.17% | 32.17% |
| 중간 평균 pairs | 24 | 24 | 19.80 |
| 중간 maximum pairs | 24 | 24 | 22 |
| 중간 exact24 fallback | 전체 .2–.6 | 전체 .2–.6 | 0 — 품질 검증 실패 |
| 중간 p=.50 | 24 | 24 | 20 — 해당 지점은 통과 |

새 매핑의 명목 fetch는 이전 Adaptive 대비 9.35% 적지만, **품질 실패 후보의 산술값일 뿐 GPU 개선 결과가 아니다**. Endpoint의 기존 early-return은 그대로다.

자료: coverage-solutions.json (로컬 자료: `coverage-solutions.json`), selected-map.json (로컬 자료: `selected-map.json`).

## E. Quality

다음은 선택된 강제 kernel의 실제 GL Output을 매핑에 대입한 결과다. 아직 하나의 새 runtime branch shader를 실행한 결과가 아니다.

| 검사 | A8 Korean | RGBA Korean | Latin |
|---|---|---|---|
| .01 grid | 41/41 ≤1 LSB | 41/41 ≤1 LSB | 41/41 ≤1 LSB |
| fine boundary | 65/71 ≤1 LSB | 68/71 ≤1 LSB | 70/71 ≤1 LSB |
| fine boundary 최대 오차 | 2 LSB | 2 LSB | 2 LSB |

Coarse grid에서는 **123/123**, fine boundary에서는 **203/213**이 통과했다. 실패는 10개 corpus/progress comparisons, 서로 다른 progress로는 9개다.

새 middle 영역(p>.2)의 coarse ROI metric 최대값은 다음과 같다. 서로 다른 frame에서 발생한 metric별 최대치이며 평균 frame의 수치가 아니다.

| corpus | max | 최대 ROI MAE | 최대 ROI RMSE | 최대 p99 | 최대 changed pixels 비율 |
|---|---:|---:|---:|---:|---:|
| A8 Korean | 1 | .13572 | .36841 | 1 | 13.57% |
| RGBA Korean | 1 | .09870 | .31417 | 1 | 25.55% |
| Latin | 1 | .14550 | .38144 | 1 | 14.55% |

1 LSB가 넓게 퍼질 수 있다는 점도 포함했다. Axis-projected periodic residual의 최대 amplitude는 이 영역에서 A8 .02408 / RGBA .01746 / Latin .03961 LSB였다. Glyph 반복 주기까지 포함하므로 단독 grid classifier로 사용하지 않았다.

4× crops에서 이전 fixed CAP6처럼 큰 격자를 확인한 것은 아니다. 이번 reject 이유는 **정량 hard target 위반과 kernel validity의 불연속적인 변화**다. 예를 들어 p=.50의 20@.50은 A8에서 단 1개 pixel이 2 LSB였고, p=.37 own22는 RGBA에서 2개 pixels가 2 LSB였다. 작다고 해서 허용하지 않았다.

- [한글 p=.50, 20/22 비교](korean-p50-limits-4x.png)
- [RGBA p=.37 비교](rgba-p37-limits-4x.png)
- [경계에서 통과/실패가 바뀌는 한글 crop](boundary-crossing-4x.png)

H/V/Output의 full-frame 및 ROI max/MAE/RMSE/p99/changed-pixel/periodic metrics는 각 `*-metrics.json`에 보존했다. Selected 결과: selected-quality.json (로컬 자료: `selected-quality.json`), selected-quality-summary.json (로컬 자료: `selected-quality-summary.json`).

## F. Temporal Boundaries — 중단 사유

각 제안 threshold에서 ±.010 / ±.005 / ±.0025 / threshold를 검사했다. 겹치는 지점을 합치면 71 progress values다.

| threshold | low → high progress kernel | kernel-to-kernel max RGB | 측정한 local window 내 단일 threshold로 양쪽 품질 유지 |
|---:|---|---:|---|
| 0.2000 | 16@0.500 → 18@0.200 | 1 LSB | 0.2000 |
| 0.2800 | 18@0.200 → 20@0.500 | 1 LSB | 0.2700, 0.2750, 0.2775, 0.2800 |
| 0.3100 | 20@0.500 → 22@0.200 | 1 LSB | 0.3000 |
| 0.3300 | 22@0.200 → 20@0.475 | 1 LSB | 찾지 못함 |
| 0.3350 | 20@0.475 → 20@0.275 | 2 LSB | 찾지 못함 |
| 0.3450 | 20@0.275 → 20@0.475 | 2 LSB | 찾지 못함 |
| 0.3550 | 20@0.475 → 20@0.275 | 2 LSB | 찾지 못함 |
| 0.3850 | 20@0.275 → 20@0.475 | 1 LSB | 0.3850, 0.3875 |
| 0.4150 | 20@0.475 → 20@0.275 | 1 LSB | 0.4125, 0.4150, 0.4175 |
| 0.5100 | 20@0.275 → 22@0.200 | 1 LSB | 0.5000, 0.5050, 0.5075, 0.5100, 0.5125, 0.5150 |
| 0.5300 | 22@0.200 → 20@0.475 | 1 LSB | 0.5275, 0.5300, 0.5325, 0.5350, 0.5400 |
| 0.5850 | 20@0.475 → 20@0.500 | 2 LSB | 찾지 못함 |

Local 검사는 이웃 구간을 고정한 상태에서, 이미 측정한 근처 threshold 후보를 옮기는 경우다. 모든 가능한 매핑이 불가능하다는 뜻은 아니다. Kernel-to-kernel 2 LSB와 exact에 대한 2 LSB도 별개로 기록했다.

특히 A8에서 다음 패턴이 나온다.

| p | 20@.475의 exact 대비 max | 20@.275의 exact 대비 max | 통과하는 쪽 |
|---:|---:|---:|---|
| .3550 | 2 | 1 | .275 |
| .3575 | 1 | 2 | .475 |
| .3600 | 2 | 1 | .275 |

같은 두 kernel 사이의 **단일 monotonic threshold를 옮기는 것으로 이 세 지점을 모두 통과시킬 수 없다**. Reverse에서는 위 순서가 반대가 되지만 문제는 같다.

선택된 매핑의 실패 지점:

- A8: .3050, .3075, .3325, .3350, .3550, .3575.
- RGBA: .3350, .3650, .5850.
- Latin: .5825.

이 문제를 메우려면 추가 구간 분할 또는 더 높은 pair fallback과 재검증이 필요하다. 요청의 stop condition에 따라 이를 계속 추가하며 맞추지 않았다. **Slow animation 녹화는 실행하지 않았고, temporal pop이 없다고 주장하지 않는다.** 정적 boundary framebuffer 단계에서 이미 1 LSB gate를 실패했기 때문이다.

원자료: boundary-check.json (로컬 자료: `boundary-check.json`), selected-boundaries.csv (로컬 자료: `selected-boundaries.csv`).

## G. Runtime Shader

**PHASE B 미실행.**

- 새 combined branch shader/variant를 생성하지 않았다.
- 새 8-set shader의 source size, compile wall, cache size, branch flattening은 측정하지 않았다.
- 이전 5-tier shader가 host에서 flatten되지 않았다는 결과를 새 후보에 일반화하지 않았다.
- Native Phase A의 private per-count uniform shader는 diagnostic capture용이며, 이를 per-frame animation 구현으로 제공하지 않았다.
- 복사해 둔 기존 `adaptive.inc` 역시 이전 PoC와 동일하다.

## H. Mid-range GPU

새 후보의 품질 gate가 실패했으므로 **mid-range GPU 개선율은 측정하지 않았다**. Pair 감소를 GPU 시간 감소로 보고하지 않는다.

이전 보고서의 p=.50 A8 single-checkpoint 기록은 다음과 같다. 신규 측정이 아니며, 반복 full-animation 평균과도 직접 비교하지 않는다.

| version | H | V | draw total |
|---|---:|---:|---:|
| production exact24 | .3842 ms | .1046 ms | .7056 ms |
| 이전 Adaptive — p=.50은 24 pairs | .4061 ms | .1134 ms | .7413 ms |
| 이번 후보 | 미측정 | 미측정 | 미측정 |

이번 요청의 .60/.55/.50/.45/.40/.35/.30/.25/.20 상세 신규 비교는 품질 gate 이후에만 하도록 했으므로 실행하지 않았다. 과거 데이터로 없던 p=.55/.45/.35/.25 수치를 보간해서 만들지 않았다.

## I. Full Reverse GPU

아래는 [이전 PoC 보고서](../reveal-adaptive-study.8A0C8e/REPORT.md)의 **기존 측정**이다. 이번에 재측정하지 않았다.

1초 Linear reverse × 6 loops, 3 independent processes, Source/H/V/Output draw GPU 합계의 ms/frame:

| corpus | exact24 | 이전 Adaptive | 이번 후보 |
|---|---:|---:|---|
| A8 | .8142 | .7072 (-13.1%) | 미측정 / 미승인 |
| RGBA | 1.1577 | .9483 (-18.1%) | 미측정 / 미승인 |

이는 전체 frame time/FPS가 아니라 draw GPU 시간이다. 이번 작업으로 peak/average GPU가 추가로 개선됐다고 주장할 수 없다.

## J. CPU / Memory

새 runtime이 없으므로 **추가 per-frame CPU 변화는 측정하지 않았다**. Offline grouping/capture/metrics의 CPU 시간을 runtime 비용으로 사용하지 않았다.

이번 native quality fixture에서 Source/H/V 크기·format·pipeline은 유지했다.

| FBO logical payload | A8 | RGBA |
|---|---:|---:|
| Source+H+V | 3,128,475 B / 2.984 MiB | 12,513,900 B / 11.934 MiB |

이는 driver VRAM allocation, window MSAA, shader/cache/RSS를 포함하지 않는 논리 texture 저장량이다. 새 FBO/RenderTask/Actor를 추가하지 않고 기존 H/V renderer의 private diagnostic shader만 교체했다.

8-set 제안의 coefficient 숫자만 계산하면 총 130 pairs × 2 floats = **1,040 B**다. 이전 5-set의 592 B와 비교하면 +448 B지만, **실제 shader/program-cache 메모리를 뜻하지 않는다**. 해당 runtime shader를 구현하지 않았으므로 compile/cache 비용은 모른다.

## K. Complexity

- Offline: 97 anchor/count IDs, identical coefficients를 합쳐 78 sets.
- Proposed runtime: 8 sets, 12 middle thresholds.
- Coarse .01 grid에서 budget 최소화와 전환 최소화를 동시에 적용해도, finer 검사에서 품질 실패.
- 단순 22-pair kernel은 중간 전체 .01 grid를 통과했다. 그러나 24→22는 pass의 명목 reads를 48→44로 줄일 뿐이다. 이것으로 목표한 중간 GPU 8~10% 이상 개선이 나온다고 단정할 수 없으며 측정하지 않았다.
- 20-pair 구간을 유지하려고 더 많은 fallback/threshold를 추가하면, 새로운 세부 구간에 대한 검증이 다시 필요하다. 이번에는 그 확장을 하지 않았다.

## L. Target Recommendation

**MID-RANGE QUALITY LIMIT REACHED — KEEP CURRENT ADAPTIVE.**

기존 exact24 fallback에는 보수적인 부분이 있다. 다만 이번 결과는 **20 pair 이하의 작은 고정 kernel 집합으로 middle-range peak를 안정적으로 해결했다는 결과가 아니다**.

Point-specific 가능성과 연속 runtime 가능성의 차이가 실제 GL에서 확인되었다. 현재 승인된 이전 PoC와 untouched production을 유지하는 것이 이번 작업의 결론이다.

- 이번 후보용 target ON/OFF build는 제공하지 않는다.
- 기존 외부 PoC의 [target instructions](../reveal-adaptive-study.8A0C8e/TARGET.md)는 그대로다.
- 타겟 성능을 통과했다고 보고하지 않는다.
- 품질 기준을 2 LSB로 완화하거나 HIGH/PER_LINE/radius/BlurTime 일반화로 확대하지 않았다.

## M. Working Tree / 재현 자료

- UI HEAD: `46d0ea182d77af03b8b75ab6e5480bf5d591624c`, production worktree/index clean.
- Foundation library SHA256: `aaee5bd2599f297e88a9098f70b09a5b01ec67cfbd4daa0f21d6f46328811f43`, 시작 시와 동일.
- 기존 adaptor의 별도 +13-line 변경은 그대로 보존했다.
- 이전 `reveal-adaptive-study.8A0C8e`도 수정하지 않았다.
- commit / stage / amend / rebase / push 없음.
- Full build, regression, UTC, sanitizer 없음. 외부 quality diagnostic만 빌드/실행했다.

Native capture cases:

| stage | cases, 세 corpus 합계 |
|---|---:|
| own-point sweep | 987 |
| anchor interval library | 1,908 |
| focused anchors | 255 |
| bounded interval extension | 864 |
| boundary sweep | 663 |
| 합계 | **4,677** |

Case 수에는 exact reference와 private24 calibration도 포함한다. H/V/Output capture를 각각 별도 case로 부풀리지 않았다.

재현 코드: quality.cpp (로컬 자료: `quality.cpp`), kernels.h (로컬 자료: `kernels.h`), make-manifest.py (로컬 자료: `make-manifest.py`), run-quality.sh (로컬 자료: `run-quality.sh`), analyze.py (로컬 자료: `analyze.py`), coverage.py (로컬 자료: `coverage.py`), select-map.py (로컬 자료: `select-map.py`), boundary-check.py (로컬 자료: `boundary-check.py`).

원시 캡처는 `raw-captures.tar.zst`(5,600,742,830 bytes)로 무손실 압축했고, `tar --compare`로 원본과 일치함을 확인했다. 압축본 검증 후 중복 원시 디렉터리만 정리했다. Metrics/coefficients/그림/코드는 압축 밖에도 남는다.

원시 자료 복원:

```bash
cd /home/bowonryuubuntu/tizen/reveal-midrange-study.Uxvyif
tar --zstd -xf raw-captures.tar.zst
```
