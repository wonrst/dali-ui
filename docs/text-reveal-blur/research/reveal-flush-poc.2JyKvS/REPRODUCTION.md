# Private PC PoC

원본 Core/Adaptor/UI repository나 설치된 DALi library는 변경하지 않았다.
이 디렉터리의 실행 파일/공유 library만 사용한다. Target package는 없다.

## 비교 실행

아래는 기존 Text Effect Demo를 같은 순서로 두 번 실행하는 자동 runner다.
두 번째 cycle이 warm sample이다. 현재 사용자 앱이나 설치 환경을 대체하지 않는다.

```bash
cd /home/bowonryuubuntu/tizen/reveal-flush-poc.2JyKvS

# Baseline
env DISPLAY=:1 DALI_WINDOW_WIDTH=1280 DALI_WINDOW_HEIGHT=720 \
  DALI_MULTI_SAMPLING_LEVEL=4 \
  LD_LIBRARY_PATH="$PWD/lib/traced-core:$PWD/lib/production:$PWD/lib/common:/home/bowonryuubuntu/dali-env/opt/lib" \
  ./pc-runner

# Candidate
env DISPLAY=:1 DALI_WINDOW_WIDTH=1280 DALI_WINDOW_HEIGHT=720 \
  DALI_MULTI_SAMPLING_LEVEL=4 \
  LD_LIBRARY_PATH="$PWD/lib/traced-core:$PWD/lib/candidate:$PWD/lib/production:$PWD/lib/common:/home/bowonryuubuntu/dali-env/opt/lib" \
  ./pc-runner
```

측정의 정확한 환경/라이브러리 경로는 `run-records.json`, 각 capture의
`libraries.txt`가 기준이다. `run_pc.py`는 기존 결과를 덮어쓰지 않도록 설계했다.
`summarize.py`는 기존 상세 parser를 재사용하며, 표의 primary sample은
각 독립 process의 두 번째 cycle이다.

## 변경 위치

- `private-poc.patch`: 기존 상세 계측 사본에 대한 Adaptor 3파일 diff.
- `private-collector-tag.patch`: 양쪽 공통 수집기에 CommandDrain tag 하나 추가.
- `STATIC_AUDIT.md`: 구현 전 boundary 결정.
- `quality.cpp`, `generic.cpp`: 외부 real-driver correctness/lifecycle fixtures.
- `meter.cpp`: 실제 GL 호출 counter. 성능 모드에는 GPU timer/readback 없음.
- `build_pc.py`, `build_drain_trace.py`: 외부 build; install/GBS를 하지 않음.

Compile-time `ENABLE_BOUNDED_GL_FLUSH_COALESCING=0/1`로 구분하며,
public option이나 product environment variable을 추가하지 않았다.
Baseline/candidate의 class layout은 같고, 나머지 objects는 공유한다.

## 재현 조건과 한계

프로젝트의 `RelWithDebInfo` 최적화 flags(`-O2 -g -DNDEBUG`)를 기존 compile flags에
적용한 diagnostic build다. 기존 `DEBUG_ENABLED`와 trace 계측을 유지했으므로
완전한 production Release의 절대 CPU 비용이라고 해석하면 안 된다.

변경하지 않은 third-party glyphy 한 파일은 optimized build에서 기존
`-Werror=maybe-uninitialized`에 걸려 이전 no-O object를 양쪽에 동일하게 사용했다.
설치된 UI Components library도 양쪽이 공유한다. Core/Adaptor/Foundation의 나머지
계측 대상 C++ objects는 새 optimized build다. 상세는 `build-third-party-warning.log`.

`captures-without-drain-tag/`는 CommandDrain tag가 collector 허용 목록에서 빠졌던
초기 실행이다. 최종 성능 표에 사용하지 않는다. collector tag를 추가한 뒤 양쪽 5회를
다시 수행한 `captures/`만 사용한다.

`generic-invalid-fixture/`는 진단 shader의 DSL 줄 배치를 바로잡기 전의 무효 자료다.
`generic-readback-invalid-fixture/`는 KeepRenderResult 호출을 REFRESH_ONCE 설정 전으로
놓았던 무효 baseline이다. 두 fixture 작성 오류 모두 원인을 확인하고 수정했으며,
최종 `generic-captures/`에서는 non-black framebuffer와 실제 readback buffer를 확인한다.

`generic-first-pass/`는 readback/FBO-only 확인을 보강하기 전의 통과 자료이고,
최종 결과는 `generic-captures/` 기준이다. 무효 자료는 correctness 통과 수에 넣지 않는다.
