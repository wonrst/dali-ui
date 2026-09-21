# TV에서 Reveal critical-path 측정

Tizen 10.1 / ARMv7l 전용 진단 패키지다. 일반 배포용이 아니다.
production과 source-only는 UI만 다르고 Core/Adaptor/샘플/계측은 같다.
새 로그/trace에는 `RYU - `가 붙는다. GPU 시간은 측정하지 않는다.

## 1. 설치 전

테스트 장비의 기존 DALi RPM을 복구할 수 있도록 먼저 확보한다.
앱을 종료한 상태에서 기존에 사용하는 장비 접근/설치 권한으로 설치한다.
Core/Adaptor는 다른 DALi 앱도 사용하는 시스템 라이브러리다.

PC에서 전달용 압축파일을 복사한다:

```sh
sdb push reveal-trace-armv7l.tar.gz /tmp/
```

장비 셸에서 새 작업 폴더로 푼다. 이미 같은 폴더가 있으면 다른 이름을
선택하고 아래 경로도 그 이름으로 바꾼다:

```sh
mkdir /tmp/reveal-trace
tar -xzf /tmp/reveal-trace-armv7l.tar.gz -C /tmp/reveal-trace
```

장비에서 우선 확인:

```sh
uname -m
rpm -qa | grep -E '^dali2|^com.samsung.dali.text'
```

아키텍처/의존성 오류가 나면 `--nodeps`나 `--force`로 우회하지 않는다.
기존 `*-devel` 패키지가 정확한 이전 release를 요구한다면 같은 빌드의
devel RPM도 필요할 수 있다. PC의 전체 GBS 결과에는 이 RPM도 보존한다.

## 2. RPM 설치 순서

이 폴더에는 TV용 패키지만 골라 넣는다. mobile/common 프로파일 RPM을
추가로 설치하지 않는다. 아래 명령은 타겟의 기존 설치 권한으로 실행한다.
`--test`가 통과한 뒤 실제 설치를 실행한다.

```sh
cd /tmp/reveal-trace
rpm -Uvh --test --replacepkgs --oldpackage common/*.rpm production/*.rpm
rpm -Uvh --replacepkgs --oldpackage common/*.rpm production/*.rpm
```

`common`은 Core·Adaptor·TV 프로파일·샘플, `production`은 UI foundation과
components다. 기존 패키지가 더 최신일 수 있어 지정 기준으로 되돌리는
`--oldpackage`를 명시했다. 현재 앱을 종료한 상태에서만 교체한다.

기존 feedback plugin/Vulkan 패키지가 의존성 오류를 내는 경우에만
`optional`의 같은 이름 RPM을 **위 두 명령에 함께 추가**한다.
기존 설치 여부는 `rpm -q dali2-adaptor-dali2-feedback-plugin dali2-adaptor-vulkan`
으로 확인한다. 사용하지 않던 optional 패키지를 새로 설치할 필요는 없다.
devel 또는 다른 패키지 의존성 문제는 우회하지 말고 오류를 알려준다.

Source-only 비교로 바꿀 때는 UI 두 개만 교체한다:

```sh
rpm -Uvh --test --replacepkgs --oldpackage source-only/*.rpm
rpm -Uvh --replacepkgs --oldpackage source-only/*.rpm
```

production으로 돌아올 때는 위 명령의 `source-only`를 `production`으로
바꾼다. Core/Adaptor/샘플은 다시 설치하지 않는다. Adaptor의 기존 RPM
설치 스크립트는 shader cache를 정리하므로 첫 설치 후 첫 진입과 warm
반복을 구분한다. 두 UI 비교 사이에 Adaptor를 재설치할 이유는 없다.

## 3. 측정 실행

RPM 설치가 끝나면, **평소 샘플이 정상 실행되는 동일한 타겟 셸 환경**에서:

```sh
cd /tmp/reveal-trace
export DALI_FPS_TRACKING=1
bash run-trace.sh off /usr/apps/com.samsung.dali.text/bin/text-effect-demo.example
bash run-trace.sh production /usr/apps/com.samsung.dali.text/bin/text-effect-demo.example
```

먼저 같은 production RPM에서 OFF/ON 각각 한 번 확인한다.
ON만 현저히 더 느리면 두 로그를 보내고 본 비교를 중단한다.
새 패키지 설치 후 첫 실행은 shader cache 상태가 달라질 수 있으므로,
첫 실행과 warm 반복을 구분한다. 별도로 cache를 삭제할 필요는 없다.

Source-only UI RPM으로 바꾼 다음:

```sh
bash run-trace.sh source-only /usr/apps/com.samsung.dali.text/bin/text-effect-demo.example
```

스크립트 인자는 **로그 이름**이다. production/source-only 경로 자체를
바꾸지는 않는다. 각 실행 전에 올바른 UI RPM이 설치되어 있어야 한다.
Source-only는 blur를 제외해 병목을 분리하는 진단 경로이며 품질 비교가 아니다.

각 실행의 조작은 동일하다:

1. `Blur Effect OFF / Blur ON / Strong / Performance / Sync(1)`.
2. Intro 로딩 후 Enter/Space → Skeleton → Cards.
3. Cards 글자·badge·action이 모두 나타난 뒤 약 2초 대기.
4. Esc/Back → Intro. 이것이 radius 48 정상 퇴장이다.
5. 2–4를 총 4회(첫 진입 1회 + warm 3회).
6. Intro에서 Esc/Back으로 **정상 종료**.

창 크기, 폰트, MSAA, UI scale, partial update 등 다른 설정은 두 실행에서
동일하게 유지한다. 계측 스크립트는 이 설정을 바꾸지 않는다.
기존 `LD_LIBRARY_PATH`가 다른 DALi 라이브러리를 가리키는지도 확인한다.

## 4. 결과 위치

시작할 때 `RYU - output directory: /tmp/ryu-production.XXXXXX`가 출력된다.
그 폴더 안에 `app.log`와 `capture.<pid>.<tid>.trace`가 저장된다.
측정 중에는 파일 I/O를 피하므로 **trace는 정상 종료할 때 저장된다**.
kill -9/abort 시에는 유실될 수 있다. `NO TRACE`는 측정값 0이 아니다.

PC로 두 결과 폴더를 가져온다. 예:

```sh
sdb pull /tmp/ryu-production.ACTUAL_SUFFIX ./target-production
sdb pull /tmp/ryu-source-only.ACTUAL_SUFFIX ./target-source-only
python3 analyse.py ./target-production --out ./production-analysis
python3 analyse.py ./target-source-only --out ./source-only-analysis
```

`ACTUAL_SUFFIX`는 시작할 때 표시된 실제 이름으로 바꾼다.
원본 결과 폴더 두 개를 그대로 보내도 된다.

분석 결과: `summary.json`, `frames.csv`, `late-frames.md`, `timeline.json`.
`dropped/unmatched/open/allocation_failed/clock_failed`가 모두 0인지 확인한다.
wall time과 thread CPU time을 구분하며, `wall - CPU`를 GPU 시간으로
해석하지 않는다. 진단 버퍼는 스레드당 약 10 MiB 추가로 사용한다.

## 5. 종료 후

계측 패키지를 계속 상품화 환경에 두지 않고 기존 정상 RPM으로 복구한다.
장비 설치/실행 및 target trace 수집은 아직 수행하지 않았다.
