# Direct 2D 9×9 — independent-strength feasibility

## A. Executive Verdict

**81-READ BUDGET FUNDAMENTALLY INSUFFICIENT — Phase 1 STOP**

요청한 판정명이며, **이번 조건의 bounded search에 대한 실용적 No-Go**를 뜻한다.
모든 81-read 필터의 수학적 불가능성이나 global optimum을 증명한 것은 아니다.

strength마다 offsets와 weights를 모두 독립적으로 풀어도 Strong24의 `.65/.50`에서
CURRENT와 큰 차이가 남았다. fixed scaling은 실제 제약이지만, 이번 실패를 그것만으로
설명할 수 없다. 요청한 중단 조건에 따라 adaptive interpolation, GPU viewer,
runtime 3→2, 성능 측정으로 진행하지 않았다.

| Strong24 strength | CURRENT reconstructed stop RMS | fixed9 | independent9 | CURRENT 대비 잔여 배율 |
|---|---:|---:|---:|---:|
| .896 | .07958 | .17872 | .15676 | 1.97× |
| .65 | .02971 | .14187 | .11927 | 4.02× |
| .50 | .00693 | .09875 | .09302 | 13.43× |

위 값은 주파수 모델의 누설 진폭 지표다. 화면 밝기·GPU 시간·FPS의 배율이 아니다.
원본 UI/Core/Adaptor와 기존 dirty state는 보존했다.

## B. Why fixed9 failed

기존 fixed9는 `offset(s)=baseOffset*s`, `weight(s)=baseWeight`였다.
CURRENT도 packed offsets에 strength를 적용하지만, axis당 16/24 reads와 LINEAR의
실제 texel 분포를 사용하므로 strength에 따라 응답의 봉우리와 상쇄 위치가 달라진다.
9 positions만 가진 별도 고정 커널을 축소한다고 그 변화까지 따라가는 것은 아니다.

이번 결과는 두 부분을 분리한다.

- **고정 scaling의 손해는 있다.** 독립화 후 Strong24 `.65`의 reconstructed RMS는
  fixed9보다 15.9% 감소했고, Soft16 `.50`에서는 약 51.1% 감소했다.
- **그 제약을 풀어도 충분하지 않다.** Strong24 `.50`은 5.8% 감소에 그쳤고,
  `.65/.50`은 각각 CURRENT의 약 4.0×/13.4×다.

따라서 “좋은 9×9 커널은 이미 존재하고 interpolation만 잘하면 된다”는 근거를
얻지 못했다. sparse sampling의 누설이라는 기존 설명을 지지하지만, 한정된
최적화 결과만으로 모든 가능한 커널의 하한을 확정하지는 않는다.

## C. Independent-strength formulation

사전에 기록한 [FORMULATION.md](FORMULATION.md)를 따랐다.

```text
positions = [-x4, -x3, -x2, -x1, 0, x1, x2, x3, x4]
weights   = [ w4,  w3,  w2,  w1,w0, w1, w2, w3, w4]
w0 + 2*(w1+w2+w3+w4) = 1
```

`xi`는 **해당 strength에서 바로 사용하는 Source-pixel 절대 offset**이다.
후보 평가에서 strength를 다시 곱하지 않는다. 각 case의 offsets와 weights는
다른 strength와 공유하거나 묶지 않았다. 2D 해석은 두 축의 tensor product로 81 reads다.

- Hard: symmetric, finite, nonnegative weights, normalized energy, zero centroid,
  ordered offsets, 기존 halo 안의 support.
- 수치적 탐색 범위: 양의 offset 간 최소 간격 `.15px`, 최대 `R+1`.
  보수적인 LINEAR reach 1px를 포함해 기존 halo `R+2` 이내다.
  이 추가 간격 제한까지 포함한 탐색이며, 모든 실수 좌표 배치의 완전 탐색은 아니다.
- Variance: CURRENT-relative **soft penalty**만 사용. exact equality로 강제하지 않았다.
- Reference: 이전 frequency.py (로컬 자료: `../reveal-frequency-quality.HryP4o/frequency.py`)의
  실제 packed Gaussian 추출값과 LINEAR gather, quarter polyphase reconstruction,
  기존 Late Smooth를 그대로 재사용. ideal Gaussian으로 대체하지 않았다.
- Phase: `0, 1/16, …, 15/16`의 16개.
- Training: `0…0.5`의 257개 uniform frequency와 기존 axis/diagonal probes.
  Validation: 2049개 uniform frequency. pass `<.125`, stop `>=.125`.
- Cases: Strong24 `.896/.65/.50/1/.8/.35/.25`, Soft16 `.896/.65/.50`.
- SLSQP: case마다 16 deterministic starts × 기존 objective family 2개 = 32회.
  총 **320회**, 각 최대 220 iterations. 모든 offset/mass는 초기값 이후 자유롭다.

목적함수는 기존 여섯 항을 유지했다: reconstructed pass error, reconstructed stop energy,
worst phase/frequency reconstructed peak, phase variation, relative variance error,
CURRENT-relative axis/diagonal error. 두 family의 계수는 사전 고정한
`(1,1,1,1,1,1)` / `(2,1,1,1,4,2)`이다. 화면을 보고 계수를 조정하지 않았다.
Transition `.10… .15` 응답 차이도 별도 검증 지표로 기록했다.

319회 수렴, 320회 모두 feasible. 1회는 iteration limit에 도달했고 선택되지 않았다.
선택된 10개 모두 수렴했다. 이는 best-found feasible kernels이지 증명된 최적해가 아니다.
후보 선택은 feasible Pareto set의 balanced score 최소값이며,
**판정은 그 점수만이 아니라 아래 개별 지표와 전체 start들의 최소 누설값으로 했다.**

모델 독립 확인: 실제 sine source의 LINEAR gather → quarter mapping → LINEAR 재구성과
모델 예측을 비교했다. 필수 6 cases × even/odd extents 3개 × CURRENT/후보 × 4 frequencies,
총 **144개**, 최대 complex error `2.07e-13`.
이는 strength 중복 곱셈 및 texel-center/odd-size mapping 확인이지 GPU 검증은 아니다.
Frequency 표는 exact 4:1 모델이고, odd-size 확인은 실제 비율을 적용한 별도 gather 검사다.
8-bit FBO quantization, driver precision, clamp 경계와 실제 글리프 영상은 이번 모델에 포함하지 않았다.

자료: OFFLINE.json (로컬 자료: `OFFLINE.json`), PARETO.json (로컬 자료: `PARETO.json`),
MODEL-CHECK.json (로컬 자료: `MODEL-CHECK.json`), VERIFICATION.json (로컬 자료: `VERIFICATION.json`).

## D. Independent optimal kernels

아래는 best-found 대표 후보다. 숫자는 Source pixels / 한쪽 tap의 weight이며,
표의 소수점 반올림 값 대신 SELECTED.json (로컬 자료: `SELECTED.json`)에 전체 정밀도를 보존했다.
`w1…w4`는 pair 합계가 아니다. 양쪽에 각각 같은 값이 들어간다.

| Radius / strength | Positive offsets x1…x4 | Weights w0, w1…w4 |
|---|---|---|
| 24 / .896 | 2.283955, 4.763750, 7.531623, 13.347231 | .135572, .138176, .106962, .124915, .062161 |
| 24 / .65 | 1.720208, 3.200304, 4.951738, 8.607319 | .120083, .120973, .133674, .096065, .089246 |
| 24 / .50 | 1.339928, 2.581894, 4.237463, 7.218503 | .117709, .133863, .133787, .108793, .064701 |
| 16 / .896 | 1.654254, 3.215424, 4.782313, 8.325047 | .128047, .140637, .133090, .090136, .072113 |
| 16 / .65 | 1.128686, 2.428452, 3.756627, 5.653936 | .152189, .145438, .116374, .084385, .077709 |
| 16 / .50 | 1.100334, 2.223249, 3.400029, 4.969446 | .183766, .160558, .127037, .077241, .043281 |

예를 들어 Strong24의 바깥 offset은 `13.347/.896 ≈14.90`, `8.607/.65 ≈13.24`,
`7.219/.5 ≈14.44`로, 동일 baseOffset의 축소가 아니다. weights도 각각 다르다.
모든 320 후보에 symmetry/normalization/centroid/order/halo 조건을 다시 검사했다.

## E. Independent optimum vs CURRENT frequency metrics

`Peak`: stop band의 worst frequency × phase amplitude.
`RMS`: stop band의 frequency/phase 평균 RMS.
`Recon RMS`: quarter sampling 후 네 output polyphases의 LINEAR 재구성까지 포함한 RMS.
`Pass error`: CURRENT와의 complex response 차이 RMS. CURRENT 자체는 0이다.
이는 1D axis 응답이고, 임의 영상의 최종 2D visual score가 아니다.

### Strong24

| s | Path | Peak | RMS | Recon RMS | Pass error |
|---:|---|---:|---:|---:|---:|
| .896 | CURRENT | .28095 | .09574 | .07958 | 0 |
| | old9 | .91833 | .35963 | .29704 | .03849 |
| | fixed9 | .54408 | .22120 | .17872 | .02427 |
| | independent9 | .46405 | .21852 | .15676 | .03989 |
| .65 | CURRENT | .06624 | .03448 | .02971 | 0 |
| | old9 | .80612 | .32917 | .30229 | .01791 |
| | fixed9 | .41400 | .17144 | .14187 | .01551 |
| | independent9 | .29024 | .15777 | .11927 | .01617 |
| .50 | CURRENT | .01841 | .00857 | .00693 | 0 |
| | old9 | .65572 | .29190 | .19712 | .01112 |
| | fixed9 | .33705 | .12392 | .09875 | .01499 |
| | independent9 | .22888 | .11802 | .09302 | .00676 |

### Soft16

| s | Path | Peak | RMS | Recon RMS | Pass error |
|---:|---|---:|---:|---:|---:|
| .896 | CURRENT | .37870 | .11051 | .09125 | 0 |
| | old9 | .84178 | .31427 | .25334 | .01768 |
| | fixed9 | .40727 | .13677 | .10302 | .01449 |
| | independent9 | .27457 | .14151 | .10460 | .00990 |
| .65 | CURRENT | .07303 | .04281 | .03424 | 0 |
| | old9 | .57782 | .25769 | .20454 | .01332 |
| | fixed9 | .17871 | .09430 | .08055 | .01677 |
| | independent9 | .17196 | .08167 | .06291 | .01722 |
| .50 | CURRENT | .16565 | .03601 | .02313 | 0 |
| | old9 | .49483 | .18427 | .15524 | .01072 |
| | fixed9 | .19114 | .08172 | .06528 | .01382 |
| | independent9 | .15862 | .04407 | .03195 | .01270 |

Soft16처럼 peak가 작아도 RMS는 클 수 있으므로, 한 지표만으로 PASS라고 판단하지 않는다.

### Phase / width / transition

| Radius / s | independent variance error range | Worst-peak phase | independent phase-complex RMS | CURRENT phase-complex RMS |
|---|---:|---:|---:|---:|
| 24 / .896 | −.377…−.265% | .5000 | .04351 | .07746 |
| 24 / .65 | −5.078…−4.689% | .0000 | .08747 | .02972 |
| 24 / .50 | −3.196…−2.792% | .6875 | .05193 | .00746 |

Phase RMS는 전체 `0… .5`에서 phase 평균 응답과의 차이다.
Strong24 `.896`은 phase variation 자체는 CURRENT보다 작아도 stop leakage가 더 크다.
이를 “모든 품질 지표가 나빠졌다”라고 해석해서는 안 된다.
필수 strength 모두 `R*s >= 8`이라 Late Smooth blur contribution은 1이다.
따라서 이 구간의 큰 차이를 sharp handoff가 가려주지는 않는다.

Strong24 `.896/.65/.50`의 transition complex error는 independent9에서
`.14209/.08754/.01339`, fixed9에서 `.07330/.03543/.01986`이다.
높은 두 strength에서는 peak를 낮추면서 transition fidelity가 악화되는 trade-off도 있다.
Stop integrated energy와 Soft16 phase/variance 상세는 FREQUENCY.json (로컬 자료: `FREQUENCY.json`)에 있다.

### 대표 점수 선택 때문에 좋은 해를 놓친 것은 아닌가?

전체 32 starts의 dense validation을 별도로 확인했다.

| Strong24 s | CURRENT Recon RMS | 대표 independent9 | 전체 후보 중 최소 Recon RMS | 최소값도 CURRENT 대비 |
|---|---:|---:|---:|---:|
| .896 | .07958 | .15676 | .14450 | 1.82× |
| .65 | .02971 | .11927 | .11582 | 3.90× |
| .50 | .00693 | .09302 | .07713 | 11.13× |

최소값은 다른 trade-off를 가진 후보이며, 해당 후보의 모든 지표가 좋은 것은 아니다.
예를 들어 `.65` 최소 Recon RMS 후보는 variance가 약 −20.9%, pass error `.05305`이고,
`.50` 최소 후보는 variance 약 −6.8…−6.3%, pass error `.02649`다.
raw peak만 최소인 후보를 고르더라도 `.65=.28886`, `.50=.22888`로 CURRENT와 큰 차이가 남는다.
서로 다른 후보의 최소값을 합쳐 하나의 가상 커널처럼 제시하지 않았다.
전체 결과: ALL-START-METRICS.json (로컬 자료: `ALL-START-METRICS.json`).

### Additional offline sanity

| Strong24 s | CURRENT Recon RMS | fixed9 | independent9 |
|---|---:|---:|---:|
| 1.00 | .10653 | .18706 | .15810 |
| .80 | .05486 | .16933 | .15501 |
| .35 | .02264 | .06099 | .03871 |
| .25 | .05464 | .05841 | .03887 |

낮은 일부 strength에서는 CURRENT보다 raw reconstructed leakage가 작아질 수도 있다.
하지만 이때의 pass-band/width fidelity와 Late Smooth contribution까지 별도로 봐야 한다.
그 사실이 mandatory `.65/.50`의 실패를 상쇄하지는 않는다.

곡선: [Strong24](frequency-radius24.png), [Soft16](frequency-radius16.png).
이 그림은 offline 모델 그래프이며 새 framebuffer 캡처가 아니다.

## F. 81-read feasibility verdict

**독립 offsets/weights만으로 이번 응답 격차를 충분히 닫지 못했다.**
새로운 임의의 PASS 임계값은 만들지 않았다. 이미 GPU 품질 실패한 fixed9와 비교해
Strong `.65/.50`의 reconstructed RMS 개선이 제한적이고, CURRENT와의 차이가
전체 start의 최저값에서도 크다는 상대 비교로 STOP을 결정했다.

이 결과는 요청한 symmetric/nonnegative 9 positions/axis, 기존 halo와 phase coverage의
실용적 sample-density/freedom 한계를 지지한다. 다른 목적함수·더 넓은 search·비 tensor
81-read 배치를 모두 배제하는 증명은 아니다. 이번 범위에서 그것을 추가 탐색하지 않는다.
Phase 1 판정은 후속 작업 전에 [PHASE1-DECISION.md](PHASE1-DECISION.md)에 기록했다.

## G. Knot selection

미수행. independent feasibility가 STOP이므로 adaptive knots를 선정하지 않았다.

## H. Continuous interpolation formulation

미수행. strength-specific shader, shader swap, interpolation 코드를 만들지 않았다.

## I. Held-out strength validation

Adaptive trajectory를 만들지 않아 미수행. 추가 `.8/.35/.25/1`은 각각 독립 최적화한
sanity cases이며, interpolation의 held-out 검증이라고 부르지 않는다.

## J. Soft16 GPU quality

새 independent/adaptive GPU viewer·캡처 없음. 기존 old9/fixed9 결과는 그대로 두었다.
이번 offline improvement를 새로운 visual PASS로 해석하지 않는다.

## K. Strong24 GPU quality

새 GPU 검증 없음. Phase 1 frequency gate에서 중단했다.
“adaptive를 실제 화면에 그려 보니 실패했다”는 결론이 아니다.

## L. Animation / pop / shimmer audit

새 8초 왕복 영상 및 knot transition 검증 없음. quality gate를 우회하지 않았다.
이번 strength 값은 progress가 아니며, 새 progress/screenshot log도 없다.

## M. Theoretical sample cost

가정: 동일 FHD Label, radius halo 포함 full Source, full-quad H/V, exact 4:1.
CURRENT H는 X만 1/4, V는 XY 1/4. H/V의 각 fragment는 해당 radius만큼 reads한다.
Direct 후보는 reduced fragment마다 81 reads다.

| Radius | Source | CURRENT H / V | CURRENT H+V reads | Direct81 reads | 차이 |
|---|---|---|---:|---:|---:|
| 16 | 1956×1116 | 489×1116 / 489×279 | 10,914,480 | 11,050,911 | +1.25% |
| 24 | 1972×1132 | 493×1132 / 493×283 | 16,742,280 | 11,301,039 | −32.5% |

독립 커널도 9×9 tensor라 81 reads는 변하지 않는다. 이는 **filtering 명령 수 추정**이며
실제 GPU/CPU 시간이나 memory bandwidth가 아니다. Source capture, Output,
FBO clear, dependency, submit, clipping/D2와 adaptive ALU 비용은 포함하지 않는다.
runtime 후보를 만들지 않아 task/FBO 감소나 실제 메모리 절감을 실현·측정하지 않았다.

## N. PER_LINE feasibility

미수행. WHOLE_TEXT의 independent feasibility부터 통과하지 못했다.
라인별 parameter 전달 비용을 추가 설계하지 않았다.

## O. Runtime 3→2

미수행. H RenderTask/FBO/Camera/Renderer/Geometry/dependency를 제거하지 않았다.
Production 및 기존 diagnostic pipeline은 변경하지 않았다.

## P. Structural inventory

새 runtime이 없어 새 inventory 측정 없음. Cards entrance `45→30`, whole entrance
`60→~40`, Cards exit `36→24`, whole exit `57→~38`, initialization `~57→~38`은
요청서의 목표이지 이번에 달성한 수치가 아니다.

## Q. PC performance gate

미수행. warm-process CPU/GPU/FPS/p95/create-count 측정 없음.
품질 실패 후보에 대해 “빠르긴 하다”는 benchmark를 하지 않았다.

## R. PC→TV interpretation

이번 결과는 offline frequency screening이다. PC runtime 개선조차 측정하지 않았으며,
이론 read 감소를 TV ms/FPS 개선으로 환산할 수 없다.
추후 PC gate를 통과하는 다른 후보도 TV에는 방향성 근거일 뿐이다.

## S. Target escalation

**NO.** GBS/RPM/build/install/target measurement 없음.
11×11 추가 offline/GPU 또는 13×13 확장도 하지 않았다.
이번 direct 9×9 접근은 여기서 종료하고 CURRENT를 유지한다.

## T. Git state / 재현 자료

시작 시 세 repo의 status/HEAD/branch를 확인했고, 종료 시 HEAD·branch·status·binary diff와
기존 dirty/untracked 파일의 SHA-256을 대조했다. **모두 시작 상태와 동일하다.**

| Repo | Branch | HEAD |
|---|---|---|
| UI | devel_blur_text | `05087317cac8ea9600bba498f00ccf8086a79d3f` |
| Core | tizen_10.1 | `8228720460a4910151f4eb4ad36976816b13a102` |
| Adaptor | tizen_10.1 | `a3ab9b8db9637fda4c973b5d59c4d4b95a9d5286` |

증빙: BASELINE.json (로컬 자료: `BASELINE.json`), FINAL_STATE.json (로컬 자료: `FINAL_STATE.json`).
기존 dirty 작업을 포함해 production source 및 sample을 변경하지 않았다.
commit/push/reset/restore/stash/rebase/amend 없음. 세 repo `git diff --check` 통과.
production build/UTC/sanitizer 없음. 기존 quality 디렉터리는 read-only 재사용했다.

새 파일은 이 외부 diagnostic 디렉터리에만 있다.
model.py (로컬 자료: `model.py`), optimize.py (로컬 자료: `optimize.py`), validate.py (로컬 자료: `validate.py`),
model-check.py (로컬 자료: `model-check.py`), finish.py (로컬 자료: `finish.py`)와 JSON/보고서/주파수 그래프다.

결과 검증/그림 재생성만 다시 실행하려면:

```bash
cd /home/bowonryuubuntu/tizen
env OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  PYTHONPATH=/home/bowonryuubuntu/tizen/reveal-frequency-quality.HryP4o/deps:/home/bowonryuubuntu/tizen/reveal-dense2d-quality.ECWtnl/deps \
  python3 reveal-independent-quality.On1JuQ/finish.py
```

전체 최적화를 재실행할 필요는 없다. 이번 단계의 새 실행형 viewer는 없다.
