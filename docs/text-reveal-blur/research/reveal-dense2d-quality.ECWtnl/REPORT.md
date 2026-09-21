# Text::Reveal — two-stage dense Blur quality gate

## A. Executive verdict

**TWO-STAGE BLUR QUALITY FAIL**

Candidate A를 수학적으로 보정한 **9×9 / 81-read Cartesian kernel 하나**로
검증했다. Soft16부터 CURRENT에 없는 획 모양의 얼룩/격자가 발생하고,
Strong24에서는 더 뚜렷하다. 왕복 애니메이션에서도 그 패턴이 변한다.

밝기 총량과 분산을 맞추는 것만으로는 충분하지 않았다. 원본의 고주파
획을 충분히 평균하지 못한 채 reduced texture에 기록하는 문제가 남는다.

- Source 크기, Reveal timing, progress, Late Smooth는 변경하지 않았다.
- 품질 FAIL이므로 **실제 3→2 runtime 구현·CPU/GPU 성능 측정은 하지 않았다.**
- 더 적은 source samples의 Candidate B가 이번 alias/격자를 해결한다는
  구조적 근거가 없어 만들지 않았다.
- Target 승격: **NO**. GBS/RPM/target 작업 없음.
- 이 결론은 **이번 9×9 설계의 No-Go**다. 모든 2-stage blur가 불가능하다는
  증명이나, 가능한 모든 9×9 배치의 최적해 검증은 아니다.

바로 확인:

```bash
bash /home/bowonryuubuntu/tizen/reveal-dense2d-quality.ECWtnl/run-viewer.sh
```

기본: CURRENT | CANDIDATE, Soft16, 8초 왕복. 텍스트는 축소하지 않는다.
`S` Soft16, `D` Strong24. `1` HIGH, `2` CURRENT, `3` CANDIDATE,
`4` CURRENT|CANDIDATE, `5` HIGH|CANDIDATE.
`Q/W/E/R` p=.20/.50/.75/.90 정지, `Space` 재생, `Esc` 종료.

## B. Baseline / 보호 범위

| Repository | HEAD | Branch |
|---|---|---|
| UI | `05087317cac8ea9600bba498f00ccf8086a79d3f` | devel_blur_text |
| Core | `8228720460a4910151f4eb4ad36976816b13a102` | tizen_10.1 |
| Adaptor | `a3ab9b8db9637fda4c973b5d59c4d4b95a9d5286` | tizen_10.1 |

현재 UI HEAD에는 Source-only/V-only diagnostic 코드가 이미 포함되어 있다.
이를 CURRENT blur라고 비교하지 않았다. 앞선 검증의
`reveal-flush-poc.2JyKvS/lib/production` Foundation과 `lib/common` Core/Adaptor를
재사용했다. Foundation snapshot은 Source-only/V-only를 제외한 기존
Source→H→V + Late Smooth 경로이고, 이번에 실제 task inventory로도 확인했다.
`lib/candidate`의 rejected glFlush coalescing Adaptor는 사용하지 않았다.

Core/Adaptor/UI의 기존 dirty files와 untracked file은 모두 보존했다.
BASELINE.json (로컬 자료: `BASELINE.json`), FINAL_STATE.json (로컬 자료: `FINAL_STATE.json`)에 HEAD,
branch, status, diff 및 dirty-file SHA-256을 기록했다.

환경: Ubuntu / GTX 1650 / NVIDIA 595.91.07 / GLES / window MSAA 4.
이전 optimized private libraries와 `-O2 -g -DNDEBUG` viewer를 사용했다.
이번에는 **품질만 측정**했으므로 이 실행을 benchmark로 해석하지 않는다.

## C. Candidate architecture

의도한 production 후보:

```text
full-resolution Source (기존 capture, Reveal 반영)
                ↓
quarter-width × quarter-height Blur2D (한 pass)
                ↓
기존 onscreen Output ← full-resolution Source
                    Late Smooth 그대로
```

이번 quality-only viewer의 실제 구성:

```text
Source ──→ 기존 H (계속 실행하지만 candidate는 결과를 사용하지 않음)
   └────→ 기존 V FBO / renderer를 2D shader로 교체 ──→ Output
   └──────────────────────────────────────────────→ sharp read
```

외부 viewer가 candidate Label의 V renderer shader/texture input만 바꾼다.
Source, H, V의 actors/tasks/cameras/FBO는 모두 남아 있다. Output의 shader,
두 texture binding, timing constraint는 바꾸지 않는다.
**이 앱에서 H 비용이나 메모리가 제거됐다고 주장하지 않는다.**

## D. Kernel math

현재 `gaussian-blur-algorithm.cpp`의 함수 본문을 그대로 추출하여 C++로
계산했다. extract-kernel.py (로컬 자료: `extract-kernel.py`),
PRODUCTION_KERNEL.json (로컬 자료: `PRODUCTION_KERNEL.json`).
별도의 추정 sigma나 과거 radius48 상수를 사용하지 않았다.

Production은 authored radius R에 대해 `numSamples=R/2`를 사용한다.
`CalculateBellCurveWidth(numSamples)`가 sigma를 찾고, 정수 위치
`−(R−1)…+(R−1)`의 Gaussian을 정규화한다.
양쪽 center contribution을 나눈 뒤 인접 정수 tap 둘을 weighted offset
하나로 packing하며, shader는 각 offset의 ± 위치를 읽는다.

| 항목 | Soft16 | Strong24 |
|---|---:|---:|
| Production bell sigma | 4.795804977 | 7.372480392 |
| Truncated discrete variance / axis | 22.68387335 | 53.49729516 |
| √variance | 4.762759006 | 7.314184518 |
| Production integer support | −15…15 | −23…23 |
| Production H/V texture expressions, 각 pass | 16 | 24 |
| Candidate samples | 9×9=81 | 9×9=81 |
| Candidate maximum offset | 14.63631615 | 22.24191320 |
| + conservative LINEAR reach ≤1px | 15.63631615 | 23.24191320 |
| Existing halo / side | 18 | 26 |

Candidate는 production discrete weights를 대칭인 9개 bin으로 나눈다.
bin의 energy와 weighted centroid를 보존하고, bin 내부 분산 손실을
전체 offset scale 하나로 복구한다. 이 scale은 정수 texel phase에서
**LINEAR interpolation 자체의 분산까지 포함하여** production variance와
같아지도록 계산한다. 그림을 보고 offset/weight를 조정하지 않았다.
2D weight는 `w[x] * w[y]`이며, energy=1, centroid=0, X/Y variance 동일이다.

1D 상수의 nonnegative 절반; ± tap은 같은 weight를 가진다:

| R | offset | weight |
|---|---|---|
| 16 | 0, 3.381001, 7.253878, 11.144325, 14.636316 | .246275866, .252019445, .101816731, .021225200, .001800691 |
| 24 | 0, 5.333194, 11.163361, 17.016997, 22.241913 | .266039435, .243228930, .099911112, .021853772, .001986469 |

모든 자릿수: kernel-generated.h (로컬 자료: `kernel-generated.h`), KERNEL.json (로컬 자료: `KERNEL.json`).
fragment에는 미리 생성된 81개의 sample × constant-weight 식만 있다.
exp/sqrt/sin/cos 없음. 기존 update-side blur strength를 offset에 곱한다.

### 왜 5/7을 건너뛰고 9만 선택했는가

**quarter framebuffer에 출력한다고 Source sampling footprint가 자동으로
연속적이 되지는 않는다.** LINEAR read 한 번은 축마다 최대 두 source
texel center를 읽는다. 5/7/9 Cartesian samples는 축마다 최대 10/14/18개
center인데, 원래 support는 31/47개다. 같은 tails와 분산을 유지하면서
5/7로 줄이면 source footprint의 빈 구간이 더 커진다.

9에서도 평균적인 축 간격은 Soft 약 3.7px, Strong 약 5.6px다.
quarter-grid spacing 약 4px에 비해 Strong은 특히 불리하다.
정규화 energy/variance/halo는 성립하므로 9를 제한된 quality probe로
선택했지만, 연속 coverage를 만족한다고 가정하지 않았다.

즉 여기서 dense는 **81개 Cartesian 조합을 모두 평가한다**는 뜻이지
source의 모든 texel을 읽는 exact dense convolution이라는 뜻은 아니다.
5/7의 모든 가능한 설계가 수학적으로 불가능하다는 주장은 하지 않는다.
또한 direct tensor product로 현재 packed kernel을 그대로 평가하면
16²=256 / 24²=576 reads가 되어 이번 81-tap 범위를 넘는다. 이를 구현하지 않았다.

## E. Source→quarter mapping / phase

현재 production의 convention을 그대로 사용한다:

```text
Source W = ceil(contentWidth)  + 2*(radius+2)
Source H = ceil(contentHeight) + 2*(radius+2)
Qw = max(1, ceil(W*.25))
Qh = max(1, ceil(H*.25))

output column j의 UV = (j+.5)/Qw
source texel-center index = (j+.5)*W/Qw - .5
```

fixture의 짝수/divisible-by-four 크기에서는 `4*j+1.5`다.
추가 half-texel 보정이나 정수 round는 넣지 않았다.
홀수 예 W=1957 → Qw=490이면 첫/마지막 center는
1.4969388 / 1954.5030612로 source 중심에 대해 대칭이다.
다만 fractional source phase가 위치마다 달라지는 것은 남는다.

Candidate는 normalized full Source pixel step을 사용한다.
V의 기존 H inverse-size를 Source inverse-size로 변경한 이유도 이것이다.
카메라/quad/viewport/V FBO 크기는 기존 그대로다.

phase 0/.25/.5/.75, strength 1/.8/.5/.25/.1에서 계산한 candidate와
production packed kernel의 축 분산 차이는 최대 Soft .16934px²,
Strong .25054px²다. **이 작은 moment 차이가 실제 aliasing도 작다는 뜻은
아니다.** 공간 phase/frequency 반응은 별개다. 홀수 크기는 수식 검토만 했으며
이번 quality failure 뒤 별도 runtime fixture로 확장하지 않았다.

## F. PER_LINE isolation contract

현재 코드의 `vertices`, `rectangle`, `ReadRevealBlurTexture()`를 확인했다.
각 line은 source page 안의 normalized padded rectangle을 가지며,
현재 H/V는 개별 read를 그 rectangle의 half-texel 안쪽으로 clamp한다.
page 전체 `CLAMP_TO_EDGE`만으로 격리하는 구조가 아니다.

Candidate에도 같은 read helper를 보존했다. 각 2D tap의 식은:

```text
localStep = sourceInverseSize / lineRectangle.zw
localUV = vTexCoord + offset2D * strength * localStep
pageUV = rectangle.xy + localUV * rectangle.zw
sample(clamp(pageUV, rectangle.xy + .5*sourceInverseSize,
                       rectangle.xy + rectangle.zw - .5*sourceInverseSize))
```

따라서 source-texel 단위 offset이며 양 축을 해당 line의 halo 안으로 clamp한다.
최대 support도 기존 radius+2 halo보다 작아 Source extent 증가가 필요 없다.
실제 runtime으로 진행한다면 full V geometry를 써야 한다. D2의 H Y-band
geometry는 2D blur가 새로 만드는 vertical halo를 잘라내므로 사용할 수 없다.

**이번 installer는 scalar WHOLE_TEXT만 허용한다.** primary quality FAIL로
PER_LINE의 실제 batch injection/edge-bleed smoke를 만들지 않았다.
위 내용은 보존해야 하는 contract와 구현 식 검토이지 PER_LINE 통과 주장 아님.

## G. Theoretical work — GPU time 측정 아님

실제 viewer의 task inventory로 확인한 크기다.

| 설정 | Source | H | V / candidate |
|---|---|---|---|
| Soft16 | 1956×1116 | 489×1116 | 489×279 |
| Strong24 | 1972×1132 | 493×1132 | 493×283 |

| 설정 | CURRENT H+V texture expressions | Candidate Blur2D | 변화 |
|---|---:|---:|---:|
| Soft16 | 10,914,480 | 11,050,911 | **+1.25%** |
| Strong24 | 16,742,280 | 11,301,039 | **−32.50%** |

blur strength가 0이 아닐 때, 전체 WHOLE_TEXT quad를 filtering하는 식의 상한이다.
Source/Output/clear/early-out/submit/cache 비용은 제외했다. D2 PER_LINE band
절감이나 GPU driver가 최적화한 실제 texture instruction 수와도 다르다.
81 taps가 Soft에서는 이미 현재 H+V sampling 합계와 비슷하다.

의도한 H 제거 후 논리 A8 FBO payload만 계산하면 Soft 2.7323→2.2119MiB,
Strong 2.7942→2.2619MiB다. **이번 viewer의 actual allocation 감소가 아니다.**
metadata/original text, CPU allocations, driver padding/VRAM은 제외했다.
THEORY.json (로컬 자료: `THEORY.json`).

## H. Quality results

기존 corpus: `따뜻한 햇살 아래 아름다운 우리들의 이야기가 시작됩니다`
12줄, font size 24, FHD Label 1920×1080, 흰 text/검정 배경.
Unit::LINE / WHOLE_TEXT / Fade0 / Stagger .25 / BlurDurationRatio1.
radius만 Soft16/Strong24. adaptive Fade1 경로는 사용하지 않는다.

HIGH/CURRENT/CANDIDATE 각각 같은 text/font/window/schedule/progress.
정지 화면 48장: 2 radii × 3 modes × 4 progress × forward/reverse.
각 progress까지 실제 짧은 animation 후 정확한 값을 설정하고 frame이
안정된 뒤 framebuffer를 읽었다. reverse는 1에서 내려온다.

| 설정 | p=.20 | p=.50 | p=.75 | p=.90 |
|---|---|---|---|---|
| Soft16 | 뚜렷한 얼룩/획 잔상, FAIL | 세부 패턴 차이 남음 | CURRENT와 거의 같음 | CURRENT와 동일 |
| Strong24 | 더 강한 격자/획 모양 modulation, FAIL | 획 모양 불균일함 지속 | 차이 작음 | CURRENT와 동일 |

대표 비교 (HIGH / CURRENT / CANDIDATE 위→아래, 3× nearest, 명암 보정 없음):

- [Soft16 p=.20](comparisons/r16-p0.20-3x.png)
- [Soft16 p=.50](comparisons/r16-p0.50-3x.png)
- [Strong24 p=.20](comparisons/r24-p0.20-3x.png)
- [Strong24 p=.50](comparisons/r24-p0.50-3x.png)

원본 1:1 파일은 captures (로컬 자료: `captures`)에 있다. HIGH 자체도 strong blur에서
packed sampling의 미세 grid를 보이지만, 후보는 그보다 훨씬 큰 형태의
modulation이 생긴다. 단순히 blur 폭이 조금 다른 정도로 판정하지 않았다.

## I. Artifact / metric audit

검정 배경의 white coverage를 대상으로 status UI를 제외한
ROI x=0…1399, y=0…799를 사용했다.
RMSE에는 배경도 포함되므로 아래 숫자를 단독 품질 점수로 쓰지 않는다.

| 설정 / p | energy vs CURRENT | RGB RMSE / max, 0…255 | centroid Δx,Δy px |
|---|---:|---:|---:|
| Soft16 .20 | +.0779% | 2.294 / 53 | +.038, +.080 |
| Soft16 .50 | +.0978% | 1.148 / 19 | −.639, +.004 |
| Soft16 .75 | +.0042% | .022 / 1 | +.003, ~0 |
| Strong24 .20 | +.1235% | 3.379 / 58 | +1.583, +.017 |
| Strong24 .50 | +.1193% | 2.173 / 45 | −1.497, +.078 |
| Strong24 .75 | +.0092% | .102 / 1 | −.030, ~0 |
| 두 radius .90 | 0% | 0 / 0 | 0, 0 |

Energy는 거의 보존됐지만 국소 밝기 차이는 크다. centroid가 p에 따라
좌우로 바뀌는 것도 고정된 geometry 이동보다는 sampling/content 상호작용과
일치한다. 24개의 forward/reverse checkpoint pair는 text ROI에서 모두
byte-identical했다. 상태 누락이나 reverse-only 문제는 관찰되지 않았다.

X/Y second moments, >1/255 halo bounds와 centroid는
Soft16 metrics (로컬 자료: `METRICS-16.json`), Strong24 metrics (로컬 자료: `METRICS-24.json`)에 있다.
예를 들어 p=.20 전체 text의 variance 차이는 Soft (−23.408,+2.279)px²,
Strong (+134.238,+1.629)px²다. 이는 **text 배치 전체의 second moment**이지
kernel variance가 아니므로 위 kernel 표와 혼동하면 안 된다.
해당 threshold의 halo bounds 차이는 최대 1px였다. 전체 halo가 크게
잘리거나 extent가 늘어나는 현상보다 내부 패턴이 주된 실패였다.

- [Soft projected profiles](comparisons/r16-profiles.png)
- [Strong projected profiles](comparisons/r24-profiles.png)
- Soft 8s video (로컬 자료: `slow-r16.mkv`) / [contact sheet](comparisons/slow-r16-contact.png)
- Strong 8s video (로컬 자료: `slow-r24.mkv`) / [contact sheet](comparisons/slow-r24-contact.png)

분할 화면은 FHD Label의 왼쪽 960px씩을 native scale로 clip한 것이다.
0→1→0 looping cycle은 8초이며, 영상은 그중 8초를 30fps로 기록했다.
영상 시작이 정확히 p=0인 것은 아니다. GPU/FPS benchmark로 쓰지 않는다.
contact sheet에서 blur 구간의 ghost/grid 형태가 시간에 따라 바뀌고,
후반에는 CURRENT와 일치하는 것이 확인된다. 별도의 near-sharp abrupt
switch는 이 자료에서 확인하지 못했지만, 전체 구간 shimmer-free PASS를
줄 수는 없다. 이미 strong 구간 패턴으로 품질 FAIL이다.

### 원인 범위

variance 보정은 blur의 전체 폭만 맞춘다. 넓은 source 영역을 9개 축
read로 대표시키면서 남긴 sampling gap의 주파수 응답은 맞추지 못한다.
이 차이를 reduced output에서 재구성하면 한글 획의 고주파가 큰 패턴으로
남을 수 있다. 마지막 bilinear reconstruction은 이미 alias된 성분을
원래 Gaussian 결과로 되돌리지 못한다.

보조 1D 선형 sampling 계산에서도 이 위험이 보인다. source phase .5,
strength .896, quarter Nyquist 이상 [.125,.5] cycles/source-pixel band에서
response magnitude 최대는 Soft CURRENT .203 / candidate .632,
Strong CURRENT .203 / candidate .809다. THEORY.json (로컬 자료: `THEORY.json`).
이는 **고정 phase 1D kernel 모델**이지 전체 multi-stage GPU의 측정 transfer
function이 아니다. 실제 Screenshot 실패와 일치하는 보조 근거로만 쓴다.

## J. Selected candidate / Candidate B 판단

검증: Candidate A 9×9 하나. **채택 없음.**
추가 tap-size/offset tuning이나 81을 넘는 확대를 하지 않았다.

실패는 CPU sample budget이 과해서가 아니라 품질이다. 더 적은 Source
samples의 shallow filter가 부족한 footprint 평균과 phase-sensitive
pattern을 제거한다는 근거가 없다. Output 2–4 read smoothing은 이미
alias된 패턴을 줄이는 대신 폭/선명도를 또 바꿀 수 있다. 이를 해결로
가정하고 Candidate B를 자동 생성하지 않았다. 별도 prefilter/A-R,
Vogel, multi-pass Kawase로도 확장하지 않았다.

## K. Runtime structural inventory

Quality gate FAIL → 실제 two-stage runtime 미구현.
viewer에서 확인한 각 Label은 Source/H/V **3 tasks, 3 FBOs**다.
후보도 H를 남겨 두었으므로 같다. 실제 camera/dependency/draw 감소 없음.

의도한 3→2 architecture가 통과했다면 page당 blur task/FBO/actor/camera
하나를 제거하고 Source/Output은 그대로 둘 수 있는 구조지만,
Text Effect Demo의 45→30 / 60→40 / 36→24 / 57→38은 이번에 검증하지 않았다.
그 숫자는 목표식이지 측정 결과가 아니다.

## L. Setup inventory

FBO create/init attempts 측정 안 함. 57→38을 달성했다고 보고하지 않는다.
Source/page count·upload·memory peak도 production two-stage 기준 미측정.

## M. PC performance

**미수행 — quality stop rule.**
5 independent warm processes, command/render-thread CPU, iteration/swap p95,
GPU timer, FPS 비교로 넘어가지 않았다. CPU/메모리 성능 결론 없음.

## N. PC→TV interpretation

**DIRECTIONAL SCREENING ONLY.**
PC arithmetic 감소율을 TV FPS/CPU 개선률로 환산하지 않는다.
기존 target 자료의 H/V stage 비용과 bounded flush No-Go는 이번 과제의
출발점이지, 품질이 실패한 후보를 승격할 이유가 아니다.

## O. Remaining risks

| 항목 | 이번 확인 범위 |
|---|---|
| Quality | Soft16/Strong24 FAIL. 가장 앞선 blocker |
| Gradient / RGBA / ImageSpan | Source capture를 재구성하지 않는 설계이나 smoke 미수행 |
| PER_LINE | rect/clamp/halo 식 검토만. actual packed fixture 미수행 |
| Scale / odd dimensions | source-size UV/ceil/symmetry 수식 확인; runtime 미검증 |
| Async | production publication/lifecycle 변경 없음; candidate runtime 미구현 |
| Lifecycle | 외부 viewer 종료는 정상; production stress/leak PASS 주장은 없음 |
| Memory | H를 남긴 quality harness. 실제 절감/peak/RSS/VRAM 결론 없음 |

## P. Target escalation

**NO.** 품질 첫 gate를 통과하지 못했다.

## Q. Target next checks

해당 없음. 이번 candidate용 package/target 실행 절차를 만들지 않았다.
비교 앱은 위 PC script로만 제공한다.

## R. Stop list / 재현 파일

하지 않은 작업: production source edit, Core/Adaptor edit, GBS/RPM,
target install/run, runtime stage removal, benchmark, UTC/sanitizer,
새 corpus 확대, 5/7/추가9 tuning, Candidate B, >81 taps,
prefilter/A-R/Vogel/Kawase, flush/fence/dependency/pooling 최적화,
public API/commit/rebase/amend/push.

새 코드: viewer.cpp (로컬 자료: `viewer.cpp`), candidate.h (로컬 자료: `candidate.h`),
kernel.py (로컬 자료: `kernel.py`), build.sh (로컬 자료: `build.sh`).
재현 절차: [REPRODUCTION.md](REPRODUCTION.md).

## S. Git state

`baseline.py verify`로 원본 3 repositories의 HEAD/branch/status/diff와
dirty files의 bytes가 시작 시점과 같음을 확인했다.
새 파일·결과는 이 외부 diagnostic directory에만 있다.
기존 report/PoC directory나 installed libraries를 수정하지 않았다.
**no commit / no push.**
