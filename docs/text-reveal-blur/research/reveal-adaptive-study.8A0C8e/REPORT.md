# WHOLE_TEXT Fade-aware Adaptive Gaussian — PoC report

2026-09-15 · production baseline `46d0ea182d77af03b8b75ab6e5480bf5d591624c`

## 결론

**ADAPTIVE GAUSSIAN PROMISING — READY FOR TARGET TEST.** Production 적용 판정은 아니다.

- 실제 FHD Linear reverse의 평균 draw GPU 비용: **A8 13.1%, RGBA 18.1% 감소**.
- 최종 후보의 FHD 45 checkpoints × A8/RGBA에서 **final RGB max error ≤ 1 LSB**. 각 format의 22/45 checkpoints는 byte-identical.
- CPU의 안정적인 증가 징후 없음. Source/H/V 크기, texture/FBO 개수, logical texture payload는 동일.
- 중요한 한계: **progress 0.2~0.6의 중간 구간은 exact24를 유지한다. 이 구간의 피크 GPU 비용을 해결한 최적화는 아니다.** 평균 비용 개선과 타겟의 최저 FPS 개선은 구분해야 한다.
- production, public API, HIGH, shared Gaussian/UBO, Source/Output, task refresh rate는 변경하지 않았다. 모든 코드와 결과는 repository 밖의 uncommitted 진단이다.

## PART A — Exact baseline

현재 production `gaussian-blur-algorithm.cpp`의 계산 함수를 그대로 추출하여 reference coefficients를 만들었다. 임의 Gaussian 식이나 재추정 sigma를 사용하지 않았다.

| 항목 | 값 |
|---|---:|
| authored radius | 48 |
| `CalculateGaussianConstants` 인수 | 24 samples |
| Gaussian bell-curve width / sigma | 15.10251141 |
| discrete kernel | 중심을 포함한 95 positions, 기존 bilinear pair construction |
| positive-side pairs | 24 |
| active H/V Gaussian의 texture instructions | ±24 = 48 / fragment / pass |
| 첫 / 마지막 positive offset | 0.666179359 / 46.44920731 |
| positive pair spacing min / max | 1.831080437 / 1.997829437 |
| symmetric mass | 1.00000003679 (production float rounding 포함) |
| positive-side first moment | 5.98997504507 |

전체 24 offsets/weights와 25-point strength/spacing trajectory: kernel-stats.csv (로컬 자료: `kernel-stats.csv`).
원문 계산 코드: production-kernel-excerpt.inc (로컬 자료: `production-kernel-excerpt.inc`).

이 recipe에서 `a(p)=p`, `s(p)=1-p*p*(3-2*p)`, effective authored radius는 `48*s(p)`다. 예:

| p | opacity | strength | effective radius | positive pair spacing |
|---:|---:|---:|---:|---:|
| .975 | .975 | .001844 | .0885 | .00338~.00368 px |
| .90 | .90 | .028 | 1.344 | .0513~.0559 px |
| .80 | .80 | .104 | 4.992 | .1904~.2078 px |
| .60 | .60 | .352 | 16.896 | .6445~.7032 px |
| .40 | .40 | .648 | 31.104 | 1.1865~1.2946 px |
| .20 | .20 | .896 | 43.008 | 1.6406~1.7901 px |

`p=1`은 기존 one-fetch copy, `p=0`은 기존 zero-return이므로 Gaussian 48-fetch 설명에서 제외한다.

### Reference 검증

native production H/V shader와 동일 24 coefficients의 private shader를 25 points × 3 corpora에서 비교했다. **Output 75/75 byte-identical**. 한글 p=.95에서 H/V 각각 최대 1 LSB 차이가 있었지만 Output은 동일했다. 이후 모든 오차는 private24가 아닌 **production shader Output**을 기준으로 계산했다.

## PART B — Diagnostic quality study

### 측정 경로와 fixture

CPU 이미지 시뮬레이터만으로 판단하지 않았다. 기존 native DALi library로 Label/Source/H/V/Output을 실제 생성하고, 외부 앱에서 H/V shader만 바꾸어 GL readback했다.

| 항목 | Phase A | FHD runtime 검증/측정 |
|---|---|---|
| Label | 540×300 | 1920×1080 |
| actual text raster | 540×204, 6줄 | 1920×1054, 31줄 |
| font size 설정 | 24 | 24 |
| window | 640×400 | 1920×1080 |
| Source FBO | 640×400 | 2020×1180 |
| H FBO | 160×400 | 505×1180 |
| V FBO | 160×100 | 505×295 |
| UI/render scale | 1 / 1 | 1 / 1 |
| recipe | PIXEL, WHOLE_TEXT, Fade=1, Stagger=0, radius48, BlurTime=1, PERFORMANCE | 동일 |

Dense Korean은 기존 grid 실험에 사용한 6줄 문장을 재사용했다. Latin은 AVATAR/ffi/IIII/WWWW와 thin/wide strokes의 6줄, RGBA는 같은 한글에 color span + linear gradient를 적용했다. FHD는 기존 측정의 자연스러운 한글 문단을 재사용했다.

호스트: GTX 1650 / NVIDIA 595.91.07 / GLES / X11, window MSAA=4. 설치된 production library를 덮어쓰지 않고 현재 foundation build를 `LD_LIBRARY_PATH`로 선택했다.

### 비교한 grouping

1. **Static**: 첫/마지막 pair를 유지하고 내부 positive pairs를 contiguous groups로 분할. K=2는 전체를 두 group으로 병합한다. 기존 fixed-cap 방식의 비교군이다.
2. **Effective-aware**: 각 strength에서 실제 bilinear sample의 fractional texel phase를 고려한 contiguous grouping. quarter H/V의 sampling axis는 이번 fixture에서 half-texel phase다. 각 group의 bilinear impulse-response 오차 제곱합을 계산하고, 그 합을 최소화하는 DP partition을 사용했다.
3. **Pixel bins**: scaled positions를 0.5px / 1px bins로 병합하는 별도 진단.

단순 offset-distance SSE에 strength만 곱하면 비용에 공통 상수만 곱해져 partition이 바뀌지 않는다. 따라서 2번은 실제 bilinear interpolation의 경계를 반영했다. 다만 이는 **선택한 additive group-error objective의 최적해**이지, 최종 화면의 전역 최적 Gaussian kernel이나 이론적 minimum-pair 증명은 아니다.

모든 group은 `weight=sum(weights)`, `offset=sum(weight*offset)/sum(weights)`다. 앞 K개만 남기지 않는다. 모든 원래 pair가 정확히 한 group에 포함되며, 양수 weight, offset 순서, symmetry, mass와 positive first moment를 검사했다. Tail mass도 병합되어 남는다. Centroid로 병합하므로 마지막 sample 좌표/second moment까지 원본과 동일하다는 뜻은 아니다.

### Metric

- 실제 Source는 **이미 production fade가 적용된 상태**다. 비교 결과에 opacity를 다시 곱하지 않았다.
- H/V/Output을 저장했다. A8 intermediate는 R8 coverage channel, RGBA는 RGB 및 alpha 최대 차이를 기록했다.
- Output은 full framebuffer와 foreground+halo ROI의 RGB max/MAE/RMSE/p99/changed pixels를 기록했다. 검은 배경의 MAE만으로 판정하지 않았다.
- axis-projected error의 periodic energy도 기록했다. 글자 자체의 반복 주기도 포함되므로 이를 단독 grid 판정기로 사용하지 않았다.
- 2×/4× crop과 확대 오차 영상을 별도로 확인했다.

주 sweep은 **25 points × 20 cases × 3 corpora = 1,500 records**다. 모든 requested budgets 24/20/16/12/10/8/6/4/2를 포함한다.

전체 수치: all-metrics.csv (로컬 자료: `all-metrics.csv`), phase-a-metrics.json (로컬 자료: `phase-a-metrics.json`).
한글의 고정 CAP6은 현재 Fade=1에서도 반복 구조의 오차를 만들었다. Strength-aware grouping은 이를 크게 줄였다: [4× 비교](fixed-vs-aware-4x.png), [2× 비교](fixed-vs-aware-2x.png), [CAP12/CAP8 비교](fixed12-fixed8-4x.png). 아래쪽 difference×32는 진단용 확대이며 실제 밝기가 아니다.

## PART C — Minimum required pairs

세 corpus 모두 통과해야 하는 기준. **시험한 grouping/budgets 중 minimum**이다. 원래 exact24는 항상 fallback 후보에 포함했다.

| p | Static ≤1 LSB | Effective-aware ≤1 LSB | Effective-aware ≤2 LSB |
|---:|---:|---:|---:|
| 1.000 | 2 | 2 | 2 |
| .975 | 2 | 2 | 2 |
| .950 | 2 | 2 | 2 |
| .925 | 2 | 2 | 2 |
| .900 | 2 | 2 | 2 |
| .875 | 2 | 2 | 2 |
| .850 | 2 | 2 | 2 |
| .825 | 4 | 4 | 2 |
| .800 | 12 | 6 | 4 |
| .750 | 16 | 8 | 6 |
| .700 | 24 | 10 | 8 |
| .650 | 20 | 12 | 10 |
| .600 | 24 | 16 | 12 |
| .550 | 20 | 16 | 16 |
| .500 | 24 | 20 | 16 |
| .450 | 24 | 20 | 16 |
| .400 | 24 | 20 | 16 |
| .350 | 24 | 20 | 12 |
| .300 | 24 | 16 | 16 |
| .250 | 24 | 16 | 16 |
| .200 | 24 | 16 | 16 |
| .150 | 24 | 16 | 12 |
| .100 | 20 | 16 | 10 |
| .050 | 16 | 10 | 4 |
| .000 | 2 | 2 | 2 |

Endpoint의 2는 명목 budget이며 실제 Gaussian branch는 실행되지 않는다. Pixel-bin 진단은 sharp-only 구간에서 1 pair까지 허용됐지만 runtime 최소 tier는 2로 유지했다. **2 LSB 표는 참고이며 최종 acceptance를 완화하지 않았다.**

전체 envelope/ROI: envelope.csv (로컬 자료: `envelope.csv`).

## PART D — Average pair budget와 현실적인 차이

Linear reverse에서 Phase A의 sampled envelope를 사다리꼴 적분하면:

| 방법 | ≤1 LSB 평균 pair estimate |
|---|---:|
| exact | 24 |
| Static | 17.525 |
| strength별 Effective-aware | 12.300 |
| bin candidates까지 포함 | 12.2375 |

이는 **각 지점에서 다시 계산한 kernel을 허용한 estimate**다. 측정점 사이의 quality 보증이나 실제 runtime fetch 비용이 아니다. 인접 endpoint 중 큰 budget을 택한 추정은 Effective-aware 13.15이며, 이것도 미측정 interval의 엄밀한 upper bound는 아니다.

소수 immutable kernel로 고정해 107 points × 3 corpora를 추가 확인했다. 작은 fixture에서는 2/12/16/20 조합으로 1 LSB를 유지했지만, **FHD에서는 p=.50~.35 일부에 2 LSB**가 발생했다. 2 LSB를 허용하지 않고 해당 중간 구간을 exact24로 보수적으로 되돌렸다. 좁은 fallback의 하단 경계 p=.295에도 2 LSB가 있어 최종적으로 0.2~0.6 전체를 exact로 유지했다.

### 최종 runtime 선택

| progress | pairs | Linear 시간 비중 |
|---|---:|---:|
| .86 ≤ p < 1 | 2 | 14% |
| .82 ≤ p < .86 | 12 | 4% |
| .60 < p < .82 | 20 | 22% |
| .20 < p ≤ .60 | 24 | 40% |
| 0 < p ≤ .20 | 16 | 20% |

평균 **17.96 pair / 35.92 texture instructions per pass**. Exact24/48 대비 명목 fetch 수 **25.17% 감소**다. 실제 texture-cache/memory transactions 감소율과 같지는 않다.

초기 12.3 estimate가 그대로 runtime 성능이 되지는 않았다. 최종 후보는 20/24 tier를 62% 시간 동안 사용한다. 이것은 최적 minimum의 증명이 아니라, 작은 고정 tier 수와 FHD quality margin을 택한 결과다. Phase A의 낮은 envelope는 가능성을 보여줬지만 최종 후보의 이득은 **중간 정도**다.

## PART E — Temporal boundaries

최종 경계 .86/.82/.60/.20에서 ±.01, ±.005, exact threshold를 확인했다. FHD A8/RGBA에서 양쪽 kernel을 각각 강제하여 actual transition과 kernel-to-kernel 차이를 구분했다.

| transition p | reverse에서 전환 | threshold의 kernel-to-kernel max RGB |
|---:|---|---:|
| .86 | 2 → 12 | A8 1 / RGBA 1 LSB |
| .82 | 12 → 20 | A8 1 / RGBA 1 LSB |
| .60 | 20 → 24 | A8 1 / RGBA 1 LSB |
| .20 | 24 → 16 | A8 1 / RGBA 1 LSB |

**경계 위에서 아직 선택하지 않는 kernel까지 항상 1 LSB라는 뜻은 아니다.** 예를 들어 A8의 강제 16 pair는 p=.205/.21에서 2 LSB다. 실제 후보는 그 구간에서 exact24를 사용한다. p=.19/.195/.20에서는 두 kernel이 target 안에 들어오며, .20 위로 16-pair 구간을 확대하지 않았다.

Kernel blending, 두 kernel의 동시 sampling, CPU tier switching은 없다. 8초 reverse를 30fps로 녹화했고 전환 주변 frames를 확대 비교했다. 검사한 영상/frames에서는 기존 CAP6의 큰 grid나 뚜렷한 brightness/halo jump를 확인하지 못했다. 모든 디스플레이/타겟에서의 지각적 동일성을 보장하는 판정은 아니다.

원자료: boundary-pair-metrics.json (로컬 자료: `boundary-pair-metrics.json`), final-boundaries-metrics.json (로컬 자료: `final-boundaries-metrics.json`).
영상/접촉 시트: [A8 영상](compare-a8-ready.mp4), [RGBA 영상](compare-rgba-ready.mp4), [A8 frames](temporal-a8.png), [RGBA frames](temporal-rgba.png).

## PART F — Runtime implementation

외부 adaptive.inc (로컬 자료: `adaptive.inc`)의 private shader factory다.

- 2/12/16/20/24의 immutable coefficients를 shader constants로 생성한다. 12는 p=.75의 grouping, 16/20은 p=.50의 grouping, 24는 production coefficients다.
- 기존 WHOLE_TEXT uniform progress로 draw-coherent branch를 선택한다. 매 frame CPU grouping/getenv/새 constraint/evaluator가 없다.
- shader/renderer 교체는 진단 설치 시 한 번이다. actual animation에서는 shader를 전환하지 않는다.
- `forcedN`은 같은 branch shader의 강제 tier 진단, `variantN`은 해당 kernel만 사용하는 별도 shader 진단이다. 실제 candidate는 forced override 없이 progress branch를 사용한다.
- shared Gaussian UBO를 가져와 수정하지 않는다. 원본 shared block을 private shader source에서만 제외하고, copied constants를 사용한다.
- `Source → H → V → Output`의 task/FBO/geometry/ownership, Source fade, Late Smooth, Output, Gaussian radius/strength clock은 그대로다. H와 V의 kernel 선택만 다르다.
- public recipe와 원본 24-pair shader/UI scale을 검사한다. HIGH/PER_LINE 및 다른 radius를 이 PoC로 일반화하지 않는다.

GLSL ES 100 형태의 scalar constants/functions/if를 사용했다. GLES2/Vulkan/TV compiler 실행 검증은 별도이며, 호스트 결과만으로 이들의 branch lowering을 보장하지 않는다.

## PART G — GPU

### 방법

기존 native library에서 publication 후 0.8초 warm-up. Reverse는 **1초 Linear 1→0 × 6 loops**, 별도 process **3회씩**, baseline/candidate 순서를 교차했다. Source/H/V/Output draw 앞뒤 `EXT_disjoint_timer_query`를 합산하고 관측 window frame 수로 나눴다. 모든 유효 로그의 dropped/disjoint query는 0이었다.

아래는 **draw execution ms/frame**이며 FBO clear/store, 전체 submit/present, VSync wait를 모두 포함한 frame time/FPS가 아니다. Host의 현재 GPU clock/governor를 강제 고정하지 않았다. CPU/영상 recording은 별도 실행했다.

### 실제 reverse 평균

| format / mode | Source | H | V | Output | 합계 |
|---|---:|---:|---:|---:|---:|
| A8 exact | .1300 | .4389 | .1226 | .1227 | **.8142** |
| A8 adaptive | .1390 | .3433 | .0950 | .1299 | **.7072** |
| RGBA exact | .2553 | .5890 | .1647 | .1487 | **1.1577** |
| RGBA adaptive | .2486 | .4302 | .1235 | .1460 | **.9483** |

- A8: H/V 합계 약 **21.9% 감소**, total **13.1% 감소**.
- RGBA: H/V 합계 약 **26.5% 감소**, total **18.1% 감소**.
- Source/Output 코드는 같으며 작은 시간 차이는 실행 부하/clock 변동을 포함한다. 이를 Source/Output 최적화 효과라고 부르지 않는다.
- A8 total 3회: exact .8207/.8250/.7968 → candidate .7147/.6957/.7113 ms.
- RGBA total 3회: exact 1.1676/1.1702/1.1351 → candidate .9792/.9461/.9196 ms.

이전 분석의 baseline A8 .8124 / RGBA 1.0663 ms는 역사적 참고다. 현재 비교는 **이번에 교차 측정한 쌍**을 사용했다. 이전 결과를 새 후보와 직접 나눠 개선율을 계산하지 않았다.

### Branch flattening 확인

p=.5에서 각 강제 tier/별도 variant를 1.5초씩, 정순/역순 **2회** 측정. 값은 H/V ms/frame.

| pairs | uniform branch H / V | static variant H / V |
|---:|---|---|
| 24 | .3782 / .1028 | .3795 / .1005 |
| 20 | .3224 / .0927 | .3215 / .0862 |
| 16 | .2557 / .0717 | .2563 / .0701 |
| 12 | .1933 / .0552 | .1941 / .0537 |
| 2 | .0458 / .0174 | .0434 / .0148 |

호스트에서는 작은 tier 선택 시 실제 H/V 시간이 내려갔다. 모든 branch의 fetch를 항상 실행하는 결과는 아니다. 별도 variant의 일부 작은 이득 때문에 매 frame shader/resource switching을 도입하지 않았다. 강제 저-budget의 p=.5 화면은 품질 승인 대상이 아니라 compiler/timing 진단이다.

### Progress별 결과

각 checkpoint 1.5초, format/mode별 1 process. 각 셀은 **Source / H / V / Output ms/frame**.

| p | A8 exact | A8 adaptive | RGBA exact | RGBA adaptive |
|---:|---|---|---|---|
| 1.0 | .1452/.0494/.0131/.1264 | .1125/.0401/.0114/.1113 | .1919/.0536/.0237/.1189 | .1879/.0579/.0236/.1442 |
| .9 | .1127/.3826/.1088/.1109 | .1126/.0463/.0177/.1230 | .1887/.4139/.1180/.1234 | .1873/.0635/.0295/.1185 |
| .8 | .1121/.3793/.1049/.1110 | .1121/.3126/.0864/.1116 | .1880/.4026/.1201/.1203 | .1912/.3340/.0965/.1229 |
| .7 | .1120/.3823/.1066/.1055 | .1123/.3129/.0862/.1060 | .1859/.3901/.1174/.1152 | .1860/.3229/.0968/.1183 |
| .6 | .1125/.3830/.1056/.1069 | .1116/.3736/.1022/.1054 | .1870/.3868/.1110/.1146 | .1860/.3826/.1092/.1144 |
| .5 | .1120/.3842/.1046/.1048 | .1156/.4061/.1134/.1062 | .1853/.3883/.1130/.1139 | .1906/.3999/.1145/.1147 |
| .4 | .1123/.3796/.1052/.1047 | .1136/.3934/.1030/.1048 | .1851/.3895/.1126/.1140 | .1844/.3825/.1091/.1134 |
| .3 | .1118/.3797/.1059/.1053 | .1120/.3740/.1014/.1058 | .1873/.4127/.1201/.1128 | .1849/.3916/.1109/.1123 |
| .2 | .1141/.3929/.1084/.1067 | .1113/.2517/.0705/.1044 | .1845/.3887/.1123/.1126 | .1937/.2859/.0881/.1144 |
| .1 | .1121/.3799/.1058/.1045 | .1126/.2697/.0858/.1100 | .1844/.3981/.1127/.1111 | .1856/.2625/.0781/.1147 |
| .0 | .1002/.0150/.0064/.0907 | .1000/.0153/.0067/.0919 | .1738/.0206/.0106/.0812 | .1698/.0202/.0106/.0807 |

Endpoint의 단일-run 차이를 개선 근거로 쓰지 않는다. 핵심은 높은 progress/낮은 opacity 쪽의 절감이며 **중간 exact 구간은 개선되지 않는다**. p=.5 일부 관측은 오히려 조금 느리므로 모든 구간이 빨라진다고 주장할 수 없다.

추가 EASE_OUT_SQUARE reverse A8: 6 loops × 2 processes씩, .7887 → .6857 ms (**13.1% 감소**). Quality mapping은 같은 progress domain이고 시간 가중치만 달라진다. 이 curve의 명목 평균은 약 18.04 pairs다.

로그/전체 수치: bench-metrics.json (로컬 자료: `bench-metrics.json`), bench-summary.txt (로컬 자료: `bench-summary.txt`), results (로컬 자료: `results`).

## PART H — CPU / Memory

GPU-query preload와 recording 없이 별도 process에서 동일 6초 reverse를 **3회씩** 측정했다. `CLOCK_PROCESS_CPUTIME_ID` 증가량 / wall duration이며 process 전체의 **CPU ms/s**다.

| format | exact 3회 | adaptive 3회 | 평균 exact → adaptive |
|---|---|---|---|
| A8 | 196.5 / 200.9 / 202.2 | 195.7 / 216.4 / 187.4 | **199.85 → 199.85** |
| RGBA | 256.4 / 212.1 / 246.1 | 184.2 / 234.6 / 195.8 | **238.21 → 204.86** |

A8는 평균상 중립. RGBA는 이번 관측에서 낮았지만 scatter가 커서 안정적인 CPU 개선율로 주장하지 않는다. **추가 per-frame CPU 작업은 없으며 안정적인 증가 징후도 보이지 않았다.** shader 생성/첫 compile latency를 이 수치로 평가하지 않았다.

| FHD logical payload / count | exact | adaptive |
|---|---:|---:|
| A8 Source+H+V | 3,128,475 B (2.984 MiB) | 동일 |
| A8 unique texture 전체 | 13,246,875 B (12.633 MiB), 5 textures | 동일 |
| RGBA Source+H+V | 12,513,900 B (11.934 MiB) | 동일 |
| RGBA unique texture 전체 | 30,728,556 B (29.305 MiB), 7 textures | 동일 |
| offscreen tasks / FBOs | 3 / 3 | 3 / 3 |
| inventory actors / renderers / geometries | 10 / 4 / 2 | 동일 |

Actor inventory는 Label subtree/진단의 전체 traversal 기준이며 모두가 blur 전용 actor라는 뜻은 아니다. 메모리는 width×height×format bytes의 **논리 texture 저장량**이다. driver VRAM allocation/alignment, window MSAA, CPU raster, shader compiler cache/RSS를 포함하지 않는다.

추가는 private shader/code/cache와 작은 constants뿐이다. 5개 tier의 coefficient 원자료는 74 pairs × 2 float = **592 B**이나 실제 compiler/program cache 크기는 이 숫자보다 크며 별도 정밀 측정하지 않았다. 새 FBO, renderer, actor, per-frame evaluator는 없다.

## PART I — Quality 판정과 한계

최종 FHD native candidate: **45 checkpoints × 2 formats = 90 comparisons**, 모든 final RGB max ≤1 LSB. 각 format 22/45는 byte-identical.

| FHD format | max | 최대 ROI MAE | 최대 ROI RMSE | 최대 p99 | 최대 changed pixels |
|---|---:|---:|---:|---:|---:|
| A8 | 1 | .15396 | .39238 | 1 | 319,256 (15.40%) |
| RGBA | 1 | .10968 | .33118 | 1 | 579,165 (27.93%, RGB 중 하나라도 변한 pixel) |

오차 단위는 8-bit LSB다. FHD ROI는 foreground+halo가 window를 채워 full frame과 같다. 최대치들은 각각의 metric에서 취한 값이며 모든 frame의 평균이 아니다. **1 LSB지만 넓은 영역이 바뀔 수 있다**는 점도 숨기지 않는다. 큰 structured grid는 검사 범위에서 보이지 않았고, 작은 quantization/periodic residual이 완전히 0인 것은 아니다.

원자료: runtime-final-fhd-metrics.json (로컬 자료: `runtime-final-fhd-metrics.json`). 초기 20-pair 중간 후보의 2 LSB 실패는 runtime-fhd-metrics.json (로컬 자료: `runtime-fhd-metrics.json`)에 그대로 남겼다.

한계:

- 1 LSB는 이번 corpus/font/좌표/driver의 검증 결과이지 모든 입력의 수학적 bound가 아니다.
- odd dimensions, fractional UI/render scale, 다른 radius/B, sync/async publication 교체, ImageSpan, PER_LINE, 타겟 compiler는 일반화하지 않았다.
- 8초/30fps 영상과 전환 주변 캡처는 temporal 검토를 위한 자료이며 모든 frame rate/display에서 flicker가 없음을 증명하지 않는다.
- Glyph 자체 품질, Source/H/V resolution, Late Smooth의 기존 한계는 그대로다.

## PART J — Target instructions

[TARGET.md](TARGET.md)에 standalone source build와 exact/adaptive FHD 실행 방법을 정리했다. Host 실행 예:

```bash
bash /home/bowonryuubuntu/tizen/reveal-adaptive-study.8A0C8e/run-compare.sh a8
bash /home/bowonryuubuntu/tizen/reveal-adaptive-study.8A0C8e/run-compare.sh rgba
```

타겟에는 `visual.cpp`, `adaptive.inc`, `fhd-corpus.h`, `run-target.sh`를 같은 SDK로 빌드하여 사용한다. **TV에서 빌드/FPS/품질을 확인한 상태는 아니다.** Host ELF를 복사해서 실행하는 방법도 아니다.

비교는 같은 target 환경에서 exact/adaptive 순서를 교차하고, WHOLE_TEXT radius48의 전체 reverse FPS와 p=.2~.6 중간의 frame drop을 함께 본다. 평균 FPS만으로 기존 UX 문제 해결 여부를 판단하지 않는다.

## PART K — Verdict / 다음 판단

**ADAPTIVE GAUSSIAN PROMISING — READY FOR TARGET TEST**는 narrow PoC 판단이다.

Phase A의 지점별 envelope는 대부분 20~24 pairs가 필수라는 결과가 아니었고, 고정 tier의 보수 후보도 실제 total GPU에서 약 13~18% 이득을 보였다. 따라서 곧바로 quality trade-off를 더 키우거나 새로운 architecture로 확장하지 않고 타겟 실효성을 볼 가치는 있다.

그러나 최종 고정 후보는 62% 구간에서 20/24를 사용하고 중간 40%는 exact다. **큰 폭의 전 구간 최적화나 peak-cost 해결을 기대한다면 부족하다.** 타겟에서 이 평균 개선이 UX/FPS로 이어지지 않으면 여기서 종료하고 production exact를 유지하는 것이 맞다.

이번 결과만으로 production 채택/기본 활성화를 권하지 않는다. Radius/B/scale 일반화, compile/cache 비용, 다른 backend/lifecycle 설계는 타겟 Go 이후의 별도 일이다. 더 낮은 품질 기준으로 이번 후보를 확대하지 않았다.

## PART L — Working tree / 재현 자료

- UI HEAD/production files/index 그대로. commit/add/amend/rebase/push 없음.
- 기존 adaptor의 별도 변경도 건드리지 않았다.
- Full build/regression, UTC, sanitizer는 실행하지 않았다. 외부 native 진단 앱/GL readback/GPU·CPU 측정/영상만 수행했다.
- phase-a.cpp (로컬 자료: `phase-a.cpp`), kernels.h (로컬 자료: `kernels.h`), adaptive.inc (로컬 자료: `adaptive.inc`), bench.cpp (로컬 자료: `bench.cpp`), visual.cpp (로컬 자료: `visual.cpp`)와 각 `run-*.sh`가 재현 자료다.
- 캡처 일부의 첫-frame race로 누락된 H/V는 해당 case만 다시 캡처했다. 기존 Output과 재캡처 Output은 byte-identical했다. 이후 캡처는 두 rendered frames를 기다리도록 외부 capture 도구만 조정했다.
- 영상 도구의 최초 X11 `BadMatch`는 창이 아직 `IsUnMapped`일 때 시작한 문제였다. `IsViewable` 확인 후 동일 창을 캡처해 A8/RGBA 각 315 frames, 10.5초 영상을 얻었다. 권한/driver/production 설정을 바꾸지 않았다.
- 이번 작업에서 생성한 약 20 GiB 원시 캡처는 `raw-captures.tar.zst`(1,986,539,015 bytes)로 무손실 압축했다. `tar --compare`로 원본과 일치함을 확인한 뒤 중복 원시 디렉터리만 제거했다. 코드/지표/영상/그림은 그대로이며, 원시 자료는 이 디렉터리에서 `tar --zstd -xf raw-captures.tar.zst`로 복원할 수 있다.

위 결과는 현재 PoC 디렉터리에만 보존된다. Host 동작에서 확인하지 않은 target 성능/portable quality를 통과했다고 보고하지 않는다.
