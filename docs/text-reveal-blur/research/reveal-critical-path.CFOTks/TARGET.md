# Target 실행 절차

이 도구는 GPU 시간을 재지 않는다. 새 blur 경로도 아니다.
`run-trace.sh`의 production/source-only 인자는 **기록 이름**이다.
실제 비교 경로는 아래 두 revision으로 빌드한 UI 라이브러리로 선택한다.

## 1. 준비된 소스

이 디렉터리의 `trees/`에 detached worktree를 준비했다.

| 디렉터리 | 기반 | 역할 |
|---|---|---|
| core | f43e95be4 | 공통 trace 수집기 + Event/Nodes 경계 |
| adaptor | dcadcfdc3 | 공통 offscreen/window/swap/Shader.Create 경계 |
| production | b54bb666 | 원래 PERFORMANCE + 계측 |
| source-only | 05087317 | Source-only + **동일한** UI 계측 |

기존 checkout/branch/commit은 바꾸지 않았다. 원본 checkout에도 계측 diff를
unstaged로 남겼다. `trees/*.patch`는 전달용이다. core에는
`core.patch`와 신규 파일을 담은 `collector.patch`가 모두 필요하다.

다른 작업 디렉터리가 필요하면:

```bash
bash /home/bowonryuubuntu/tizen/reveal-critical-path.CFOTks/prepare-builds.sh /ABSOLUTE/NEW/DIRECTORY
```

이미 존재하는 디렉터리는 거부하며 reset/restore/stash/rebase를 하지 않는다.
스크립트는 실행 당시 진단 파일의 diff를 내보내므로, 이후 같은 파일에 다른
수정이 생겼다면 먼저 diff를 검토한다.

## 2. 빌드 / 설치

**두 UI 빌드에 같은 Core/Adaptor, 툴체인, 최적화 옵션을 사용한다.**
Target 아키텍처/패키징/설치 경로는 연결된 장비가 없어 검증하지 못했다.
호스트의 `host/` 라이브러리를 TV에 복사하면 안 된다.

기존 TV 빌드 절차에서 추가할 것은 다음뿐이다:

```text
Core, Adaptor, UI CMake: -DENABLE_TRACE=ON
Core, Adaptor, UI C++ flags: 기존 flags 뒤에 -DRYU_REVEAL_ATTRIBUTION
text-effect-demo C++ flags: 기존 flags 뒤에 -DTRACE_ENABLED -DRYU_REVEAL_ATTRIBUTION
```

Core/UI packaging spec은 이미 `ENABLE_TRACE=ON`을 전달하고,
Adaptor spec도 trace/Streamline 분기에서 전달한다. **CXXFLAGS를 통째로
교체하지 말고** 마지막에 위 define을 추가한다. 이번 diff는 spec/CMake를
변경하지 않았다. 샘플은 별도 프로젝트이므로 `TRACE_ENABLED`도 명시한다.

CMake cross-build를 사용하는 경우의 정확한 설정 방식:

```bash
# 각 기존 target build directory에서, 다른 configure 옵션은 그대로 유지.
# ORIGINAL_CXX_FLAGS는 해당 디렉터리 CMakeCache.txt의 기존 CMAKE_CXX_FLAGS 값.
cmake -S /PATH/TO/trees/core/build/tizen -B /PATH/TO/core-build \
  -DENABLE_TRACE=ON -DCMAKE_CXX_FLAGS="$ORIGINAL_CXX_FLAGS -DRYU_REVEAL_ATTRIBUTION"
cmake --build /PATH/TO/core-build -j8

# adaptor, production UI, source-only UI에도 같은 방식으로 적용.
# 샘플의 경우:
cmake -S /PATH/TO/trees/production/samples/text -B /PATH/TO/sample-build \
  -DCMAKE_CXX_FLAGS="$ORIGINAL_SAMPLE_CXX_FLAGS -DTRACE_ENABLED -DRYU_REVEAL_ATTRIBUTION"
cmake --build /PATH/TO/sample-build --target text-effect-demo.example -j8
```

`/PATH/TO`는 기존 TV 빌드의 실제 경로이며 위 명령만으로 새 cross-toolchain을
설정하지 않는다. 기존 빌드 서버/패키지 install 절차로 **계측 Core/Adaptor를
먼저**, 각 UI와 demo를 설치한다. 시스템 라이브러리 위치/권한을 추측해서
덮어쓰는 install 명령은 제공하지 않는다.

분리된 prefix를 지원하는 기존 환경이라면 두 UI만 다른 lib 디렉터리에
두고, 같은 Core/Adaptor 경로를 `LD_LIBRARY_PATH`에 이어 붙일 수 있다.
`ldd` 또는 `/proc/<pid>/maps`로 로드된 **세 라이브러리의 경로**를 확인한다.
실제 생산 비교에서는 mixed-object `build-host.py`를 사용하지 않는다.

## 3. 먼저 OFF / ON sanity

타겟으로 `run-trace.sh`를 복사한다. 스크립트는 Bash를 사용한다.
권한이 있는 임시 디렉터리를 `TMPDIR`로 지정할 수 있다.

```bash
# 같은 production executable/library 구성, 같은 UX로 OFF와 ON 한 번씩.
# FPS logger는 두 실행에서 동일하게 설정. 1초 평균은 보조 정보뿐이다.
export DALI_FPS_TRACKING=1
bash /PATH/TO/run-trace.sh off /ABSOLUTE/PATH/text-effect-demo.example
bash /PATH/TO/run-trace.sh production /ABSOLUTE/PATH/text-effect-demo.example
```

ON이 명백하게 더 버벅이거나 대략 10fps 급 하락을 추가한다면 본 비교를
진행하지 말고 두 실행의 로그를 보내준다. host sanity 통과가 target에서의
무시 가능한 overhead를 보증하지 않는다.

## 4. 본 비교 — 두 빌드, 두 scenario

```bash
# A: production UI를 실제로 로드한 상태
bash /PATH/TO/run-trace.sh production /ABSOLUTE/PATH/text-effect-demo.example

# B: Source-only UI로 교체하거나 해당 prefix를 선택한 상태
bash /PATH/TO/run-trace.sh source-only /ABSOLUTE/PATH/text-effect-demo.example
```

양쪽 모두 **Blur ON / Strong / Performance / Sync(1)**로 유지한다.
창 크기, 폰트, MSAA, scale, partial-update, governor 등 기존 환경도 동일하게.

1. 로딩과 Intro 완료 후 Enter/Space → Skeleton → Cards entrance를 본다.
2. Cards의 글자와 badge/action까지 완전히 나타난 뒤 잠시(약 2초) 기다린다.
3. Esc/Back으로 Intro로 돌아간다. 이것이 radius48 정상 퇴장이다.
   Enter를 눌러 Markdown으로 넘어갈 필요는 없다.
4. 1–3을 총 4회: 첫 진입 1회 + 같은 프로세스 안에서 warm 3회.
5. Intro에서 Esc/Back으로 **정상 종료**한다.

첫 실행을 위해 shader cache를 삭제하지 않는다. 첫 진입과 실제 compile/cache
miss는 같은 말이 아니다. `Shader.Create`는 cache load/compile/link/reflection을
포함하는 프로그램 생성 구간이며 compile-only 수치가 아니다.

`RYU - output directory: /tmp/ryu-production.XXXXXX`처럼 출력된다.
그 디렉터리에 `app.log`, `capture.<pid>.<tid>.trace`가 저장된다.
**종료 전에는 trace 파일이 없는 것이 정상**이다. kill -9, abort, `_exit`이면
일부/전체 데이터가 유실될 수 있다. 별도의 stop 신호나 GPU 동기화는 없다.

## 5. Bash wrapper를 쓰지 않을 경우

```sh
# 앱 실행과 동일한 쉘/프로세스 환경에 전달해야 한다.
export RYU_REVEAL_TRACE=/tmp/ryu-production-unique
export DALI_TRACE_COMBINED=1 DALI_TRACE_UPDATE_PROCESS=1 DALI_TRACE_EGL=1
export DALI_TRACE_REVEAL_BOUNDARIES=1
export DALI_TRACE_RENDER_PROCESS=0 DALI_TRACE_PERFORMANCE_MARKER=0
export DALI_TRACE_ENABLE_PRINT_LOG=0 DALI_PERFORMANCE_TIMESTAMP_OUTPUT=0
export DALI_FPS_TRACKING=1
/ABSOLUTE/PATH/text-effect-demo.example
```

앱 런처를 경유하면 셸 환경이 전달되지 않을 수 있다. 이 경우 기존 런처의
환경 전달 방식을 사용해야 한다. 파일이 생성되지 않으면 측정값을 0으로
해석하지 말고 compile flags/loaded libraries/환경 전달/정상 종료부터 확인한다.

## 6. 수집 / 분석

타겟 디렉터리 두 개를 PC로 가져온다. SDB가 연결되어 있다면 예:

```bash
sdb pull /tmp/ryu-production.ACTUAL_SUFFIX ./target-production
sdb pull /tmp/ryu-source-only.ACTUAL_SUFFIX ./target-source-only

python3 /home/bowonryuubuntu/tizen/reveal-critical-path.CFOTks/analyse.py \
  ./target-production --out ./production-analysis
python3 /home/bowonryuubuntu/tizen/reveal-critical-path.CFOTks/analyse.py \
  ./target-source-only --out ./source-only-analysis
```

출력 디렉터리는 새 경로여야 한다. 기존 결과를 덮어쓰지 않는다.

- `timeline.json`: Chrome trace/Perfetto JSON importer에서 스레드별 duration.
- `frames.csv`: frame-local ID, wall/CPU/non-running, 다음 frame-start 및
  swap-return 간격, 각 scope, Event setup와의 시간상 overlap.
- `late-frames.md`: iteration wall >18ms 목록. 앞뒤 프레임은 CSV/timeline에서.
- `summary.json`: >18/25/33ms bucket, health, phase markers.

원본 파일의 `dropped`, `unmatched`, `open`, `allocation_failed`, `clock_failed`가
모두 0인지 먼저 확인한다. overflow이면 더 짧게(최소 warm2회까지 허용) 재수집.
GPU는 항상 unknown. API call wall-time 또는 `wall-CPU`를 GPU ms로 바꾸지 않는다.
CPU running/runnable/blocked 세 상태 구분은 별도 scheduler trace 없이는 불가능하다.

최종 판정은 production의 늦은 프레임과 Source-only의 **같은 phase**를 비교한다.
frame 번호 자체는 두 실행 사이에서 대응하지 않는다. setup overlap이 0이어도
그 직전 Event 작업의 후속 upload/creation이 존재할 수 있으므로 timeline을 함께
본다. 여러 중첩 scope나 서로 다른 스레드의 시간을 더해 frame total로 만들지 않는다.
