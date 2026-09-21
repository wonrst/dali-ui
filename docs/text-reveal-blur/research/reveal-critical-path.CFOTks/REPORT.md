# Reveal Blur critical-path attribution

## 결론

**TARGET CRITICAL-PATH TRACE READY — 계측 소스/호스트 smoke 검증 완료.**
타겟 빌드·로드·overhead 및 실제 병목은 아직 검증하지 않았다.

**로컬 계측도 가능하며, 이미 production과 Source-only를 로컬에서 수집했다.**
당장 Core/Adaptor까지 타겟 재빌드할 필요는 없다. 아래 로컬 단서를 먼저 볼 수 있다.
다만 TV의 frame drop 원인을 확정하려면 결국 같은 구간의 TV 데이터가 필요하다.

새 blur 구현/최적화는 없다. 모든 신규 출력과 trace 이름에 `RYU - `를 사용한다.
render hot path에서 console/file 출력 없이 메모리에 기록하고 정상 종료 시 저장한다.

## 1. 로컬에서 확인한 단서

Ubuntu / GTX1650 / NVIDIA595.91.07 / GLES / 1280×720 / MSAA4.
같은 샘플 동작, Strong, Sync, PERFORMANCE. 자동화는 기존 Primary/Back action을
호출했으며 animation/layout/transition 구현은 그대로다.

**이 표는 계측 sanity 중 수집한 첫 진입/퇴장 각 1회의 참고값이다.**
기존 빌드의 다른 object를 재사용한 private mixed-object 호스트 빌드이므로
정식 성능 벤치마크, 반복 통계, 타겟 비용 예측으로 쓰면 안 된다.
두 비교본의 계측은 동일하다. Core/Adaptor도 동일한 private library를 사용했다.

아래는 해당 phase의 프레임별 **thread CPU time 중앙값(ms)**이다.

| 구간 | production | Source-only |
|---|---:|---:|
| Cards 진입: Update | 1.000 | 0.699 |
| Cards 진입: offscreen RenderScene | 4.596 | 1.542 |
| Cards 진입: GLES command processing | 3.908 | 1.541 |
| Cards 진입: 전체 update/render iteration | 7.344 | 3.495 |
| 정상 퇴장: Update | 0.747 | 0.450 |
| 정상 퇴장: offscreen RenderScene | 4.829 | 1.444 |
| 정상 퇴장: GLES command processing | 3.773 | 1.441 |
| 정상 퇴장: 전체 update/render iteration | 9.776 | 3.067 |

진입 프레임 수는 210/209, 퇴장은 25/25이다. **행들은 중첩되므로 더하면 안 된다.**
offscreen RenderScene에는 instruction 처리/메시지/API 제출이 포함된다.
GLES command processing CPU 시간은 DALi 코드와 그 안에서 실행되는 driver의
CPU 시간을 포함하지만 GPU execution time은 아니다.

현재 로컬 단서는 **Update만의 차이보다 render/command 경로의 CPU 차이가 크다**는 것이다.
그러나 이것으로 TV의 주 병목을 확정하거나 새 최적화를 바로 시작하지 않는다.

frame loop wall time은 production 진입 중앙값16.674ms, Source-only6.014ms였다.
후자의 나머지 시간은 loop 바깥 pacing sleep 등에 쓰인다. 따라서 이를
“Source-only가 166fps로 렌더링한다”거나 “프레임 시간이 64% 개선됐다”로 읽으면 안 된다.
두 실행 모두 FPS 중앙값은60이었다.

### 늦은 프레임을 구분할 수 있는가?

production 첫 Cards 진입에 >25ms frame 하나가 있었다:

| Frame | loop wall | loop CPU | Update CPU | Offscreen CPU | Swap wall | Event setup overlap |
|---|---:|---:|---:|---:|---:|---:|
| 554 | 2.215 | 2.215 | 0.415 | 1.613 | 미관측 | 1.760 |
| 555 | 25.889 | 25.889 | 10.459 | 13.074 | 0.268 | 0 |
| 556 | 7.834 | 7.825 | 2.183 | 4.602 | 0.058 | 0 |

이 예는 setup와 겹친 Event 작업의 **다음** update/render frame이 길어질 수
있음을 보여준다. overlap=0을 곧바로 steady frame으로 간주하면 안 된다.
이 frame에 Shader.Create marker는 없었다. GPU 시간은 여전히 unknown.
직전/직후 frame까지 연결해서 읽어야 한다는 도구 검증 사례이지 TV 원인 확정이 아니다.

## 2. 왜 UI만 계측하지 않았나?

UI만으로 preparation/publication/companion setup은 볼 수 있지만,
Animation·constraints·PrepareRender는 Core, graphics submission·swap은
Adaptor 안에서 실행된다. CPU critical path를 구분하려면 그 경계가 필요하다.

기존 기반은 그대로 활용했다:

| 시설 | 실제 경로 / 조건 | 판단 |
|---|---|---|
| Trace API | Core `integration-api/trace.h/.cpp`, `ENABLE_TRACE=ON` | Filter + TLS callback + begin/end 재사용 |
| Frame/Update/Render | Adaptor `combined-update-render-controller-debug.h`, `DALI_TRACE_COMBINED=1` | 기존 frame iteration/sleep 경계 재사용 |
| Animation/Renderers/Tasks | Core `update-manager.cpp`, `DALI_TRACE_UPDATE_PROCESS=1` | 기존 aggregate scope 재사용 |
| GLES queues | Adaptor `egl-graphics-controller.cpp`, `DALI_TRACE_EGL=1` | 생성/폐기/upload/명령 처리 재사용 |
| TV backend | `trace-manager-impl-tizen.cpp` | TV에서는 ttrace 호출 제외; print는 per-event dlog이므로 사용 안 함 |
| Swap | `egl-implementation.cpp` | 기존 trace가 TV/Ubuntu에서 제외됨; 진단 scope만 추가 |
| Streamline | 별도 `ENABLE_TRACE_STREAMLINE` backend | 외부 collector와 target 지원 미확인, 새 의존성 추가 안 함 |
| Kernel/performance logger | `performance-server.cpp`, `kernel-trace.cpp` | mutex/string formatting 및 tracefs 권한 필요, 이번 primary sink로 쓰지 않음 |

새 수집기는 기존 `Trace::LogContext`에서 선택된 event만 받아 저장하는
diagnostic sink다. 공개 API/ABI, trace callback API, CMake/spec은 변경하지 않았다.
`RYU_REVEAL_ATTRIBUTION && TRACE_ENABLED && __linux__`일 때만 수집 코드가 들어간다.

## 3. 추가한 정확한 경계

- Event: Core::ProcessEvents 전체, Reveal.Prepare/Publish/Construct/Attach.
  Prepare는 async에서 worker에서도 호출될 수 있다. 이번 실행은 Sync다.
- Update: actor node update의 aggregate scope 하나. 개별 constraint 계측 없음.
  UpdateRenderers는 renderer constraints와 PrepareRender를 함께 포함한다.
- Render: offscreen RenderScene / window RenderScene. 개별 task 이름·페이지 정보 없음.
- Adaptor: EGL.Swap/SwapDamage, Shader.Create. 후자는 cache load/compile/link 등을
  포함하며 compile-only marker가 아니다.
- Demo: CardsStart/CardsReady, RESULTS_READY에서의 ExitStart, 기타 ExitStart,
  layout EXIT 완료. marker만 추가; timer/animation/callback 순서는 변경하지 않았다.

H/V별 독립 CPU duration이나 per-frame task count는 새로 수집하지 않는다.
기존 task actor 이름은 Core render 단계의 안정적인 category API가 아니며,
이를 위해 task별 mapping/새 hot-path 문자열 처리를 추가하지 않았다.
Source/H/V의 **전체 offscreen bundle 차이**까지가 이번 계측의 범위다.
기존 명령-buffer trace는 버퍼별 duration을 제공하지만 draw별 신규 계측은 없다.

## 4. 수집 방식 / overhead

- `CLOCK_MONOTONIC` + `CLOCK_THREAD_CPUTIME_ID`: 경계당 clock 두 번.
- 스레드 로컬 frame sequence: 기존 DALI_UPDATE_RENDER 시작마다 증가.
  Event/worker의 frame ID=0은 render frame과 동일 ID라는 뜻이 아니다.
- 고정 이름과 64-depth pending stack; 동적 message 문자열은 저장하지 않는다.
- 스레드당 최대262144 duration records, 현 구성에서 **10MiB**의 진단 전용
  버퍼. 이번 Sync 실행은 Event/Render 두 개, 총20MiB. 일반 blur 메모리 수치와 별개다.
- 첫 선택 event에서 한 번 할당/초기화한다. 이 진단 startup 비용을 warm setup
  비용으로 읽지 않는다. 새 스레드가 trace를 내면 별도 버퍼가 생긴다.
- 가득 차면 overwrite하지 않고 drop을 센다. 정상 종료 시 thread별 파일을 저장한다.
- I/O/printf는 종료 시에만 한다. Hot path에 atomic, GPU wait/readback, flush 없음.
- 기존 TRACE message-generator의 `ostringstream` 비용은 남는다. 같은 patch라도
  production은 명령 버퍼가 많아 event 수가 더 많으므로 **target OFF/ON sanity가 중요**하다.

### Host sanity

| 실행 | 결과 | FPS logger 중앙값 | logger 범위 |
|---|---|---:|---:|
| production trace OFF | 진입·정상 퇴장 완료, trace 파일 없음 | 60.00 | 50.96–60.11 |
| production trace ON | 완료, render941frames | 60.00 | 50.85–60.58 |
| Source-only trace ON | 완료, render944frames | 60.00 | 53.92–60.00 |

ON record 수: production Event401 / Render38733,
Source-only Event401 / Render27042. 모두 dropped/unmatched/open/allocation_failed/
clock_failed=0. 한 번씩의 quick check이며 target overhead 상한이나 성능 비교가 아니다.
최초 수집기 검증 run(구 format v1)은 별도로 보존했으며 위 표는 최종 v2 ON 결과다.

## 5. 사용 방법

타겟 절차는 [TARGET.md](TARGET.md), 구현 전 계획은 [PLAN.md](PLAN.md).
현재 사용자의 package build 방식에서는 타겟 계측을 하기로 결정했을 때
Core/Adaptor도 계측본으로 함께 빌드·설치해야 한다. **지금 당장 할 필요는 없다.**

로컬에서 재확인하려면 다음을 **하나씩 순서대로** 실행한다:

```bash
bash /home/bowonryuubuntu/tizen/reveal-critical-path.CFOTks/run-host.sh off
bash /home/bowonryuubuntu/tizen/reveal-critical-path.CFOTks/run-host.sh production
bash /home/bowonryuubuntu/tizen/reveal-critical-path.CFOTks/run-host.sh source-only
```

앱이 자동으로 진입/퇴장하고 종료한다. 설치된 라이브러리를 바꾸지 않고
`LD_LIBRARY_PATH`로 이 디렉터리의 private 계측본을 선택한다.
이것은 호스트 smoke용이며 정식 성능 평가에는 같은 설정의 전체 빌드가 필요하다.

이미 수집된 결과:

- production raw: `/tmp/ryu-production.NYrjMO/`
- Source-only raw: `/tmp/ryu-source-only.aBsdYX/`
- OFF log: `/tmp/ryu-off.Fy24UO/app.log`
- production timeline/frames (로컬 자료: `host-production-final-analysis/`)
- Source-only timeline/frames (로컬 자료: `host-source-only-analysis/`)

`timeline.json`은 Chrome trace/Perfetto의 JSON importer에서 열 수 있는
duration 형식이다. 이 연결은 offline export이며 target Perfetto daemon
통합이 아니다. `frames.csv`, `late-frames.md`, `summary.json`도 함께 제공한다.

## 6. 한계 / 다음 판단

1. GPU execution, 실제 scanout/display 완료 시각은 **unknown**.
2. wall−threadCPU는 non-running 시간. blocked/runnable/vsync/GPU backlog를
   구분하려면 scheduler/GPU trace가 추가로 필요하다.
3. Event와 frame의 연결은 동일 monotonic clock상의 overlap/인접 관계다.
   Event publication에서 정확히 어느 present까지 이어졌는지의 flow ID는 아니다.
4. Nested/parallel duration을 합산하면 안 된다. parser는 같은 category의
   중첩 command scopes를 wall/CPU clock 각각의 interval union으로 처리한다.
5. frame loop는 pacing sleep을 제외한다. frame-start/swap-return 간격은
   pacing/idle/user 대기까지 포함할 수 있으므로 active phase에서 읽어야 한다.
6. normal exit가 필요. kill/crash에 대한 별도 dump/signal handler는 만들지 않았다.
7. 타겟은 연결되어 있지 않았고, 타겟 build/load/실행은 수행하지 않았다.

최종 target verdict는 **INSUFFICIENT TRACE DATA**를 유지한다.
현재 추천은 **로컬 단서를 검토한 뒤, 필요하면 동일 계측으로 target 두 경로를 수집**하는 것 하나다.
이 자료만으로 2-stage나 constraints/camera/Gaussian 변경을 시작하지 않는다.

## 7. Git / 검증 상태

- 원본 UI HEAD05087317, Core f43e95be4, Adaptor dcadcfdc3 유지.
- UI4파일 + Core3파일/신규 collector1파일 + Adaptor3파일에 진단 변경만 unstaged.
- 생산/Source-only에 동일 UI patch 적용 확인. 현재 작업 트리와 detached 계측
  소스가 일치함을 확인했다. 셰이더/FBO/Task/geometry/constraints/refresh/timing 변경 없음.
- private host 빌드 성공, 실제 샘플 진입/퇴장 OFF/ON 검증, parser/shell syntax 확인.
- 전체 production build/UTC/성능 benchmark/target 측정은 하지 않았다.
- commit/amend/push/rebase/reset/restore/stash 없음. 기존 설치 라이브러리 변경 없음.
- 이전 분석 디렉터리/결과는 그대로 보존했다.
