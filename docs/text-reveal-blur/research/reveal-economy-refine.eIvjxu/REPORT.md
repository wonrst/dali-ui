# ECONOMY-A refinement + PC screening

## A. Executive Verdict

**A VISUAL PASS / PC PERF MIXED — EXIT GPU PROMISING, NO STRONG OVERALL GATE.**

최종 시각 후보는 **max radius32, gamma1.5, 기존 Balanced alpha 유지**다.
blur의 존재감을 중간 구간까지 유지하는 목표에는 맞으며, A를 폐기하지 않는다.
다만 이 후보가 Text Effect Demo 전체에서 더 저렴하다는 결론은 얻지 못했다.

- 퇴장 active offscreen **draw GPU**: 0.892→0.477ms/frame, 약46.5% 감소.
  같은radius32 CURRENT와 비교해도0.711→0.477ms, 약32.8% 감소.
- 진입 offscreen draw GPU: 0.413→0.445ms, 개선 없음. 같은32에서는0.442→0.445ms.
- 진입 render-thread CPU는3.227→3.229ms로 거의 같고, command CPU는 소폭 증가했다.
  퇴장 CPU의 중앙값 감소는 run 간 편차가 커 확정적 개선으로 보지 않는다.
- task 수는 그대로다. 큰radius로 인해 진입 logical FBO payload는 오히려 약9.8% 늘었다.
- **Target escalation: NO, 이번 gate에서는 보류.** 전체 PC 결과가 강하게 긍정적일 때만
  target 검증을 권한다는 조건을 적용했다. 시각 후보/viewer는 다음 판단을 위해 그대로 보존한다.

## B. Previous A baseline

[Phase2](../reveal-economy-phase2.2zaqod/REPORT.md)의 A mixed를 사용자 승인된 출발점으로 유지했다.
CURRENT와 Gaussian fidelity가 다르다는 이유로 재탈락시키지 않았다.
Soft는 max16/floor10, Strong은 max24/floor12였으며 native timing gamma1을 사용했다.
full Source→quarter-X/Y H→quarter-X/Y V→full Source+V mixed Output 구조는 그대로다.

원본 repository 대신 별도 private runtime overlay를 만들었다. 이전 결과물도 덮어쓰지 않았다.
최적화된 production object set은 이전 `reveal-flush-poc.2JyKvS`에서 재사용했다.
기존 dirty UI의 source-only/one-tap 진단 상태를 CURRENT라고 부르지 않는다.

## C. Radius candidates

먼저 gamma1과 Balanced alpha를 고정하고 다음만 비교했다.

| family | 고정 floor | max radius 비교 |
|---|---:|---|
| Soft | 10 | 16 / 24 / 32 |
| Strong | 12 | 24 / 32 / 40 |

각 case에서 CURRENT / old A / candidate A를 같은 actual Reveal progress로 비교했다.
1920×1080 Label, font24px, 기존 한국어 문장12줄, white/black, Unit::LINE,
WHOLE_TEXT, Fade0, Stagger.25, BlurDurationRatio1을 유지했다.
분할 화면은 원래 픽셀 크기로 clip하며 text/layout을 축소하지 않는다.

## D. Radius cost table

실제 생성된 A8 Source/H/V 크기에서 계산한 값이다. CPU/GPU 시간이나 VRAM 실측이 아니다.

| maxR | Source | A H / V 각각 | Source bytes | A H+V bytes | A H+V filtering expressions |
|---:|---:|---:|---:|---:|---:|
| 16 | 1956×1116 | 489×279 | 2,182,896 | 272,862 | 2,728,620 |
| 24 | 1972×1132 | 493×283 | 2,232,304 | 279,038 | 4,185,570 |
| 32 | 1988×1148 | 497×287 | 2,282,224 | 285,278 | 5,705,560 |
| 40 | 2004×1164 | 501×291 | 2,332,656 | 291,582 | 7,289,550 |

H는 원래 Gaussian construction의 Rmax reads/fragment를 유지하고,
V는 실제 quarter-Y 입력에 맞춰 sigma/support를 변환한다. 이 fixture의 V reads는 4/6/8/10이다.
weights를 정규화하고 adjacent pair로 구성하는 Phase1/2의 derivation 그대로다.
radius가 커져도 별도 tap rescue/prefilter는 넣지 않았다.

## E. Selected radius

**32px 하나를 선택했다.** p=.1∼.4의 blur가 더 넓고 읽기 어려운 층으로 나타난다.
16/24보다 blur 존재감은 커지지만 기존 Y decimation의 줄 무늬를 완전히 없애지는 않는다.
40은 더 넓어지는 데 비해32 이상의 시각적 가치가 뚜렷하지 않고 비용이 늘어 제외했다.

radius만 올려서는 p=.5 이후 기존 alpha/timing 때문에 sharp가 빨리 나타나는 문제는 그대로였다.
[radius 결정 기록](STEP_A.md)은 timing capture와 성능 실행 전에 작성했다.

## F. Timing formulation

기존 sequence start/native blur duration으로 계산하는 normalized q는 유지한다.

```text
q = clamp((RevealProgress − sequenceStart) / nativeBlurDuration, 0, 1)
qBlur = pow(q, gamma)
b = 1 − smoothstep(0, 1, qBlur)
```

전체 animation duration, Reveal progress, Source visibility/unit schedule은 바꾸지 않는다.
현재 q만 읽으며 이전 frame/state, 방향별 분기, hysteresis, 별도 clock은 없다.
PER_LINE에서도 각 sequence의 기존 q에 같은 mapping을 사용한다.
기존 endpoint 규칙 p=1→q=1도 유지한다.

## G. gamma comparison

32를 고정한 뒤 gamma1/1.5/2만 비교했다. 2.5나 alpha 추가 tuning은 사용하지 않았다.

- 1: 이전 timing. 후반에 visible glyph가 늘어났을 때 이미 sharp가 상당 부분 보인다.
- 1.5: p≈.5∼.6에도 강한 blur가 남으며, p≈.7∼.9에서 sharp가 올라온다.
- 2: strong blur 유지가 더 길지만 sharp 전환 구간이 더 뒤로 압축된다. 이번에는1.5를 선택한다.

[Strong gamma checkpoints](comparisons/timing-soft0-R32-mode8.png),
[Soft gamma checkpoints](comparisons/timing-soft1-R32-mode8.png).
gamma2를 모든 UX에서 실패라고 단정하는 것은 아니지만, 추가 지연이 이번 목표에 필요하지 않았다.

## H. Selected timing

**gamma1.5**. `BlurDurationRatio=1` 상태에서 blur의 존재감이 p≈.2∼.6까지 유지된다.
예를 들어 p=.5에서 b는 native .5→.7134가 된다.
이는 시간을 늘린 것이 아니라 같은 timeline 안에서 blur 궤적을 후반으로 옮긴 것이다.
처음/끝은 동일하고 reverse도 같은 progress에서 같은 상태를 갖는다.

## I. Selected final curve

FROZEN.json (로컬 자료: `FROZEN.json`)을 성능 실행 전에 작성했으며 이후 변경하지 않았다.

```text
Rmax = 32
gamma = 1.5
floor = Soft10 / Strong12
radius = floor + (32−floor) × smoothstep(.35, 1, b)
sharpAlpha = 1−smoothstep(0, .60, b)
blurAlpha = min(1−sharpAlpha, .90×smoothstep(0, .65, b))
Output = current Source(progress)×sharpAlpha + blurred current Source(progress)×blurAlpha
```

성능 후보는 Strong floor12로 고정한다. alpha 합≤1이며 Balanced 고유의 약15% 계수 합 감소는
유지된다. 화면 전체 luminance가15% 감소한다는 뜻은 아니다.
Source(progress)가 빈 p=0에서는 maximum b라도 화면은 비어야 한다.

## J. Soft16 visual result

old Soft16과 비교해 max32가 blurred layer의 폭과 존재감을 늘린다.
floor10은 유지하므로 후반의 가장 약한 blur 표현을 radius 증가와 함께 바꾸지 않았다.
gamma1.5가 더 많은 visible glyph가 남는 중간 구간까지 blur를 유지한다.
기존의 줄/획 modulation은 남아 있으며 Gaussian fidelity 개선 완료로 표현하지 않는다.

## K. Strong24 visual result

max24→32에서 halo가 넓어지며 gamma1.5가 중간의 blur를 더 명확하게 만든다.
gamma2처럼 후반을 더 압축하지 않고 Balanced alpha를 그대로 유지했다.
새로운 심각한 glow/중복 glyph/mode-switch는 확인한 native 이미지·영상 추출 frame에서 보지 못했다.
모든 디스플레이·전체 동작에서 artifact-free라고 보증하는 결과는 아니다.

## L. Reveal-progress checkpoint table

정지점은 **p=0,.1,.2,…,1**이다. b 고정 비교가 아니다.
[STRUCTURE.md](STRUCTURE.md)에 p/q/qBlur/b/radius/alpha 전체 표를 기록했다.
각 capture의 `states.json`에는 실제 renderer progress와 대응하는 수치를 저장했고,
H/V strength가 remapped radius와 맞는지 실행 중 확인했다.
Source의 visible line 수는 Unit::LINE/Fade0의 기존 동작에 따라 변한다.

보관된 capture의 CURRENT HUD는 Balanced alpha 참고값 옆에
`native Late Smooth, not these alphas`라고 명시한다. 이는 CURRENT의 실제 alpha가 아니다.
실제 CURRENT 계수는 CURRENT_NATIVE_ALPHA.json (로컬 자료: `CURRENT_NATIVE_ALPHA.json`)에
동일 checkpoint의 native 식으로 별도 계산했다. 전달하는 viewer의 HUD는 그 native 계수를
직접 표시하도록 정정했다. A의 수치·curve·rendering이나 성능 실행 binary는 바꾸지 않았다.
최종 CURRENT|A 비교는 정정한 HUD로 Soft/Strong 각각11점을 다시 저장했다:
[Strong p=.5](captures/final-native-soft0-R32-mode5/p0.50.png),
[Soft p=.5](captures/final-native-soft1-R32-mode5/p0.50.png).
원래110장도 유지하여 총132장이고, 아래 CURRENT|final 영상 링크도 정정한 HUD의 재녹화본이다.

최종 Strong의 대표 수치(이 WHOLE_TEXT fixture에서는 q=p):

| p=q | qBlur | b | effective radius | sharpAlpha | blurAlpha |
|---:|---:|---:|---:|---:|---:|
| 0.0 | 0.0000 | 1.0000 | 32.000 | 0.0000 | 0.9000 |
| 0.1 | 0.0316 | 0.9971 | 31.999 | 0.0000 | 0.9000 |
| 0.2 | 0.0894 | 0.9774 | 31.929 | 0.0000 | 0.9000 |
| 0.3 | 0.1643 | 0.9279 | 31.316 | 0.0000 | 0.9000 |
| 0.4 | 0.2530 | 0.8404 | 28.974 | 0.0000 | 0.9000 |
| 0.5 | 0.3536 | 0.7134 | 23.764 | 0.0000 | 0.9000 |
| 0.6 | 0.4648 | 0.5528 | 16.625 | 0.0176 | 0.8456 |
| 0.7 | 0.5857 | 0.3728 | 12.072 | 0.3217 | 0.5485 |
| 0.8 | 0.7155 | 0.1967 | 12.000 | 0.7480 | 0.1974 |
| 0.9 | 0.8538 | 0.0579 | 12.000 | 0.9739 | 0.0201 |
| 1.0 | 1.0000 | 0.0000 | 12.000 | 1.0000 | 0.0000 |

p=0은 Source가 비어 있어 "강한 blur가 표시된다"는 뜻이 아니다.
p=1은 blurAlpha=0이므로 full-resolution Source만 표시된다.

## M. 8s animation audit

실제 Reveal p0→1→0을4초씩, 총8초 구동했다. 모든 panel은 같은 Animation을 공유한다.
30fps lossless RGB 영상의30fps는 녹화 설정이지 앱 성능 지표가 아니다.

| 비교 | Soft floor10 | Strong floor12 |
|---|---|---|
| CURRENT / final A | 영상 (로컬 자료: `videos-native/soft1-R32-gamma1.5-mode5.mkv`) | 영상 (로컬 자료: `videos-native/soft0-R32-gamma1.5-mode5.mkv`) |
| old A / final A | 영상 (로컬 자료: `videos/soft1-R32-gamma1.5-mode6.mkv`) | 영상 (로컬 자료: `videos/soft0-R32-gamma1.5-mode6.mkv`) |
| gamma1 / 1.5 / 2 | 영상 (로컬 자료: `videos/soft1-R32-gamma1.5-mode8.mkv`) | 영상 (로컬 자료: `videos/soft0-R32-gamma1.5-mode8.mkv`) |

sharp/blur 계수와 radius는 연속이고 별도 mode switch는 없다. radius floor에서 증가하는 구간도
smoothstep으로 연결한다. 다만 더 긴 strong blur가 기존 A의 무늬를 가려주는 것은 아니며,
새 candidate의 목표는 그 허용된 blur를 timeline상 더 유용하게 배치하는 것이다.
역방향도 같은 곡선을 역으로 거친다. native checkpoint와 여러 영상 frame을 확인했으며
미세 shimmer까지 모든 frame에서 없다고 보증하지 않는다.

직접 비교:

```bash
bash /home/bowonryuubuntu/tizen/reveal-economy-refine.eIvjxu/run-current-final.sh
bash /home/bowonryuubuntu/tizen/reveal-economy-refine.eIvjxu/run-old-final.sh
```

Space 왕복, S/D Soft/Strong, 1 CURRENT, 2 old A, 3 radius만 변경한 A,
4 final A, 5 CURRENT|final, 6 old|final, 7 radius 비교, 8 gamma 비교.
Q/W/E/R/T/Y/U/I/O/P/J는 p0∼1의 정지점이다.
Z/X/C는 gamma1/1.5/2, [/]는 radius 후보 전환이며 보고서의 frozen 값은 바뀌지 않는다.

## N. Source halo / memory impact

FHD fixture의 Source는 max24에서1972×1132, max32에서1988×1148로 커졌다.
max32의 Source+H+V A8 payload는2,567,502 bytes다.
기존 A24의2,511,342 bytes보다 늘지만 CURRENT24의2,929,899 bytes보다는 작다.

실제 Demo에서는 text content 주변 halo 비중이 더 크므로 증가율도 다르다.
진입 Source 픽셀은561,629→719,277이며 FBO payload는816,864→897,243 bytes로 늘어난다.
H/V가 줄어도 **큰 radius의 전체 메모리 절감은 자동으로 성립하지 않는다**.
퇴장은 기본 radius48→최종32이므로 source 감소까지 포함한다. 같은32 CURRENT 대조군을 별도로 둔 이유다.

| Demo | CURRENT 기본 | A32 | 변화 | CURRENT 동일32 대비 A |
|---|---:|---:|---:|---:|
| 진입 Source+H+V | 0.779 MiB | 0.856 MiB | +9.84% | −14.22% |
| 퇴장 Source+H+V | 2.544 MiB | 1.608 MiB | −36.80% | −14.22% |

이 수치는 unique Source/H/V texture handle의 논리 payload다. RSS/VRAM 실측이나
원본 text/metadata textures·CPU buffers·window MSAA를 포함한 앱 전체 메모리가 아니다.

## O. Final H/V structural cost

FHD에서 final32의 H/V expressions는5,705,560이다.
CURRENT16의10,914,480 대비 약52.28%, CURRENT24의16,742,280 대비 약34.08%다.
이제 기존의 “A는25%” 수치를 서로 다른 radius 비교에 그대로 사용하지 않는다.

Demo inventory의 모든 dimensions/format/read 수/Source·H·V pixels와 logical bytes는
[STRUCTURE.md](STRUCTURE.md), raw resources는 STRUCTURE.json (로컬 자료: `STRUCTURE.json`)에 있다.
진입60개·퇴장57개의 offscreen tasks는 모든 arm에서 같다. window main task는별도1개다.
같은 page의 Source/H/V와 full Source retention을 유지하며 새 pass/FBO/tap rescue는 없다.

FBO 전체 extent×kernel reads는 구조적 상한이다. CURRENT의 D2 H geometry는 전체 H보다
작은 영역을 그릴 수 있고, early return/cache/driver compiler도 실제 실행량에 영향을 준다.
따라서 이 산술을 실제 fragment invocation이나 GPU ms로 부르지 않는다.

## P. PC performance method

양쪽은 optimized diagnostic build이며 project RelWithDebInfo의
`-O2 -g -DNDEBUG`를 사용한다. 기존 trace/debug 계측을 유지하므로 순수 Release 절대 비용은 아니다.
기존 glyphy 한 object와 설치된 Components는 모든 arm에서 동일하게 공유한다.

1280×720, MSAA4, Sync, Text Effect Demo의 실제 scene/actions/corpus/layout을 사용한다.
모든 arm에서 BlurDurationRatio를1로 맞췄다. 원래 긴 text 진입은.5였으므로 기존 과거 보고서의
시간 수치를 그대로 새로운 baseline으로 사용하지 않았다. Animation duration·Fade·Stagger는 기존값이다.

- baseline: CURRENT PERFORMANCE, 기존 Late Smooth, 진입24/퇴장48.
- economy: final A mixed, radius32/floor12/gamma1.5, 진입·퇴장 모두 동일.
- matched: CURRENT PERFORMANCE/Late Smooth, 진입·퇴장 모두32. radius 차이와 topology 차이를 구분하는 대조군.

각 arm5개 독립 process, process당2 cycles. 첫 cycle은 warm-up, 두 번째가 표본이다.
실행 순서를 교대한다. CPU/counters 실행과 GPU query 실행을 분리한다.
inventory는 별도 비계측 실행이다.

실행 환경은 Ubuntu / GTX1650 / NVIDIA595.91.07 / GLES다. 각 process는 약29.5초다.
값은 **각 process 구간 평균의 중앙값**, p95는 **각 process 안의 p95를 구한 뒤 그 중앙값**이다.
한 process의 많은 frame을 독립 표본처럼 취급하지 않았다. IQR/min/max와 raw 실행 결과는
[PC_METRICS.md](PC_METRICS.md), PC_RESULTS.json (로컬 자료: `PC_RESULTS.json`)에 남긴다.

구간 정의:

- Cards entrance: CardsStart+1초∼+3초.
- Normal exit 전체: ExitStart∼ExitFinished, 약.4초. setup 포함.
- Exit active: ExitStart+50ms∼ExitFinished. 약.35초.
- 기존 tail 비교: ExitStart+300ms∼ExitFinished. 약.1초로 짧으므로 단독 판단에 쓰지 않는다.
- Exit creation frame: 퇴장 전체 중 framebuffer initialize attempt가 가장 많은 frame.

기존 private collector의 duration/exclusive category parser를 재사용한다.
offscreen command drain CPU, render CPU, combined update/render thread CPU, iteration wall p95,
present/swap cadence p95를 분리한다. Draw/RenderPass/Dependency는 계측 block count이고,
별도로 GL draw/fence/wait/FBO 생성 호출도 기록한다.

GPU는 기존 안전한 disjoint timer 방식의 private meter를 확장해 H/V/Source draw를 분류한다.
query result는 available인 경우만 읽고 glFinish/동기 readback은 하지 않는다.
GPU의 offscreen total은 **Source+H+V draw timer 합계**이며 clear/driver CPU/context wait 전체가 아니다.

PoC의 H/V shader는 이전 quality viewer와 같이 coefficients를 상수로 전개한다.
CURRENT의 UBO loop와 shader 표현까지 동일한 비교는 아니므로, timing 차이를 오직
pixel 수 하나에 귀속하지 않는다. 후보에는 추가 tap/pass나 endpoint task 생략을 넣지 않았다.
PER_LINE 제약은 기존 constraint 안에서 q를 remap하며, 별도 timer/프레임별 shader 재작성은 없다.

또한 native H/V의 `uAnimationRatio==0`에서는 center texel 복사로 끝나지만,
A의 filter strength는 radius floor/Rmax까지 유지된다. 따라서 q=1에서 Output의 blurAlpha가0이어도
A의 H/V는 floor kernel을 계산한다. 이는 이전 A의 표현을 유지한 현재 후보의 비용이며,
측정 중 이를 별도로 최적화하지 않았다. 완료된 Label이 섞이는 진입 구간과 계속 blur 중인 퇴장 구간을
동일한 "filtering 산술 비율"로 일반화할 수 없는 이유 중 하나다.

## Q. PC performance result

각 arm CPU/counter 실행5회, 모두 warm cycle2를 사용했다. GPU query가 없는 결과다.
단위는 ms/frame이며, count는 별도 표기다.

| 구간 / 지표 | CURRENT 기본24/48 | final A32 | CURRENT 동일32 |
|---|---:|---:|---:|
| 진입 offscreen command drain CPU | 1.809 | 1.855 | 1.782 |
| 진입 offscreen render CPU | 2.066 | 2.069 | 1.992 |
| 진입 render-thread CPU | 3.227 | 3.229 | 3.202 |
| 진입 iteration wall p95 | 13.966 | 14.669 | 12.754 |
| 진입 swap API wall p95 | 11.530 | 12.088 | 10.141 |
| 진입 swap 완료 간격 p95 | 17.525 | 17.463 | 17.292 |
| 퇴장 active offscreen command drain CPU | 2.003 | 2.112 | 1.993 |
| 퇴장 active offscreen render CPU | 2.689 | 2.841 | 2.430 |
| 퇴장 active render-thread CPU | 5.426 | 4.479 | 4.609 |
| 퇴장 active iteration wall p95 | 16.756 | 17.156 | 15.836 |
| 퇴장 active swap API wall p95 | 12.170 | 12.932 | 12.192 |
| 퇴장 active swap 완료 간격 p95 | 17.247 | 17.247 | 17.243 |

**CPU 측면에서 강한 PASS는 아니다.** 진입 thread CPU는 거의 같고,
offscreen command drain은 진입+2.5%, 퇴장+5.5%다. command body만 보면 각각
1.733→1.775ms, 1.919→2.028ms다. task/command 수를 줄이지 않았기 때문에
filtering 산술 감소가 그대로 submission CPU 감소가 되지 않는다.

퇴장 thread CPU 중앙값은 약17% 낮지만 IQR이 CURRENT5.049∼6.112ms,
A3.358∼6.355ms로 크게 겹친다. 동일32 대조군도4.609ms여서 이를 확정된 A의
CPU speedup으로 주장하지 않는다. offscreen render CPU의 IQR도
CURRENT2.583∼2.854ms, A2.115∼4.238ms로 넓다.

iteration wall p95 중앙값은 진입+0.70ms/퇴장+0.40ms지만 IQR이 겹친다.
swap **호출 소요 시간**에는 vsync 대기가 들어가므로, 이 수치 증가만으로
GPU가 느려졌다고 판정하지 않는다. 실제 swap **완료 간격** p95는 약17.2∼17.5ms로 비슷하다.
tail 비악화를 엄밀히 증명한 것도, 큰 frame cadence 개선을 얻은 것도 아니다.

### Counts / warm setup

| 구간 | offscreen GL draw/frame | RenderPass 계측 block/frame | DependencySync block/frame | glFlush/frame | FBO create attempts |
|---|---:|---:|---:|---:|---:|
| 진입 steady | 53 | 106 | 177.67 | 54 | 0/frame |
| 퇴장 active | 57 | 114 | 192 | 58 | 0/frame |
| 퇴장 creation frame | — | — | — | — | 57/frame |

모든 arm의 같은 steady 구간에서 count는 같다. RenderPass block은 begin/end를 모두 포함하므로
106을106개 독립 offscreen task로 해석하지 않는다. DependencySync도 GPU stall 횟수/시간이 아니라
계측된 backend operation 수다. 전체 GL draw에는 화면 Output/UI도 포함되어 진입87.21,
퇴장약97.7/frame이며 이 또한 arm 간 동일하다.

퇴장 creation frame의 CPU는 CURRENT→A:
iteration6.551→6.483ms, create queue2.433→2.355ms,
framebuffer initialize2.188→2.149ms였다. FBO attempt57, texture initialize attempt97은 같다.
이 **warm 퇴장 resource 교체**에서 큰 setup regression은 관찰하지 않았다.
하지만 cold launch/첫 shader compile/전체 main-thread setup을 독립 benchmark한 것은 아니므로
모든 setup에서 regression이 없다고 일반화하지 않는다.

전체 퇴장(약.4초, setup 포함)의 상세 결과와 짧은 tail 결과도 원자료에 보존했다.
전체 퇴장 thread CPU는5.547→4.627ms/frame, offscreen CPU는2.642→2.946ms/frame이며
역시 run 간 편차가 커 단독 speedup 주장에는 쓰지 않는다.

## R. GPU/offscreen result

CPU 실행과 별도로 각 arm5개 독립 process에서 asynchronous GPU query를 실행했다.
drop/disjoint는 모든 실행에서0이며, 선택한 warm 구간은 submitted query가 모두 완료된 frame만 사용했다.
종료 직후 일부 프로세스의 pending22 queries는 마지막 shutdown frame에 남은 것으로,
미완료 frame을0ms로 집계하지 않았다.

아래는 각 run의 구간 평균을 구한 뒤5회 중앙값이다. 단위ms/frame.

| 구간 / GPU draw 지표 | CURRENT 기본24/48 | final A32 | CURRENT 동일32 |
|---|---:|---:|---:|
| 진입 Source | 0.1202 | 0.1184 | 0.1181 |
| 진입 H | 0.1451 | 0.1839 | 0.1552 |
| 진입 V | 0.1486 | 0.1364 | 0.1561 |
| 진입 offscreen 합 | 0.4134 | 0.4450 | 0.4415 |
| 진입 offscreen 합 p95 | 0.4955 | 0.4987 | 0.5348 |
| 퇴장 active Source | 0.1004 | 0.1101 | 0.1206 |
| 퇴장 active H | 0.4617 | 0.2052 | 0.3330 |
| 퇴장 active V | 0.3014 | 0.1553 | 0.2401 |
| 퇴장 active offscreen 합 | 0.8923 | 0.4774 | 0.7107 |
| 퇴장 active offscreen 합 p95 | 0.9526 | 0.5069 | 0.9711 |

합계는 **각 frame에서합→run평균→5회중앙값**이므로, 행별 중앙값을 더한 값과 다를 수 있다.
offscreen 합은 Source/H/V **draw만**이며 GPU clear, semaphore wait, window swap 등을 포함하는
전체 GPU frame time이 아니다. query available만 확인하며 glFinish/blocking readback은 없다.

**퇴장 filtering 이득은 반복에서도 명확하다.** offscreen 합IQR은
CURRENT0.852∼0.894ms, A0.461∼0.483ms, 동일32 CURRENT0.658∼0.725ms다.
H−55.6%, V−48.5%, offscreen 합−46.5%이며, 전체 퇴장(setup포함)도0.913→0.496ms였다.
기본48→32의radius 감소만으로 설명되는 결과는 아니다. 동일32 대조군 대비 offscreen 약−32.8%가 남는다.
다만 kernel 표현/late timing/output alpha도 달라져 topology 하나만의 순수 효과를 측정한 것은 아니다.

**진입은 다르다.** A의H는+26.7%, V는−8.2%, 합은+7.6%다.
offscreen 합IQR은CURRENT0.413∼0.424ms, A0.419∼0.452ms로 일부 겹친다.
같은32 대조군 대비 합도약+0.8%로 사실상 동일하다.
Source halo 증가, 늦어진strong blur, CURRENT의D2 coverage/zero-strength copy와
A의floor 유지가 함께 포함된 결과다. 각각의 기여를 분리해서 실측한 것은 아니므로 단일 원인으로 확정하지 않는다.

화면 Output/UI draw 합도 저장했다(진입0.344→0.299ms, 퇴장0.443→0.426ms).
이 범주는 Reveal Output만의 isolated timer가 아니라 일반UI draw까지 포함한다.
따라서 offscreen 수치에 임의로 더해 "전체 앱 GPU가몇% 빨라졌다"고 주장하지 않는다.

실제draw 수는 퇴장Source/H/V각19개로 동일하다. 진입 측정 구간에는Label 상태 변화가 있어
각stage평균17.67개/frame이며, capture시점의최대20개/page 구성과 모순되지 않는다.
sampling 최적화는 task/flush/dependency 수를 줄이지 않았다.

## S. PC→TV interpretation

**DIRECTIONAL SCREENING ONLY.** PC speedup으로 TV FPS를 계산하지 않는다.
A-R의 PC/target 반례처럼 task/dependency 고정 비용이 target에서 지배적일 수 있다.

이번에는 퇴장 GPU draw 절감이라는 실제 이득은 있지만, command CPU와 frame cadence의
일관된 개선이 함께 따라오지는 않았다. 특히 진입은GPU/메모리의전반적이득이없다.
따라서 "A의이론sample수가적다→TV에서항상빠르다"는 결론은 낼 수 없다.
기존A를Gaussian fidelity문제로탈락시킨것이 아니라, **이 최종32/1.5 후보의 PC gate가 mixed**라는 뜻이다.

## T. Target escalation recommendation

**NO — 이번 gate에서는 target1회 검증을 우선 권하지 않는다.**

사용자 조건은 PC gate가강하게긍정일때만 escalation이다. 이번 결과는 퇴장 GPU에는
충분한이득이있지만, 진입은개선되지않고 CPU/submission 비용도줄지않았다.
그 조건을 충족했다고 과장하지 않는다. **A 자체와 시각 후보는 유지**하며,
사용자가 시각적trade-off를 직접 확인할 수 있도록 viewer/영상/데이터를 제공한다.
이번 작업에서는target build, GBS, RPM, install, TV FPS 추산을 하지 않았다.

확인된 완료시점floor filtering 비용은 향후 아주 제한된 최적화 후보로 남길 수 있지만,
이번freeze 이후 이를 구현하거나 다시tuning/benchmark하지 않았다.

## U. Git state

원본 UI/Core/Adaptor의 기존 dirty 상태를 보존했다. 시작 상태는 BASELINE.json (로컬 자료: `BASELINE.json`),
종료 대조는 FINAL_STATE.json (로컬 자료: `FINAL_STATE.json`)에 기록한다.
이전 Phase1/2 결과물은 수정하지 않았다. commit/push/reset/restore/stash/rebase/amend 없음.
신규 코드·빌드·자료는 이 외부 디렉터리에만 있다.

UI HEAD `05087317cac8ea9600bba498f00ccf8086a79d3f` / `devel_blur_text` 그대로다.
Core/Adaptor HEAD·branch·status·dirty/untracked file hash까지 시작과 종료가 동일함을 확인했다.
원본 production build/install/UTC는 하지 않았다. private optimized overlay/viewer/runner만 빌드했다.
artifact 수·영상 길이·반복 수·문서 링크 검증은 VALIDATION.json (로컬 자료: `VALIDATION.json`)에 기록한다.
세 repository의 기존 `git diff --check`도 모두 통과했고, 새로 실행한 viewer/runner는 종료했다.
