# Vogel12 full-resolution Source blur — quality-only PoC

## 결론

**VOGEL12 SPARSE ARTIFACT VISIBLE — STOP SINGLE-PASS 12-TAP**

Soft16부터 한글 획이 겹쳐 복제된 듯한 structured pattern이 명확하다.
CURRENT PERFORMANCE와 다른 blur profile 정도가 아니라, sparse sample 자체가 보인다.
따라서 Strong24/Latin/Card/PER_LINE/r48로 확대하지 않았다.
후보 변경, tap 증가, random rotation, temporal accumulation도 하지 않았다.

사용자의 후속 요청에 따라 **현재 `devel_blur_text`에 opt-in PoC를 unstaged로 남겼다.**
기본 HIGH/PERFORMANCE 경로는 유지한다. 이것은 채택한 production optimization이 아니다.

## 바로 확인

동일 화면, 왼쪽부터 HIGH / CURRENT PERFORMANCE / VOGEL12:

```bash
bash /home/bowonryuubuntu/tizen/reveal-vogel12-quality.0TCvfD/run-viewer.sh
```

- Q/W/E/R: p=.20/.50/.75/.90.
- Space: 8초 0→1→0 왕복.
- 0: 세 경로 비교, 1/2/3: 각각 단독 화면.
- S: radius16, D: radius24. **24는 수식/코드만 준비했으며 품질 검증은 중단했다.**
- Esc: 종료.

실제 로컬 repository 변경을 컴파일한 라이브러리로 후보 단독 확인:

```bash
bash /home/bowonryuubuntu/tizen/reveal-vogel12-quality.0TCvfD/run-local-poc.sh
```

로컬 소스를 평소대로 빌드/install한 뒤 기존 reveal sample에서 확인하려면:

```bash
cd /home/bowonryuubuntu/tizen/dali/dali-ui/samples/text
DALI_REVEAL_VOGEL12_POC=1 ./bin/text-reveal.example
```

조건: **PERFORMANCE + Whole Text + radius16 또는24**. Fade0, Blur Time1,
한국어, 흰 text/검정 배경에서 p=.20/.50을 먼저 비교하면 차이가 잘 보인다.
환경변수를 생략하거나 0으로 실행하면 기존 경로다. HIGH/PER_LINE/다른 radius는
스위치가 켜져 있어도 기존 경로다. 샘플/public API는 수정하지 않았다.

현재 설치된 라이브러리는 덮어쓰지 않았다. 위 private launcher는 별도 build 없이 실행된다.
repository만 변경하고 평소 sample을 바로 실행하면, 직접 rebuild/install하기 전에는
새 PoC가 적용되지 않는다는 점에 주의한다.

## A. Candidate definition / calibration

후보는 하나다. 중앙1+외곽11 대신 **12개 모두 Vogel points**를 쓴다.
12개를 하나의 면적 분포로 정의하고, finite-N 중심/분산 편향을 일관되게 보정하기 위해서다.

```text
r_i = sqrt((i+0.5)/12), i=0..11
theta_i = i*pi*(3-sqrt(5))
raw_i = r_i*(cos(theta_i), sin(theta_i))
w_i = exp(-2*r_i^2) / sum(exp(-2*r_j^2))
centered_i = raw_i - sum(w_j*raw_j)
C = weighted covariance(centered)
v = trace(C)/2
offset_i = sqrt(v) * C^(-1/2) * centered_i
K = sqrt(productionDiscreteVariance / v)
sampleUV = uv + offset_i*K*existingBlurStrength*inverseSourceSize
```

whitening은 finite-N의 작은 타원 편향만 보정한다. 시각 결과를 보고 계수를 조정하지 않았다.
분산을 맞춘다는 것이 kernel 모양/고차 moment/연속성을 맞춘다는 뜻은 아니다.

실제 shader 상수:

| i | offset x | offset y | weight |
|---|---:|---:|---:|
| 0 | .1900943071 | -.01552454568 | .1775465906 |
| 1 | -.2867541909 | .2083818763 | .1502899528 |
| 2 | .01274508424 | -.4591001272 | .1272176951 |
| 3 | .3258993030 | .4020947516 | .1076874509 |
| 4 | -.6465112567 | -.1322446018 | .09115546197 |
| 5 | .5638131499 | -.3618007898 | .07716143131 |
| 6 | -.2069527805 | .6671127677 | .06531573832 |
| 7 | -.4094866812 | -.7051591277 | .05528858304 |
| 8 | .8014182448 | .2740730345 | .04680077359 |
| 9 | -.8661261797 | .2967384458 | .03961599991 |
| 10 | .3750880361 | -.8338454962 | .03353422135 |
| 11 | .2973921001 | .8916276097 | .02838610485 |

모든 weight는 positive/finite. float32 상수 합 = **1.000000003725**.
중심은 양 축 모두 절댓값 4e-9 미만이다.

```text
raw covariance after centering:
 [ .1608438464  -.0056465971 ]
 [ -.0056465971  .1830276742 ]

corrected float32 offset covariance:
 [ .1719357566   .00000000079 ]
 [ .00000000079  .1719357604  ]
```

생산 Gaussian 계산 함수를 원본에서 그대로 추출해 실행했다
(kernel-probe.cpp (로컬 자료: `kernel-probe.cpp`), build.py (로컬 자료: `build.py`)).
그 sigma로 생성된 실제 truncated/discrete kernel의 2차 moment를 사용한다.
압축 bilinear tap의 moment는 `2*sum(w*(offset^2+frac*(1-frac)))`다.

| authored radius | 생산 bell sigma | 실제 discrete sigma | 목표 per-axis variance | K |
|---|---:|---:|---:|---:|
| 16 | 4.795804977 | 4.762758774 | 22.68387114 | 11.48617649 |
| 24 | 7.372480392 | 7.314184574 | 53.49729598 | 17.63935852 |

float32 offset의 r16 보정 후 variance는 X=22.68387129, Y=22.68387178,
covXY=1.04e-7이다. r24는 X=53.49728954, Y=53.49729070이다.
후보에도 bilinear sampling 자체의 작은 추가 분산은 남는다.
r16 point sanity에서 실제 bilinear kernel variance는 22.8701/22.8777로,
생산 kernel보다 약 0.8% 넓다. 이것을 시각 튜닝하지 않았다.

전체 계산값: calibration.json (로컬 자료: `calibration.json`).
fragment에는 trig/exp/sqrt/random이 없다. active branch는 정확히 12개의 명시적
texture read를 사용하고 strength<=0 endpoint는 center 1회다.
GPU instruction counter를 측정한 것은 아니다.

## B. 비교 구조 / 보호 범위

| 구분 | 입력/필터 | Output |
|---|---|---|
| HIGH | 생산 full Source/H/V | 기존 Output |
| CURRENT | 생산 full Source → X-reduced H → XY-reduced V | 기존 V/Source Late Smooth |
| VOGEL12 quality PoC | **CURRENT의 Source/H/V 전부 유지** | full Source 직접 12-tap; V/Late Smooth 미사용 |

후보에서 Output TextureSet의 첫 슬롯을 기존 full Source에 연결했다.
sampler는 기존 Source LINEAR sampler를 유지한다. Source 렌더링, Reveal metadata,
color, A8 text-color 복원, premultiplied output, progress constraint는 재구현하지 않았다.
RGBA용 전달도 코드에는 있지만 이번 STOP 이후 별도로 검증하지 않았다.

세 경로 한 화면은 offscreen 9 + 기본 화면1 = task10이었다.
로컬 후보 단독은 offscreen3 + 기본 화면1 = task4다. H/V 제거 이득을 측정한 것이 아니다.
Source는 radius16에서 **1956×1116**, Label은 1920×1080이다.

WHOLE_TEXT scalar만 지원한다. Source 전체가 자기 padded rectangle인 경우에만
half-texel clamp를 사용한다. **PER_LINE에 page-wide clamp를 적용하지 않았다.**
PER_LINE은 기존 코드로 남겼다.

## C. Korean Soft16 — FAIL

이전 corpus를 그대로 사용: 24px, 별도 font-family override 없음,
`따뜻한 햇살 아래 아름다운 우리들의 이야기가 시작됩니다` ×12줄.
각 패널은 동일 FHD Label의 왼쪽 640px을 native pixel로 clip한다. text/layout 축소 없음.
검정 배경/흰 text, Unit LINE, WHOLE_TEXT, Fade0, Stagger.25, BlurDuration1.
Ubuntu/GLES, MSAA4. 같은 viewer process/Animation으로 구동했다.

| progress | 실제 CURRENT/VOGEL strength | 관찰 |
|---|---:|---|
| .20 | .896000028 | 한글 획이 여러 방향으로 겹치는 sparse pattern이 매우 분명함 |
| .50 | .500000000 | 윤곽/세로획 복제와 불균일한 밝기가 여전히 보임 |
| .75 | .156250000 | offsets가 작아지며 겹침은 완화됨; early/mid FAIL을 구제하지 못함 |
| .90 | .028000057 | 거의 sharp에 접근; Late Smooth는 사용하지 않음 |

HIGH Output에는 strength property가 없어 로그가 -1로 표시된다. 이것은 HIGH의
실제 strength가 -1이라는 뜻이 아니다. HIGH H/V는 동일 생산 BlurStrength 계산을 사용한다.

대표 이미지: [p=.20](captures/r16-p0-detail.png), [p=.50](captures/r16-p1-detail.png).
원본 native screenshot 4장도 captures (로컬 자료: `captures/`)에 있다.
전체 상태 로그: capture-r16.log (로컬 자료: `capture-r16.log`).

로컬 소스를 컴파일한 후보와 외부 installer 후보의 동일 text ROI를 네 checkpoint에서
비교했고 모두 pixel-identical했다. 별도 후보 둘을 만든 것이 아니라 동일 shader의
두 적용 경로를 확인한 것이다. 로그: local-capture.log (로컬 자료: `local-capture.log`).

## D. Korean Strong24

**미실행.** Soft16에서 명백한 STOP 조건을 만족했다.
radius24의 production sigma와 K 계산 및 진단 스위치 지원만 준비되어 있다.
radius24의 품질을 Soft16 결과로 측정한 것처럼 추정하지 않는다.

## E. Latin

**미실행.** 한글 Soft16 실패 후 `IIII/AVATAR/office/ffi/WWWW`로 확대하지 않았다.

## F. 실제 Text Effect Demo Card

**미실행.** Card title/places/subtitle 및 PER_LINE generalization으로 확대하지 않았다.
샘플 코드는 수정하지 않았다. 이 PoC를 현재 demo 전체에 적용했다고 해석하면 안 된다.

## G. Temporal quality

8초 왕복 영상 (로컬 자료: `slow-8s.mkv`): HIGH | CURRENT | VOGEL12.
0→1 4초, 1→0 4초, linear. frame capture는 30 samples/sec이며 FPS 성능 측정이 아니다.
시작 전 구간 포함 원본 (로컬 자료: `slow-with-leadin.mkv`), [contact sheet](slow-contact.png)도 보존했다.

연속 구간에서 겹친 획의 패턴이 strength 감소에 따라 수축하고 역방향에서 다시 벌어진다.
고정 pattern이므로 random flicker는 만들지 않지만, 고정이라는 것만으로 움직이는
ghost/세부 명암 변화가 없어지는 것은 아니다. early/mid의 spatial artifact는 영상에서도
유지된다. 프레임 contact만으로 미세 shimmer/pop이 전혀 없다고 보증하지 않는다.
그런 보증이 없어도 이번 spatial FAIL은 충분히 명확하다.

## H. Artifact / energy sanity

- 한글 repeated-stroke/ghost: 실제 GPU screenshot에서 명확함.
- dot/방향성: 아래 point/stroke 수학 fixture에서 sparse lobes가 드러남.
- 전역 X/Y variance는 맞지만, projected profile은 peak와 빈 구간을 반복함.
- 체계적인 energy loss가 주된 실패 원인은 아니다. **에너지가 보존되어도 부드러운 blur는 아니다.**

energy.py (로컬 자료: `energy.py`)는 CPU float 수학 sanity이며 **GPU readback/성능 측정이 아니다.**
동일 생산 Gaussian offsets/weights, 후보 offsets, bilinear sampling, rectangle clamp를
모델링했다. point / 24px vertical stroke / horizontal line의 중앙과 padded edge를 비교했다.

| fixture 위치, r16 strength1 | HIGH integrated coverage | VOGEL12 | 해석 |
|---|---:|---:|---|
| center point | .999999805 | 1.000000004 | 에너지 보존 |
| center 24px stroke | 23.999995329 | 24.000000089 | 에너지 보존 |
| 18px halo 다음 point | .999999805 | 1.000000004 | 정상 padded content도 보존 |
| padded rectangle 경계 위 point | 2.440090521 | 2.491613387 | clamp로 둘 다 증가; 후보 +2.11% |

경계 위 밝은 point는 정상 text halo의 보통 배치가 아니라 clamp 한계 확인용이다.
normalized weights만으로 arbitrary edge energy가 보장되지 않는다는 것을 확인한다.

center point의 centroid는 양쪽 모두 (48,48)에 수치 오차 수준으로 일치한다.
HIGH footprint(>1e-7)는 x/y 33..63, 후보는 x38..58/y37..58로,
같은 2차 moment에서도 꼬리/공간 분포가 다르다.
energy.json (로컬 자료: `energy.json`)에 X/Y profile, covariance, energy, footprint 전체를 기록했다.
[kernel-shapes.png](kernel-shapes.png)는 수학 impulse/stroke 이미지다.
point만 작은 값을 볼 수 있도록 **40배 밝기 표시**했으며 GPU screenshot과 구분한다.

## I. Optional r48

미실행. Soft16/Strong24 PASS 조건이 충족되지 않았다.

## J. Potential production topology — 이론만

품질이 통과했다면 Source/H/V offscreen3 → Source only1/page를 검토할 수 있었다.
15 pages라면 45→15 tasks라는 구조적 가능성은 있지만, 이번에는 H/V를 제거하지 않았고
그 개선을 측정하지도 않았다. Late Smooth 없이 full Source로 수렴하는 구조 자체는
가능하나, 이번 12-tap 후보의 초기/중간 품질은 충분하지 않다.

## K. Performance caveat

GPU time/CPU time/FPS/실제 메모리 benchmark 없음.
12개의 full-resolution Source read는 항상 기존 PERFORMANCE보다 싸다고 할 수 없다.
현재 PoC는 기존 H/V 비용도 그대로 부담한다. task 감소나 성능 개선 수치를 보고하지 않는다.
별도 production build 전체/UTC/sanitizer/타겟 benchmark도 하지 않았다.
진단 라이브러리와 viewer만 컴파일하고 품질 화면/영상을 확인했다.

## L. 최종 판정

**VOGEL12 SPARSE ARTIFACT VISIBLE — STOP SINGLE-PASS 12-TAP**

단일 밝은 점에는 12개의 bilinear footprint만 남고, text에서는 이 footprint들이
완만한 Gaussian 면을 만드는 대신 복제된 획으로 보인다. 중심/분산의 isotropy 보정만으로
이 빈 공간을 메울 수 없다. 이 후보를 production PERFORMANCE로 채택하지 않는다.

## M. Git / 변경 상태

- branch: `devel_blur_text`; HEAD `b54bb666dbc197c0d290fc9cffeba37bca042274`, 변경 없음.
- 시작 UI worktree clean. A-R/prefilter/adaptive production path가 없는 baseline 확인.
- 사용자 후속 요청에 따라 `text-reveal-runtime-blur.cpp` **1파일 +86줄, unstaged**.
- default path 유지. opt-in scalar Output 교체만 추가. 기존 D2/batching/lifecycle 코드 보존.
- core/adaptor, 기존 diagnostic 디렉터리와 worktree를 수정하지 않았다.
- `git diff --check` 통과. commit/add/amend/push/reset/restore/stash 없음.
- private baseline library는 기존 b54 object들에서 이전 V-only diagnostic runtime object를
  제외하고, 수정 전 b54 runtime TU를 새로 컴파일해 링크했다. 기존 설치본의 A-R 빌드를 사용하지 않았다.
- private `local-poc` library는 위 기준의 runtime TU만 실제 작업 트리 수정본으로 교체했다.
- baseline 기록: BASELINE.txt (로컬 자료: `BASELINE.txt`). 설치된 DALi library는 변경하지 않았다.
