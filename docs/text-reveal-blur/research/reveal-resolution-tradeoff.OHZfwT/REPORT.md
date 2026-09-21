# Direct 2D two-stage blur — resolution/tap trade-off

## A. Executive Verdict

**DIRECT RASTER TWO-STAGE APPROACH STOP**

A `1/3 × 1/3 + 7×7`와 B `1/2 × 1/2 + 5×5` 모두 **offline No-Go**다.
각 strength의 offsets/weights를 독립적으로 최적화했지만, Strong24 `.65/.50`의
누설과 최종 출력 오차가 CURRENT에 근접하지 못했다. 같은 alias 대역으로 다시 계산한
기존 independent quarter9보다도 대표 후보의 reconstructed leakage가 더 컸다.

이론적인 stage/storage 이점은 있으나, 이를 확인하기 위해 품질 gate를 우회하지 않았다.
**GPU viewer, animation capture, runtime 3→2, performance/target 작업은 하지 않았다.**
이는 아래 제한된 설계 공간의 실용적 No-Go이며 모든 direct convolution의 불가능성 증명은 아니다.

| Strong24 / candidate-own alias band | CURRENT Recon RMS | 기존 quarter9 | 새 후보 | 전체 starts의 최저 Recon RMS |
|---|---:|---:|---:|---:|
| s=.65 / A band | .03085 | .10839 | .20014 | .17331 |
| s=.65 / B band | .03255 | .09670 | .20249 | .15260 |
| s=.50 / A band | .00031 | .10566 | .15627 | .14733 |
| s=.50 / B band | .00023 | .10033 | .21956 | .14914 |

각 행은 **같은 source-frequency 대역**으로 비교했다. A/B 행은 대역이 다르다.
RMS는 이론 신호 진폭 지표이며 화면 밝기·GPU 시간·FPS가 아니다.
특히 `.50`의 작은 CURRENT 분모로 과도한 배율을 강조하지 않는다. 아래 G의 phase 설명 참조.

## B. Why quarter9 failed

기존 [independent9 연구](../reveal-independent-quality.On1JuQ/REPORT.md)는
고정 scaling 제약을 풀어도 9 positions/axis의 practical fidelity가 부족함을 보였다.
이번에는 그 원인을 “quarter decimation이 너무 강했기 때문”으로만 볼 수 있는지 분리했다.

결과적으로 **Nyquist 완화만으로 회복되지 않았다.**

1. 목표 blur 폭은 그대로다. target 해상도를 높여도 Source 공간에서 넓은 범위를
   소수의 위치로 샘플링해야 하는 문제는 남는다.
2. positions를 9→7→5로 줄이면 blur response의 진동/누설을 제어할 자유도도 감소한다.
3. 높은 해상도 target의 LINEAR reconstruction은 quarter와 다른 응답을 갖는다.
   “alias 대역이 좁아졌다”와 “최종 고주파 누설이 줄었다”는 같은 말이 아니다.
4. 실제 후보에서 일부 pass/transition error는 줄었지만 다른 대역의 큰 봉우리가 남았고,
   전체 reconstructed fidelity는 악화됐다.

따라서 이번 두 조합에서는 더 약한 decimation의 이점이 tap 감소를 보상하지 못했다.
기존 quarter9는 재최적화하지 않고 저장된 독립 커널 그대로 비교했다.

## C. Candidate topology

| Path | Offscreen 구성 | Output |
|---|---|---|
| CURRENT PERFORMANCE | Full Source → H(X/4, Y 유지) → V(X/4,Y/4) | V + full Source / Late Smooth |
| A, 계산상 후보 | Full Source → 7×7 direct Blur(X/3,Y/3) | Blur + 동일 full Source / Late Smooth |
| B, 계산상 후보 | Full Source → 5×5 direct Blur(X/2,Y/2) | Blur + 동일 full Source / Late Smooth |

Source capture/Reveal 의미/색/gradient/emoji/ImageSpan을 바꾸는 후보가 아니다.
새 후보도 완성된 Source를 입력으로 가정한다. 실제 production pipeline은 변경하지 않았다.

## D. Actual dimensions / Nyquist

기존 FHD fixture `1920×1080`에 halo `R+2`를 양쪽으로 더한다.
target extent는 축별로 `ceil(SourceExtent/divisor)`다.

| R | Source | CURRENT H | CURRENT V | A Blur | B Blur |
|---|---|---|---|---|---|
| 16 | 1956×1116 | 489×1116 | 489×279 | 652×372 | 978×558 |
| 24 | 1972×1132 | 493×1132 | 493×283 | 658×378 | 986×566 |

| R / path | Source spacing X/Y | Nyquist X/Y, cycles/Source-pixel |
|---|---|---|
| 16 / CURRENT | 4 / 4 | .125 / .125 |
| 16 / A | 3 / 3 | .16666667 / .16666667 |
| 16 / B | 2 / 2 | .25 / .25 |
| 24 / CURRENT | 4 / 4 | .125 / .125 |
| 24 / A | 2.99696049 / 2.99470899 | .16683570 / .16696113 |
| 24 / B | 2 / 2 | .25 / .25 |

A의 Strong24를 정확한 3:1이라고 가정하지 않았다. 실제 식은 다음과 같다.

```text
T = ceil(S / divisor)
d = S / T
c(j) = (j + 0.5) * d - 0.5        // target texel center in Source pixels
l(n) = (n + 0.5) / d - 0.5        // full output pixel's target lookup
j = floor(l(n)); t = l(n) - j
output(n) = (1-t)*filtered(c(j)) + t*filtered(c(j+1))
```

`c(j)`마다 다른 Source LINEAR phase와 complex phase shift를 그대로 계산한다.
quarter 전용 reconstruction 계수에서 숫자 4만 바꾼 모델이 아니다.
CURRENT와 후보를 동일 full-source output 위치에서 비교했다.

## E. Kernel formulation

이전 packed Gaussian extraction / LINEAR gather / Late Smooth를 재사용했다.
목표는 ideal Gaussian이 아니라 CURRENT PERFORMANCE의 실제 계수 기반 응답이다.

- A: `[-x3,-x2,-x1,0,x1,x2,x3]`, B: `[-x2,-x1,0,x1,x2]`.
- weights는 좌우 대칭, 비음수, `w0+2*sum(wi)=1`.
- 각 R/strength에서 offsets와 weights 모두 **독립**. offset은 절대 Source pixels이며
  평가 시 strength를 다시 곱하지 않는다.
- finite / centroid 0 / ordered offsets / 기존 halo 내 support.
- 기존 연구와 같은 수치적 최소 간격 `.15px`, 최대 offset `R+1`.
  LINEAR reach 1px를 보수적으로 더해도 halo `R+2` 이내다.
- variance는 CURRENT-relative soft penalty. exact matching은 강제하지 않았다.
- Source 크기 증가, negative weights, extra pass 없음.

사전 고정한 [PROTOCOL.md](PROTOCOL.md):

- R16/R24 × `.896/.65/.50` × A/B의 12 cases.
- case마다 16 deterministic starts × 2 scalarizations = 32회, 총 **384회**.
- SLSQP 최대 220 iterations. **384/384 수렴 및 feasible**.
- 시작점: 다양한 폭의 uniform/quantile/Hermite/고정 간격/full-halo 분포.
  모든 값은 초기화 이후 제약 안에서 자유롭게 움직인다.
- 목적항: reconstructed pass fidelity, reconstructed own-stop energy,
  raw worst phase/frequency stop peak, phase variation, soft variance,
  reconstructed own-transition fidelity.
- balanced `(1,1,1,1,1,1)` / width `(2,1,1,1,4,2)` 계수는 사전 고정.
  눈으로 화면을 보고 coefficient를 조정하지 않았다.
- feasible Pareto set의 balanced-score 대표값을 선택하되, 각 case의 전체 32해에서
  **최소 Recon RMS / 최소 raw peak 후보도 별도로 검사**했다.

Training은 실제 width/height의 연속 12 output 위치 + 범위에 분산한 24 위치,
257 uniform frequencies 및 경계 주파수를 사용했다. Raw phase는 16개다.
Validation은 **모든 interior full-source output pixels**, 2049 frequencies와 실제
Nyquist 경계점, 실제 target-center phase progression을 사용했다.
raw worst/phase variation 지표는 비교 가능한 `0…15/16` 16 phases 기준이다.

독립 확인:

- 실제 sampled sine source를 LINEAR로 읽고 재구성한 값과 모델 비교 **360개**.
  even/odd extent, 세 sampling lattices 포함. 최대 complex error `7.16e-12`.
- 최적화의 analytic gradient와 finite differences 비교 4개: 최대 오차 `5.61e-9`.
- 선택된 12개 커널의 odd width/height 추가 검증 24개.
- GPU precision / A8 quantization / 실제 glyph의 2D spectral distribution /
  Source 경계 clamp 동작은 모델링하지 않았다. output 통계는 경계를 제외한 interior다.

이것은 bounded multistart의 best-found 결과이며 global optimum 증명은 아니다.
자료: OFFLINE.json (로컬 자료: `OFFLINE.json`), PARETO.json (로컬 자료: `PARETO.json`),
MODEL-CHECK.json (로컬 자료: `MODEL-CHECK.json`), ODD-SANITY.json (로컬 자료: `ODD-SANITY.json`).

## F. Independent optimum results

Strong24 대표 커널의 양수 offsets 및 `w0, w1…`은 다음과 같다.
weight는 pair 합계가 아니라 한쪽 tap의 값이다. 음수 쪽은 대칭이다.

| Case | Positive offsets, Source px | Weights w0, w1… |
|---|---|---|
| A / .896 | 2.648611, 6.383195, 10.272882 | .156363, .167226, .142275, .112318 |
| A / .65 | 2.154282, 4.651428, 8.276888 | .172974, .164112, .154713, .094688 |
| A / .50 | 1.262332, 2.924392, 5.580837 | .082020, .165881, .157737, .135373 |
| B / .896 | 2.785892, 8.836443 | .176986, .220340, .191167 |
| B / .65 | 2.829609, 5.781880 | .236397, .191332, .190469 |
| B / .50 | 2.779945, 5.253652 | .262222, .217342, .151547 |

A의 Strong variance error는 약 −12…−6%, B는 약 −30…−12% 범위다.
B의 최저 누설 해 중에는 폭이 더 넓고 pass fidelity가 나쁜 해도 있다.
**누설 감소와 폭/fidelity를 동시에 충족한 성공 사례로 해석하지 않는다.**

대표 선택에 가려진 좋은 해가 있는지도 확인했다.

- A `.65`: 전체 최저 Recon RMS `.17331`도 CURRENT `.03085`와 큰 격차.
  그 해는 variance 약 −31.9%, pass error `.07712`라는 trade-off도 있다.
- B `.65`: 전체 최저 `.15260`, CURRENT `.03255`.
  variance는 약 +3.9…+4.7%지만 pass error `.13231`, raw peak `.88811`이다.
- `.50`의 최저 RMS도 A `.14733`, B `.14914`로 충분히 낮아지지 않았다.
- 최소 peak 해와 최소 RMS 해는 서로 다를 수 있으며, 최소값들을 하나의 가상 커널로 합치지 않았다.

Soft16 포함 전체 커널/width/support/phase/minimum 표:
[TABLES.md](TABLES.md), 전체 정밀도 SELECTED.json (로컬 자료: `SELECTED.json`),
ALTERNATIVES.json (로컬 자료: `ALTERNATIVES.json`), ALL-START-METRICS.json (로컬 자료: `ALL-START-METRICS.json`).

## G. Frequency comparison

두 질문을 분리했다.

1. **Own anti-alias sufficiency:** 후보 자신의 `target/(2*source)` 이상에서
   raw peak/RMS와 reconstructed RMS를 계산한다.
2. **Final fidelity:** 실제 output 위치에서 후보와 CURRENT의 complex response 차이를
   pass / transition / 전체 frequency 범위에서 계산한다.

A/B의 band에 CURRENT와 quarter9도 각각 다시 평가했다.
과거 quarter `[.125,.5]` 결과를 그대로 분모에 넣지 않았다.
축별 차이를 계산한 뒤 X/Y mean power에 동일 가중치를 줬다.

### Strong24 — 같은 band 안의 비교

| s / band | CURRENT raw peak / Recon RMS | quarter9 raw peak / Recon RMS | 해당 후보 raw peak / Recon RMS |
|---|---|---|---|
| .896 / A | .28095 / .06081 | .46405 / .16908 | .52806 / .24087 |
| .65 / A | .06624 / .03085 | .29024 / .10839 | .39081 / .20014 |
| .50 / A | .01436 / .00031 | .22888 / .10566 | .31312 / .15627 |
| .896 / B | .28095 / .07000 | .46405 / .17620 | .77127 / .19299 |
| .65 / B | .06624 / .03255 | .29024 / .09670 | .78651 / .20249 |
| .50 / B | .01436 / .00023 | .22888 / .10033 | .66482 / .21956 |

특히 `.65`는 A/B 대표 모두 CURRENT의 약 6배, 각 전체 최저값도 약 5.6배/4.7배다.
동시에 quarter9보다 커졌으므로 요청한 topology-level improvement의 증거가 없다.
`R*s >=8`인 필수 strength들에서는 Late Smooth의 blur contribution이 1이라
sharp handoff로 이 차이가 가려지지 않는다.

### 전체 주파수의 final complex error vs CURRENT

아래는 band cutoff로 잘라 보지 않고 `[0,.5]` 전체를 비교한 값이다.
각 validation grid에는 해당 band의 실제 경계점도 소수 추가되어 있으므로 세 자리로 표시한다.

| R / s | CURRENT | quarter9 | A | B |
|---|---:|---:|---:|---:|
| 24 / .896 | 0 | .119 | .205 | .199 |
| 24 / .65 | 0 | .092 | .166 | .159 |
| 24 / .50 | 0 | .087 | .130 | .163 |
| 16 / .896 | 0 | .124 | .162 | .196 |
| 16 / .65 | 0 | .056 | .108 | .131 |
| 16 / .50 | 0 | .018 | .045 | .118 |

이 값만으로 시각 품질의 크기를 단정하지 않는다. 다만 alias band 완화와 별개로
출력 fidelity 역시 개선되지 않았다는 보조 근거다. 모든 raw RMS/pass/transition/
variance/phase/support 수치는 [TABLES.md](TABLES.md), FREQUENCY.json (로컬 자료: `FREQUENCY.json`)에 있다.

### `.50` CURRENT 값이 과거보다 매우 작게 보이는 이유

과거 보고서는 16 gather phases와 quarter output polyphases를 평균했다.
이번 최종 출력은 **실제 fixture의 texel centers와 모든 interior output 위치**를 사용한다.
정확한 4:1의 half-texel phase와 s=.50 packed kernel 조합에서는 특정 고주파 tail이
매우 작아진다. 여기에 A/B용 더 높은 stop cutoff를 사용했다.
따라서 과거 `.00693`과 이번 `.00031/.00023`은 같은 지표가 아니다.

그 작은 분모에서 수백 배라는 숫자를 주요 결론으로 쓰지 않았다.
홀수 크기로 phase progression을 바꿔도 결론은 유지된다.

| Strong24 odd-size sanity | CURRENT same-band Recon RMS | Candidate Recon RMS |
|---|---:|---:|
| A / .65 | .03126… .03128 | .19944… .19990 |
| A / .50 | .00718… .00722 | .15584… .15609 |
| B / .65 | .03370… .03375 | .26580… .26712 |
| B / .50 | .00699… .00703 | .28087… .28215 |

Odd sanity는 width/height를 각각 1px 늘린 경우, 513 frequencies와 실제 경계점의
offline 검사다. GPU에서 odd-size 렌더링을 검증했다는 뜻은 아니다.

그래프: [Strong24](frequency-radius24.png), [Soft16](frequency-radius16.png).
왼쪽은 16-phase raw worst amplitude, 오른쪽은 실제 lattice의 reconstructed RMS다.
곡선은 513점, 판정 표는 2049점+실제 경계점을 사용했다. 새 화면 캡처가 아니다.

## H. Sampling-cost comparison

Full-quad filtering만 계산한다. Source capture, Output, clear, submission,
dependency, D2 coverage reduction 등은 제외했다. 실제 GPU 시간 추정이 아니다.

```text
CURRENT reads = R * (H_width*H_height + V_width*V_height)
A reads       = 49 * ceil(SourceW/3)*ceil(SourceH/3)
B reads       = 25 * ceil(SourceW/2)*ceil(SourceH/2)
```

| R | CURRENT reads | A reads / 차이 | B reads / 차이 |
|---|---:|---:|---:|
| 16 | 10,914,480 | 11,884,656 / +8.89% | 13,643,100 / +25.00% |
| 24 | 16,742,280 | 12,187,476 / −27.21% | 13,951,900 / −16.67% |

Soft16에서 reads가 늘더라도 stage 하나 제거가 도움이 될 가능성은 별개다.
따라서 read 수 증가만으로 탈락시키지 않았다. 이번 STOP 이유는 offline 품질 격차다.

## I. Intermediate-storage comparison

CURRENT `H+V`와 후보의 단일 Blur를 비교한다. Source는 모든 경우 동일하다.

| R | CURRENT H+V pixels | A Blur pixels / 변화 | B Blur pixels / 변화 |
|---|---:|---:|---:|
| 16 | 682,155 | 242,544 / −64.44% | 545,724 / −20.00% |
| 24 | 697,595 | 248,724 / −64.35% | 558,076 / −20.00% |

**Blur target을 quarter보다 크게 만들어도 H를 없애면 총 intermediate는 줄어드는 것이 맞다.**
다만 Source까지 포함한 논리 FBO payload 감소는 A 약15.3%, B 약4.76%로 작아진다.

| R / format | CURRENT Source+H+V | A Source+Blur | B Source+Blur |
|---|---:|---:|---:|
| 16 / A8 | 2.7323 MiB | 2.3131 MiB | 2.6022 MiB |
| 16 / RGBA8 | 10.9293 MiB | 9.2523 MiB | 10.4089 MiB |
| 24 / A8 | 2.7942 MiB | 2.3661 MiB | 2.6611 MiB |
| 24 / RGBA8 | 11.1767 MiB | 9.4644 MiB | 10.6445 MiB |

A8=1 byte/texel, RGBA8=4 bytes/texel의 **계산값**이다. VRAM/RSS 실측이 아니며
driver alignment/internal format, 원본 text/metadata textures, window/MSAA,
CPU buffers 및 교체 중 중첩 생존 자원은 포함하지 않는다. COST.json (로컬 자료: `COST.json`).

## J. Candidate selection

**GPU 승격 후보 없음.** A를 먼저 최적화·검토했고 B도 같은 기준으로 검토했다.
각 strength의 independent 커널부터 성공하지 못했으므로 continuous animation용
parameterization도 만들지 않았다. sample을 추가하거나 prototype flag를 넣지 않았다.

offline 판정은 [OFFLINE-DECISION.md](OFFLINE-DECISION.md)에 기록했다.
새 absolute PASS threshold를 만들지 않고 CURRENT/quarter9와의 상대 격차 및
전체 starts의 최소값으로 판단했다.

## K. GPU quality result

미수행. offline GO 후보가 없어 CURRENT/candidate viewer 및 Korean GPU 캡처를 만들지 않았다.
이 보고서는 A/B를 실제 화면에서 보았다는 주장이나 visual quality FAIL 판정이 아니다.

## L. Animation artifact audit

미수행. 8초 왕복/lattice/pop/shimmer/near-sharp 검증은 offline GO 후의 단계다.
이번 offline kernel들은 strength마다 독립이며, 연속 애니메이션용 API/구현이 아니다.

## M. Runtime 3→2 implementation

미수행. H task/FBO/camera/renderer/geometry/dependency를 실제로 제거하지 않았다.
현재 production/runtime/sample 동작은 그대로다.

## N. Structural inventory

새 runtime이 없으므로 새 count 측정 없음. offscreen stage `3→2`, Cards entrance
`45→30`, whole entrance `60→~40`, Cards exit `36→24`, whole exit `57→~38`은
architecture 목표였으며 달성 수치가 아니다.

## O. Memory/FBO inventory

I의 logical payload만 계산했다. 실제 live/peak lifetime, FBO initialization count,
자원 재사용·해제·lifecycle을 측정하지 않았다.
`~57→~38` initialization attempts도 미검증 목표이며 TV latency로 환산하지 않는다.

## P. PC performance gate

미수행. quality PASS가 없으므로 private runtime build 및 5-process benchmark 없음.
command CPU, render-thread CPU, GPU timer, FPS, swap/iteration p95를 새로 측정하지 않았다.

## Q. PC→TV interpretation

이번 결과는 offline screening이다. theoretical read/storage 감소를 실제 PC 또는
TV 성능 개선율로 옮겨 적을 수 없다. 향후 다른 후보의 PC 결과 역시 방향성 검증일 뿐이다.

## R. Direct-raster final verdict

**이번 quarter/third/half direct 2D single-pass raster 후보군은 여기서 종료한다.**
해상도를 높이고 tap 수를 줄이는 이번 마지막 두 조합도 요청한 gate를 통과하지 못했다.
추가 taps, 더 큰 halo, prefilter/재구성 pass를 붙여 이 후보를 연장하지 않았다.

현행 CURRENT PERFORMANCE를 유지한다. 이후 방향으로는 compute/shared-memory separable,
scale-space 기반 구조, 제품 차원의 blur 정책을 별도 과제로 검토할 수 있지만,
이번에는 구현하지 않았고 가능성/비용/품질도 검증하지 않았다.

## S. Target escalation

**NO.** GBS/RPM/target install/measurement 없음.

## T. Git state

Core/Adaptor/UI의 시작 status, HEAD, branch와 dirty/untracked 파일 hash를 기록했다.
종료 시 HEAD/branch/status/binary diff/dirty 파일 SHA-256이 **모두 시작과 동일**하다.

| Repo | Branch | HEAD |
|---|---|---|
| UI | devel_blur_text | `05087317cac8ea9600bba498f00ccf8086a79d3f` |
| Core | tizen_10.1 | `8228720460a4910151f4eb4ad36976816b13a102` |
| Adaptor | tizen_10.1 | `a3ab9b8db9637fda4c973b5d59c4d4b95a9d5286` |

기존 diagnostic dirty state와 sample 수정은 보존했다. 이전 quality 디렉터리는
read-only로 재사용했다. 신규 파일은 이 외부 diagnostic 디렉터리에만 있다.
commit/push/reset/restore/stash/rebase/amend 없음. 세 repo `git diff --check` 통과.
production build/UTC/sanitizer 없음.

증빙: BASELINE.json (로컬 자료: `BASELINE.json`), FINAL_STATE.json (로컬 자료: `FINAL_STATE.json`).
실행 자료: model.py (로컬 자료: `model.py`), check-model.py (로컬 자료: `check-model.py`),
optimize.py (로컬 자료: `optimize.py`), validate.py (로컬 자료: `validate.py`), summarize.py (로컬 자료: `summarize.py`).

보고서/표/그래프를 확인하면 된다. **이번 단계의 새 실행형 GPU viewer는 없다.**
