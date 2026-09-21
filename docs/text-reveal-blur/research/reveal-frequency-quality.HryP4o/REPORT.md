# Text::Reveal — frequency-optimized direct 2D blur quality research

## A. Executive Verdict

**DIRECT 2D TWO-STAGE QUALITY NOT PRACTICAL**

이번 제한된 **nonnegative, fixed 9×9 설계 및 optional 11×11 조사 범위**의
No-Go다. 모든 direct-2D/2-stage 방식이 불가능하다는 수학적 결론은 아니다.

- 주파수 응답을 직접 최적화한 9×9는 old9보다 분명히 좋아졌다.
  Soft16의 큰 획 잔상은 상당히 줄었다.
- 그러나 Strong24의 p=.20/.50 및 왕복 애니메이션에 CURRENT에는 없는
  반복적인 밝기 무늬가 남았다. **9×9 quality FAIL.**
- 11×11은 같은 목적함수로 **오프라인까지만** 평가했다. Strong24의
  중간 strength에서 CURRENT와의 차이가 여전히 커서 GPU 진입 조건을
  충족하지 못했다. **11×11 화면 품질을 테스트해서 실패했다는 뜻은 아니다.**
- 실제 H 제거, runtime 3→2 구현, CPU/GPU/FPS 측정, target 작업은 하지 않았다.
- Core/Adaptor/UI 및 기존 local changes는 모두 보존했다. 새 파일은 이
  외부 디렉터리에만 있다.

### 직접 비교

```bash
bash /home/bowonryuubuntu/tizen/reveal-frequency-quality.HryP4o/run-viewer.sh
```

기본은 **왼쪽 CURRENT / 오른쪽 optimized9**, Soft16, 8초 왕복이다.
각 Label은 FHD 원래 크기로 렌더링하고 panel에서 clip한다. 글자를 축소하지 않는다.

| 키 | 동작 |
|---|---|
| S / D | Soft16 / Strong24 |
| Q / W / E / R | p=.20 / .50 / .75 / .90 정지 |
| Space | 8초 0→1→0 반복 |
| 1 / 2 / 3 / 4 | HIGH / CURRENT / old9 / optimized9 단독 |
| 5 / 6 / 7 | CURRENT·optimized9 / old9·optimized9 / CURRENT·old9·optimized9 |
| Esc | 종료 |

먼저 `D` → `Q`, `W`로 Strong24를 비교하면 남은 차이가 잘 보인다.
실행 스크립트는 이전 private **production** Foundation 및 common Core/Adaptor를
사용한다. 현재 workspace의 diagnostic library나 rejected glFlush 후보를 사용하지 않는다.

## B. Old9 failure analysis

old9는 energy/centroid/variance를 맞췄지만 주파수 응답을 맞추지 않았다.
특히 연속적인 Gaussian support를 적은 위치로 대표하면서 고주파 획의
일부가 강하게 남았다. quarter target에 출력한다는 사실만으로 이 고주파가
자동 평균되는 것은 아니다.

이번에는 GL_LINEAR의 실제 두 texel 가중치까지 포함했다. 같은 radius와
strength에서 old9의 stop-band peak/RMS가 CURRENT보다 크고, 그 차이가
실제 격자/획 잔상과 일치한다. 새 설계에서 leakage와 시각적 무늬가 함께
줄었지만 완전히 제거되지는 않았다.

Strong24 새9의 양수 offsets는 약 `2.33, 5.21, 8.92, 14.54`다.
strength=.896에서 바깥 인접 간격은 약 `3.32, 5.04` source pixels다.
LINEAR 한 read의 축 footprint는 두 texel center이므로 빈 구간이 남는다.
이는 sampling-density 문제를 뒷받침한다. 다만 gap 하나만으로 모든
가능한 비균일 9-tap 설계의 실패를 증명하지는 않는다.

## C. Production CURRENT frequency response

이전 production kernel extraction (로컬 자료: `../reveal-dense2d-quality.ECWtnl/PRODUCTION_KERNEL.json`)을
재사용했다. 현재 Gaussian의 packed offsets/weights를 reference로 썼으며
ideal Gaussian 곡선으로 대체하지 않았다.

| 항목 | Soft16 | Strong24 |
|---|---:|---:|
| Gaussian bell sigma | 4.795804977 | 7.372480392 |
| Truncated discrete variance / axis | 22.68387335 | 53.49729516 |
| packed ± texture expressions / pass | 16 | 24 |
| Source | 1956×1116 | 1972×1132 |
| H: reduced X, full Y | 489×1116 | 493×1132 |
| V: reduced X/Y | 489×279 | 493×283 |
| Source halo / side | 18 | 26 |

각 fixture는 정확히 4:1이므로 quarter Nyquist는 **.125 cycles/source pixel**이다.
output center는 `(j+.5)*SourceExtent/QuarterExtent−.5`이며 이 fixture에서는
`4*j+1.5`다. 추가 half-texel 보정은 없다.

홀수 예 `1957→490`에서는 spacing≈3.993878, Nyquist≈.125192가 된다.
phase는 위치마다 달라진다. 홀수 너비/높이를 포함한 독립적인 sine gather →
downsample → reconstruction 확인 **192건**의 모델 오차는 최대
`3.90e−13`이었다. MODEL-CHECK.json (로컬 자료: `MODEL-CHECK.json`).
이는 **수학적 interpolation 검증**이지 홀수 크기의 GPU 품질 통과는 아니다.

CURRENT도 phase/strength에 따라 이산 응답이 달라지므로, 이전 보고서의
한 phase에서 계산한 `~.203`을 모든 phase의 상한처럼 사용하지 않았다.
아래는 더 촘촘한 phase 집합의 worst peak와 평균 에너지다.

## D. Optimizer formulation

1D 변수: 양수 offsets `x1…x4`, center mass와 네 symmetric-pair masses.
이를 대칭인 9개 위치로 펼치고, 2D weight는 `wx*wy`로 만든다.
실행은 separable **두 pass가 아니라 한 pass의 81 Cartesian reads**다.

Hard constraints:

- offsets strictly increasing, symmetric, 최소 간격 .15 source pixels.
- weights≥0, 합=1, centroid=0.
- max offset≤R+1. 보수적인 LINEAR reach 1px를 더해도 기존 halo R+2 이내.
- radius별 고정 상수. strength는 기존처럼 offset에 곱할 뿐이다.
- fragment exp/sqrt/sin/cos 없음. strength별 kernel variant 없음.

각 tap의 응답은 point-sample 지수함수로 끝내지 않고 다음처럼 계산했다:

```text
q = phase + strength * offset
n = floor(q), t = q − n
G(f) = Σ weight * [(1−t)exp(i2πfn) + t exp(i2πf(n+1))]
       * exp(−i2πf*phase)
```

quarter sampling 후 full output의 네 polyphase에서 LINEAR reconstruction
energy를 계산했다. 이 시스템을 하나의 shift-invariant transfer function이라고
가정하지 않았다. 기존 Late Smooth의 blurred contribution도 반영했다.
작은 strength에서 이미 sharp Source로 넘어가는 부분에 불필요하게 강한
low-pass를 강요하지 않기 위해서다. **raw response도 별도로 모두 공개했다.**

6개 목적항:

1. low band `[0,.125)`에서 CURRENT complex-response RMS error.
2. stop band `[.125,.5]`의 reconstructed integrated energy.
3. strength별 worst frequency·phase stop peak.
4. phase 간 complex-response variation.
5. CURRENT 대비 effective variance 상대 오차.
6. radial .04/.08/.12에서 CURRENT의 axis/diagonal 차이와의 오차.

정규화 기준은 각각 gain .05/.10/.25/.10, variance .15, angular .05.
두 scalarization만 사전 정의했다: balanced `(1,1,1,1,1,1)`,
width `(2,1,1,1,4,2)`. 시각 결과에 따라 계수를 바꾸지 않았다.
자세한 식은 [FORMULATION.md](FORMULATION.md), frequency.py (로컬 자료: `frequency.py`).

SLSQP를 사용했고 결과는 로컬 최적해다. global optimum 주장은 하지 않는다.
Pareto non-dominated 결과 중 사전에 정한 balanced score 최소를 선택했다.
PARETO-9.json (로컬 자료: `PARETO-9.json`), GPU-SELECTION-9.json (로컬 자료: `GPU-SELECTION-9.json`).

모델 한계: A8 중간 quantization, driver filtering precision, 실제 glyph의
2D spectral distribution, 경계 clamp 비선형성은 완전히 모델링하지 않는다.
따라서 수치 개선만으로 PASS하지 않고 실제 GPU 캡처를 최종 기준으로 삼았다.

## E. Phase / strength coverage

- Pilot: strength `1/.8/.5/.25/.1`, phase `0/.25/.5/.75`.
- Pilot의 held-out strength=.65에서 강한 peak가 발견되어, **GPU 확인 전**
  `1.00→.10`, .05 간격 19 strengths로 refinement했다. 계수는 그대로다.
- radius·scalarization별 5개 deterministic starts, 최대 220 iterations.
  Pilot 20회 및 refinement 16회/axis-count. 최종 결과는 모두 feasible/converged.
- 최종 validation: phase `0…15/16`, 주파수 0… .5의 2049점,
  strengths `1/.98/.896/.8/.65/.5/.35/.25/.15625/.1`.
- GPU에는 **optimized9 한 설계의 radius16/24 상수 두 세트만** 올렸다.
  다른 Pareto 후보를 화면에 올려 시각적으로 선택하지 않았다.
- 11×11의 오프라인 평가도 동일한 목적함수/절차를 사용했다.

Pilot 결과는 pilot/ (로컬 자료: `pilot/`)에 보존했다. 실제 최종 상수는
`GPU-SELECTION-*.json`과 `optimized-*.h`다. `SELECTED-*.json`은 pilot 단계다.

## F. Selected optimized9 kernels

아래는 0/양수 위치다. 음수 위치는 대칭이고, 표의 `wi`는 **각 한쪽 tap**의
weight다. 따라서 정규화 식은 `w0 + 2*(w1+w2+w3+w4)=1`이다.

| R | offsets: 0, x1, x2, x3, x4 | weights: w0, w1, w2, w3, w4 |
|---|---|---|
| 16 | 0, 1.6858669444, 3.3762803618, 5.3202486921, 8.9846648248 | .1232370132, .1305578035, .1286256259, .1033977678, .0758002961 |
| 24 | 0, 2.3314778051, 5.2135774959, 8.9160980701, 14.5411076041 | .1087927151, .1370942455, .1306232940, .1185295707, .0593565322 |

모든 자릿수: optimized-9.h (로컬 자료: `optimized-9.h`).
support+LINEAR reach는 Soft 약9.985≤18, Strong 약15.541≤26이다.
halo를 키우지 않았다. variance는 soft objective이며 exact 일치는 아니다.
strength=.896에서 variance 상대 오차는 Soft 약−3.24…−2.87%,
Strong 약−1.52…−1.22%다. 밝기/width를 완벽히 같다고 과장하지 않는다.

## G. Frequency metrics

아래 `peak / RMS`는 stop band에서 **16 phases 전체의 최대 gain / 평균 power의
제곱근**이다. reconstruction 전 raw 1D response이며, GPU 시간이나 픽셀
artifact 크기 자체가 아니다. 낮을수록 누설이 적다.

| R / strength | CURRENT peak / RMS | old9 | optimized9 |
|---|---:|---:|---:|
| 16 / .896 | .379 / .111 | .842 / .314 | .407 / .137 |
| 16 / .65 | .073 / .043 | .578 / .258 | .179 / .094 |
| 16 / .5 | .166 / .036 | .495 / .184 | .191 / .082 |
| 24 / .896 | .281 / .096 | .918 / .360 | .544 / .221 |
| 24 / .8 | .168 / .072 | .869 / .350 | .435 / .206 |
| 24 / .65 | .066 / .034 | .806 / .329 | .414 / .171 |
| 24 / .5 | .018 / .009 | .656 / .292 | .337 / .124 |

대표 strength=.896의 보조 수치:

| R / path | stop ∫mean-phase | low-band complex RMS error | reconstruction + handoff stop RMS |
|---|---:|---:|---:|
| 16 CURRENT | .004575 | 0 | .091245 |
| 16 old9 | .037058 | .017684 | .253338 |
| 16 optimized9 | .007018 | .014495 | .103016 |
| 24 CURRENT | .003436 | 0 | .079577 |
| 24 old9 | .048531 | .038492 | .297044 |
| 24 optimized9 | .018354 | .024269 | .178716 |

old9 대비 integrated stop energy는 Soft 약81%, Strong 약62% 감소했다.
그러나 CURRENT 수준까지 내려갔다는 뜻은 아니다. 특히 Strong의
strength=.5에서는 reconstructed RMS도 CURRENT .00693 대비 .09875로 높다.
이때 radius*strength=12이므로 Late Smooth가 아직 이 차이를 가려주지 않는다.

전체 metrics/최악 phase/variance: FREQUENCY-9.json (로컬 자료: `FREQUENCY-9.json`).
곡선: [Soft16 .896](frequency-plots/r16-s0.896.png),
[Strong24 .896](frequency-plots/r24-s0.896.png),
[Strong24 .5](frequency-plots/r24-s0.5.png).
곡선의 OPT11은 오프라인 결과다.

## H. Soft16 visual result

동일 한국어 문장 12줄, font size24, FHD Label1920×1080, 흰 text/검정 배경.
Unit::LINE / WHOLE_TEXT / Fade0 / Stagger .25 / BlurDurationRatio1.
Ubuntu, GTX1650, NVIDIA595.91.07, GLES, window MSAA4.

old9의 p=.20 획 모양 얼룩은 크게 줄었다. 그러나 CURRENT보다 text별 밝기
분포가 고르지 않은 구간이 남는다. Soft만으로는 유망하지만 무조건적인
quality PASS를 주지는 않는다.

| p | old9 RMSE / max | optimized9 RMSE / max | optimized9 energy 변화 |
|---|---:|---:|---:|
| .20 | 2.294 / 53 | 1.240 / 28 | +.120% |
| .50 | 1.148 / 19 | 1.123 / 20 | +.141% |
| .75 | — | .0132 / 1 | +.0017% |
| .90 | 0 / 0 | 0 / 0 | 0% |

RMSE/max는 CURRENT 대비 8-bit RGB 차이이며 background 포함 ROI다.
단독 품질 점수로 사용하지 않았다.
[p=.20 비교](comparisons/r16-p0.20-3x.png),
[p=.50 비교](comparisons/r16-p0.50-3x.png).
위→아래 CURRENT/old9/optimized9, 3× nearest, 명암 보정 없음.

## I. Strong24 visual result

**FAIL.** old9보다 좋아졌지만 p=.20/.50에서 CURRENT에는 없는 반복적인
세로 밝기 변조와 덩어리/획 모양의 패턴이 남는다. 단순한 blur 폭 차이만은 아니다.

| p | old9 RMSE / max | optimized9 RMSE / max | optimized9 energy 변화 |
|---|---:|---:|---:|
| .20 | 3.379 / 58 | 1.402 / 26 | +.138% |
| .50 | 2.173 / 45 | 1.142 / 20 | +.042% |
| .75 | .1016 / 1 | .1310 / 1 | +.028% |
| .90 | 0 / 0 | 0 / 0 | 0% |

[p=.20 비교](comparisons/r24-p0.20-3x.png),
[p=.50 비교](comparisons/r24-p0.50-3x.png),
[p=.75 비교](comparisons/r24-p0.75-3x.png).

energy 변화는 두 radius 모두 약.15% 미만이다. halo의 체계적인 확대/잘림은
관찰되지 않았다. 다만 threshold bounds가 pixel-identical하지는 않으며,
Strong p=.20 ROI centroid X도 CURRENT 대비 약+1.14px 변한다. 대칭 kernel이라도
sampling phase에 따른 출력 분포 변화가 사라지는 것은 아니다.

energy, centroid, variance, threshold bounds, reverse equality:
Soft metrics (로컬 자료: `CAPTURE-METRICS-16.json`), Strong metrics (로컬 자료: `CAPTURE-METRICS-24.json`).
X/Y projected profiles는 comparisons/ (로컬 자료: `comparisons/`)의 `*-profiles.json`에 저장했다.
최종 판단은 이 집계 수치보다 structured artifact를 우선했다.

## J. Animation artifact audit

Soft16 8초 영상 (로컬 자료: `slow-r16.mkv`), Strong24 8초 영상 (로컬 자료: `slow-r24.mkv`).
좌 CURRENT / 우 optimized9, 동일 Animation. 0→1 4초, 1→0 4초다.
[Soft contact sheet](comparisons/slow-r16-contact.png),
[Strong contact sheet](comparisons/slow-r24-contact.png).

- Strong의 early/mid blur에서 반복 밝기 패턴의 모양이 시간에 따라 변한다.
  old9보다 약하지만 moving alias-pattern 조건을 만족하지 못했다.
- Soft에서도 미세한 밝기 분포 차이가 남는다. 전체 밝기가 크게 출렁이는
  현상과는 구별해야 한다.
- near-sharp에서 새로운 명백한 pop은 관찰하지 못했다. p=.90은 두 radius
  모두 CURRENT와 text ROI가 byte-identical했다.
- forward/reverse의 동일 p 체크포인트도 모두 byte-identical했다.
  이는 checkpoint 검증이지 모든 프레임의 shimmer 부재 보증은 아니다.

기존 HIGH/CURRENT/old9 정적 캡처 48장을 재사용했다. 새 optimized9 캡처는
2 radii×4 p×2 directions=16장이고, reference 재사용 확인용 CURRENT
p=.20만 radius별 한 장씩 더 캡처했다. 두 reference가 이전 CURRENT와
정확히 일치했다. REFERENCE-CHECK.json (로컬 자료: `REFERENCE-CHECK.json`).

## K. 11×11 decision

**GPU viewer 진입: NO.** 오프라인 검토는 수행했다.
[사전 결정 기록](DECISION-11.md), FREQUENCY-11.json (로컬 자료: `FREQUENCY-11.json`).

| Strong24 strength | CURRENT peak / RMS | optimized9 | offline11 |
|---|---:|---:|---:|
| .896 | .281 / .096 | .544 / .221 | .443 / .166 |
| .8 | .168 / .072 | .435 / .206 | .301 / .150 |
| .65 | .066 / .034 | .414 / .171 | .260 / .124 |
| .5 | .018 / .009 | .337 / .124 | .249 / .095 |

11의 stop RMS는9보다 약23–28% 감소한다. 하지만 Strong .896에서 stop
integrated energy가 CURRENT의 약3배, .5에서는 약124배다.
reconstruction 이후 .5 RMS도 .0761 대 .00693이고 이때 blurred mix=1이다.
따라서 **CURRENT에 상당히 가까워짐**이라는 진입 조건은 충족하지 못한다고
판단했다. 절대적인 perceptual threshold나 전역 최적해를 주장하는 것은 아니다.

Soft16은 더 좋아지지만 filtering expression 수가 CURRENT보다51.25% 많다.
Strong24의 +.83%만 보면 비현실적인 비용은 아니다. 그러므로 비용 하나로
탈락시킨 것이 아니라 **남은 품질 격차와 비용을 함께 고려한 보수적인 No-Go**다.

11×11 GLSL 설치/캡처/영상은 없다. 오프라인 상수만
optimized-11.h (로컬 자료: `optimized-11.h`)에 남겼다. 13×13 이상은 검토하지 않았다.

## L. Theoretical sampling budget

실제 fixture dimensions로 계산했다. `Q=Vwidth*Vheight`, `Harea=4Q`이므로:

```text
CURRENT H+V = (4Q + Q) * R
9×9 = Q * 81
11×11 = Q * 121
```

| Case | CURRENT H+V expressions | 9×9 | 11×11 |
|---|---:|---:|---:|
| Soft16 | 10,914,480 | 11,050,911 (+1.25%) | 16,508,151 (+51.25%) |
| Strong24 | 16,742,280 | 11,301,039 (−32.50%) | 16,881,799 (+.83%) |

이는 full-quad·nonzero strength의 **shader texture 식 개수 계산**이다.
실측 GPU 명령 수, 대역폭, cache, CPU, FPS의 개선율이 아니다.
Source/Output/clear/early-out/submission, PER_LINE의 D2 band는 포함하지 않는다.
SAMPLING-BUDGET.json (로컬 자료: `SAMPLING-BUDGET.json`).

## M. Runtime 3→2 implementation

**미수행.** quality PASS가 없으므로 runtime에 진입하지 않았다.
외부 viewer는 선택 Label의 V renderer만 full Source를 읽는2D shader로 바꾼다.
Source capture, 기존 H, V FBO, Output의 sharp read/Late Smooth는 유지한다.

```text
Source ──→ H (그대로 실행, candidate가 결과를 사용하지 않음)
   └────→ V 자리의 81-read 2D shader ──→ 기존 Output
   └────────────────────────────────→ 기존 sharp read
```

quality viewer가 H를 제거하지 않았으므로 이 앱의 성능으로2-stage 이득을
주장할 수 없다. PER_LINE generalization, 다른 corpus/size, ImageSpan,
async/lifecycle 검증도 이 gate에서는 하지 않았다.

## N. Runtime inventory

실제 viewer는 **Label당3 offscreen task/FBO**를 그대로 보유한다.
단독 보기3, 양쪽 비교6, 3-way 비교9이며 window task는 별도다.
기존 Source/H/V 크기는 C절 표 그대로이고, candidate에서 H camera/renderer/
dependency를 제거하지 않았다. inventory는 각 capture의 `app.log`에 있다.

Demo의45→30,57→38 같은 수치는 이번에도 목표 topology에 불과하다.
실제 Demo task/FBO/create count를 줄인 결과로 보고하지 않는다.

## O. PC performance gate

**미수행.** CPU timing, GPU timer, FPS benchmark, RSS/VRAM 측정 없음.
8초 영상과 shader 상수 생성/검증은 품질 확인이지 성능 측정이 아니다.
프로덕션 build/UTC/sanitizer도 수행하지 않았다.

## P. PC→TV interpretation

PC의 theoretical expression 감소를 TV 개선률로 환산하지 않는다.
이번에는 quality 단계에서 멈췄으므로 directional performance screening조차
진행하지 않았다. 현재 후보의 TV 성능·품질을 예측해 보증하지 않는다.

## Q. Target escalation

**NOT WORTH ESCALATING 이 후보 기준.** GBS/RPM/target 설치·측정 없음.
현재 PERFORMANCE baseline 유지가 권고안이다. 이번 결과만으로 generic
shallow blur가 문제를 해결할 것이라고 가정하지 않는다.

## R. Explicitly rejected extensions

- 13×13+, negative weights, visual 결과를 보고 objective coefficient tuning.
- strength별 shader variant, 추가 prefilter/reconstruction 필터, 새 halo.
- Generic Kawase/downsample candidate B.
- glFlush/fence/dependency bypass, FBO pooling, camera sharing/backend 변경.
- quality FAIL 뒤의 실제 H 제거와 성능/target 실험.

## S. Git state / artifacts

| Repository | HEAD | Branch |
|---|---|---|
| UI | `05087317cac8ea9600bba498f00ccf8086a79d3f` | devel_blur_text |
| Core | `8228720460a4910151f4eb4ad36976816b13a102` | tizen_10.1 |
| Adaptor | `a3ab9b8db9637fda4c973b5d59c4d4b95a9d5286` | tizen_10.1 |

시작/종료의 HEAD, branch, status, diff hash 및 dirty/untracked file hash가
모두 동일하다. 기존 working tree가 dirty였으므로 **clean으로 만들었다는 뜻이
아니라 기존 변경을 그대로 보존했다**는 뜻이다.
BASELINE.json (로컬 자료: `BASELINE.json`), FINAL_STATE.json (로컬 자료: `FINAL_STATE.json`).

`git diff --check`: Core/Adaptor/UI 모두 통과.
commit/amend/rebase/push/reset/restore/stash 없음.
기존 결과 디렉터리는 읽기만 했으며 덮어쓰지 않았다.

최종 상수/81 reads/halo/reverse/reference/model 검증:
VERIFICATION.json (로컬 자료: `VERIFICATION.json`). 사용 private library와 viewer의 SHA-256도 기록했다.
외부 Python 의존성은 DEPENDENCIES.json (로컬 자료: `DEPENDENCIES.json`)에 기록했다.
global package나 production 설치 경로를 변경하지 않았다.

새 정적 캡처18장, 왕복 영상2개, 기존 정적 reference48장 재사용.
이 보고서의 범위를 넘어선 production 정확성/성능 통과 주장은 하지 않는다.
