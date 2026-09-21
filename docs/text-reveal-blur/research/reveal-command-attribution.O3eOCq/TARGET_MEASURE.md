# 타겟 측정 — command/create 세부 계측

Tizen10.1 / armv7l 전용. **설치·실행은 사용자만 수행한다.**
기존 정상 RPM을 먼저 보관한다. 앱이 꺼진 상태에서 기존 승인된 장비 접근/설치 방법을 사용한다.
`.sh` 실행은 필요 없다. 아래 명령은 평소 앱이 정상 실행되는 타겟 셸에 직접 입력한다.

## 1. Production 설치

`reveal-command-trace-armv7l.tar.gz`를 평소 방법으로 타겟 `/tmp/`에 복사한다.
새 디렉터리에 푼다:

```sh
mkdir /tmp/reveal-command-trace
tar -xzf /tmp/reveal-command-trace-armv7l.tar.gz -C /tmp/reveal-command-trace
cd /tmp/reveal-command-trace
rpm -Uvh --test --replacepkgs --oldpackage artifacts/common/*.rpm artifacts/production/*.rpm
rpm -Uvh --replacepkgs --oldpackage artifacts/common/*.rpm artifacts/production/*.rpm
```

`--test`가 성공한 경우에만 두 번째 명령을 실행한다.
common: Core·Adaptor·TV profile·기존 동일 샘플. production: UI foundation·components.
두 variant의 Core/Adaptor/샘플은 **같은 RPM 파일**이다.

의존성/서명/권한 오류는 우회하지 말고 알려준다. `--nodeps`, `--force`, 서명 정책 변경은 하지 않는다.
이미 설치된 feedback plugin/Vulkan만 이전처럼 정확한 release를 요구할 경우
`artifacts/optional/`의 **해당 패키지**를 같은 transaction에 추가한다. 불필요한 패키지를 새로 설치하지 않는다.
devel 패키지 의존성 오류도 알려준다. 전체 GBS 결과에 동일 release devel RPM이 있다.

## 2. OFF sanity — 같은 Production 패키지

```sh
reveal_trace_dir=$(mktemp -d /tmp/ryu-cmd-off.XXXXXX)
unset RYU_REVEAL_TRACE
export DALI_TRACE_COMMAND_CATEGORIES=0
export DALI_TRACE_COMBINED=0 DALI_TRACE_UPDATE_PROCESS=0 DALI_TRACE_EGL=0
export DALI_TRACE_REVEAL_BOUNDARIES=0 DALI_TRACE_RENDER_PROCESS=0
export DALI_TRACE_PERFORMANCE_MARKER=0 DALI_TRACE_ENABLE_PRINT_LOG=0
export DALI_PERFORMANCE_TIMESTAMP_OUTPUT=0 DALI_FPS_TRACKING=1
rpm -q dali2 dali2-adaptor dali2-adaptor-profile_tv dali2-ui-foundation dali2-ui-components com.samsung.dali.text > "$reveal_trace_dir/packages.txt"
echo "RYU - output directory: $reveal_trace_dir"
/usr/apps/com.samsung.dali.text/bin/text-effect-demo.example > "$reveal_trace_dir/app.log" 2>&1
```

아래 4번 동작을 1–2회 수행하고 정상 종료한다. OFF에서는 trace 파일이 없는 것이 정상이다.
설치 직후 첫 실행은 shader cache 준비가 섞일 수 있으므로 warm 동작도 확인한다.
FPS 로그가 app.log에 없으면 **기존 타겟 dlog 수집 방법으로** 같은 앱 PID의
`Frame count ... FPS ...` 1초 로그도 보관한다. 이전 app.log에는 FPS가 없었다.
다른 환경변수, 창 크기, MSAA, UI scale, partial update, LD_LIBRARY_PATH는 바꾸지 않는다.

## 3. ON — Production

```sh
reveal_trace_dir=$(mktemp -d /tmp/ryu-cmd-production.XXXXXX)
export RYU_REVEAL_TRACE="$reveal_trace_dir/capture"
export DALI_TRACE_COMMAND_CATEGORIES=1
export DALI_TRACE_COMBINED=1 DALI_TRACE_UPDATE_PROCESS=1 DALI_TRACE_EGL=1
export DALI_TRACE_REVEAL_BOUNDARIES=1 DALI_TRACE_RENDER_PROCESS=0
export DALI_TRACE_PERFORMANCE_MARKER=0 DALI_TRACE_ENABLE_PRINT_LOG=0
export DALI_PERFORMANCE_TIMESTAMP_OUTPUT=0 DALI_FPS_TRACKING=1
rpm -q dali2 dali2-adaptor dali2-adaptor-profile_tv dali2-ui-foundation dali2-ui-components com.samsung.dali.text > "$reveal_trace_dir/packages.txt"
echo "RYU - output directory: $reveal_trace_dir"
/usr/apps/com.samsung.dali.text/bin/text-effect-demo.example > "$reveal_trace_dir/app.log" 2>&1
```

OFF보다 ON에서 눈에 띄게 더 느려지면 본 비교를 멈추고 두 실행 결과를 보내준다.
필요시 기존 coarse trace와 추가 계측을 구분하려면 위 ON 환경에서
`DALI_TRACE_COMMAND_CATEGORIES=0`만 설정한 별도 실행을 보낼 수 있다.
그 실행은 상세 parser 비교용으로 사용하지 않는다.

## 4. 동일 동작 반복

1. **Blur Effect OFF / Blur ON / Strong / Performance / Sync(1)** 확인.
2. Intro에서 Enter/Space → Skeleton → Cards.
3. 글자·badge·action이 모두 나타난 후 약2초 대기.
4. Esc/Back → Intro: 완료된 Cards의 정상 퇴장.
5. 2–4를 **4회 권장, 최소3회** 반복. 첫 회와 warm 반복을 구분한다.
6. 마지막 Intro에서 Esc/Back으로 **정상 종료**한다. kill/강제종료하지 않는다.

장면 입력 직후 잠깐 멈췄는지, 애니메이션 도중 지속적으로 버벅였는지 메모한다.
첫 실행/warm 여부와 버튼 설정을 기록한다. **실행마다 새 mktemp 디렉터리**를 사용한다.

## 5. Source-only로 교체 후 동일 측정

앱 종료 후 **UI 두 패키지만** 교체한다. Core/Adaptor/샘플은 재설치하지 않는다.

```sh
cd /tmp/reveal-command-trace
rpm -Uvh --test --replacepkgs --oldpackage artifacts/source-only/*.rpm
rpm -Uvh --replacepkgs --oldpackage artifacts/source-only/*.rpm
```

ON 환경을 유지하고 새 출력 폴더로 실행한다:

```sh
reveal_trace_dir=$(mktemp -d /tmp/ryu-cmd-sourceonly.XXXXXX)
export RYU_REVEAL_TRACE="$reveal_trace_dir/capture"
export DALI_TRACE_COMMAND_CATEGORIES=1
rpm -q dali2 dali2-adaptor dali2-adaptor-profile_tv dali2-ui-foundation dali2-ui-components com.samsung.dali.text > "$reveal_trace_dir/packages.txt"
echo "RYU - output directory: $reveal_trace_dir"
/usr/apps/com.samsung.dali.text/bin/text-effect-demo.example > "$reveal_trace_dir/app.log" 2>&1
```

새 셸이라면 먼저 3번의 trace flag export도 다시 실행한다.
4번과 같은 동작/횟수로 측정한다. Source-only는 blur가 빠진 **진단용**이다.
디렉터리 이름이나 환경변수만 바꿔서는 production/source-only 구현이 전환되지 않는다.

## 6. 종료 후 확인 / 전달

정상 종료한 뒤 출력 디렉터리에서:

```sh
ls -lh "$reveal_trace_dir"
grep '^RYU - meta\|^RYU - categories' "$reveal_trace_dir"/*.trace
```

파일은 `capture.<PID>.<TID>.trace`. 일반적으로 PID=TID는 Event,
다른 TID는 Update/Render다. 상세 categories는 Render 파일에 기록된다.
`dropped/unmatched/open/allocation_failed/clock_failed`는 모두0이어야 한다.
`categories enabled=1`이고 Render의 category records가0보다 커야 한다.
측정 중에는 파일을 쓰지 않으므로 **정상 종료 전 trace 파일이 없는 것은 정상**이다.

다음을 폴더 그대로 PC로 복사해 전달한다:

- Production OFF: app.log, packages.txt, FPS 로그, 첫 실행/warm 메모.
- Production ON / Source-only ON: 각각 app.log, packages.txt, **모든 capture.*.trace**, FPS 로그.
- 어느 진입/퇴장 구간이 버벅였는지와 버튼 설정 기록.

타겟에서 Python 분석을 할 필요 없다. 측정이 끝나면 기존 정상 RPM으로 복구한다.
계측 패키지는 상품화용이 아니다.

## PC에서 직접 분석할 경우

현재 PC의 기존 parser를 사용하는 명령이다:

```sh
python3 /home/bowonryuubuntu/tizen/reveal-command-attribution.O3eOCq/analyze.py \
  /path/to/production-result --source-only /path/to/sourceonly-result \
  --out /path/to/new-analysis-directory
```

PID가 섞였으면 `--production-pid N` / `--source-only-pid N`을 지정한다.
출력: `COMPARISON.md`, variant별 `categories.json`, `category-frames.csv`, 기존 baseline timeline/frames.
새 category가 없는 과거 trace는 이번 상세 분석 데이터로 사용할 수 없다.
