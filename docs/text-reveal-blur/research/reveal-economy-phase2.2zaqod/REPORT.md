# Text::Reveal ECONOMY Phase 2 — perceptual mixed effect

## A. Executive Verdict

**ECONOMY-A MARGINAL / B CLEARLY IMPROVES QUALITY**

Phase 1의 Gaussian fidelity 기준으로 다시 중단하지 않았다. A의 싼 topology를 유지하면서
radius와 sharp/blur opacity를 분리한 네 가지 effect를 실제 `Source(progress)`로 구현했다.
그중 **BALANCED**가 가장 무난한 비교 기본값이다.

- A mixed는 약한 저해상도 blur를 직접 보여주는 대신 sharp를 먼저 줄이고 이미 흐린 layer를 드러낸다. 단순 blur-only와는 분명 다른 전환이다.
- 그러나 sharp가 사라진 뒤에는 A의 줄별 밝기 차이·획 형태의 얼룩이 다시 드러난다. Soft16에서 특히 쉽게 보인다. opacity 0.9는 강도를 줄일 뿐 무늬를 없애지는 않는다.
- sharp를 더 빨리 없애는 두 preset은 중간 opacity 감소와 이후 blur의 재등장이 더 강해진다. 품질을 구하기 위해 alpha를 계속 낮추는 방향은 선택하지 않았다.
- 같은 곡선의 B는 strong 구간의 줄·획 modulation을 눈에 띄게 줄인다. **B는 원인/품질 비교용이지 자동 상품화 대안이 아니다.**

따라서 현재 판단은 **A 상품화 승인 보류**다. CURRENT와 다르다는 이유가 아니라,
최종 mixed effect에도 남는 구조적 무늬와 sharp/blur의 이중 layer 느낌 때문이다.
아래 viewer/video로 사용자가 실제 ECONOMY UX 허용 수준을 직접 판단할 수 있게 했다.
성능 benchmark 및 PER_LINE/gradient 확대는 진행하지 않았다.

### 바로 비교하기

```bash
# 기본: Strong24, Balanced, CURRENT | A mixed
bash /home/bowonryuubuntu/tizen/reveal-economy-phase2.2zaqod/run-current-a.sh

# A blur-only | A mixed
bash /home/bowonryuubuntu/tizen/reveal-economy-phase2.2zaqod/run-a-layers.sh

# CURRENT | B mixed
bash /home/bowonryuubuntu/tizen/reveal-economy-phase2.2zaqod/run-current-b.sh

# A mixed | B mixed
bash /home/bowonryuubuntu/tizen/reveal-economy-phase2.2zaqod/run-a-b.sh
```

시작 후 **Space**로 8초 왕복, **S** Soft16 / **D** Strong24.
한 번에 viewer 하나만 실행한다. 이 스크립트는 원본 install을 바꾸지 않고 private library를 사용한다.

| 조작 | 동작 |
|---|---|
| 1 / 2 | HIGH / CURRENT 단독, 기존 shader·timing 유지 |
| 3 / 4 | A blur-only / A mixed |
| 5 / 6 | CURRENT\|A mixed / CURRENT\|A-only\|A-mixed |
| 7 / 8 / 9 | B-only / B-mixed / A-mixed\|B-mixed |
| 0 / L | CURRENT\|B-mixed / A-only\|A-mixed |
| Z / X / C / V | Balanced / Early Fade / Aggressive / Conservative |
| Q W E R T Y U I O P K J | b = 0, .05, .10, .15, .20, .25, .30, .40, .50, .65, .80, 1 정지점 |
| Space | b 0→1→0: sharp→blur→sharp, 실제 Reveal p 1→0→1 |
| Enter | 실제 Reveal p 0→1→0, 등장부터 왕복 |
| [ / ] | radius floor −/+1 px |
| , / . | sharp fade 종료 b −/+.05 |
| ; / ' | blur 최대 alpha −/+.05 |
| Esc | 종료 |

debug 조절은 수동 확인용이며 보고서의 선택 값에는 반영하지 않았다. Z/X/C/V로 초기화한다.
화면의 radius/alpha 수치는 **선택된 ECONOMY mixed 곡선**이다. CURRENT/HIGH는 해당 값으로
변경되지 않으며, blur-only 화면의 실제 alpha는 sharp=0/blur=1이다.

## B. Why Phase 1 stopped

[Phase 1 보고서](../reveal-economy-quality.fxgYr6/REPORT.md)의 A blur-only는 Soft16/Strong24에서
최대 blur까지 줄·획 형태의 unevenness가 남아 strict safe-floor gate를 통과하지 못했다.
H에서 Y low-pass 전에 4× 줄이는 구조가 주요 원인 후보였다.
그 결과물이나 결론은 수정하지 않았다.

## C. Why Phase 2 was intentionally continued

이번에는 정확한 weak Gaussian을 목표로 하지 않는다. sharp core fade와 already-blurred
layer의 합성이 저사양용 완성 effect로 자연스러운지를 평가했다.
Galaxy 관찰은 시각적 참고일 뿐 해당 제품의 구현 사실로 가정하지 않았다.

환경은 Ubuntu / GTX 1650 / NVIDIA 595.91.07 / GLES / window MSAA 4.
Label 1920×1080, font 24px, 아래 같은 문장을 12줄 사용했다.

> 따뜻한 햇살 아래 아름다운 우리들의 이야기가 시작됩니다

흰 text/검정 배경, Unit::LINE, WHOLE_TEXT, Fade0, Stagger .25, BlurDurationRatio1.
분할 화면은 같은 크기의 Label을 원래 픽셀 크기로 clip하며, 글꼴이나 layout을 축소하지 않는다.
따라서 2분할은 각 Label의 왼쪽 960px, 3분할은 왼쪽 640px를 보여준다.
단독 모드에서는 더 넓은 영역을 볼 수 있다.

Phase 1과 같은 private CURRENT production objects/common libraries를 재사용했다.
원본 dirty tree의 one-tap/source-only 등 진단 경로를 CURRENT reference로 사용하지 않았다.
`reveal-flush-poc.2JyKvS`의 production runtime에 allocation overlay 한 object만 새로 빌드했다.
Core/Adaptor 및 설치 library는 다시 빌드하거나 수정하지 않았다.
빌드 기록 (로컬 자료: `build-record.json`), overlay diff (로컬 자료: `runtime-overlay.diff`).

## D. A topology

```text
Source(progress), full W×H
    → H ceil(W/4)×ceil(H/4)
    → V ceil(W/4)×ceil(H/4)
    → Output = current sharp Source × sharpAlpha + V × blurAlpha
```

H를 처음부터 해당 크기로 할당한다. 교체 전 큰 H를 잡아두는 방식이 아니다.
H의 X Gaussian은 원래 weights/offsets/read 수를 보존한다.
V만 축소된 입력 좌표계의 equivalent kernel로 구성하며 Phase 1과 같은 계산을 사용한다.
추가 prefilter/reconstruction/tap/texture/FBO/task는 없다.
실험 후보는 full H quad를 사용하며 production D2 band를 후보에 섞지 않았다.
CURRENT/HIGH는 기존 geometry·shader 그대로다.

## E. B topology

```text
Source(progress), full W×H
    → H ceil(W/4)×ceil(H/2)
    → V ceil(W/4)×ceil(H/4)
    → 동일한 Output 합성
```

A/B의 radius/alpha 곡선은 완전히 동일하다. 차이는 H의 Y 해상도와 그 입력에 맞는 V kernel이다.
V offset/clamp는 실제 H 크기에 맞췄다. full-space sigma를 실제 Y 축소율로 나누고,
support도 같은 축소율로 변환한 뒤 bilinear pair weights를 정규화한다.
이는 이산 filtering 결과가 CURRENT와 정확히 같다는 주장은 아니다.

## F. A/B structural cost

실제 RenderTask에서 조회한 크기다. halo 포함 Source 크기이며 모든 측정 fixture는 A8.
각 Label당 Source/H/V **3 task, 3 FBO**다. window main task와 화면 합성은 이 개수에서 제외한다.

| radius / path | Source | H | V | H/V reads per fragment |
|---|---:|---:|---:|---:|
| 16 CURRENT | 1956×1116 | 489×1116 | 489×279 | 16 / 16 |
| 16 A | 동일 | 489×279 | 489×279 | 16 / 4 |
| 16 B | 동일 | 489×558 | 489×279 | 16 / 8 |
| 24 CURRENT | 1972×1132 | 493×1132 | 493×283 | 24 / 24 |
| 24 A | 동일 | 493×283 | 493×283 | 24 / 6 |
| 24 B | 동일 | 493×566 | 493×283 | 24 / 12 |

| radius / path | H pixels | V pixels | H+V A8 bytes | H+V RGBA8 bytes* | Source+H+V A8 bytes | filtering expressions |
|---|---:|---:|---:|---:|---:|---:|
| 16 CURRENT | 545,724 | 136,431 | 682,155 | 2,728,620 | 2,865,051 | 10,914,480 |
| 16 A | 136,431 | 136,431 | 272,862 | 1,091,448 | 2,455,758 | 2,728,620 |
| 16 B | 272,862 | 136,431 | 409,293 | 1,637,172 | 2,592,189 | 5,457,240 |
| 24 CURRENT | 558,076 | 139,519 | 697,595 | 2,790,380 | 2,929,899 | 16,742,280 |
| 24 A | 139,519 | 139,519 | 279,038 | 1,116,152 | 2,511,342 | 4,185,570 |
| 24 B | 279,038 | 139,519 | 418,557 | 1,674,228 | 2,650,861 | 8,371,140 |

*RGBA8 열은 동일 dimensions를 4 bytes/pixel로 환산한 산술값이며 RGBA quality 실행 결과가 아니다.

필터링 식 수는 `H_pixels × H_reads + V_pixels × V_reads`다. full quad의 shader texture
expression 기준이며 blank-fragment 조기 return, GPU compiler/cache/latency 효과는 반영하지 않는다.
Source raster/clear/Output/submission도 제외한다. **실제 GPU 시간·FPS·메모리 bandwidth 수치가 아니다.**

bytes는 논리 texture payload이며 driver allocation/alignment/RSS/VRAM 실측이 아니다.
원본 text/metadata textures, CPU buffers, window MSAA 등은 포함하지 않는다.
실제 inventory 로그는 각 capture 폴더 `app.log`, 전체 산술은 structural-cost.json (로컬 자료: `structural-cost.json`).

## G. Curve formulation

`S(a,z,b)=t²(3−2t)`, `t=clamp((b−a)/(z−a),0,1)`.

```text
radius     = floor + (Rmax − floor) × S(rampStart, 1, b)
sharpAlpha = 1 − S(0, sharpEnd, b)
blurAlpha  = min(1 − sharpAlpha, blurMax × S(blurStart, blurEnd, b))
final      = Source(progress) × sharpAlpha + Blur(Source(progress)) × blurAlpha
```

H/V kernel allocation/tap count는 Rmax 기준으로 고정하고 sample offset strength만
`radius/Rmax`로 바꾼다. radius floor는 artifact-free safe floor라는 뜻이 아니다.
sharp가 사라질 때까지 weak low-resolution glyph를 주 visual로 쓰지 않기 위한 effect parameter다.

`b`는 별도 animation clock이 아니라 **기존 renderer의 `uAnimationRatio`**, 즉 현재 native blur amount다.
이번 WHOLE fixture의 실제 blur start=0/duration=1을 확인했다.

```text
q = clamp((RevealProgress − sequenceStart) / nativeBlurDuration, 0, 1)
b = 1 − q²(3−2q)
```

정지점은 위 식을 역산해서 실제 Reveal progress를 설정한다. H/V의 mapped radius가 일치하는지
capture 시 검사했다. live animation에서는 update-side constraint로 기존 b를 읽는다.
40ms timer는 viewer 상태 표시/촬영 제어용이며 effect animation을 25fps로 구동하는 것이 아니다.

**b=1은 p=0이므로 실제 Source도 빈 화면이다.** b=1이 검게 나오는 것을 max-radius 품질 PASS로
사용하지 않았다. 강한 blur 평가는 b=.65/.8 및 영상의 p>0, b≈.97 구간을 사용했다.
Unit::LINE + Fade0에 따른 줄의 등장/퇴장 단계는 CURRENT/A/B 모두 공유하는 기존 Reveal 동작이다.

## H. Curve presets

네 family를 캡처 전에 고정했다. 계수 sweep/반복 hand tuning은 하지 않았다.

| preset | floor16 / floor24 | rampStart | sharpEnd | blurStart / blurEnd | blurMax | 최소 alpha 합 |
|---|---:|---:|---:|---:|---:|---:|
| BALANCED | 10 / 12 | .35 | .60 | 0 / .65 | .90 | .8515 (b≈.479) |
| EARLY FADE | 10 / 12 | .40 | .40 | .02 / .50 | .85 | .6848 (b≈.315) |
| AGGRESSIVE | 12 / 14 | .55 | .28 | .04 / .45 | .75 | .4166 (b≈.238) |
| CONSERVATIVE | 8 / 10 | .20 | 1 | 0 / 1 | 1 | 1 throughout |

Conservative가 standard crossfade reference다. 모든 preset에서 alpha 합 ≤1이며 수동 debug 조절에도
같은 제한을 둔다. alpha 합은 실제 화면 luminance가 아니다. blur의 에너지 재분배와
Source(progress)의 visible glyph 수 변화까지 포함한 화면 밝기는 별도로 봐야 한다.

- Balanced: opacity dip이 상대적으로 작고 core fade/blur 존재감의 균형이 가장 낫다.
- Early Fade: readable core를 빨리 제거하지만 어두워졌다가 blur가 더 보이는 전환이 강해진다.
- Aggressive: opacity dip이 너무 크다. blur가 뒤늦게 다시 올라오는 느낌 때문에 채택하지 않는다.
- Conservative: 에너지 합은 일정하지만 sharp와 넓은 halo가 오래 공존한다. 이번 목표의 sharp 조기 제거와 맞지 않는다.

네 preset의 수식/전체 checkpoint (로컬 자료: `curve-parameters.json`).
Strong24 CURRENT/A 영상: Balanced (로컬 자료: `videos/R24-preset0-mode5.mkv`),
Early Fade (로컬 자료: `videos/R24-preset1-mode5.mkv`), Aggressive (로컬 자료: `videos/R24-preset2-mode5.mkv`),
Conservative (로컬 자료: `videos/R24-preset3-mode5.mkv`).

## I. Selected A curve

**Balanced를 viewer 기본 비교값으로 선택했다. 상품화 PASS라는 뜻은 아니다.**

| b | radius Soft16 | radius Strong24 | sharpAlpha | blurAlpha |
|---:|---:|---:|---:|---:|
| 0 | 10.00 | 12.00 | 1.0000 | 0.0000 |
| .05 | 10.00 | 12.00 | .9803 | .0152 |
| .10 | 10.00 | 12.00 | .9259 | .0574 |
| .15 | 10.00 | 12.00 | .8438 | .1217 |
| .20 | 10.00 | 12.00 | .7407 | .2032 |
| .25 | 10.00 | 12.00 | .6238 | .2970 |
| .30 | 10.00 | 12.00 | .5000 | .3982 |
| .40 | 10.10 | 12.20 | .2593 | .6030 |
| .50 | 10.81 | 13.62 | .0741 | .7783 |
| .65 | 12.65 | 17.31 | 0 | .9000 |
| .80 | 14.65 | 21.29 | 0 | .9000 |
| 1 | 16.00 | 24.00 | 0 | .9000 |

마지막 행은 곡선의 값이다. 앞서 설명한 대로 Source(p=0)는 비어 있으므로 화면상의 blur opacity가 아니다.

낮은 b에서 blur가 실제 합성되는지 native screenshot의 동일 visible rows로 확인했다.
`sharp × alpha + A-only × alpha`와 A-mixed의 평균 8bit 오차는 최대 약 0.302로,
양자화 오차 수준에서 실제 weighted composition과 맞았다.
이는 효과 구현 확인이며 **RMSE로 preset 품질을 고른 것이 아니다**.
composite-check.json (로컬 자료: `composite-check.json`).

## J. Soft16 A result

초기 b≤.2는 sharp가 주 visual이고 broad blur가 낮은 alpha로 나타난다.
b=.3∼.5는 sharp core가 줄어드는 동작이 분명하다. 하지만 넓은 low-resolution layer 위에
작은 readable core가 남는 이중 layer 느낌도 보인다.
b≥.65에서 sharp는 0이므로 A의 획/줄 unevenness를 더 이상 sharp로 가릴 수 없다.

Soft16은 Strong24보다 이 무늬가 더 쉽게 보인다. Gaussian fidelity가 다르다는 이유가 아니라
완성 effect에서도 cheap-looking structured blur가 남는다는 점이 보류 이유다.

[12 checkpoints: CURRENT/A-only/A-mixed](comparisons/R16-preset0-mode6.png),
[A/B checkpoints](comparisons/R16-preset0-mode9.png),
[왕복 frame contact](comparisons/R16-preset0-mode5-slow.png).
원본 native PNG 및 실제 p/b/radius/alpha는 capture 폴더 (로컬 자료: `captures/R16-preset0-mode6/`).

## K. Strong24 A result

더 넓은 radius가 획 식별성을 줄여 Soft16보다 낫지만, strong 구간에도 줄별 밝기 차이가 남는다.
Balanced mixed의 b=.5는 sharpAlpha .074로 core가 거의 사라졌고 충분히 blur가 보인다.
단순 fade-out만 발생한 것은 아니다. b=.65 이후에는 A-only에 .9를 곱한 형태라
Phase 1의 구조적 한계가 완전히 없어지지는 않는다.

[12 checkpoints: CURRENT/A-only/A-mixed](comparisons/R24-preset0-mode6.png),
[A/B checkpoints](comparisons/R24-preset0-mode9.png),
[왕복 frame contact](comparisons/R24-preset0-mode5-slow.png).
원본: capture 폴더 (로컬 자료: `captures/R24-preset0-mode6/`).

## L. A animation audit

각 8초 영상은 실제 Reveal progress를 4초씩 왕복시킨 것이다. 1920×1080 X11 화면을
30fps lossless RGB video로 저장했다. **30fps는 녹화 설정이지 앱 FPS 측정값이 아니다.**
아래 8개는 두 radius에서 네 가지 필수 비교를 모두 제공한다.

| 비교 | Soft16 | Strong24 |
|---|---|---|
| CURRENT / A mixed | 영상 (로컬 자료: `videos/R16-preset0-mode5.mkv`) | 영상 (로컬 자료: `videos/R24-preset0-mode5.mkv`) |
| A blur-only / A mixed | 영상 (로컬 자료: `videos/R16-preset0-mode11.mkv`) | 영상 (로컬 자료: `videos/R24-preset0-mode11.mkv`) |
| CURRENT / B mixed | 영상 (로컬 자료: `videos/R16-preset0-mode10.mkv`) | 영상 (로컬 자료: `videos/R24-preset0-mode10.mkv`) |
| A mixed / B mixed | 영상 (로컬 자료: `videos/R16-preset0-mode9.mkv`) | 영상 (로컬 자료: `videos/R24-preset0-mode9.mkv`) |

추가 Strong24 curve 비교 3개를 합쳐 영상 총 11개다. static은 9케이스×12점=108장이다.
media-inventory.json (로컬 자료: `media-inventory.json`)에서 모든 영상의 8초 길이·해상도와 capture 수를 확인했다.

| 관점 | 이번 확인 결과 |
|---|---|
| sharp fade / already-blurred layer | 모두 구현·확인. b=0의 mixed는 sharp만 표시. blur-only도 같은 radius mapping 사용 |
| weak 저해상도 glyph | 주 visual로 쓰지 않지만 mid의 sharp core＋halo 이중 layer 느낌은 남음 |
| sudden blur pop / radius restart | 곡선은 연속이고 ramp 시작도 smooth. 강제 mode switch 없음. 다만 fixed floor 구간은 의도적으로 존재 |
| line/glyph modulation | A의 strong 구간에 남음. B에서 뚜렷하게 완화 |
| glow / brightness | alpha 합>1 없음. Balanced에도 계수 합이 약 15% 감소하는 구간이 있으므로 밝기 무영향이라고 하지 않음 |
| pumping | Early/Aggressive에서 dip→blur 재상승이 강함. Aggressive는 채택하지 않음 |
| reverse | 같은 b에 같은 curve 값을 쓰는 history-free 설계. 역방향 영상도 제공 |
| halo clipping / duplicated glyph | 확인한 native 이미지·추출 frame에서는 새로운 심각한 clipping/중복 glyph를 관찰하지 못함. 분할 화면의 clip 경계는 의도적 |
| shimmer / moving modulation | A에 남는 구조적 무늬를 motion 중 허용할 수 있는지는 사용자 판단 필요. 모든 frame에 미세 shimmer가 없다고 보증하지 않음 |

판단에는 native checkpoint와 영상에서 추출한 여러 frame을 사용했다. 임의의 디스플레이·전체 frame의
육안 평가를 완료했다는 뜻은 아니다. 특히 Source의 LINE/Fade0에 따른 단계적 변화와 후보 고유의
grid/pumping을 혼동하지 않도록 CURRENT도 항상 같은 Animation으로 구동한다.

## M. Selected B comparison

B 전용 curve는 만들지 않고 A와 완전히 같은 Balanced를 사용했다.
Soft16 b=.65/.8, Strong24 b=.65/.8 및 최대치 근처 blur 영상 frame에서 B가 A보다 균일해 보인다.
sharp가 주로 보이는 약한 구간에서는 차이가 작다.

[Soft16 b=.65 native](captures/R16-preset0-mode9/b0.65.png),
[Strong24 b=.80 native](captures/R24-preset0-mode9/b0.80.png),
[Strong24 A/B video contact](comparisons/R24-preset0-mode9-slow.png).

## N. A vs B quality delta

**first-stage Y 해상도를 유지하는 것이 품질에 효과가 있다**는 가설을 지지하는 결과다.
다만 B에서는 입력 해상도에 맞춰 V kernel의 이산화도 바뀌므로 Y decimation만 유일한
원인이라고 증명한 것은 아니다. CURRENT와의 pixel equivalence도 주장하지 않는다.

A에서 남는 blur의 구조적 무늬가 B에서는 줄어든다. alpha/radius 곡선은 같으므로
단순히 B만 opacity를 낮춰서 숨긴 비교가 아니다.

## O. A vs B cost trade-off

| 지표 | A / CURRENT | B / CURRENT | B / A |
|---|---:|---:|---:|
| H+V logical payload | .40 (−60%) | .60 (−40%) | **1.5×** |
| H+V filtering expressions | .25 (−75%) | .50 (−50%) | **2×** |
| Source+H+V logical payload | .8571 | .9048 | **1.0556×** |
| offscreen tasks/FBO | 3 / 3 | 3 / 3 | 동일 |

B의 “total FBO bytes가 A 대비 약 5.56% 증가”만 보면 작아 보이지만 full Source가 큰 비중을 차지하기 때문이다.
H/V filtering 작업량 자체는 A의 2배다. 저사양 target에서 A 수준의 절감이 필요하다는 상품 조건을
B가 자동으로 만족하는 것은 아니다.

## P. Product interpretation

- **A primary**: structural cost 조건은 유지했다. 그러나 이번 Balanced 품질만으로 상품화 GO를 내리지 않는다.
- **B reference/comparison**: early Y reduction의 quality cost를 보여주는 자료다. 자동 추천·자동 fallback으로 삼지 않는다.
- **CURRENT/HIGH**: 기존 production path를 유지했다. 원본에 ECONOMY API/enum을 추가하지 않았다.

A가 충분히 promising/acceptable하다고 확정할 수 없었으므로 조건부 PER_LINE/stagger와
gradient smoke는 실행하지 않았다. viewer에 gradient 선택 코드는 있지만 이번에 검증한 기능은 아니다.
ImageSpan/generalization도 이 quality fixture로 보증하지 않는다.

## Q. Source retention limitation

매 frame current Source(progress)를 생성하고 같은 Source를 sharp Output과 A/B blur가 참조한다.
fully revealed texture를 고정해서 미공개 glyph의 halo를 먼저 보여주는 구성이 아니다.

full-resolution Source의 allocation/lifetime, Output의 sharp sample, Source dependency는 남는다.
Source/H/V 3단의 fixed overhead도 그대로다. 이번에 줄인 것은 주로 H/V의 pixel/filtering 작업량이며
Source retention이나 target command-chain overhead를 해결한 PoC가 아니다.

## R. If A promising: next PC perf screening plan

이번에는 performance를 측정하지 않았다. 사용자가 위 A의 시각 품질을 acceptable하다고 판단할 때만
선택한 **동일 A curve/topology**를 고정하고 CURRENT와 소규모 optimized PC screening을 진행한다.
그 단계에서 setup/steady CPU, GPU, logical/live texture 차이를 분리해서 확인한다.
task 수가 같으므로 filtering 식−75%를 그대로 frame time−75%라고 해석하지 않는다.
이번에는 GBS/RPM/target install/FPS 측정을 하지 않았다.

## S. If A fails: do not add rendering cost to rescue it

tap/pass/FBO/prefilter/reconstruction을 A에 추가해서 품질을 구하지 않는다.
B를 암묵적으로 product 후보로 바꾸지 않는다. 네 가지 고정 curve를 넘어서 무한 조절하지 않는다.
이 A에 남는 modulation을 UX상 허용할 수 없다면 이 perceptual strategy의 A는 No-Go로 하고
다음 방향은 별도로 결정한다.

## T. Git state / deliverables

원본 UI/Core/Adaptor는 시작 상태에서 변경 없음. HEAD·branch·status·diff hash·기존 dirty/untracked
파일 hash를 BASELINE.json (로컬 자료: `BASELINE.json`)과 FINAL_STATE.json (로컬 자료: `FINAL_STATE.json`)으로 대조했다.
full `git status`와 HEAD는 git-state.json (로컬 자료: `git-state.json`)에 기록했다.

| repo | branch | HEAD | preserved dirty state |
|---|---|---|---|
| UI | devel_blur_text | 05087317cac8ea9600bba498f00ccf8086a79d3f | 기존 4 modified |
| Core | tizen_10.1 | 8228720460a4910151f4eb4ad36976816b13a102 | 기존 3 modified + 1 untracked |
| Adaptor | tizen_10.1 | a3ab9b8db9637fda4c973b5d59c4d4b95a9d5286 | 기존 3 modified |

- `python3 baseline.py verify`: PASS.
- 3 repo `git diff --check`: PASS.
- private overlay/viewer build: PASS. capture의 native b/radius 일치·FBO inventory 검사: PASS.
- alpha invariant, native composite 검사, media completeness 확인: PASS.
- capture/video app logs에 shader compile/link 실패·assert 없음.
- production build/UTC/sanitizer/performance benchmark/GBS/RPM 미실행.
- commit/amend/push/reset/restore/stash/rebase 없음. Phase 1 및 이전 진단 결과물 변경 없음.

신규 결과물은 이 외부 디렉터리 안에만 만들었다.
viewer.cpp (로컬 자료: `viewer.cpp`), candidate.h (로컬 자료: `candidate.h`), curves.h (로컬 자료: `curves.h`),
runtime-overlay.diff (로컬 자료: `runtime-overlay.diff`), capture data (로컬 자료: `captures/`), videos (로컬 자료: `videos/`), comparisons (로컬 자료: `comparisons/`).
실행에는 기존 `../reveal-flush-poc.2JyKvS/lib/common`과 DALi prefix를 사용한다.
