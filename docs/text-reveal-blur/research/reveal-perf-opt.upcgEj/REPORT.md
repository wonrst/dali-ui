# Text::Reveal PERFORMANCE optimization — staged host evaluation

2026-09-14 · `devel_blur_text` · baseline HEAD `70fe035551deed33fed02078c9354edfcb06adc2`

## 결론

- **OUTPUT BATCHING READY — host에서 확인한 후보.** 타이밍 계산을 바꾸지 않고 줄별 final output을 묶었다. A/B 1,308개 캡처와 ImageSpan sync/async 126개 캡처가 byte-identical이다.
- **REDUCED TAP NEEDS MORE QUALITY WORK.** CAP_12/8/6을 각각 구현·측정했다. 큰 radius의 촘촘한 한글에서 CAP_12도 격자 패턴이 보인다. 수학적 정규화만으로 지각 품질이 보장되지는 않는다. 사용자가 최종 품질을 결정할 수 있도록 opt-in 실험으로 남겼으며, 기본값은 exact Gaussian이다.
- Stage 2 timing consolidation은 분석만 했다. batching 후 CPU가 HIGH에 근접하므로 지금 timing oracle을 GPU로 옮기는 추가 위험을 감수할 근거가 약하다.
- **HOST VERIFIED / TARGET VERIFICATION REQUIRED.** 아래 수치는 이 PC·현재 빌드 설정의 결과이며 타겟 성능이나 최종 merge 승인을 의미하지 않는다.

## A. Current Architecture Audit

HIGH: full-resolution Source → full-resolution H → full-resolution V → final output.

PERFORMANCE: full-resolution Source → H(X 1/4, Y 유지) → V(X/Y 각각 1/4) → V와 full-resolution Source의 Late Smooth 합성.

Source·H·V는 offscreen RenderTask 3개/page이고, final output은 기본 scene task에 그린다. output renderer를 줄여도 task 수는 변하지 않는다. 기존 PERFORMANCE는 output의 line-local blur strength 때문에 줄마다 Actor/Renderer를 만들었다. HIGH는 같은 page의 연속된 줄을 이미 묶고 있었다.

이번 변경에서도 Source raster/metadata, page 정책, D2 H-band, FBO 크기, refresh rate, Late Smooth 곡선, 공개 API, progress/sequence/fade/stagger/blur duration 계산은 그대로다. Source를 제거하거나 H/V의 해상도를 더 낮추지 않았다.

Gaussian은 `N = kernelRadius >> 1`개의 bilinear pair를 이용한다. 각 pair는 ±offset의 두 번 읽기이다. authored radius가 아니라 기존 정책으로 보정된 kernel radius에 적용하는 수이다. 실제 filter branch의 읽기 수는 radius 24/40/64에서 각각 24/40/64회/fragment/pass다. 아직 시작하지 않은 줄은 투명 branch, blur가 끝난 줄은 copy branch이므로 매 frame 모든 fragment가 이 횟수를 읽는 것은 아니다.

## B. Baseline 및 측정 방법

- Host: Intel i7-11700, GeForce GTX 1650, NVIDIA 595.91.07, X11/GLES, window 680×440, MSAA 4. 기존 빌드 설정을 바꾸지 않았다. foundation의 `CMAKE_BUILD_TYPE`은 빈 값이고 실제 CXX_FLAGS에 `-O2/-O3`는 없다. 이 수치를 optimized target build의 절대 비용으로 옮겨 해석하면 안 된다.
- 주요 fixture: Label 8개, 각 550×300, 6줄 Latin, font 24, absolute line-height 40. 실제 text raster는 550×240이다. PIXEL / PER_LINE / stagger .25 / fade 0 / blur time .5 / progress .5 / REFRESH_ALWAYS. A8 radius 24·40·64, RGBA gradient radius 40. WHOLE_TEXT와 HIGH도 기준 측정했다. Label은 24px 간격으로 일부 겹쳐 놓았다. 별도의 8개 비중첩 full-screen UI를 측정한 것이 아니다.
- 상태 A는 수정 전 보존 library, B는 batching만 적용한 보존 library이다. C는 최종 library에서 batching OFF + 각 cap, D는 batching ON + 각 cap이다. launch 시 설정되고 애니메이션 중에는 바뀌지 않는다.
- fixture당 독립 프로세스 3회. 각 프로세스에서 첫 생성 1회 + warm 재생성 2회 후 0.5초 안정화, 약 2초 runtime을 측정했다. warm setup은 총 6개 표본이다. CPU run과 GPU timer run은 분리했다.
- CPU는 `CLOCK_PROCESS_CPUTIME_ID`의 구간 차이이다. event/update/render 및 userspace driver를 포함한 **process CPU ms/second**이며 update thread 또는 render submission만의 비용이 아니다. 아래 CPU/frame은 이 값을 60으로 나눈 **60Hz 환산치**다. CPU 측정 run의 실제 frame별 profile을 얻은 것은 아니다.
- setup은 첫 Label 생성 직전부터 마지막 Label의 첫 LayoutFinished까지다. construct-only 시간과 구분한다. GPU filtering·shader compilation 완료를 기다린 시간은 아니다. cold는 새 프로세스의 첫 사용이며 디스크 shader cache까지 비운 진정한 cold driver compile은 측정하지 않았다. kernel 생성·shader 생성·compile을 개별 함수 비용으로 분리하지도 않았다.
- GPU는 별도 GL_TIME_ELAPSED_EXT 비동기 query로 Source/H/V/window-output draw를 분류했다. clear·upload·swap·compositor는 제외한다. Total은 네 draw 항목의 합이지 전체 GPU frame 비용이 아니다. offscreen query 회수가 약 한 frame 뒤여서 draw 수/시간의 분모에 약 1-frame 오차가 있다. dropped/disjoint는 0이었다.
- GPU clock을 고정하지 않았다. 초기 A/B 측정 일부는 가벼운 진단 작업과 겹쳤고, 자동 clock·OS 부하가 있어 작은 차이는 noise로 취급한다. 후속 C/D는 CPU/GPU app끼리 겹치지 않도록 순차 실행했다. 다른 프로세스의 잠깐의 파일 비교까지 완전히 배제한 전용 benchmark machine은 아니다. 원자료에는 3회 범위도 보존했다.

| 수정 전 A / 8 Labels | CPU ms/s | 60Hz 환산 ms/frame | GPU Source | H | V | Output | Total |
|---|---|---|---|---|---|---|---|
| performance a8 r24 line | 296.636 | 4.944 | 0.238 | 0.093 | 0.098 | 0.373 | 0.801 |
| performance a8 r40 line | 336.699 | 5.612 | 0.237 | 0.118 | 0.114 | 0.404 | 0.872 |
| performance a8 r64 line | 333.832 | 5.564 | 0.236 | 0.146 | 0.165 | 0.445 | 0.991 |
| performance rgba r40 line | 332.941 | 5.549 | 0.292 | 0.141 | 0.127 | 0.398 | 0.958 |
| performance a8 r40 whole | 216.747 | 3.612 | 0.087 | 0.061 | 0.051 | 0.141 | 0.340 |
| high a8 r40 line | 269.796 | 4.497 | 0.247 | 0.198 | 0.563 | 0.128 | 1.136 |
| high rgba r40 line | 281.689 | 4.695 | 0.308 | 0.241 | 0.599 | 0.135 | 1.283 |

### 메모리 해석

아래 texture payload는 발견한 **고유 Texture의 width × height × format bytes** 합이다. 물리적인 GPU VRAM 사용량이 아니다. driver allocation alignment, framebuffer object bookkeeping, window MSAA/backbuffer, shader binary, allocator cache는 포함하지 않는다. FBO payload는 그중 Source/H/V color attachment만 중복 제거한 값이다. 전체 texture에는 기존 text/metadata와 줄별 source texture도 들어간다.

| 8 Labels | 한 Label의 Source / H / V (W×H×bytes) | 고유 FBO MiB | 전체 texture payload MiB |
|---|---|---|---|
| performance a8 r24 | 402x474x1 / 101x474x1 / 101x119x1 | 1.911 | 9.024 |
| performance a8 r40 | 434x666x1 / 109x666x1 / 109x167x1 | 2.898 | 10.011 |
| performance a8 r64 | 482x954x1 / 121x954x1 / 121x239x1 | 4.610 | 11.722 |
| performance rgba r40 | 434x666x4 / 109x666x4 / 109x167x4 | 11.592 | 18.716 |
| high a8 r40 | 434x666x1 / 434x666x1 / 434x666x1 | 6.616 | 13.729 |
| high rgba r40 | 434x666x4 / 434x666x4 / 434x666x4 | 26.463 | 33.587 |

A/B/C/D 사이에서 같은 fixture의 FBO와 전체 texture payload는 동일했다. HIGH와 PERFORMANCE의 FBO 차이는 이번 변경 전부터 존재한 해상도 정책의 차이다.

## C. Output Batching

같은 page / V texture / retained Source / A8·RGBA shader variant 안에서, **논리적 presentation order가 연속인 줄만** 묶는다. 최대 64줄인 H/V draw 경계도 그대로 적용한다. ImageSpan 때문에 A8 줄과 RGBA 줄이 교차하면 그 순서가 달라지지 않도록 output을 분리한다. Label 전체를 무조건 하나로 묶지 않는다.

quad마다 기존 H/V draw-local line index를 넣고, CPU가 계산하던 `BlurStrength`를 output의 line-state uniform에 그대로 바인딩했다. CPU equation, start tolerance, endpoint, Late Smooth equation은 그대로다. 혼합 포맷에서 output 구간이 slot 1부터 시작해도 0으로 재번호하지 않는다.

| 8 Labels / radius40 | 항목 | A | B | 변화 |
|---|---|---|---|---|
| a8 | CPU ms/s | 336.699 | 267.290 | 20.6% 감소 |
| a8 | warm layout wall ms/8 Labels | 63.274 | 60.269 | 4.7% 감소 |
| a8 | Actors | 128.000 | 88.000 | 31.2% 감소 |
| a8 | Renderers | 120.000 | 80.000 | 33.3% 감소 |
| a8 | Geometries | 65.000 | 25.000 | 61.5% 감소 |
| a8 | 추가 Tasks | 24.000 | 24.000 | 동일 |
| a8 | GPU Output ms/frame | 0.404 | 0.150 | 62.7% 감소 |
| a8 | GPU Total ms/frame | 0.872 | 0.639 | 26.7% 감소 |
| rgba | CPU ms/s | 332.941 | 289.811 | 13.0% 감소 |
| rgba | warm layout wall ms/8 Labels | 66.884 | 68.847 | 2.9% 증가 |
| rgba | Actors | 128.000 | 88.000 | 31.2% 감소 |
| rgba | Renderers | 120.000 | 80.000 | 33.3% 감소 |
| rgba | Geometries | 65.000 | 25.000 | 61.5% 감소 |
| rgba | 추가 Tasks | 24.000 | 24.000 | 동일 |
| rgba | GPU Output ms/frame | 0.398 | 0.147 | 63.0% 감소 |
| rgba | GPU Total ms/frame | 0.958 | 0.724 | 24.4% 감소 |

대표 PER_LINE 6줄/한 page: H 1 / V 1 / Output 6→1 renderer. 8 Labels의 Source 48 + H 8 + V 8 + output 48→8 = **112→72 draws/frame**(해당 draw가 모두 제출되는 경우). 원래 숨겨진 Label renderer와 camera 등도 object inventory에 포함되며, output 전용 숫자와 구분한다.

0→1의 실제 2초 linear 반복 animation 구간 CPU도 별도 측정했다.

| 포맷 | 상태 | CPU ms/s | 60Hz 환산 ms/frame | 3회 범위 ms/s |
|---|---|---|---|---|
| a8 | A performance | 315.749 | 5.262 | 311.3–321.8 |
| a8 | B performance | 282.718 | 4.712 | 250.9–299.8 |
| a8 | B high | 272.312 | 4.539 | 264.5–284.5 |
| rgba | A performance | 338.765 | 5.646 | 334.1–345.4 |
| rgba | B performance | 293.088 | 4.885 | 272.7–312.5 |
| rgba | B high | 292.407 | 4.873 | 275.7–313.9 |

A/B Source·H·V·Output 캡처 1,308/1,308 exact. WHOLE_TEXT / PER_LINE stagger 0·.25, A8/RGBA, 1·6·64·65줄 및 multi-page 포함. progress 0, 시작 전후(.11535/.11539; 실제 두 번째 시작 약 .1153719), .2/.4/.6/.8/1 및 역방향 point를 비교했다. 추가 ImageSpan sync/async 126/126 exact. 최종 reduced-kernel 코드까지 들어간 상태에서 HIGH 864/864 exact도 별도로 재확인했다.

H/V shader·geometry·texture 크기는 B에서 바뀌지 않는다. GPU H/V 수치의 작은 변동은 작업량 감소가 아니라 측정 변동이다. output draw 감소가 직접적인 GPU 개선 항목이다.

FBO가 같다는 것이 모든 allocation이 같다는 뜻은 아니다. PERFORMANCE output vertex에 float line index를 추가하여 vertex payload는 줄당 16B, 6줄 Label당 96B 늘어난다. 대신 5개의 output Actor/Renderer/Geometry/vertex-buffer object가 사라진다. strength를 배열 uniform으로 묶는 데 따른 backend별 UBO 정렬·driver allocation은 FBO 표에 포함하지 않았다.

## D. Timing Consolidation Analysis

| format | 한 Label / 6줄 | cold publication 전체 Constraint::Apply 횟수 | H/V/output timing constraints | output color constraints |
|---|---|---|---|---|
| a8 | A performance | 101 | 20 | 6 |
| a8 | B performance | 96 | 20 | 1 |
| a8 | B high | 90 | 14 | 1 |
| rgba | A performance | 155 | 20 | 0 |
| rgba | B performance | 155 | 20 | 0 |
| rgba | B high | 149 | 14 | 0 |

전체 Apply 횟수는 생성부터 측정 시작까지 native hook으로 센 등록 횟수이며, Source/Label의 기존 constraint도 포함한다. live-object 수나 per-frame 전체 evaluator 호출 수와 동일한 지표라고 해석하지 않는다. timing 부분은 실제 코드상 H/V 각각 N+1, PERFORMANCE output N이다. 한 page custom renderer property 수는 H=N+4=10, V=10; PERFORMANCE output은 이전 6×3=18→N+2=8(A8), HIGH output은 color 1이다. RGBA output에는 A8 color mirror가 없다. 연결된 활성 page의 update당 strength evaluator는 H 6 + V 6 + PERFORMANCE output 6 = 18회, HIGH는 12회이며 별도 progress mirror 2회가 공통이다. 이는 코드로 산출한 정상 active-update 기준이며 전체 process/frame evaluator profiler 결과가 아니다. native raw renderer property count도 H/V 각각43, A8 output36×6→41, RGBA output35×6→40으로 확인했다(33개 기본 property 포함).

PER_LINE 한 page/6줄에서 H와 V에는 각각 6개 strength + 1개 progress constraint가 있다. PERFORMANCE output에는 여전히 6개 strength constraint가 있다. batching은 이 6개를 없애지 않았다. A8 output color mirror만 6→1로 줄었다. 기존 core의 connected/initialized ApplyRate=1 constraint는 update마다 evaluator를 호출하므로 이 timing 평가도 남아 있다. 모든 registration이 항상 실행된다는 뜻은 아니며, 입력 준비·연결 여부에 따른 일반 core 조건은 적용된다.

후속 후보는 `sequenceStart`를 static vertex attribute로, 공통 progress와 blur duration을 uniform으로 전달하고 H/V/output vertex에서 동일 equation을 계산하는 것이다. 정상 6줄/한 draw/page라면 timing 관련 20개(3×6+2)를 progress 전달 3개로 줄여 **17개/page**를 없앨 수 있다. line draw/page가 더 많으면 그 granularity대로 계산해야 한다. Source의 기존 property mirror는 그대로 남는다.

하지만 CPU→GLSL 부동소수점, 1/65535 start tolerance, completion epsilon, reverse/seek, short tail 및 async publication을 새 parity 범위로 떠안는다. Source/H/V/FBO/pass 수나 blur 픽셀 비용은 줄지 않는다. GLSL vertex 계산으로 구현 가능해 보이나 **이번에는 구현하지 않았고 GPU/CPU timing parity가 증명된 후보라고 주장하지 않는다.** batching 후 수치로 판단하면 현재 우선순위는 낮다.

## E. Reduced-Tap Design

공통 GaussianBlurAlgorithm/BlurEffect 코드는 수정하지 않았다. 공유 GaussianBlurSampleBlock에 접근하여 값을 덮어쓰지도 않는다. private PERFORMANCE shader에 constants를 넣어 unroll했고 cache key는 radius / cap / scalar-or-batch이다. 각 runtime publication에서 한 번 선택한다. HIGH는 cap을 읽지 않고 기존 exact factory를 사용한다. exact pair 수 ≤ cap이면 PERFORMANCE도 기존 shared shader를 그대로 사용한다.

여기서 small-radius **exact kernel 사용은 해상도를 HIGH로 바꾸는 fallback이 아니다.** PERFORMANCE의 Source/H/V 해상도 정책은 그대로 유지한다.

기존 Gaussian의 sigma는 고정 `radius/k`가 아니다. pair count N에 대해 offset 2N−1에서 Gaussian 값이 .01/(2N) 근처가 되도록, 기존 upper bound 64.062302와 최대 20회 탐색을 재현했다. 반쪽 discrete kernel 2N개를 정규화하고 중심값을 반으로 나눈 뒤 인접 두 sample을 bilinear pair로 합친다. 따라서 `2×sum(pair.weight)≈1`이다.

근사는 첫 pair와 마지막 pair를 유지하고, **나머지 모든 interior pair**를 K−2개의 연속 그룹으로 합친다. 그룹 weight는 합, offset은 weight-centroid이다. 양수·유한값·monotone offset·정규화·최외곽 support·positive-half first moment를 유지한다. 앞 K개 잘라 쓰기가 아니다. 다만 second moment 및 전체 frequency response까지 exact가 되는 방식은 아니다.

| kernel radius | Exact | CAP_12 pairs/reads | CAP_8 | CAP_6 |
|---|---|---|---|---|
| 8 | 4 pairs / 8 reads | 4 / 8 | 4 / 8 | 4 / 8 |
| 16 | 8 pairs / 16 reads | 8 / 16 | 8 / 16 | 6 / 12 |
| 24 | 12 pairs / 24 reads | 12 / 24 | 8 / 16 | 6 / 12 |
| 32 | 16 pairs / 32 reads | 12 / 24 | 8 / 16 | 6 / 12 |
| 40 | 20 pairs / 40 reads | 12 / 24 | 8 / 16 | 6 / 12 |
| 64 | 32 pairs / 64 reads | 12 / 24 | 8 / 16 | 6 / 12 |

radius 4..200(odd 포함), cap 6/8/12에서 수학적 UTC가 통과했다. 기존 공유 UBO의 실제 계수와 private reference 계수의 비교도 포함했다. 이 조건은 **상수 입력의 밝기와 kernel 범위에 대한 조건**이지 실제 text raster가 같은 밝기/부드러움으로 보인다는 보장은 아니다.

kernel vector는 shader 생성 중에만 사용하고 소멸한다. 영구 private UBO/계수 vector는 0 bytes. 생성 중 coefficient payload는 radius 64 기준 half 256B + exact pairs 256B + reduced pairs 96/64/48B = 608/576/560B(컨테이너/문자열 제외). 이후 계수는 shader source에 literal로 남는다. K개 pair의 수학적 offset/weight 값은 8K bytes지만 **shader source/driver binary 크기를 8K bytes라고 볼 수 없다.** 그 source·cache·driver 비용은 따로 남는다. 실제 GPU binary byte 수는 이 harness로 측정하지 못했다.

현재 생성기에서 pair 한 개의 unrolled source는 240 UTF-8 bytes이다. batch fragment 입력 source는 CAP_12/8/6 각각 6,338 / 5,378 / 4,898 bytes(공통 body 3,355 bytes 포함)다. 이는 Shader::New에 전달하는 source 문자열 길이이지 DALi/driver 내부 복사와 compiled binary까지 합한 상주 메모리가 아니다. 같은 radius/cap/batch variant는 재사용한다.

## F. Reduced-Tap Quality

reference는 batching ON + exact PERFORMANCE이다. 각 cap마다 2,360개 Source/H/V/Output 캡처를 비교했다. radius 8/16/24/32/40/64 × A8/RGBA × Latin/Korean/Arabic-mixed, 6줄 stagger .25와 WHOLE_TEXT RGBA 40 포함. A8 FBO는 coverage인 R channel로, RGBA는 premultiplied RGBA로 비교했다. Output은 검은 배경 위 window 결과다.

progress .001/.03/.06/.09/.12/.16/.2/.25/.4/.6/.8/1 및 reverse .4/0. 첫 줄 actual strength .75·.1은 CPU strength 로그로 역산한 progress를 추가하여 확인했다(72개 point, 최대 오차 2.74e−7). 다른 point에서 large/medium/small blur와 Late Smooth 구간을 함께 확인했다. 셋 모두 Source와 progress 0/1은 exact였다.

| radius | cap | H max diff | V max diff | final max diff | 비고 |
|---|---|---|---|---|---|
| 8 | 12 | 0 | 0 | 0 | exact kernel |
| 8 | 8 | 0 | 0 | 0 | exact kernel |
| 8 | 6 | 0 | 0 | 0 | exact kernel |
| 16 | 12 | 0 | 0 | 0 | exact kernel |
| 16 | 8 | 0 | 0 | 0 | exact kernel |
| 16 | 6 | 29 | 21 | 19 | 중간 blur 차이 |
| 24 | 12 | 0 | 0 | 0 | exact kernel |
| 24 | 8 | 33 | 19 | 18 | 중간 blur 차이 |
| 24 | 6 | 54 | 43 | 40 | 중간 blur 차이 |
| 32 | 12 | 25 | 16 | 15 | 중간 blur 차이 |
| 32 | 8 | 48 | 30 | 28 | 중간 blur 차이 |
| 32 | 6 | 90 | 65 | 58 | 중간 blur 차이 |
| 40 | 12 | 33 | 14 | 14 | 격자 증가, slow Korean 주의 |
| 40 | 8 | 90 | 39 | 35 | 격자 증가, slow Korean 주의 |
| 40 | 6 | 125 | 68 | 51 | 격자 증가, slow Korean 주의 |
| 64 | 12 | 50 | 19 | 19 | 격자 증가, slow Korean 주의 |
| 64 | 8 | 104 | 40 | 39 | 격자 증가, slow Korean 주의 |
| 64 | 6 | 123 | 70 | 68 | 격자 증가, slow Korean 주의 |

수치는 8-bit channel 단위 최대 차이이며, 여러 corpus/strength 중 worst case이다. mean/RMSE/p99/changed-pixel ratio는 quality-summary.csv (로컬 자료: `quality-summary.csv`)에 있다. 큰 검정 영역을 포함한 전체-frame MAE는 작게 보이므로 그것만으로 “안 보이는 차이”라고 판단하지 않았다.

CAP_12도 radius 64 한글의 중간 blur에서 규칙적인 격자가 보인다. CAP_8, CAP_6은 더 거칠고 final output의 최대 차이도 커진다. sparse grouped sample은 full support와 weight 합을 유지하더라도 촘촘한 획의 sampling alias를 충분히 평균내지 못한다. 지금 후보를 단순히 “미세 Gaussian falloff 차이만 있음”으로 분류할 수 없다. [동일 조건 crop 비교](korean-r64-p2-comparison.png)와 아래 slow sample 기록을 함께 참고한다.

실제 `text-reveal.example`의 Korean corpus, Pixel / PER_LINE / fade0 / stagger .25 / radius64 / blur time .5 / duration8s / linear / white-on-black에서 녹화했다. [Exact](slow-korean-cap0.mp4), [CAP_12](slow-korean-cap12.mp4), [CAP_8](slow-korean-cap8.mp4), [CAP_6](slow-korean-cap6.mp4). 영상은 품질 검토용 30fps 화면 녹화이며 성능 측정에 사용하지 않았다. 각 `slow-korean-cap*-settings.png`에 실제 메뉴 값도 남겼다.

## G. Reduced-Tap Performance

각 cap을 batching OFF(C), ON(D)에서 독립 측정했다. 아래는 B(exact) 대비 D이며 A/C 원자료도 CSV에 있다. 단위 ms/GPU frame, 8 Labels 합계.

| format | radius | B/D | Source | H | V | Output | Total | H+V 절감 | Total 절감 |
|---|---|---|---|---|---|---|---|---|---|
| a8 | 24 | exact | 0.257 | 0.099 | 0.100 | 0.126 | 0.582 | 0.0% | 0.0% |
| a8 | 24 | CAP_12 | 0.247 | 0.106 | 0.098 | 0.124 | 0.575 | -2.7% | 1.2% |
| a8 | 24 | CAP_8 | 0.246 | 0.067 | 0.071 | 0.123 | 0.507 | 30.5% | 12.9% |
| a8 | 24 | CAP_6 | 0.263 | 0.066 | 0.072 | 0.125 | 0.525 | 30.8% | 9.8% |
| a8 | 40 | exact | 0.245 | 0.127 | 0.117 | 0.150 | 0.639 | 0.0% | 0.0% |
| a8 | 40 | CAP_12 | 0.243 | 0.067 | 0.074 | 0.153 | 0.537 | 42.1% | 16.0% |
| a8 | 40 | CAP_8 | 0.251 | 0.072 | 0.069 | 0.153 | 0.544 | 42.5% | 15.0% |
| a8 | 40 | CAP_6 | 0.247 | 0.068 | 0.067 | 0.154 | 0.536 | 45.1% | 16.2% |
| a8 | 64 | exact | 0.244 | 0.157 | 0.178 | 0.200 | 0.780 | 0.0% | 0.0% |
| a8 | 64 | CAP_12 | 0.244 | 0.060 | 0.083 | 0.196 | 0.582 | 57.6% | 25.3% |
| a8 | 64 | CAP_8 | 0.245 | 0.060 | 0.071 | 0.200 | 0.576 | 61.1% | 26.2% |
| a8 | 64 | CAP_6 | 0.251 | 0.055 | 0.067 | 0.202 | 0.576 | 63.7% | 26.2% |
| rgba | 40 | exact | 0.302 | 0.148 | 0.126 | 0.147 | 0.724 | 0.0% | 0.0% |
| rgba | 40 | CAP_12 | 0.302 | 0.090 | 0.083 | 0.147 | 0.622 | 36.9% | 14.1% |
| rgba | 40 | CAP_8 | 0.310 | 0.080 | 0.076 | 0.151 | 0.618 | 42.9% | 14.6% |
| rgba | 40 | CAP_6 | 0.307 | 0.083 | 0.075 | 0.148 | 0.614 | 42.3% | 15.2% |

이론상 texture fetch 절감률과 전체 frame 개선율은 다르다. Source·output·clear/upload 및 이미 blur가 끝난 줄의 copy 비용은 cap으로 줄어들지 않는다. progress .5의 staggered fixture에서는 약 2/6줄만 Gaussian branch가 활성화된다. WHOLE_TEXT의 progress .5 / blur time .5는 blur가 이미 끝난 구간이므로 그 hold 결과로 kernel 절감 효과를 주장하지 않는다.

## H. Combined Result

| r40/8 Labels | 상태 | CPU ms/s | 60Hz ms/frame | Source | H | V | Output | Total | Actors | Renderers | Draws | 등록 constraints/Label | Tasks | FBO MiB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| a8 | A | 336.699 | 5.612 | 0.237 | 0.118 | 0.114 | 0.404 | 0.872 | 128 | 120 | 112 | 101 | 24 | 2.898 |
| a8 | B | 267.290 | 4.455 | 0.245 | 0.127 | 0.117 | 0.150 | 0.639 | 88 | 80 | 72 | 96 | 24 | 2.898 |
| a8 | C cap12 | 309.570 | 5.160 | 0.238 | 0.070 | 0.075 | 0.405 | 0.787 | 128 | 120 | 112 | 101 | 24 | 2.898 |
| a8 | D cap12 | 274.112 | 4.569 | 0.243 | 0.067 | 0.074 | 0.153 | 0.537 | 88 | 80 | 72 | 96 | 24 | 2.898 |
| a8 | C cap8 | 310.060 | 5.168 | 0.236 | 0.066 | 0.067 | 0.398 | 0.767 | 128 | 120 | 112 | 101 | 24 | 2.898 |
| a8 | D cap8 | 273.152 | 4.553 | 0.251 | 0.072 | 0.069 | 0.153 | 0.544 | 88 | 80 | 72 | 96 | 24 | 2.898 |
| a8 | C cap6 | 296.434 | 4.941 | 0.236 | 0.060 | 0.068 | 0.400 | 0.763 | 128 | 120 | 112 | 101 | 24 | 2.898 |
| a8 | D cap6 | 275.125 | 4.585 | 0.247 | 0.068 | 0.067 | 0.154 | 0.536 | 88 | 80 | 72 | 96 | 24 | 2.898 |
| rgba | A | 332.941 | 5.549 | 0.292 | 0.141 | 0.127 | 0.398 | 0.958 | 128 | 120 | 112 | 155 | 24 | 11.592 |
| rgba | B | 289.811 | 4.830 | 0.302 | 0.148 | 0.126 | 0.147 | 0.724 | 88 | 80 | 72 | 155 | 24 | 11.592 |
| rgba | C cap12 | 327.702 | 5.462 | 0.298 | 0.087 | 0.079 | 0.403 | 0.867 | 128 | 120 | 112 | 155 | 24 | 11.592 |
| rgba | D cap12 | 276.926 | 4.615 | 0.302 | 0.090 | 0.083 | 0.147 | 0.622 | 88 | 80 | 72 | 155 | 24 | 11.592 |
| rgba | C cap8 | 335.020 | 5.584 | 0.295 | 0.080 | 0.074 | 0.393 | 0.842 | 128 | 120 | 112 | 155 | 24 | 11.592 |
| rgba | D cap8 | 277.542 | 4.626 | 0.310 | 0.080 | 0.076 | 0.151 | 0.618 | 88 | 80 | 72 | 155 | 24 | 11.592 |
| rgba | C cap6 | 320.847 | 5.347 | 0.297 | 0.079 | 0.075 | 0.402 | 0.853 | 128 | 120 | 112 | 155 | 24 | 11.592 |
| rgba | D cap6 | 288.721 | 4.812 | 0.307 | 0.083 | 0.075 | 0.148 | 0.614 | 88 | 80 | 72 | 155 | 24 | 11.592 |

| r40/8 Labels | 상태 | first-process layout wall ms | warm layout wall ms | warm process CPU ms | warm construct-only ms | warm heap delta MiB |
|---|---|---|---|---|---|---|
| a8 | A | 79.143 | 63.274 | 65.523 | 2.165 | 14.260 |
| a8 | B | 75.186 | 60.269 | 62.563 | 2.108 | 13.921 |
| a8 | C cap12 | 79.475 | 62.999 | 65.386 | 2.090 | 14.265 |
| a8 | D cap12 | 78.239 | 60.207 | 62.362 | 2.048 | 13.921 |
| a8 | C cap8 | 80.351 | 64.003 | 66.445 | 2.072 | 14.264 |
| a8 | D cap8 | 76.041 | 60.297 | 62.371 | 2.119 | 13.922 |
| a8 | C cap6 | 79.109 | 65.426 | 67.761 | 2.288 | 14.275 |
| a8 | D cap6 | 76.861 | 59.381 | 61.489 | 2.064 | 13.921 |
| rgba | A | 84.088 | 66.884 | 69.290 | 2.146 | 23.909 |
| rgba | B | 85.471 | 68.847 | 71.606 | 2.363 | 23.381 |
| rgba | C cap12 | 84.473 | 68.394 | 70.805 | 2.225 | 23.908 |
| rgba | D cap12 | 79.600 | 64.165 | 66.595 | 2.132 | 23.379 |
| rgba | C cap8 | 85.001 | 68.111 | 70.497 | 2.168 | 23.912 |
| rgba | D cap8 | 80.673 | 65.908 | 68.354 | 2.170 | 23.378 |
| rgba | C cap6 | 84.359 | 67.700 | 70.330 | 2.207 | 23.914 |
| rgba | D cap6 | 81.434 | 64.466 | 66.895 | 2.152 | 23.377 |

Heap은 mallinfo2의 uordblks+hblkhd 생성 전후 차이이고 driver/allocator의 영향을 포함한 snapshot이다. peak나 retained leak 크기가 아니다. CPU/raw RSS range도 measurement-summary.csv에 보존했다.

CPU 수치는 각 상태의 독립 run 평균이며, 작은 cap 간 차이를 새로운 CPU 최적화로 주장하지 않는다. reduced-tap 자체는 constraint/Actor/Renderer/task/FBO 수를 바꾸지 않는다. CPU의 주된 구조적 절감은 output batching이다. setup도 compilation을 완전히 분리한 수치는 아니므로 표의 cold 차이만으로 shader compile 개선을 단정하지 않는다.

## I. Stability

- foundation/components build 성공. exact/CAP_12/CAP_8/CAP_6 각각 focused `UtcDaliTextReveal*` **121/121**. 마지막 formatting 이후 재build/reinstall 및 동일 4회 UTC도 통과했다(`verified-focused-cap*.log`).
- 기존 runtime lifecycle/reentry/async stale publication, resource readiness, None, scene disconnect/reconnect, Adaptor stop/cancellation, progress endpoints/forward/reverse/seek, A8/RGBA, decoration/scale/fallback coverage를 유지했다.
- 추가 UTC: 전체 private kernel의 수학적 조건, shader/shared UBO 격리, mixed ImageSpan output order 및 0이 아닌 첫 uniform slot. 기존 64/65 draw boundary와 sampling path UTC의 output state 기대값을 batching에 맞췄다.
- cap 실험에서 ReentryConfig UTC의 “exact shader name” 문자열 가정만 해당 cap의 최종 radius shader identity 비교로 고쳤다. reentry/publication 요구사항을 완화한 것이 아니다.
- 모든 native measurement/capture의 정상 해제에서 추가 task count가 0으로 돌아왔다. 이것만으로 모든 heap leak 부재를 증명하지는 않는다.
- 사용자 선택 전 단계이므로 **full regression 및 ASan/LSan/UBSan은 아직 실행하지 않았다.** 선택한 candidate에서 full internal/public UTC와 sanitizer/lifecycle stress를 이어가는 것이 다음 gate다.

## J. Recommendation

**권장 후보는 B: OUTPUT-BATCHED + EXACT Gaussian**이다. CPU와 scene graph overhead를 줄이면서 이번 host framebuffer 비교에서는 원래 품질을 그대로 유지했다. reduced cap은 기본값으로 승격하지 않았다.

CAP_12/8/6의 품질 허용 여부는 사용자가 결정한다. 현재 관찰된 한글 격자가 허용 범위를 넘는다면 reduced-tap prototype을 채택하지 않는 것이 맞다. 이 결과를 계기로 page/Source/Late Smooth/timing을 한꺼번에 다시 설계하지 않았다.

Stage 2는 필요할 때 재검토할 수 있는 구체적 후보지만 지금 추가 구현을 추천하지 않는다. 먼저 batching-only 후보를 타겟에서 평가하는 편이 변경 위험 대비 이득이 명확하다.

## K. Target Status

Host NVIDIA/GLES에서 실행·query·readback 검증. **Tizen target / low-end Mali / GLES2 / Vulkan은 NOT VERIFIED.** GLES2 output array indexing 및 private constant shader의 실제 compile/precision/performance도 해당 backend에서 확인해야 한다. host 결과를 근거로 다른 backend 품질·속도·memory regression이 없다고 보장하지 않는다.

## L. Working Tree / 재현

HEAD 유지, index 비어 있음, commit/add/amend/rebase 없음. UI 소스 6개 파일의 변경을 unstaged로 남겼다. 보고서·capture·benchmark·launch script는 repository 밖의 이 폴더에만 있다. 공통 GaussianBlurAlgorithm, BlurEffect shader, public API, sample은 수정하지 않았다. 기존 adaptor `gles-texture-dependency-checker.cpp`의 +13줄은 그대로 보존했다.

기본값: output batching ON, reduced-tap OFF. public BlurQuality::PERFORMANCE/HIGH 값이나 기본 enum은 바꾸지 않았다. 다음은 **PERFORMANCE를 선택했을 때만** 적용하는 creation-time prototype switch다.

```bash
# A-equivalent geometry / exact kernel (current library diagnostic)
bash /home/bowonryuubuntu/tizen/reveal-perf-opt.upcgEj/run-current.sh
# B: output batching / exact Gaussian — 권장 비교본
bash /home/bowonryuubuntu/tizen/reveal-perf-opt.upcgEj/run-perf-exact.sh
# D: output batching / private reduced taps
bash /home/bowonryuubuntu/tizen/reveal-perf-opt.upcgEj/run-perf-tap12.sh
bash /home/bowonryuubuntu/tizen/reveal-perf-opt.upcgEj/run-perf-tap8.sh
bash /home/bowonryuubuntu/tizen/reveal-perf-opt.upcgEj/run-perf-tap6.sh
# C: output unbatched / CAP_12
bash /home/bowonryuubuntu/tizen/reveal-perf-opt.upcgEj/run-tap12-unbatched.sh
```

직접 지정할 경우 `DALI_REVEAL_PERF_OUTPUT_BATCH=0/1`, `DALI_REVEAL_PERF_TAP_BUDGET=0/12/8/6`. 미지원 값은 exact로 처리된다. HIGH는 둘 다 무시한다. benchmark의 A/B는 보존된 별도 library로 측정했으므로 위 A-equivalent current-library 진단과 구분했다.

원자료: measurement-summary.csv (로컬 자료: `measurement-summary.csv`), measurement-summary.json (로컬 자료: `measurement-summary.json`), quality-summary.csv (로컬 자료: `quality-summary.csv`), quality-integrity.json (로컬 자료: `quality-integrity.json`), `final-focused-cap*.log`, `final-*-build.log`, `A/B/C/D-*.log`, `captures-v3/`, `quality-v3/`, `extra-captures/`.
