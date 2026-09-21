# Text::Reveal A8 direct-source — GPU gate

## A. Executive Verdict

**STOP SOURCE-PASS ELIMINATION**

현재의 **4-center coverage+metadata 재구성 PoC는 GPU gate FAIL**이다.
Source capture/task/메모리는 줄지만, H에서 반복하는 virtual Source 계산과
Output의 추가 계산이 절약한 Source GPU 비용보다 크다.

ECONOMY, A8 multiline 4개 합계, p=.20, GPU clear+draw 평균:

| 반경 | BASE | DIRECT | GPU 비용 변화 |
|---|---:|---:|---:|
| R16 | 0.243 ms | 0.322 ms | +32.5% |
| R24 | 0.287 ms | 0.485 ms | **+68.8%** |
| R48 | 0.331 ms | 0.704 ms | **+112.3%** |

5개 독립 process/arm이며, 위 세 조건 모두 실행별 평균의 min–max 범위가
서로 겹치지 않는다. 실제 Cards에서도 eligible8의 중앙값이 일관되게 증가한다.
Cards의 산술평균은 간헐적 긴 지연 때문에 불확실하므로, 이를 개선으로 해석하지 않는다.

**Phase 2 lifecycle/binding audit·PoC는 진행하지 않았다.**
이 판정은 현재의 virtual-source 구현에 대한 No-Go이며,
모든 가능한 Source 제거 방식이 불가능하다는 증명은 아니다.
Target 성능이나 FPS 결과로 확대하지 않는다.

## B. GPU timer methodology

상세: [METHOD.md](METHOD.md), 계측기 (로컬 자료: `meter.cpp`), 실행 probe (로컬 자료: `probe.cpp`).

- Ubuntu / GTX 1650 / NVIDIA 595.91.07 / GLES 3.2 / MSAA 4.
- `GL_EXT_disjoint_timer_query`, `GL_TIME_ELAPSED_EXT`, 실제 counter bits=64.
- EGL context별 query pool. 결과는 **정확히 4프레임 뒤** AVAILABLE 확인 후 회수됐다.
- 4프레임 중 1프레임만 clear/draw를 측정하고 Source/H/V/Output 및 category별 합산.
- `glFinish`, `glReadPixels`, blocking RESULT read, 추가 fence, task 순서 변경 없음.
- 최종 40개 process / 80개 context summary에서 disjoint=0, dropped=0,
  pending=0. 측정 중 unknown-category draw=0.
- 실제 Cards: ECONOMY·PERFORMANCE 각각 BASE/DIRECT 5회, 총 20회.
- ECONOMY R16/24/48×6 progress: BASE/DIRECT 5회, 총 10회.
- timer OFF 대조: ECONOMY Cards BASE/DIRECT 5회, 총 10회.
- 순서는 repeat마다 BASE/DIRECT ↔ DIRECT/BASE로 교대했다.
- Cards는 process당 약 42–43개의 sampled frames, matrix cell당 13–14개.
  초기 생성/셰이더 warm-up 및 phase 경계 프레임은 측정 창에서 제외했다.

각 범위는 **GPU elapsed clear+draw**다. FBO bind, dependency wait, present,
대부분의 CPU 제출 간격은 제외한다. 따라서 전체 frame GPU critical path나
기기 FPS를 측정한 값이 아니다. GPU scheduling/preemption에 의한 긴 값은
삭제하지 않고 평균과 중앙값을 모두 보고한다.

Timer ON/OFF의 render-thread CPU/frame 평균:

| 같은 interposer, query 발행만 OFF/ON | OFF | ON | 차이 |
|---|---:|---:|---:|
| BASE | 4.384 ms | 4.408 ms | +0.024 ms / +0.5% |
| DIRECT | 4.229 ms | 4.310 ms | +0.081 ms / +1.9% |

둘 다 독립 실행 범위가 겹친다. 눈에 띄는 CPU blocking 증가의 증거는 없으나,
이를 “계측 오버헤드 0”이라고 주장하지 않는다. 이 대조는 query의 추가 비용이며,
공통 state-tracking interposer 자체의 비용까지 제거한 비교는 아니다.
GPU timer의 작은 draw perturbation도 완전히 제거할 수 없으므로 절대 ns는
이 환경의 진단값이다. 최종 R24/R48 차이는 실행 편차보다 훨씬 크다.

초기 smoke에서 context 미분리 query 회수 실패와 넓은 FBO scope의 제출 간격
혼입을 발견했다. 수정 후 재실행했고, 해당 smoke 결과는 최종 통계에서 제외했다.
raw 자료는 덮어쓰지 않고 별도 prefix로 남겼다. 세부는 METHOD의 제외 항목 참조.

## C. GPU per-stage results

ECONOMY, four-corpus PER_LINE, p=.20. 단위 **µs**, 독립 process 평균들의 평균.
Source/H/V는 clear+draw, Output은 해당 V를 읽는 실제 output draw만 포함한다.

| Stage | R24 BASE | R24 DIRECT | R48 BASE | R48 DIRECT |
|---|---:|---:|---:|---:|
| Source | 78.10 | 0 | 74.58 | 0 |
| H / H-direct | 102.09 | 343.30 | 137.23 | 541.28 |
| V | 78.66 | 83.82 | 84.99 | 85.14 |
| Output / Output-direct | 28.53 | 57.93 | 34.61 | 77.24 |
| **Total** | **287.38** | **485.04** | **331.41** | **703.65** |

R24: Source **−78.10µs**, H **+241.21µs**, Output **+29.40µs**.
R48: Source **−74.58µs**, H **+404.05µs**, Output **+42.63µs**.
H 증가분만으로도 제거한 Source 비용을 크게 초과한다.

V는 알고리즘·입력 크기·draw 개수 모두 같다. R48/.20의 V 평균은
84.99→85.14µs로 사실상 동일하다. R24/.20은 +5.15µs로 실행 범위가 겹친다.
Cards에서 처음 관찰한 큰 V 평균 차이는 긴 query 지연의 영향이었다.
동일 draw scope의 실행별 중앙값은 40.336→41.408µs로 가깝다.
이를 V 최적화 또는 V workload 변경으로 해석하지 않는다.

## D. R16 results

ECONOMY, 네 multiline Label 합계. 각 process의 frame 평균을 5회 평균한 값.

| p | BASE µs | DIRECT µs | 변화 |
|---|---:|---:|---:|
| .20 | 243.07 | 322.14 | +32.5% |
| .50 | 253.87 | 371.46 | +46.3% |
| .75 | 267.48 | 365.00 | +36.5% |
| .90 | 259.58 | 322.47 | +24.2% |

p=.20 실행별 평균 범위: BASE **238.3–246.7**, DIRECT **313.5–328.2µs**.
작은 반경에서도 중간 progress의 비용이 증가한다.

## E. R24 results

| p | BASE µs | DIRECT µs | 변화 |
|---|---:|---:|---:|
| .20 | 287.38 | 485.04 | +68.8% |
| .50 | 279.52 | 477.02 | +70.7% |
| .75 | 269.31 | 426.20 | +58.3% |
| .90 | 257.85 | 387.31 | +50.2% |

p=.20의 독립 실행 통계, 단위 µs:

| Arm | 평균 | 중앙값 | min–max | Q1–Q3 |
|---|---:|---:|---:|---:|
| BASE | 287.38 | 293.73 | 265.81–303.96 | 273.40–299.99 |
| DIRECT | 485.04 | 487.70 | 447.87–530.73 | 450.61–508.31 |

중앙값 기반 요약으로 바꿔도 결론은 같다. 단일 이상치에 의존한 FAIL이 아니다.

## F. R48 results

| p | BASE µs | DIRECT µs | 변화 |
|---|---:|---:|---:|
| .20 | 331.41 | 703.65 | +112.3% |
| .50 | 325.07 | 658.73 | +102.6% |
| .75 | 311.54 | 580.96 | +86.5% |
| .90 | 285.21 | 493.73 | +73.1% |

p=.20의 독립 실행 통계, 단위 µs:

| Arm | 평균 | 중앙값 | min–max | Q1–Q3 |
|---|---:|---:|---:|---:|
| BASE | 331.41 | 337.56 | 302.89–349.13 | 328.80–338.68 |
| DIRECT | 703.65 | 705.10 | 659.79–761.66 | 679.36–712.35 |

이는 R48 **PER_LINE/Fade0의 정적 progress** 검사다.
실제 WHOLE_TEXT/Fade1 퇴장 FPS를 새로 측정했다는 의미는 아니다.
반경 증가 시 virtual Source를 반복 평가하는 비용 증가를 확인한 것이다.

## G. Progress dependence / corpus

강한 blur 구간 .20/.50에서 특히 나쁘며 .75/.90도 회복되지 않는다.
endpoints만 보면 이를 놓칠 수 있다.

| Reference | BASE µs | DIRECT µs | 변화 |
|---|---:|---:|---:|
| R16 p=0 | 214.78 | 177.45 | −17.4% |
| R16 p=1 | 236.47 | 237.24 | +0.3% |
| R24 p=0 | 200.48 | 170.81 | −14.8% |
| R24 p=1 | 209.33 | 214.69 | +2.6% |
| R48 p=0 | 213.55 | 195.95 | −8.2% |
| R48 p=1 | 230.18 | 252.62 | +9.7% |

English / Korean / thin / bold 네 A8 multiline Label이 동시에 존재한다.
이 표는 네 Label 합계이며 특정 글꼴 하나의 결과가 아니다.
현재 packing 그대로 R16=7pages, R24/R48=6pages; 각 반경에서 BASE와 DIRECT의
H/V/Output 개수는 동일하고 Source만 사라진다. Matrix는 기존 quality corpus를
사용하는 별도 측정 fixture이며 실제 Cards의 문장/폰트/레이아웃을 바꾸지 않았다.

새 품질 튜닝·스크린샷 비교는 하지 않았다. direct-helper/library가 이전 품질 PASS
때와 같은 파일임을 hash로 확인했다. 이전의 최대 1–2/255 차이와 진단용 제한은
그대로이며, GPU 결과가 좋다는 이유로 품질 범위를 확대하는 일도 없었다.

## H. Actual Cards result

실제 demo 소스를 포함하고 원래 Cards 진입 함수를 그대로 실행했다.
12 Labels / 15 pages / R24 실제 entrance timeline / 1280×720.
측정 중 매 frame eligible H/V/Output=8, fallback S/H/V/Output=7을 assert했다.

아래 중앙값은 **process별 sampled-frame 합계의 중앙값을 구한 뒤,
5개 process의 중앙값**을 취한 것이다. 단위 µs.

| ECONOMY scope | BASE | DIRECT | 변화 |
|---|---:|---:|---:|
| eligible 8pages | 238.544 | 277.120 | **+16.2%** |
| fallback 7pages | 198.336 | 198.704 | +0.2% |
| Cards 전체 15pages | 445.392 | 472.064 | +6.0% |
| 다른 scene Reveal까지 포함 | 554.464 | 604.672 | +9.1% |

산술평균도 숨기지 않는다:

| ECONOMY scope | BASE 평균 / 실행 범위 | DIRECT 평균 / 실행 범위 |
|---|---:|---:|
| eligible8 | 368.31 / 275.97–503.78 | 314.12 / 272.62–420.99 |
| Cards15 | 690.40 / 623.73–750.40 | 623.75 / 494.33–726.98 |
| scene relevant | 786.10 / 721.01–852.68 | 741.86 / 648.78–857.13 |

**Cards 산술평균은 INCONCLUSIVE.** 긴 clear/draw query 지연에 따라 방향도
바뀌고 범위가 넓게 겹친다. 평균 −5.6%를 “scene GPU 개선”이라고 주장하지 않는다.
eligible8의 process별 frame 중앙값 범위는 BASE **234.59–244.16**, DIRECT
**262.05–296.30µs**로 겹치지 않는다. 이를 안정적인 중간 수준의 비용 증가
증거로 보되, 최종 STOP은 더 명확한 R24/R48 matrix 결과로도 독립적으로 성립한다.

## I. Eligible-page isolated attribution

Cards의 8페이지를 **실제 scene 안에서 분리 계측**했다. 8페이지만 별도 재생해
앱 workload를 바꾸거나 fallback7을 삭제한 결과가 아니다.

ECONOMY stage별 process/frame 중앙값, µs:

| Stage | BASE | DIRECT |
|---|---:|---:|
| Source | 77.424 | 0 |
| H | 76.160 | 183.312 |
| V | 67.952 | 68.928 |
| Output | 12.224 | 29.184 |

stage별 중앙값의 합은 합계의 중앙값과 일치할 필요가 없다.
각 scope의 산술평균·중앙값·min/max·Q1/Q3 전체는 [STATISTICS.md](STATISTICS.md).
실제 raw context CSV, runs.json (로컬 자료: `runs.json`), statistics.json (로컬 자료: `statistics.json`)도 보존했다.

## J. CPU structural result — 이번 build로 재확인

이전 CPU 수치를 복사한 결론이 아니다. 같은 private BASE/DIRECT library로
timer OFF 대조 5회씩 다시 실행했다. GL/state tracking은 양쪽 동일하다.

| Actual Cards | BASE | DIRECT |
|---|---:|---:|
| Cards offscreen tasks/FBOs/cameras | 45 | 37 |
| Cards Source/H/V | 15/15/15 | 7/15/15 |
| scene 평균 draws/frame | 89.62 | 81.59 |
| scene 평균 clears/frame | 55.15 | 47.13 |
| scene 평균 framebuffer binds/frame | 108.48 | 92.45 |
| render-thread CPU/frame 평균 | 4.384 ms | 4.229 ms |
| 동일 측정 창 process CPU 평균 | 774.64 ms | 743.53 ms |

Source draw8/clear8/bind16 감소는 구조적으로 재현됐다.
CPU 평균은 각각 −3.5%, −4.0%지만 render-thread 실행 범위는
BASE **4.041–4.556ms**, DIRECT **4.086–4.421ms**로 겹친다.
따라서 이번 CPU 변화는 작은 개선 경향이지, 안정적인 큰 개선률의 증거는 아니다.
Core command-queue 세부 scope는 새로 계측하지 않았다.

Cards S/H/V 논리 payload도 native task inventory로 다시 확인했다:
ECONOMY **517,635→230,586 bytes**, PERFORMANCE **603,494→316,445 bytes**.
실제 VRAM/RSS/peak 전체를 측정한 값은 아니다.

## K. GPU/CPU trade-off

CPU 제출·객체·Source FBO 절약은 실제다. 그러나 GPU 쪽은 저장한 Source 값을
한 번 읽는 대신, H의 각 lookup에서 coverage/metadata 읽기, Reveal decode,
alpha, 8-bit materialization 재현, bilinear 재구성을 반복한다.
Output sharp branch에도 같은 추가 일이 생긴다.

이를 단순히 “8 reads이므로 GPU 8배”라고 계산하지 않았다.
실측은 조건에 따라 +24%에서 +112%였고, 특히 R24/R48 강한 blur에서 불리했다.
이 gate의 목적은 CPU 이득으로 GPU 손해를 숨기지 않는 것이다.

## L. PERFORMANCE reference

PERFORMANCE는 **동일 실제 Cards 5회/arm**만 reference로 측정했다.
HIGH와 PERFORMANCE의 추가 matrix/production 확대는 하지 않았다.

| Cards, process/frame 중앙값 | BASE | DIRECT |
|---|---:|---:|
| eligible8 | 250.896µs | 312.320µs (+24.5%) |
| Cards15 | 456.496µs | 524.352µs (+14.9%) |
| scene relevant | 598.816µs | 674.848µs (+12.7%) |

eligible8 중앙값 범위: BASE 238.59–261.84, DIRECT 288.90–348.98µs.
산술평균은 잡음이 커서 별도 STATISTICS에 모두 기록했다.
ECONOMY보다 H fragment footprint가 큰 이 경로를 후보로 확장할 근거가 없다.

## M. ECONOMY candidate verdict

**FAIL / STOP SOURCE-PASS ELIMINATION.**
R24 total regression, R48 큰 regression, 추가 H 비용이 Source 절약을 압도,
Output 비용 증가까지 모두 확인했다. 사용자 STOP 조건 A–D에 해당하는 근거다.
실제 Cards의 산술평균 불확실성을 임의의 개선으로 바꾸지 않았다.

## N. Production eligibility

**Phase 2 미진행.** GPU FAIL이므로 production eligibility를 확정하지 않는다.
현재 private predicate를 production-ready로 승격하지 않는다.

## O. Transform / update binding

추가 감사·구현하지 않았다. 이전 PoC의 fixed mapping snapshot 제한이 남는다.
새 transform subsystem이나 animation binding을 만들지 않았다.

## P. Alpha / color semantics

추가 alpha/owner-opacity animation 검증하지 않았다.
기존 PoC binding이 모든 renderer draw-skip semantics를 보장한다고 주장하지 않는다.

## Q. Reentry / cancellation

ObjectCreated reentry/candidate cancellation guard 확대는 하지 않았다.
이전 diagnostic helper의 production guard 미완성 상태를 그대로 보존했다.

## R. Async stale result

이번 GPU run은 sync다. async stale completion 시나리오는 **미검증**이며,
GPU gate FAIL 후 async PoC를 확장하지 않았다.

## S. None / disconnect / reconnect

focused lifecycle scenario를 새로 실행하지 않았다.
측정 앱의 정상 종료를 lifecycle correctness 증명으로 대체하지 않는다.

## T. Quality switching

각 process에서 quality를 고정했다. 실행 중 HIGH/fallback/ECONOMY 전환의
atomicity·binding 복구는 이번 작업에서 검증하지 않았다.

## U. Resource / leak

GPU query drain은 전 process에서 완료됐지만 이것은 product resource leak
검증이 아니다. weak-object/task cycle audit, ASAN/LSAN은 수행하지 않았다.
Source/H/V task inventory와 논리 payload만 재확인했다.

## V. Complexity assessment

이미 GPU에서 No-Go이므로 lifecycle 안전성을 위해 코드를 추가할 이유가 없다.
새 cache/scene manager/transform 복제, non-atlased 확대, kernel/curve tuning 없음.
이전 diagnostic shader helper를 더 복잡하게 다듬어 구제하려는 작업도 하지 않았다.

## W. Exact supported fast-path scope

**production 지원 범위: 새로 확정한 것 없음.**
이번에 GPU를 검증한 범위는 기존 private PoC의 simple A8 source-atlas PER_LINE,
Cards eligible8 및 동일 형태의 네 corpus다. 다른 7페이지는 capture fallback 유지.

## X. Remaining unsupported / unvalidated cases

non-atlased A8 6페이지, gradient/RGBA/ImageSpan/replacement, HIGH를 direct로
확대하지 않았다. 동적 transform/alpha, reentry, cancellation, async 교체 및
파괴 중 publication은 기존 미완성/미검증 상태다. 이 상태로 shipping하지 않는다.

## Y. Recommended next action — 하나

**현재 materialized Source를 사용하는 production ECONOMY을 유지하고,
이 direct-source fast path 연구는 여기서 종료한다.**

## Z. Git state / artifacts

- UI: `devel_blur_text`, HEAD `5e3130213bfaed3cf4436b5a048b01a6ed652314`.
- Core: `f43e95be477ad301f84ecc772c753f9357821ecc`.
- Adaptor: `dcadcfdc3e0d1abdce20767bee2858cb9e1851a4`.
- 시작/종료 UI/Core/Adaptor working tree clean. Production source unchanged.
- `git diff --check` PASS. commit/amend/push/reset/restore/stash 없음.
- 이전 PoC/library/report 디렉터리는 수정하지 않았다.
- 새 파일은 이 외부 `reveal-direct-gpu.VMDaVs/` 디렉터리에만 있다.
- production build/UTC/sanitizer/target/GBS/새 screenshot 비교 미수행.
- 외부 probe/interposer만 컴파일했으며 시스템 설치/사용자 실행 환경 변경 없음.

실행·통계 생성 명령은 [METHOD.md](METHOD.md)의 Reproduce 절에 있다.
