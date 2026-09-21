# A8 Source batching PoC — focused CPU attribution / medium-Label ROI

2026-09-14 · `devel_blur_text` · HEAD `47be37cf`

## 결론

**B — SOURCE BATCHING PROMISING FOR REALISTIC LABEL SIZES / FHD HOST ANOMALY REMAINS.**

타겟 측정에 진행할 가치는 있다. 그러나 현재 PoC를 그대로 production에 채택한다는 결론은 아니다.

- F24 HIGH의 기존 CPU +28.3%는 이번 동일 PoC 바이너리의 switch OFF/ON 비교에서 재현되지 않았다. 이번 3회 평균은 **231.11→234.58 ms/s (+1.5%)**이고 범위가 크게 겹친다.
- Static resource를 매 프레임 재생성하거나 업로드하는 PoC bug는 발견하지 못했다. 짧은 profile에서 DALi update/offscreen CPU는 감소했고, 증가한 상위 샘플은 NVIDIA 드라이버 내부였다. 기존 +51.05 ms/s의 정확한 원인까지 확정한 것은 아니다.
- 8개 medium Label에서 GPU draw time은 **16.2~38.2% 감소**했다. PERFORMANCE CPU는 평균 **17.8~33.8% 감소**했고, HIGH CPU는 개선 평균이지만 실행별 편차가 커 확정적인 개선으로 보지 않는다.
- Atlas 추가 조립은 Label당 **0.18~0.41 ms CPU**. 기존 line texture를 교체한다고 가정한 순증가는 Label당 **0.042 / 0.136 MiB**다. 현재 PoC는 실제로 교체하지 않고 중복 보관한다.
- Production/PoC 코드를 수정하지 않았다. 기존 348-pair framebuffer 검증을 재사용하고 새 parity matrix는 돌리지 않았다.

이 보고서는 [이전 측정의 CPU 미확인 상태에 따른 채택 보류/거절 판단](../reveal-source-atlas.3igp9v/REPORT.md)에 대한 후속 판단이다. 이전 raw 결과 자체를 취소하거나 수정하지 않는다.

## 측정 공통 조건

- Intel Core i7-11700 / NVIDIA GTX 1650 / NVIDIA 595.91.07 / GLES / MSAA 4.
- 현재 로컬 DALi 빌드 그대로 사용. UI CMake build type은 미지정이며 현재 flags에 `-O` 옵션은 없다. 타겟/Release 절대 성능으로 일반화하지 않는다.
- OFF/ON 모두 동일한 보존 라이브러리 `../reveal-source-atlas.3igp9v/candidate/libdali2-ui-foundation.so.2` 사용. `DALI_REVEAL_A8_SOURCE_BATCH_POC=0/1`만 변경.
- Sync, white single-color text(A8), font24, PIXEL / PER_LINE, radius24, Fade=0, Stagger=0.25, BlurDurationRatio=0.5, progress=0.5.
- Progress는 steady 측정 동안 고정하고 `KeepRendering`으로 계속 렌더링한다. 따라서 실제 progress 애니메이션 전체의 평균 비용이나 첫 생성 비용을 나타내는 표가 아니다.
- Medium primary: 2000×1000 창에 500px 폭 Label을 4열×2행으로 배치, 총 8개. 두 fixture/quality/variant 모두 같은 배치. 각 프로세스는 한 case만 실행한다.
- Medium CPU/GPU 각각 독립 프로세스 3회: 2 sizes × 2 qualities × 2 variants × 3 runs × 2 meters = **48개 유효 timing 로그**. 2회차는 candidate→baseline 순서로 반전했다.
- CPU: ordinary Label이 준비된 뒤 Reveal 활성화, runtime companion 생성 확인, 0.8초 warmup 이후 약 3초간 `CLOCK_PROCESS_CPUTIME_ID` 차이 / monotonic elapsed time. GPU query preload 없이 측정한다.
- GPU: 별도 실행에서 기존 GL timer-query 도구로 Source/H/V/Output draw를 측정. 약 178~181 submitted frames. CPU 표에는 이 실행의 CPU 시간을 사용하지 않는다.
- GPU total은 **draw GPU elapsed 합계**다. FBO clear, upload, swap/present, compositor, 전체 frame latency는 포함하지 않는다. Query dropped/disjoint는 모든 medium run에서 0.
- Single Label은 8개 별도 실행으로 inventory만 확인했다. Single-Label GPU/CPU timing matrix는 추가하지 않았다.

## 1. F24 HIGH CPU attribution

### 1.1 Thread CPU / 재현성

1920×1080 / font24 / 31 lines / A8 / HIGH / r24 / p0.5, 같은 라이브러리의 switch 0/1 비교다. `/proc/self/task/*/schedstat`의 thread 실행시간 차이도 함께 수집했다.

| 무계측 실행 | baseline process ms/s | PoC process ms/s |
|---|---:|---:|
| 1 | 193.01 | 222.24 |
| 2 | 236.70 | 240.01 |
| 3 | 263.63 | 241.48 |
| 평균 | **231.11** | **234.58** |
| 범위 | 193.01~263.63 | 222.24~241.48 |

실행 순서는 B,C,C,B 후 마지막 B,C였다. 마지막 쌍은 medium 측정 이후 수행했다. 같은 variant 안에서도 baseline이 약 70.6 ms/s 변한다. 기존 180.37→231.42 ms/s를 고정적인 per-frame 추가 연산으로 확정할 수 없다.

| Thread | baseline 평균 ms/s | PoC 평균 ms/s |
|---|---:|---:|
| main/event (`probe`) | 3.35 | 2.33 |
| combined update/render (`RenderThread`) | 227.82 | 232.40 |
| Evas workers / gmain / gdbus | 0 | 0 |

이 환경은 update/render가 같은 `RenderThread`에서 실행된다. 관찰된 CPU 차이/변동은 거의 전부 이 thread이며, main에서 수행하는 일회성 atlas assembly나 async worker가 원인은 아니다. Thread snapshot과 process clock의 경계/계측 방식 차이 때문에 소수점 합계는 정확히 일치하지 않는다.

Raw: B1 (로컬 자료: `thread-0-0.log`), B2 (로컬 자료: `thread-0-3.log`), B3 (로컬 자료: `thread-final-0.log`), C1 (로컬 자료: `thread-1-1.log`), C2 (로컬 자료: `thread-1-2.log`), C3 (로컬 자료: `thread-final-1.log`).

### 1.2 짧은 CPU sampling / phase 계측

Host의 `perf_event_paranoid=4` 정책을 변경하지 않고, 진단 프로세스 내부 `CLOCK_THREAD_CPUTIME_ID` timer / SIGPROF sampler와 API observer를 사용했다. 각각 6초 1회, RenderThread CPU 약 1ms당 RIP를 수집했다. Baseline 1,621개 / candidate 1,502개 유효 leaf samples. Driver는 심볼과 frame pointer가 충분하지 않아 전체 call-chain 복원이나 내부 함수의 의미를 단정하지 않는다.

이 계측은 자체 overhead와 scheduling 영향이 있다. 아래는 원인 분리를 위한 자료이며 위 무계측 CPU 평균과 섞지 않는다.

| 계측 구간 | baseline CPU ms / 약 6초 | PoC CPU ms / 약 6초 | 관찰 |
|---|---:|---:|---|
| `Core::Update` | 253.55 | 150.17 | 감소 |
| `Core::PreRender` | 3.42 | 3.71 | 작음 |
| Offscreen `Core::RenderScene` | 663.92 | 566.12 | 감소 |
| Window `Core::RenderScene` | 678.96 | 745.45 | 증가 |
| └ Swap (위 Window에 포함) | 46.79 | 49.30 | 작은 증가 |
| GL Draw wrapper (위 Render에 포함) | 24.43 | 19.26 | 감소 |
| Texture bind wrapper (위 Render에 포함) | 14.49 | 8.36 | 감소 |
| 계측 실행 전체 process ms/s | 284.70 | 262.61 | 이번 profile에서는 오히려 감소 |

Nested 항목은 합산하면 중복 계산된다. Window 구간 증가 전체를 `eglSwapBuffers` 비용이라고 부를 수도 없다.

Leaf sample 분포:

| Module / 확인 가능한 작업군 | baseline | PoC |
|---|---:|---:|
| DALi core | 431 | 286 |
| DALi adaptor | 277 | 262 |
| DALi UI | 24 | 31 |
| NVIDIA eglcore | 536 | 601 |
| NVIDIA eglcore 중 Window 구간 | 357 | 415 |
| constraint / property-reset 이름 포함 | 33 | 11 |
| uniform / texture-state 이름 포함 | 83 | 62 |
| render-instruction generation 이름 포함 | 17 | 9 |
| `malloc/free/calloc/realloc` leaf | 5 | 9 |
| 이름으로 식별 가능한 memcpy/memset leaf | 1 | 0 |

Module 집계와 작업군 집계는 서로 중복되므로 합산하지 않는다. Allocator/copy의 심볼 미해결 내부 함수까지 완전히 계측했다는 뜻도 아니다.

증가한 상위 leaf 위치/함수:

| 위치 / 함수 | baseline→PoC samples |
|---|---:|
| NVIDIA eglcore `+0xaf7a16` | 2→29 |
| NVIDIA eglcore `+0xae9a08` | 0→20 |
| NVIDIA eglcore `+0xaf7c20` | 27→38 |
| NVIDIA eglcore `+0xaf7c19` | 40→46 |
| NVIDIA eglcore `+0xaf61ef` | 1→7 |
| `malloc` | 3→7 |
| `GLES::Buffer::GetBufferChangedCount()` | 1→5 |

상위 증가 위치는 대부분 드라이버 내부다. 다만 내부 심볼이 없어 이를 특정 fence wait/spin 결함이라고 확정하지 않는다. CPU 총량의 실행별 변동, 실제 DALi 작업 감소, driver/window 측 CPU shift까지가 관찰된 근거다.

Raw: baseline sampling/counters (로컬 자료: `sample-0.log`), candidate sampling/counters (로컬 자료: `sample-1.log`), sampler (로컬 자료: `sampler.cpp`), observer (로컬 자료: `observer.cpp`), 분석 스크립트 (로컬 자료: `analyze-samples.py`). 별도의 observer-only 결과도 baseline (로컬 자료: `observe-0.log`), candidate (로컬 자료: `observe-1.log`)에 보존했다. 초기 observer-only 로그의 Offscreen=0은 3-argument overload 계측 누락이며, 실제 작업이 0이라는 뜻이 아니다. 위 표는 그 overload까지 계측한 sample 로그만 사용한다.

### 1.3 Frame count / static resource 검증

| 6초 profile counter | baseline | PoC |
|---|---:|---:|
| Update | 361 | 361 |
| PreRender | 361 | 361 |
| Offscreen RenderScene | 361 | 361 |
| Window RenderScene | 361 | 361 |
| Swap/submitted frame | 361 | 360 |
| GL draws | 17,689 | 8,658 |
| Texture binds | 37,905 | 19,848 |

CPU 증가를 설명할 candidate의 추가 update/render loop는 없다. Swap의 1회 차이는 event/render 계측 경계 차이다. Offscreen RenderScene 1회는 여러 RenderTask 실행을 포함하며, FBO task 수를 뜻하지 않는다.

Steady 구간에서 두 variant 모두 다음 값은 **0**:

- `BuildPocSourceAtlas()` 실행 로그: candidate는 초기 publication 전 1회, steady 종료 이후 재생성 시 1회뿐.
- `PixelBuffer::New`, atlas `Texture::Upload`, `glTexImage2D/SubImage2D`.
- `VertexBuffer::SetData`, `Geometry::New`, `glBufferData/SubData`.
- `Shader::New`, 관찰 대상 `RegisterProperty`, `Renderer::SetTextures`.
- 관찰 대상 `glClientWaitSync` / `eglClientWaitSyncKHR` 호출도 0. 이것이 모든 driver 내부 wait가 없음을 뜻하지는 않는다.

각 hook은 initialization에서 nonzero lifetime count를 기록하여 작동을 확인했다. Atlas rect 계산과 batch vertex 배열 생성은 `Initialize()` 안에만 있으며, steady 호출 경로가 없다. PoC shader/geometry를 매 frame dirty하게 만드는 근거를 찾지 못했다.

### 1.4 원인 분류

**주 분류: `MEASUREMENT INSTABILITY`.**

부가 관찰: `DRIVER / HOST-SPECIFIC SHIFT`와 부합하는 증거는 있다(DALi update/offscreen 감소, driver/window samples 증가). 그러나 기존 +51.05 ms/s를 특정 driver 함수/정책으로 모두 설명했다고 말할 수는 없다.

`POC BUG / ACCIDENTAL WORK` 또는 `STRUCTURAL SOURCE-BATCH COST`로 분류할 증거는 없었다. 따라서 추측에 기반한 PoC 수정은 하지 않았다. FHD anomaly는 완전 closure가 아닌 미확정 상태로 남긴다.

## 2. Medium fixture / 실제 layout 및 pixels

개행 없는 자연스러운 한국어 생활/여행 문단을 사용했다. 기본 font metrics/wrapping을 유지했다. M1은 약 6줄 분량, M2는 높이에 들어가는 분량을 기존 `GetHeightForWidth()`로 결정했다. LineHeight/MAX_LINES나 exact line-count assertion은 설정하지 않았다.

| Fixture | Label 크기 | Unicode 문자 수 | 실제 줄 수 | 실제 text 높이 | Source page 수 / Label |
|---|---:|---:|---:|---:|---:|
| M1 | 500×350 | 157 | 6 | 204px | 1 |
| M2 | 500×500 | 358 | 14 | 476px | 1 |

Corpus: M1 (로컬 자료: `corpus-350.txt`), M2 (로컬 자료: `corpus-500.txt`). 최종 line count는 이 로컬 font 환경의 결과이며 모든 플랫폼에서 같은 수라고 가정하지 않는다.

| Per-Label texture/area | M1 | M2 |
|---|---:|---:|
| ordinary text texture | 500×204 = 102,000 px | 500×476 = 238,000 px |
| eligible line foreground 총 면적 | 79,660 px | 180,992 px |
| atlas (foreground와 metadata 각각 동일 면적) | 492×180 = 88,560 px | 499×420 = 209,580 px |
| Source FBO | 542×480 = 260,160 px | 549×1120 = 614,880 px |
| PERFORMANCE H FBO | 136×480 = 65,280 px | 138×1120 = 154,560 px |
| PERFORMANCE V FBO | 136×120 = 16,320 px | 138×280 = 38,640 px |
| HIGH H / V, 각각 | Source와 동일 | Source와 동일 |

Pixels는 할당 면적이다. 실제 실행된 fragment 수를 hardware counter로 센 값이 아니다. Source 페이지는 halo가 붙은 줄들을 쌓으므로 Label 표시 높이보다 높을 수 있다. PoC는 위 Source/H/V FBO 크기를 전혀 바꾸지 않는다.

## 3. Draw / object 구조

| Fixture, 8 Labels | Source | H | V | Output | Total draw / full frame |
|---|---:|---:|---:|---:|---:|
| M1 baseline | 48 | 8 | 8 | 8 | 72 |
| M1 PoC | 8 | 8 | 8 | 8 | 32 |
| M2 baseline | 112 | 8 | 8 | 8 | 136 |
| M2 PoC | 8 | 8 | 8 | 8 | 32 |

GPU query에서 관찰한 평균 draw/frame도 이 구조와 일치한다. 계측 시작/종료의 부분 frame과 in-flight query 때문에 일부는 Source 47.73/111.38, H/V 7.94~7.96처럼 1% 미만 차이가 난다. 이 차이는 아래 GPU 시간에 그대로 포함했고, 이상적으로 보정하여 절감률을 키우지 않았다.

- 두 quality 모두 offscreen **24 tasks 그대로**, window task까지 합치면 25. Source draw batching이지 task batching이 아니다.
- 8 Labels의 companion 관련 Actor inventory는 **88 그대로**.
- Renderer inventory: M1 **80→40**, M2 **144→40**.
- Geometry inventory: **25→33**. 기존 line shared quad 대신 page별 batch geometry가 추가된다.
- Single inventory: Actor 11 그대로, M1 Renderer 10→5 / M2 18→5, Geometry 4→5, offscreen task 3 그대로. 합계에는 숨겨진 ordinary renderer도 포함되어 draw 수와 같지 않다.

## 4. GPU 결과 — 8 Labels

단위: ms/frame, 3회 평균. B=baseline, C=PoC.

| Fixture / quality | Source B→C | H B→C | V B→C | Output B→C | Total B→C | 절감 |
|---|---:|---:|---:|---:|---:|---:|
| M1 PERFORMANCE | .2645→.0705 | .1025→.0996 | .0953→.1056 | .1500→.1608 | **.6122→.4365** | **.1758 ms / 28.7%** |
| M1 HIGH | .2579→.0655 | .1629→.1743 | .3254→.3561 | .1285→.1368 | **.8747→.7327** | **.1420 ms / 16.2%** |
| M2 PERFORMANCE | .6119→.1319 | .1265→.1257 | .1068→.1207 | .2793→.3172 | **1.1245→.6954** | **.4291 ms / 38.2%** |
| M2 HIGH | .5791→.1109 | .1647→.1974 | .3872→.4554 | .2221→.2499 | **1.3531→1.0136** | **.3395 ms / 25.1%** |

Source 단독 절감률은 각각 **73.3%, 74.6%, 78.4%, 80.9%**.

| GPU total 실행 범위 (ms/frame) | baseline | PoC |
|---|---:|---:|
| M1 PERFORMANCE | .5697~.6427 | .4273~.4476 |
| M1 HIGH | .8595~.8958 | .7144~.7459 |
| M2 PERFORMANCE | 1.1195~1.1271 | .6758~.7268 |
| M2 HIGH | 1.3449~1.3673 | .9984~1.0222 |

H/V/Output도 시간이 완전히 같지는 않다. 이들 shader/geometry/FBO는 바꾸지 않았지만, Source 제출 구조가 달라진 뒤의 GPU 실행 환경에서 측정된 차이까지 포함한 것이 Total이다. H/V 증가를 임의로 noise로 지우거나 Source 절감량 전체를 Total 이득으로 계산하지 않았다. 추가 원인 규명은 이번 범위 밖이다.

Single Label timing을 따로 측정하지 않았으므로 위 수치를 8로 나눈 값을 독립 Single Label 성능이라고 보고하지 않는다.

## 5. CPU 결과 — 8 Labels

`CLOCK_PROCESS_CPUTIME_ID`, query/profiler 없는 별도 3회. CPU ms/frame@60은 ms/s ÷ 60이며 실제 frame wall latency가 아니다.

| Fixture / quality | 평균 CPU ms/s B→C | CPU ms/frame@60 B→C | 평균 변화 |
|---|---:|---:|---:|
| M1 PERFORMANCE | **261.21→214.59** | 4.353→3.576 | −17.8% |
| M1 HIGH | **636.61→567.68** | 10.610→9.461 | −10.8%, 편차 큼 |
| M2 PERFORMANCE | **340.12→225.03** | 5.669→3.750 | −33.8% |
| M2 HIGH | **733.77→706.52** | 12.229→11.775 | −3.7%, neutral 범주 |

| Fixture / quality | baseline 3 runs (ms/s) | PoC 3 runs (ms/s) |
|---|---|---|
| M1 PERFORMANCE | 266.81, 267.64, 249.18 | 194.35, 200.59, 248.82 |
| M1 HIGH | 690.30, 701.36, 518.18 | 599.56, 527.16, 576.33 |
| M2 PERFORMANCE | 344.55, 325.47, 350.34 | 232.62, 229.95, 212.52 |
| M2 HIGH | 650.89, 776.26, 774.16 | 704.44, 710.02, 705.10 |

| CPU 범위 ms/s | baseline | PoC |
|---|---:|---:|
| M1 PERFORMANCE | 249.18~267.64 | 194.35~248.82 |
| M1 HIGH | 518.18~701.36 | 527.16~599.56 |
| M2 PERFORMANCE | 325.47~350.34 | 212.52~232.62 |
| M2 HIGH | 650.89~776.26 | 704.44~710.02 |

PERFORMANCE는 두 fixture 모두 3회 평균/범위에서 유리하다. HIGH는 일부 대응 run에서는 candidate가 느리고 범위도 겹치므로 “CPU 개선을 보장”하거나 “무회귀 입증 완료”라고 결론내리지 않는다. 반대로 모든 크기에서 +10~30%가 지속되는 structural CPU regression도 관찰되지 않았다.

## 6. Setup 비용

### 6.1 Atlas construction

각 quality의 CPU-only 3 processes × 8 Labels = 24개 atlas 생성 호출 평균. CPU는 `CLOCK_THREAD_CPUTIME_ID`, wall은 monotonic. PixelBuffer 생성/zero/copy/Texture handle 생성 및 Upload 요청까지 포함하지만 GPU upload 완료 대기는 포함하지 않는다.

| Fixture / quality | Label당 CPU / wall ms | 8개 atlas CPU / wall 합 ms |
|---|---:|---:|
| M1 PERFORMANCE | .179 / .180 | 1.429 / 1.441 |
| M1 HIGH | .187 / .189 | 1.498 / 1.510 |
| M2 PERFORMANCE | .372 / .374 | 2.975 / 2.990 |
| M2 HIGH | .407 / .410 | 3.260 / 3.277 |

| 데이터 이동 | M1 per Label / 8 Labels | M2 per Label / 8 Labels |
|---|---:|---:|
| copy bytes (guard 복제 포함) | 428,550 / 3,428,400 | 973,800 / 7,790,400 |
| 별도 zero-fill bytes | 442,800 / 3,542,400 | 1,047,900 / 8,383,200 |

Copy bytes는 모든 메모리 트래픽의 합이 아니다. 원본 raster 생성과 GPU upload도 별개다.

### 6.2 Publication 전체

8개 ordinary Label이 준비된 상태에서 첫 `SetTextReveal` 직전부터 모든 runtime companion이 event-side에 생성되었음을 확인할 때까지 측정. Sync relayout/raster/setup과 frame scheduling을 포함한다. 첫 GPU frame 완료나 앱 시작 전체 시간이 아니다.

| Fixture / quality | 8개 publication wall ms B→C | 같은 구간 process CPU ms B→C |
|---|---:|---:|
| M1 PERFORMANCE | 81.03→76.29 | 81.12→77.48 |
| M1 HIGH | 82.68→74.80 | 80.02→73.43 |
| M2 PERFORMANCE | 156.46→138.01 | 163.04→137.33 |
| M2 HIGH | 152.79→138.44 | 156.13→141.06 |

추가 atlas assembly 비용은 발생하지만, 이 fixture의 전체 publication 측정에서는 이를 상쇄했다. Line별 renderer/property/constraint 설정 감소와 부합한다. Setup 함수별 시간을 완전히 분해한 실험은 아니므로 감소 전부를 특정 함수 하나의 효과로 단정하지 않는다.

## 7. Memory

### 7.1 Input atlas 자체

모든 수치는 논리적인 texture payload(`width × height × bytes-per-pixel`)다. GPU driver의 실제 resident allocation, alignment, implicit staging, process peak RSS를 뜻하지 않는다. A8 foreground + RGBA metadata 합계 5B/pixel.

| Per Label | M1 | M2 |
|---|---:|---:|
| 기존 line foreground + metadata | 398,300 B / .3798 MiB | 904,960 B / .8630 MiB |
| 새 atlas pair | 442,800 B / .4223 MiB | 1,047,900 B / .9994 MiB |
| 기존 line texture 교체 가정 시 순증가 | **44,500 B / .0424 MiB** | **142,940 B / .1363 MiB** |
| atlas / line input − 1 | **11.17%** | **15.80%** |
| atlas 내 overhead 비율 `(atlas−line)/atlas` | 10.05% | 13.64% |

위 overhead는 guard와 각 shelf의 빈 폭을 합한 것이다. 실제 PoC에서는 기존 eligible line TextureSet을 `mSequences`에 계속 보관하므로 atlas 전체가 추가 메모리다.

### 7.2 전체 texture payload

Ordinary full text texture/metadata, eligible line input, Source/H/V FBO까지 포함. 실제 texture identity를 중복 제거하고, scene traversal에 드러나지 않는 `mSequences`의 기존 line input은 PoC에서 추가 합산했다.

| Per-Label MiB | baseline | 실제 PoC (중복 보관) | projected replacement |
|---|---:|---:|---:|
| M1 PERFORMANCE | 1.1922 | 1.6144 | 1.2346 |
| M1 HIGH | 1.6105 | 2.0328 | 1.6530 |
| M2 PERFORMANCE | 2.7686 | 3.7679 | 2.9049 |
| M2 HIGH | 3.7571 | 4.7565 | 3.8934 |

| 8 Labels MiB | baseline | 실제 PoC | projected replacement |
|---|---:|---:|---:|
| M1 PERFORMANCE | 9.5372 | 12.9155 | 9.8767 |
| M1 HIGH | 12.8844 | 16.2627 | 13.2239 |
| M2 PERFORMANCE | 22.1484 | 30.1433 | 23.2390 |
| M2 HIGH | 30.0568 | 38.0516 | 31.1473 |

즉 8개 기준 현재 중복 PoC의 증가량은 **+3.3783 / +7.9948 MiB**, 실제 replacement가 가능하다고 가정한 증가량은 **+.3395 / +1.0905 MiB**다. Replacement는 수식으로만 계산했으며 구현·안전성 검증이 완료된 상태가 아니다.

Source/H/V FBO payload는 B/C 동일: M1 PERFORMANCE .3259 MiB/Label, M1 HIGH .7443, M2 PERFORMANCE .7706, M2 HIGH 1.7592. Output은 별도 FBO가 아니라 window에 그린다.

CPU 측면에서는 assembly 시 atlas PixelBuffer가 추가로 필요하다. 그 용량은 atlas pair와 같으며 기존 input을 읽어 조립한다. GPU 업로드 staging까지 포함한 물리 peak heap/RSS 감소를 이번 측정으로 주장하지 않는다. 이 PoC는 메모리 절감 구현이 아니라 draw/object overhead 감소 실험이다.

Single inventory 확인 예: M1 baseline (로컬 자료: `single-350-performance-0.log`), M1 PoC (로컬 자료: `single-350-performance-1.log`), M2 HIGH baseline (로컬 자료: `single-500-high-0.log`), M2 HIGH PoC (로컬 자료: `single-500-high-1.log`). 개별 texture 크기/bytes와 aggregate가 8 Labels 측정과 일치한다.

## 8. Workload scaling

FHD GPU는 이전 3회 측정을 재사용했다. FHD=1 Label, M1/M2=8 Labels라서 total absolute GPU를 동일 크기/개수로 오해하면 안 된다.

| Fixture / quality | Lines × Labels | Source draw B→C | Source GPU 감소 | Total GPU 감소 | CPU 평균 변화 | replacement 순증가 / Label | Atlas CPU / Label |
|---|---:|---:|---:|---:|---|---:|---:|
| FHD24 PERFORMANCE (이전) | 31×1 | 31→6 | 45.4% | 14.0% | 이전 +11.5%, 편차 있음 | +.6937 MiB | 약 3.2ms |
| FHD24 HIGH | 31×1 | 31→6 | 54.0% | 13.3% | 이전 +28.3%, 이번 +1.5%/미확정 | +.6937 MiB | 약 3.2ms |
| M1 PERFORMANCE | 6×8 | 48→8 | 73.3% | 28.7% | −17.8% | +.0424 MiB | .179ms |
| M1 HIGH | 6×8 | 48→8 | 74.6% | 16.2% | −10.8%, 편차 큼 | +.0424 MiB | .187ms |
| M2 PERFORMANCE | 14×8 | 112→8 | 78.4% | 38.2% | −33.8% | +.1363 MiB | .372ms |
| M2 HIGH | 14×8 | 112→8 | 80.9% | 25.1% | −3.7%, neutral | +.1363 MiB | .407ms |

Medium에서는 한 Source draw의 texture/fragment 면적이 작고 line별 제출의 고정 비용 비중이 커지는 상황과 부합한다. 8개에서 절감된 total GPU는 .14~.43ms/frame이며, percentage뿐 아니라 absolute 기준으로도 검토할 가치가 있다. 특히 M2 PERFORMANCE는 CPU/GPU 둘 다 유리했다.

그러나 “Label이 작아질수록 항상 이득 증가”라고 일반화하지 않는다. M1과 M2의 line count, 한 페이지에 들어가는 line 수, 8 Label 누적 객체 수도 다르다. H/V가 상대적으로 큰 HIGH에서는 Source 절감의 total 비중이 작다. Font32 추가 matrix는 이 판단에 필요하지 않아 하지 않았다.

## 9. Recommendation

선택: **B**.

1. **이번 PoC를 structural CPU regression 때문에 폐기할 근거는 부족하다.** 초기 +28.3%는 재현성이 없었고, static per-frame accidental work가 없으며 profile에서 DALi 측 비용은 감소했다.
2. **실사용 크기에서 추가 검증할 ROI는 있다.** Source draw 감소, 안정적인 GPU total 감소, PERFORMANCE CPU 이득, 작은 assembly/교체 가정 overhead가 근거다.
3. **다음 gate는 타겟 측정이다.** 같은 PoC switch로 M1/M2 8 Labels, 우선 PERFORMANCE와 HIGH를 확인한다. FHD HIGH host anomaly와 HIGH CPU 편차가 남아 있어 지금 바로 production 승인을 내리지는 않는다.
4. 타겟에서 CPU/GPU가 유리할 때만 기존 line texture replacement의 ownership/lifetime 검토와 production 설계를 별도 작업으로 판단한다. 그 구현을 이번에 선행하지 않았다.

Driver 버그를 확정하거나 host 정책/driver 옵션/adaptor를 바꾸는 workaround도 추가하지 않았다.

## 10. Working tree / 산출물

- HEAD `47be37cf`, blur production commit `7dbebd1d`, factory `a78547fa` 그대로. Commit/amend/rebase/push 없음.
- 기존 UI PoC 3개 파일은 **unstaged/uncommitted 그대로**. 이번 작업에서 바뀐 production/PoC source는 0개.
- `git diff --binary`와 기존 `reveal-source-atlas.3igp9v/poc.diff`의 SHA-256이 동일:
  `22ac1c7490745f0a0ad337963f3d0289b36308cc19d347d2065cee23fad8c24d`.
- Staged diff 없음. `git diff --check` 통과. 기존 adaptor dirty file도 손대지 않음.
- 기존 framebuffer parity **348 pairs byte-identical** 결과 재사용. PoC 수정이 없으므로 capture smoke/full regression/build를 추가하지 않았다. 진단 harness/shared library만 이 외부 작업 디렉터리에서 컴파일했다.
- 새 파일은 모두 이 보고서가 있는 repo 바깥 측정 디렉터리에만 생성했다.

재현/검토 파일:

- Medium runner (로컬 자료: `run-medium.sh`), measurement harness (로컬 자료: `probe.cpp`), medium raw summary JSON (로컬 자료: `medium-summary.json`), summary script (로컬 자료: `analyze-medium.py`).
- Raw timing: `medium-{350,500}-{performance,high}-{0,1}-{cpu,gpu}-{1,2,3}.log`.
- Sample profiler 분석 (로컬 자료: `analyze-samples.py`), observer (로컬 자료: `observer.cpp`), sampler (로컬 자료: `sampler.cpp`), `sample-{0,1}.log`, `thread-*.log`.
- 사용한 PoC library SHA-256: `bc2edb8a2c671e7f6c941bb19012a5380a823cf96cb86a215cb0f1051f933b9f`.

계측 준비 중 medium GPU harness가 종료 전에 query 결과를 출력할 시간을 주지 못한 경우에는 결과 없는 실행을 사용하지 않고, 측정 종료 뒤 0.4초 drain 구간을 두어 다시 수집했다. 이 변경은 측정 harness에만 적용했고 timing window와 PoC 동작은 바꾸지 않았다. 위 48개 유효 로그 모두 필요한 CPU/GPU 결과가 존재한다.
