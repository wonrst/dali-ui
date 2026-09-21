# A-R performance feasibility — CURRENT 대비 최소 범위 측정

## 결론

**A-R PERFORMANCE STRONGLY PROMISING — READY FOR PRODUCTION DESIGN**

이는 **성능 관점의 설계 검토 Go**이며 production 구현/품질 승인/merge 승인이 아니다.
기존 A-R의 품질은 전혀 수정하지 않았다.

| 항목 | CURRENT | A-R | 결과 |
|---|---:|---:|---|
| A8 p=.20 total draw GPU | 0.7479 ms/frame | 0.4332 ms/frame | **42.1% 감소** |
| A8 8초 왕복 total draw GPU | 0.7728 ms/frame | 0.4178 ms/frame | **45.9% 감소** |
| 왕복 process CPU | 207.9 ms/s | 185.7 ms/s | 악화 관찰 없음; 개선율 확정은 보류 |
| 준비 후 RSS 평균 | 138.197 MiB | 138.367 MiB | **+0.171 MiB** |
| 활성 FBO A8 color payload | 2.9835 MiB | 3.1256 MiB | **+0.1421 MiB / +4.76%** |
| Gradient 왕복 total draw GPU, 각 1회 | 0.7363 ms/frame | 0.4258 ms/frame | **42.2% 감소**, sanity only |

GPU 수치는 Source/Prefilter/H/V/Output의 **draw 시간 합계**다.
clear/resolve/present/CPU 대기를 포함한 전체 프레임 시간이 아니며, 타겟 FPS 개선율로 환산하지 않는다.

메모리 주의:

1. 외부 PoC에는 **교체 전 H FBO/texture가 추가로 남는 것**을 native GL에서 확인했다.
   위 +4.76%는 활성 경로만의 논리 저장량이다.
2. 최초 GPU 계측 워밍업 A-R 프로세스에서 **VmHWM 197.54 MiB**가 한 번 관찰됐다.
   이후 계측 없는 3회 peak는 139.53–139.61 MiB였다.
   원인은 확정하지 않았으며, 첫 사용 메모리를 해결된 것으로 간주하지 않는다.

## A. Baseline / fixture / 측정 방법

- Repository: `/home/bowonryuubuntu/tizen/dali/dali-ui`
- HEAD: `5aae0d2b389bb9811bed6e21b754cee629f4ecfc`
- Branch: `devel_blur_text`
- 시작/종료 UI worktree clean. BASELINE.txt (로컬 자료: `BASELINE.txt`)
- 기존 adaptor의 `gles-texture-dependency-checker.cpp` +13줄은 보존했다.
- GTX 1650 / NVIDIA 595.91.07 / GLES / MSAA 4 / DISPLAY :1.
- 현재 설치/빌드된 DALi 라이브러리 사용. Foundation flags에 명시적 `-O` 최적화가 없고
  CMAKE_BUILD_TYPE도 비어 있다. **Release CPU 성능 또는 TV 수치가 아니다.**
- 비교는 CURRENT와 A-R만. 기존 외부 PoC의 A-R installer를 그대로 include했다.

Primary는 기존 **Korean24, 12줄 corpus, FHD 1920×1080 Label**이다.
위치 (60,60), 검정 배경, 흰 글자, 창 1920×1080.
Unit::LINE / WHOLE_TEXT / PERFORMANCE / radius48 / Fade0 / Stagger .25 / BlurDurationRatio1.
Fade0이므로 HEAD의 Fade1 adaptive recipe는 제외된다.
새 corpus/layout을 만들지 않았다. 측정용 앱은 비교 HUD만 제거해 다른 text draw가
Output 시간에 들어오지 않게 했다.

```text
CURRENT: Source 2020×1180 → H 505×1180 → V 505×295 → Output
A-R:     Source 2020×1180 → Y prefilter 2020×295
                         → H 505×295 → V 505×295 → Output
```

A-R Output은 기존 그대로 Y ±0.5/295의 두 V samples를 평균한다.
Sharp Source/Late Smooth/Reveal timing/fade/kernel/sigma/delta를 변경하지 않았다.

반복/순서:

- CURRENT/A-R 각각 제외용 워밍업 **1회**.
- GPU: CURRENT→A-R을 교대로 **각각 독립 프로세스 3회**.
  각 프로세스에서 p=.20 고정 약 3초, 다음 8초 linear 0→1→0(4초+4초) 측정.
- CPU/RSS: GPU 계측 없이 CURRENT→A-R 교대로 **각각 독립 프로세스 3회**.
  8초 왕복만 측정. 일반 scene/준비/animation RSS도 이 실행에서 기록.
- A8에서 충분한 GPU 개선 확인 후 기존 Gradient fixture로 **각각 1회**.
- GPU/CPU 실행을 동시에 돌리지 않았다. radius/font/progress matrix는 만들지 않았다.

GPU timer는 각 실제 draw의 EXT_disjoint_timer_query다. 결과는 비동기 수집하며
측정 시작/종료를 swap frame 경계에 맞췄다. 종료 후 pending query를 모두 회수했다.
glFinish/readback 없이 측정했고, 최종 run에 dropped/disjoint query는 **0**이었다.
각 stage draw count는 같은 측정 frame 수와 일치한다.
고정 구간 180–182 frames, 왕복 482 frames/run. 이는 분모 확인이지 FPS benchmark가 아니다.

CPU는 GPU hook/LD_PRELOAD 없이 CLOCK_PROCESS_CPUTIME_ID로 측정했다.
메인/update/render 및 해당 프로세스의 driver thread CPU가 합산된다.
Timer 경계 때문에 실제 CPU window는 약 8.039초이며, 실제 wall time으로 나눴다.
3회 평균은 run별 ms/frame 또는 CPU ms/s의 산술평균이다.

세부 경계: [METHODS.md](METHODS.md).

## B. GPU stage timing — fixed p=.20

단위: ms/frame, 독립 실행 3회 평균.

| Stage | CURRENT | A-R | delta ms | delta % |
|---|---:|---:|---:|---:|
| Source | 0.054877 | 0.062475 | +0.007598 | +13.85% |
| Prefilter | — | **0.051519** | +0.051519 | 신규 |
| H | 0.459327 | **0.145185** | **−0.314143** | **−68.39%** |
| V | 0.125026 | **0.036887** | **−0.088139** | **−70.50%** |
| Output | 0.108670 | **0.137147** | **+0.028477** | **+26.21%** |
| **TOTAL** | **0.747900** | **0.433213** | **−0.314687** | **−42.08%** |

추가 prefilter draw와 Output read 비용은 위 합계에 포함된다.
prefilter의 clear/driver submission은 GPU draw query 밖이며, submission의 CPU 영향은
별도 CPU run에서 확인했다. 전체 RenderTask 경과시간을 측정했다고 해석하면 안 된다.

TOTAL 원시 run 평균:

| Run | CURRENT | A-R |
|---|---:|---:|
| 1 | 0.697431 | 0.443423 |
| 2 | 0.861888 | 0.430292 |
| 3 | 0.684380 | 0.425923 |
| 표본 표준편차 | 0.098932 | 0.009109 |

CURRENT 편차가 있지만, CURRENT의 가장 낮은 run도 A-R의 가장 높은 run보다 크다.
단순 noise 수준의 개선은 아니다. GPU clock을 고정하지 않았으므로 세부 stage의
작은 차이까지 모두 알고리즘 변화로 단정하지 않는다.
Source는 코드가 동일하지만 관측 시간은 약 .008ms 증가했다. 이 증가까지 TOTAL에 포함했다.

## C. Active animation GPU — 8초 linear 왕복

| Stage | CURRENT | A-R | delta ms | delta % |
|---|---:|---:|---:|---:|
| Source | 0.058260 | 0.062110 | +0.003850 | +6.61% |
| Prefilter | — | 0.052133 | +0.052133 | 신규 |
| H | 0.474099 | 0.135907 | −0.338192 | −71.33% |
| V | 0.128962 | 0.034821 | −0.094141 | −73.00% |
| Output | 0.111520 | 0.132848 | +0.021328 | +19.13% |
| **TOTAL** | **0.772840** | **0.417819** | **−0.355021** | **−45.94%** |

| Run | CURRENT TOTAL | A-R TOTAL |
|---|---:|---:|
| 1 | 0.837208 | 0.429793 |
| 2 | 0.704586 | 0.390428 |
| 3 | 0.776726 | 0.433236 |
| 평균 | **0.772840** | **0.417819** |
| min–max | 0.704586–0.837208 | 0.390428–0.433236 |
| 표본 표준편차 | 0.066396 | 0.023784 |

progress는 실제 애니메이션 중 약 0–1 범위를 왕복한 것을 로그에서 확인했다.
모든 offscreen task refresh rate는 1이다. half-rate/프레임 생략을 사용하지 않았다.

## D. Total GPU verdict

**큰 H의 절감이 추가 prefilter + 출력 샘플 비용을 충분히 상쇄한다.**

p=.20에서:

- H+V 절감: 약 **0.4023 ms/frame**
- 새 prefilter: **+0.0515 ms/frame**
- Output 증가: **+0.0285 ms/frame**
- Source 관측 증가까지 포함한 최종 절감: **0.3147 ms/frame**

기존 이론 filtering texture instructions는 약 **−70%**였고,
실제 Source/Output까지 포함한 **draw GPU 감소는 42–46%**다.
두 수치를 동일시하지 않는다. 전체 앱 frame time/TV 성능은 이번에 측정하지 않았다.

## E. CPU — steady / preparation 분리

GPU timer hook과 GL capture가 없는 별도 3회다.

| Run | CURRENT CPU ms/s | A-R CPU ms/s |
|---|---:|---:|
| 1 | 203.84 | 192.79 |
| 2 | 234.05 | 182.43 |
| 3 | 185.77 | 181.84 |
| 평균 | **207.88** | **185.69** |
| min–max | 185.77–234.05 | 181.84–192.79 |
| 표본 표준편차 | 24.39 | 6.16 |

평균상 약 **10.7% 낮지만**, CPU run 편차/범위가 겹치므로 안정적인 CPU 최적화율로
홍보할 수치는 아니다. 이번 제한 측정의 결론은 **+1 task로 인한 뚜렷한
steady process CPU 악화가 관찰되지 않았다**는 것이다.
이는 추가 task의 bookkeeping이 무료라는 뜻도, update thread만 빨라졌다는 뜻도 아니다.

매초 비용도 기록했다. 3회 평균:

| 왕복 구간 초 | CURRENT CPU ms/s | A-R CPU ms/s |
|---|---:|---:|
| 1 | 232.53 | 174.37 |
| 2 | 192.53 | 177.37 |
| 3 | 196.62 | 171.01 |
| 4 | 218.96 | 211.10 |
| 5 | 215.97 | 189.40 |
| 6 | 202.70 | 187.07 |
| 7 | 215.88 | 185.38 |
| 8 | 186.90 | 190.24 |

### One-time preparation에 대해 확인한 범위

- A-R 외부 installer 호출 자체: 평균 wall **0.825ms**, process CPU **0.823ms**
  (wall min–max .796–.860ms).
- CURRENT에는 이 교체 호출이 없으므로 “CURRENT 생성 0ms vs A-R .825ms”라고 비교하면 안 된다.
- Reveal 설정부터 준비 후 관찰까지 약 **3.760초**의 전체 process CPU는
  CURRENT **779.1ms**, A-R **743.4ms**였다.
- 이 구간에는 레이아웃/raster/FBO/shader 준비 외에도 **settling 동안 반복 렌더링한 CPU**가
  섞여 있다. pure setup/first-frame latency 또는 creation 개선값이 아니다.
- PoC는 CURRENT를 먼저 만든 뒤 A-R로 바꾼다. production에서 처음부터 A-R만 생성하는
  비용은 아직 측정되지 않았다. deferred GL compile/allocation도 .825ms 호출 시간 밖이다.

따라서 **steady 비용과 preparation 관측값은 분리했지만, 순수 production setup의
증감까지 입증한 것은 아니다.**

## F. Resource topology — 실제 task dimensions 확인

아래는 사용 중인 offscreen chain만의 수치다.

| 항목 | CURRENT | A-R |
|---|---:|---:|
| offscreen RenderTask | 3 | 4 |
| scene 기본 task 포함 | 4 | 5 |
| active FBO | 3 | 4 |
| active FBO color textures | 3 | 4 |
| Source | 2020×1180 | 2020×1180 |
| Prefilter | — | 2020×295 |
| H | 505×1180 | 505×295 |
| V | 505×295 | 505×295 |
| Output blurred-V fetch | 1 | 2 |

일반 text/metadata/gradient texture, window MSAA/swapchain은 위 texture count에 포함하지 않았다.
Output은 기본 scene의 renderer이며 별도 offscreen FBO가 아니다.
실제 task 로그의 A8 active payload는 각각 **3,128,475 / 3,277,450 bytes**다.
Gradient 1회에서도 각각 **12,513,900 / 13,109,800 bytes**를 확인했다.

### PoC의 inactive old H — 실제 관찰된 차이

A-R 적용 후 native GL에서 이전 **505×1180 H의 FBO와 texture가 모두 live**였다.
하지만 draw count는 새 H 한 번/frame뿐이다. 이전 H를 추가로 그리지는 않는다.

| offscreen color attachments 범위 | CURRENT | A-R |
|---|---:|---:|
| active FBO/texture | 3 / 3 | 4 / 4 |
| 관찰된 live FBO/texture, inactive old H 포함 | 3 / 3 | **5 / 5** |
| A8 크기 기반 live color payload | 2.9835 MiB | **3.6939 MiB** |

A-R의 old H가 **0.5683 MiB(A8)**를 추가로 보유한다.
실제 PoC의 color-payload 증가는 약 **+0.7104 MiB / +23.81%**이고,
선택된 topology만 생성한다고 가정한 production 설계의 net 증가는
**+0.1421 MiB / +4.76%**다. 두 값을 구분해야 한다.
이는 GL attachment 크기 기반 계산이며 실제 driver VRAM 측정은 아니다.

## G. Memory

### 1. 활성 topology의 논리 color payload

| 포맷 | CURRENT | A-R | 증가 |
|---|---:|---:|---:|
| A8 | 2.9835 MiB | 3.1256 MiB | **+0.1421 MiB (+4.76%)** |
| RGBA8 | 11.9342 MiB | 12.5025 MiB | **+0.5683 MiB (+4.76%)** |

새 prefilter는 595,900px이지만 H가 595,900→148,975px로 줄어,
net 증가는 quarter target 한 장인 **148,975px**다.
driver alignment/allocator, original text/metadata, CPU buffers, MSAA는 제외한다.

### 2. 계측 없는 process RSS

각 CURRENT/A-R 독립 실행 3회 평균 [min, max]. 단위 MiB.
ordinary는 같은 corpus의 Reveal 없는 Label scene이 안정화된 뒤다.
prepared는 Reveal/PoC 적용 후 추가 2.5초 대기한 상태다.

| 시점 | CURRENT | A-R |
|---|---:|---:|
| Ordinary scene | 123.965 [123.246,125.305] | 123.441 [123.297,123.609] |
| Prepared steady | **138.197 [138.137,138.234]** | **138.367 [138.223,138.539]** |
| 8초 animation 종료 | 138.201 [138.141,138.238] | 138.367 [138.223,138.539] |
| 각 process의 prepared−ordinary | 14.232 [12.914,14.891] | 14.926 [14.922,14.930] |
| 관찰 VmHWM 평균 | 140.025 | 139.569 |
| 3회 중 최고 VmHWM | 141.453 | 139.613 |

- prepared 절대 RSS 평균 차이: **+0.171 MiB**.
- 각 process baseline으로 보정한 평균 증가량의 차이: **+0.694 MiB**.
- ordinary 자체의 변동도 있으므로 .171MiB를 고정적인 per-Label 메모리 증가로 보장하지 않는다.
- RSS는 CPU 주소 공간에서 resident한 allocator/driver/GL object 등을 포함하며,
  위 FBO payload와 합산하거나 동일시할 수 없다.
- 관측된 준비 후→animation 종료 RSS 증가는 CURRENT 약 .004MiB, A-R은 이 정밀도에서 0이었다.
  단 한 번의 짧은 animation 관측이며 **누수 부재를 증명한 것은 아니다.**

### 3. 첫 워밍업의 높은 RSS

제외용 GPU 계측 warm-up에서 A-R prepared RSS는 **197.46MiB**,
종료 VmHWM은 **197.54MiB**였다. CURRENT warm-up의 VmHWM은 **139.71MiB**였다.
이후 GPU 3회 및 계측 없는 CPU/RSS 3회에서는 같은 수준의 상승이 재현되지 않았다.

shader compilation/cache/driver 초기화 등의 가능성은 있으나, 이번 범위에서는 원인을
분리하지 않았다. 계측 유무도 다르므로 “A-R은 항상 첫 생성에 +58MiB”라고 단정할 수 없다.
반대로 **첫 사용 peak가 문제없다고 결론내릴 수도 없다.**
상품화 설계 후 cold first-use 메모리 확인 항목으로 남긴다.
이를 위해 추가 캐시 삭제/장시간 반복/driver profiler를 수행하지 않았다.

## H. Optional RGBA sanity — 기존 Gradient 한 번씩

A8의 42–46% 개선을 확인한 후 기존 **Korean32, 12줄 red/green/blue gradient**
fixture로 8초 왕복을 CURRENT/A-R 각각 한 번만 실행했다.

| CURRENT total draw GPU | A-R total draw GPU | 감소 |
|---:|---:|---:|
| **0.736283 ms/frame** | **0.425833 ms/frame** | **42.16%** |

RGBA에서도 방향은 같았다. 1회 sanity이므로 A8보다 빨리/느리다는 포맷 간 비교나
정밀한 재현성 주장은 하지 않는다. RGBA stage matrix/CPU/RSS 반복 분석은 하지 않았다.
로그는 동일 측정기 출력 형식이지만 보고 결론은 total GPU 한 번 비교로 제한했다.

## I. 질문별 비용/효과 요약

1. **큰 H를 줄인 이득이 추가 비용을 상쇄하는가?**
   이 FHD/radius48 host fixture에서는 충분하다. total draw GPU **42–46% 감소**.
2. **Prefilter 자체는 싼가?**
   p=.20 **.0515ms**, 왕복 평균 **.0521ms**의 draw GPU다.
   큰 H에서 절감한 .314–.338ms보다 훨씬 작다.
3. **H 감소:** 고정 **68.4%**, 왕복 **71.3%**.
4. **V 감소:** 고정 **70.5%**, 왕복 **73.0%**.
5. **Output 추가 비용:** 고정 **+.0285ms**, 왕복 **+.0213ms**. 비용은 실제로 늘었지만 전체 이득을 뒤집지 않았다.
6. **+1 task의 CPU 문제:** 3회 process CPU에서 유의한 악화는 관찰하지 못했다.
   update/submission만 분리한 프로파일은 아니므로 추가 task 자체가 무비용이라고 하지는 않는다.
7. **메모리:** 활성 color payload 증가량은 작고, 계측 없는 steady RSS 차이도 작았다.
   다만 PoC의 old H와 첫 워밍업 RSS 예외는 별도 검토가 필요하다.

## J. Verdict

**A-R PERFORMANCE STRONGLY PROMISING — READY FOR PRODUCTION DESIGN**

“+1 RenderTask, net +1 quarter-size color target, Output +1 V fetch”를 지불해도
현재 큰 H/V 비용을 줄이는 이득이 명확하다. 최소 feasibility 목표에는 충분하다.

바로 production replacement를 작성하거나 commit하지 않았다.
다음 단계에서 검토할 조건은:

- 기존 품질 보고서의 p=.75 softness trade-off를 UX에서 수용할지.
- 처음부터 선택한 topology만 생성해 old H를 보유하지 않는 설계.
- production 형태의 cold first-use CPU/peak RSS 확인.
- 이후 별도 scope에서 packed PER_LINE/odd dimensions/기능 호환성 등 검증.

이번에는 위 항목을 구현/확장하지 않았다.

## K. Working tree / 재현 자료

- production source unchanged; UI HEAD/branch/worktree unchanged.
- 기존 adaptor 수정 preserved.
- commit/amend/push/reset/restore/stash 없음.
- quality/sigma/delta tuning, Y/2, PER_LINE, 다른 radius, target deployment 없음.
- production build/full regression/sanitizer/leak stress 없음.
- 이전 diagnostic/report를 수정하지 않았다.

파일:

- bench.cpp (로컬 자료: `bench.cpp`): 동일 fixture/타이밍, 기존 A-R installer 호출.
- meter.cpp (로컬 자료: `meter.cpp`): draw GPU query와 관측된 FBO 정보.
- run.py (로컬 자료: `run.py`): 제한된 반복/실행 순서.
- summary.json (로컬 자료: `summary.json`): run별 원시 집계와 평균/min/max/stddev.
- [MEASURED_TABLES.md](MEASURED_TABLES.md): 계산된 표.
- `gpu-0/1/2-0/1.log`: 최종 A8 GPU 원시 로그.
- `cpu-0/1/2-0/1.log`: 계측 없는 CPU/RSS 원시 로그.
- `warm-0/1.log`: 제외용 warm-up 및 최초 RSS 예외.
- `rgba-0/1.log`: 선택적 Gradient 한 번씩.

숫자를 다시 집계만 하려면:

```bash
/home/bowonryuubuntu/tizen/reveal-perf-opt.upcgEj/venv/bin/python \
  /home/bowonryuubuntu/tizen/reveal-ar-feasibility.wIhiCC/analyze.py
```

이 명령은 저장된 로그를 읽을 뿐 새 성능 측정을 하지 않는다.
