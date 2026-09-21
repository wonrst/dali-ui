# Target Reveal Blur — production vs Source-only attribution

## 결론

**계측된 실행의 주 차이는 offscreen Render / GLES command processing의 CPU 시간이다.**
**전환 직후의 resource setup spike도 별도로 크다.**

- 지속적인 Animation/Constraint/Update 계산량이 주원인이라는 근거는 없다.
- 초기 생성만 느린 것도 아니다. 새 Reveal 생성이 없는 진입 1–3초에도 차이가 유지된다.
- EGL swap에서 오래 잠드는 현상만으로 설명되지 않는다. production은 CPU 실행 시간 자체가 16.67ms를 초과한다.
- 다만 command processing에는 DALi와 graphics driver가 모두 포함된다. 어떤 GL 호출/driver 작업이 비싼지, GPU backlog가 영향을 주는지는 이 trace로 분리할 수 없다.
- **GPU 시간은 unknown.** Gaussian GPU 계산이 주범이라고 결론내리지 않는다.

분류: **RENDER / DRIVER-SIDE CPU COST + RESOURCE SETUP SPIKE**.
이는 계측본의 attribution이며, 무계측 production의 정확한 비용 또는 최종 root-cause 함수 확정은 아니다.

## 1. 입력과 수집 품질

사용자가 전달한 target 파일만 오프라인 분석했다. 새로운 앱 실행/벤치마크는 하지 않았다.

| 입력 | PID / Event TID / Render TID | Event / Render records | render iterations | 수집 기간 |
|---|---|---:|---:|---:|
| `ryu-production` | 3230 / 3230 / 3236 | 2,660 / 171,819 | 4,560 | 97.55s |
| `ryu-sourceonly` 주 비교 | 4013 / 4013 / 4019 | 2,463 / 150,175 | 5,067 | 85.96s |
| `ryu-sourceonly` 보조 실행 | 3557 / 3557 / 3563 | 643 / 43,391 | 1,531 | 26.90s |

모든 파일에서 `dropped=unmatched=open=allocation_failed=clock_failed=0`.
Source-only에는 두 프로세스가 있으므로 합치지 않았다. `app.log`도 마지막 PID4013에 대응한다.
두 app.log 모두 GLES 사용을 기록한다. FPS 숫자/trace-OFF 결과는 포함하지 않는다.

빌드 기준은 기존 [전달 보고서](../reveal-trace-tizen101.prtWoU/REPORT.md)와 같다.
production은 Source→H→V, Source-only는 H/V를 제거한 diagnostic이다.
**Source-only는 blur 품질을 제공하는 대체 구현이 아니다.**

로그 자체에는 설치 RPM hash, 창 크기, quality/radius 버튼 선택 기록이 없다.
아래 비교는 전달한 두 RPM을 같은 설정으로 실행했다는 전제다.
기본 샘플 설정은 PERFORMANCE / Strong / Sync이며, 반복별 Prepare/Publish 호출 수와 phase 패턴도 일치한다.
이것이 런타임 설정의 완전한 증명은 아니다.

## 2. 구간 선정과 계산

주 비교는 양쪽 모두 새 Cards 진입 **4회**, 각 진입 뒤의 첫 정상 Cards 퇴장 **4회**다.
production은 그 밖에 Cards 재방문 후 퇴장 5회, Source-only는 4회가 있어 별도로 집계했다.
전체 실행 시간을 통째로 나누면 사용자 조작/대기 차이가 섞이므로 그렇게 비교하지 않았다.

- 진입 전체: `Demo.CardsStart` 시작 → `Demo.CardsReady` 시작, 약 3.59–3.62초.
- 진입 중간: 양쪽 동일하게 CardsStart 후 **1–3초**. 이 구간에는 Reveal Prepare/Publish/Construct/Attach 및 Shader.Create가 없다. 기존 장면의 다른 작업까지 완전히 없다는 뜻은 아니다.
- 퇴장 전체: 해당 CardsReady 다음 첫 `Demo.ExitStart` → 다음 `Demo.ExitFinished` 시작.
- 퇴장 후반: 양쪽 동일하게 ExitStart 후 **300ms → ExitFinished**. 초기 resource spike 이후이며 Reveal 생성/셰이더 생성 기록이 없다.
- 첫 회와 2–4회 warm 반복을 별도로 확인했다. 평균은 해당 구간의 프레임을 합친 frame-weighted mean이다.

프레임은 `DALI_UPDATE_RENDER` 시작이 구간 안에 있는 것으로 선택하고 그 프레임의 전체 duration을 사용했다.
따라서 마지막 프레임은 구간 끝을 조금 넘을 수 있다. 프레임/swap 간격 통계는 양 끝이 같은 구간 안에 있을 때만 포함했다.
퇴장 후반은 production 27 / Source-only 42프레임으로 표본이 작다. 진입 중간 269 / 480프레임이 지속 비용의 주 근거다.

CPU는 `CLOCK_THREAD_CPUTIME_ID`, wall은 `CLOCK_MONOTONIC`이다.
loop wall은 바깥 pacing sleep을 제외한다. wall−CPU는 선점/대기 등의 **non-running 시간**이지 GPU 시간은 아니다.
중첩 command scopes는 interval union으로 중복을 제거했다.
`offscreen commands`는 offscreen RenderScene 안에 완전히 포함된 ProcessCommandBuffer scopes만 사용했다.
표의 부모/자식 행은 합산하면 안 된다. Render 스레드 CPU는 전체 프로세스 CPU가 아니다.

## 3. 지속 프레임 비용

### 진입 중간 1–3초, 4회 합산

단위: 프레임당 평균 ms. CPU와 wall을 구분했다.

| 구간 | production | Source-only | production 추가 비용 |
|---|---:|---:|---:|
| 전체 Update/Render **CPU** | **23.69** | **9.17** | **+14.52** |
| Update CPU | 1.20 | 0.87 | +0.32 |
| └ Animation CPU | 0.035 | 0.033 | +0.002 |
| └ Nodes CPU | 0.078 | 0.053 | +0.025 |
| └ Renderer update/PrepareRender CPU | 0.325 | 0.245 | +0.080 |
| └ RenderTask preparation CPU | 0.216 | 0.127 | +0.089 |
| **Offscreen Render CPU** | **18.22** | **5.40** | **+12.82** |
| └ **Offscreen command processing CPU** | **16.62** | **4.81** | **+11.81** |
| Window Render CPU | 2.52 | 1.97 | +0.55 |
| └ Swap CPU | 0.53 | 0.44 | +0.09 |
| Swap wall | 0.86 | 0.78 | +0.08 |
| 전체 loop wall | 29.78 | 11.12 | +18.66 |
| 전체 loop non-running | 6.09 | 1.95 | +4.14 |
| 같은 구간 내 frame-start interval | 29.79 | 16.67 | — |

Offscreen CPU 차이 +12.82ms는 전체 CPU 차이 +14.52ms의 약88%다.
그 안쪽의 command processing 차이만으로도 약81%에 해당한다.
이는 관측한 구간별 차이의 구성이지, H/V를 최적화하면 반드시 그만큼 개선된다는 예측은 아니다.

production은269/269프레임에서 CPU18ms 초과. Source-only는480/480프레임에서 CPU18ms 이하다.
여기에는 큰 초기 resource 생성 spike가 없으므로 setup만으로 설명할 수 없다.

요약하면, **매 프레임 offscreen 명령을 처리하는 비용이 훨씬 크며,
Animation/Constraint 수를 줄이는 것만으로 이 차이를 해소하기는 어렵다.**

반복별 전체 CPU / offscreen CPU 평균:

| Cards 진입 회차 | production CPU / offscreen | Source-only CPU / offscreen |
|---|---:|---:|
| 1 | 22.93 / 17.04 | 8.51 / 4.91 |
| 2 | 23.47 / 18.15 | 9.19 / 5.38 |
| 3 | 23.93 / 18.55 | 9.42 / 5.60 |
| 4 | 24.43 / 19.11 | 9.57 / 5.70 |

양쪽 모두 반복 중 비용이 조금 증가하지만, 양쪽의 큰 차이는 모든 반복에서 유지된다.
CPU 주파수/열/다른 프로세스 등은 수집하지 않았으므로 이 작은 증가의 원인은 확정하지 않는다.

### 퇴장 후반, 4회 합산

| 프레임당 평균 ms | production | Source-only |
|---|---:|---:|
| 전체 Update/Render CPU | **22.95** | **9.34** |
| Update CPU | 0.94 | 0.56 |
| Offscreen Render CPU | **17.84** | **5.96** |
| └ Offscreen command processing CPU | **16.21** | **5.32** |
| Window Render CPU | 2.36 | 1.85 |
| Swap wall | 0.90 | 0.76 |
| 전체 loop wall | 29.28 | 12.13 |
| 전체 loop non-running | 6.33 | 2.79 |

새 resource가 만들어지는 순간을 지난 뒤에도 진입과 같은 방향이다.
이 구간의 offscreen command scope 호출 수는 프레임당 **57 vs 19**다.
이는 중첩을 포함한 명령 버퍼 처리 scope 횟수이며, 실제 task/FBO/draw 개수와 동의어가 아니다.
H/V bundle 차이와 일관된 관측이지만 task 개수 하나를 단독 원인으로 확정하지 않는다.

## 4. 전환 직후의 별도 setup spike

정상 Cards 퇴장 4회마다 큰 render iteration이 하나씩 있다. 첫 실행만의 문제가 아니다.

| 회차 | production frame / wall / CPU ms | Source-only frame / wall / CPU ms |
|---|---:|---:|
| 1 | 779 / 141.97 / 117.38 | 843 / 54.60 / 42.70 |
| 2 | 1798 / 146.81 / 124.27 | 2041 / 48.88 / 42.72 |
| 3 | 3024 / 148.93 / 129.57 | 3212 / 54.11 / 46.71 |
| 4 | 4046 / 146.87 / 126.00 | 4491 / 59.09 / 47.17 |

그 spike 프레임들의 평균:

| 항목 | production | Source-only |
|---|---:|---:|
| loop wall | **146.15ms** | **54.17ms** |
| loop CPU | 124.31ms | 44.82ms |
| Update CPU | 21.48ms | 17.13ms |
| Offscreen Render CPU | 97.34ms | 24.02ms |
| Offscreen command processing CPU | 41.47ms | 9.92ms |
| Resource create queue CPU | **51.26ms** | **10.76ms** |
| Texture update CPU | 17.41ms | 15.97ms |

위 scopes도 일부 중첩되므로 합산하면 안 된다.
`ProcessCreateQueues()`는 texture / buffer / framebuffer 생성을 묶어서 처리한다.
따라서51.26ms를 FBO 생성만의 시간이나 메모리 할당만의 시간이라고 할 수 없다.
이 phase에는 `Shader.Create`가 없으므로 관측한 spike를 shader compile 때문이라고 볼 근거는 없다.

Event 쪽 `Demo.ExitStart`도 평균 **wall84.64 / CPU73.65ms**
(Source-only **wall73.24 / CPU64.04ms**)가 걸린다.
UI 설정·기존 장면 처리·Reveal 준비/공개 등을 포함하며, blur 생성 함수만의 시간은 아니다.
이는 위 render spike와 다른 스레드의 구간이므로 단순 합산해 입력 지연이라고 보고하지 않는다.

예: 두 번째 퇴장, ExitStart를 기준으로 한 상대 시각.

```text
production:
  t=  0.0–90.5ms : Event 쪽 ExitStart
  t= 11.6–78.3ms : 이전 상태를 처리하는 가벼운 render iterations의 시작 시각
  t= 95.0–241.8ms: 새 resource 관련 무거운 iteration (CPU124.27ms)
  t=241.8ms 이후 : 매 프레임 CPU 약23ms의 지속 렌더링

Source-only:
  t=  0.0–79.1ms : Event 쪽 ExitStart
  t= 80.9–129.8ms: resource 관련 iteration (CPU42.72ms)
  t=129.8ms 이후 : 매 프레임 CPU 약9–10ms의 지속 렌더링
```

동일 monotonic clock으로 연결한 인접 관계다. Publication→특정 present의 flow ID는 없으므로 정확한 input-to-display latency를 뜻하지 않는다.
샘플의 퇴장 animation 상수는 **0.40초**다. phase marker 간 0.47–0.51초는 사용자 애니메이션 duration 자체가 아니다.

Prepare/Publish 호출 수는 진입마다 양쪽 17회, 정상 퇴장마다 19회로 동일했다.
각각 함수 진입 횟수이지 unique Label 수나 성공한 publication 수의 증명은 아니다.
진입 Prepare CPU 합은 회당 production25.2–26.4ms / Source-only24.3–26.1ms로 비슷하다.
퇴장 Prepare는 양쪽 회당 약10ms다. CPU raster 준비량 차이만을 주 병목으로 보기 어렵다.

## 5. 전체 구간과 보조 실행

아래는 setup/초기 전환 프레임까지 포함하므로 앞의 중간 구간과 구별한다.

| 구간 | production frames / 평균 loop CPU | Source-only frames / 평균 loop CPU |
|---|---:|---:|
| 진입 전체 4회 | 471 / 24.34ms | 855 / 9.20ms |
| 진입 2–4회만 | 360 / 24.62ms | 641 / 9.41ms |
| 정상 퇴장 전체 4회 | 63 / 23.06ms | 107 / 9.56ms |
| 재방문 Cards 퇴장 | 83 / 22.65ms (5회) | 106 / 9.67ms (4회) |

Source-only PID3557의 1회 진입 중간 CPU8.63ms / offscreen5.00ms,
퇴장 후반 CPU8.42ms / offscreen5.13ms로 주 실행과 같은 방향이다.
반복 수/실행 길이가 달라 주 비교 통계에 섞지 않았다.

전체 구간의 frame-start/swap-return 간격은 JSON/CSV에 있다.
이는 실제 화면 scanout FPS나 GPU 완료 시각이 아니므로 `1000 / loop wall`을 FPS로 보고하지 않는다.

## 6. 다음 작업 방향과 한계

이번 자료가 지지하는 우선순위:

1. **Offscreen ProcessCommandBuffer 내부 CPU 비용을 우선 좁힌다.**
   기존 코드의 render-pass/FBO 전환, draw 제출, texture dependency/sync, buffer/uniform 처리 중 어디가 비싼지 구별해야 한다.
   현재 scope만으로 특정 호출이나 driver 버그를 지목할 수 없다.
   이 구간에 대한 target CPU sampling 또는 제한된 duration 분해가 다음 근거가 된다.
2. **퇴장 시 resource 생성/첫 submission spike를 별도 과제로 본다.**
   CPU51ms의 create queue와 command CPU41ms가 반복해서 나타난다.
   재사용/생성 분산의 효과와 메모리/lifecycle 비용을 검토할 근거는 있지만, 지금 바로 cache를 넣을 근거는 아니다.
3. Gaussian tap/filter 품질을 다시 희생하거나 constraint 구조를 크게 바꾸는 일은 우선하지 않는다.
   Source-only는 H/V bundle와 output을 함께 바꾸므로 GPU Gaussian 단독비용을 분리한 비교가 아니다.

이전 H/V 1-tap에서 target 개선이 작았다는 관측과도 방향이 일치하지만,
이번 데이터에 1-tap 실행은 없으므로 새 정량값으로 재사용하지 않았다.

중요한 제한:

- **Target trace OFF/ON 비교가 전달되지 않았다.** 계측 코드가 같아도 production은 command scopes가 더 많아 계측 오버헤드도 더 클 수 있다. 새 production 변경 전 간단한 OFF/ON sanity가 필요하다.
- 고정 trace buffer는 스레드당 약10MiB이고 이번 프로세스에는 두 스레드가 기록했다. 일반 blur의 CPU/GPU memory 측정과는 별개다.
- GPU execution, CPU 주파수/스케줄러 상태, VRAM/RSS, GL 호출별 비용은 없다.
- CPU time에는 driver 내부에서 CPU를 쓰는 처리/대기가 포함될 수 있다. CPU 차이를 곧바로 순수한 DALi 알고리즘 연산량으로 해석하지 않는다.
- 이번 report는 render/driver 쪽 비용 위치를 좁힌 결과이며, target에서의 유일한 root cause 또는 해결책을 확정한 결과가 아니다.

## 7. 산출물 / 재현 / 보존

- 집계 전체 (로컬 자료: `comparison.json`): 회차/구간별 mean/median/p95/max, setup scopes, 늦은 프레임.
- production 프레임 CSV (로컬 자료: `production-3230/frames.csv`), Source-only 프레임 CSV (로컬 자료: `sourceonly-4013/frames.csv`).
- production timeline (로컬 자료: `production-3230/timeline.json`), Source-only timeline (로컬 자료: `sourceonly-4013/timeline.json`): offline Chrome trace 형식.
- 비교 스크립트 (로컬 자료: `compare.py`). 추가 분석은 `python3 compare.py`로 재생성 가능하다.
- `inputs/`는 원본 trace를 가리키는 symlink다. 전달 trace/app.log를 수정하지 않았다.

기존 parser로 PID별 JSON/CSV를 만든 다음 비교 스크립트를 실행했다.
`comparison.json`의 Event scope 합계는 start가 구간 안에 있는 scope의 전체 duration이므로 경계 밖까지 연장될 수 있다.
보고서의 Event 비용은 명시한 Demo.ExitStart 자체를 사용했고, Event.Process 합계를 구간 전체 CPU라고 보고하지 않았다.

UI/Core/Adaptor production 코드·기존 진단 코드·git index/history는 변경하지 않았다.
새 build, UTC, target 실행, GPU/메모리 측정, commit/push는 하지 않았다.

### 원본 SHA-256

```text
a86bb80e753ba4b126790454d63a737abc2b9eceb99750dc4bf91ddd1c4236ed  production/capture.3230.3230.trace
f84c2f2fbf3a8792af6423d51fa2802608551350c8c019163503388f06a2ab12  production/capture.3230.3236.trace
8f745c7ef02a4f1c3e7fc272875948275a9e97e9ea2943fe3a9869b84c210934  sourceonly/capture.3557.3557.trace
89f23aba4ef785951df710daef224ca7174e2d79e5fdebf3f5ffa7d356564199  sourceonly/capture.3557.3563.trace
8baba0acc9a5957f714955c20c82f1a468f3aaf42d41171552b3bff71da33bfb  sourceonly/capture.4013.4013.trace
d218d6fc1e7095cd2205791a9e6ec45efac211232223660d87e60f91a1ccefb2  sourceonly/capture.4013.4019.trace
```
