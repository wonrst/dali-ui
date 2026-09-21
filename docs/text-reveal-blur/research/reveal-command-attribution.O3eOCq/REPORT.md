# Reveal Blur — additional target command attribution

## A. Executive summary

**ADDITIONAL TARGET ATTRIBUTION READY**

추가 계측, parser, Core/Adaptor 및 두 UI variant의 ARMv7l GBS 빌드를 완료했다.
RPM 묶음 (로컬 자료: `reveal-command-trace-armv7l.tar.gz`)과 [직접 입력용 측정 가이드](TARGET_MEASURE.md)를 준비했다.

기존 target에서 확인한 두 비용을 분리하기 위한 계측만 추가했다:

1. steady offscreen command CPU: production 16.62ms vs Source-only 4.81ms.
2. exit setup create queue CPU: production 51.26ms vs Source-only 10.76ms.

이 숫자는 [이전 수집 분석](../reveal-target-attribution.01tPcM/REPORT.md)의 결과이며
이번에 새로 측정한 값이 아니다. 이번 작업에서 target 설치/실행/수집/분석은 하지 않는다.

## B. Command processing categories

실제 GLES 소스를 따라 dispatch와 deferred Context::Flush를 구분했다.
신규 계측 category는 6개, parser residual을 포함한 비교표는 7개다.

| Category | 실제 범위 |
|---|---|
| `RYU - Cmd.RenderPass` | BEGIN/END_RENDERPASS, context/FBO/clear/기존 flush 처리 |
| `RYU - Cmd.DependencySync` | texture/framebuffer dependency check, fence tracking/정리 |
| `RYU - Cmd.PipelineState` | deferred program/pipeline/blend/raster 상태 적용 및 종료 상태 정리 |
| `RYU - Cmd.BufferUniform` | uniform/vertex state 적용 + Memory2/3 lock/unlock block |
| `RYU - Cmd.TextureSampler` | deferred texture/sampler binding loop |
| `RYU - Cmd.Draw` | draw descriptor block와 실제 draw 제출 |
| `RYU - Cmd.Other` | 기존 command CPU union − 위 exclusive CPU. 직접 계측하지 않음 |

파일/함수/count 의미와 경계는 [INSTRUMENTATION.md](INSTRUMENTATION.md)에 기록했다.
command 바깥의 buffer mapping 비용은 별도로 구분하므로 command CPU에 잘못 더하지 않는다.
GPU timing이나 개별 GL 함수별 profiler를 추가하지 않았다.

## C. Resource creation categories

실제 존재하는 `mCreateTextureQueue`, `mCreateBufferQueue`, `mCreateFramebufferQueue`만 계측한다.
tag는 `RYU - Create.Texture / Buffer / Framebuffer`다.

비어 있지 않은 queue batch 단위 CPU/wall/max와 InitializeResource 시도 수를 기록한다.
resource별 clock/log/주소/이름은 없다. max는 가장 느린 **batch 호출**이지 개별 resource 시간은 아니다.
기존 실패 후 재시도, recycling 및 빠른 skip 흐름은 그대로다.

## D. Instrumentation overhead

기존 Core TLS collector + Trace::LogContext + monotonic/thread CPU clocks를 확장했다.
new public API/ABI, ostringstream, hot-path I/O, lock/atomic, GPU wait/readback은 없다.
duration은 실제 handler/연속 block에만 추가하고 resource item count는 clock 없이 증가한다.
category 호출은 매 호출 record 대신 frame aggregate로 보관한다.

- 새 runtime gate: `DALI_TRACE_COMMAND_CATEGORIES=1`, 기존 `RYU_REVEAL_TRACE`도 필요.
- nested exclusive CPU/wall을 보관해 부모/자식 중복을 제거한다.
- diagnostic 메모리: 기존 traced thread당10MiB + 상세 render aggregate 약9MiB.
- OFF 때 새 category clock/기록은 없지만 helper의 enabled check는 남는다.
- **target OFF/ON overhead는 미검증**이다. 로컬 성공으로 타겟 오버헤드를 보증하지 않는다.

## E. Parser / local checks

analyze.py (로컬 자료: `analyze.py`)는 기존 parser를 직접 재사용하면서 추가 기록을 읽는다.

- 진입 CardsStart +1–3초의 category count/CPU/frame/delta.
- 각 정상 exit의 가장 느린 iteration에서 create 종류별 items/CPU/delta.
- ExitStart +300ms 이후 tail도 비교.
- command exclusive table은 additive, create table은 queue끼리만 additive.
- disabled/missing/dropped/incomplete category capture는 0ms로 해석하지 않고 실패.
- 여러 PID가 섞이면 선택을 요청하고 자동 합산하지 않는다.

수행한 로컬 검사:

- collector standalone compile `-Wall -Wextra -Werror`.
- collector ON/OFF/coarse-only, nested command/context classification smoke.
- 같은 collector host ASan/UBSan smoke 통과. production/target sanitizer 검증은 아님.
- synthetic parser test: exclusive/residual, command 밖 비용, phase/spike 선택,
  resource count, disabled capture 거부 통과.
- 변경한 Adaptor6개 기존 파일의 신규 macro/include를 제거하면 이전 소스와 byte-identical.
- diagnostic source의 `git diff --check` 통과.

## F. GBS build

이전 성공한 `armv7l / tizen_10.1 / --include-all / --threads 8`와 동일 build root를 재사용했다.
사용자 `.gbs.conf`, repository/profile/optimization flags는 바꾸지 않았다.
정확한 명령은 [BUILD.md](BUILD.md).

| 대상 | 상태 / release |
|---|---|
| Core | 성공 / `2.ryucmd` |
| Adaptor, TV profile 포함 | 성공 / `2.ryucmd` |
| Production UI foundation/components | 성공 / `2.ryucmd.production` |
| Source-only UI foundation/components | 성공 / `2.ryucmd.sourceonly` |
| Text Effect Demo sample | 이전 성공 RPM `1.ryutrace`를 byte-for-byte 재사용 |

sample 코드와 trace API는 바뀌지 않았다. 이전 sample의 runtime dependency도
동일 SONAME을 요구하므로 두 경로에 같은 sample binary를 사용한다.
Core/Adaptor 및 두 UI RPM의 ARM ELF32 payload와 계측 문자열을 확인했다.
두 UI 빌드 로그에서 새 Core/Adaptor `2.ryucmd`를 dependency로 사용한 것도 확인했다.

wrapper build 로그는 이 디렉터리, 상세 rpmbuild 로그는 `logs/`에 보관한다.

## G. RPM artifacts

`artifacts/common`, `artifacts/production`, `artifacts/source-only`에 분리해 보관했다.
optional feedback/Vulkan은 기존에 설치되어 의존성상 필요한 경우만 사용한다.
개발/debug RPM은 설치 globs와 분리한 `all-rpms/` 및 GBS repo에 보존한다.

파일별 **이름·절대 경로·크기·전체 SHA256**은 [RPM_MANIFEST.md](RPM_MANIFEST.md)에 기록했다.
기계 판독용 JSON (로컬 자료: `RPM_MANIFEST.json`)과 SHA256SUMS (로컬 자료: `SHA256SUMS`)도 제공하며,
런타임 RPM 10개 전부 checksum 검증을 통과했다.

| Variant | 설치 대상 | 경로 | SHA256 |
|---|---|---|---|
| 공통 | Core | dali2 RPM (로컬 자료: `artifacts/common/dali2-2.5.39-2.ryucmd.armv7l.rpm`) | `22b60aff4818c51e8090f54824c0cc01c368f170ccc91a8f26814c016e0a4d90` |
| 공통 | Adaptor GLES | Adaptor RPM (로컬 자료: `artifacts/common/dali2-adaptor-2.5.39-2.ryucmd.armv7l.rpm`) | `a4866185a93ad825894f01dbea0920196d574184467297892f763e42e692cda0` |
| 공통 | TV profile | TV RPM (로컬 자료: `artifacts/common/dali2-adaptor-profile_tv-2.5.39-2.ryucmd.armv7l.rpm`) | `383e6bf60cab3f6177b834b1c8c99a77778a6a7d736cf77b731291b226d8b539` |
| 공통 | 기존 동일 sample | sample RPM (로컬 자료: `artifacts/common/com.samsung.dali.text-2.0.0-1.ryutrace.armv7l.rpm`) | `1b3de217151e947bde33962c5e27d816f82c2894e13556688f0a88b6e6828f49` |
| Production | UI foundation | foundation RPM (로컬 자료: `artifacts/production/dali2-ui-foundation-2.5.39.11414-2.ryucmd.production.armv7l.rpm`) | `4ba7fa2677760253c3a2b3593102b619f4c9c9db53de5567e3950f12ba62b4c8` |
| Production | UI components | components RPM (로컬 자료: `artifacts/production/dali2-ui-components-2.5.39.11414-2.ryucmd.production.armv7l.rpm`) | `1e9a5a7df6604d11081f38f8172304712a2272107b07144c05a19f1737ba5cbd` |
| Source-only | UI foundation | foundation RPM (로컬 자료: `artifacts/source-only/dali2-ui-foundation-2.5.39.11414-2.ryucmd.sourceonly.armv7l.rpm`) | `722584f3e101831dccaf9f70b89e9fa4018566bb34f27546d72f4fffc47e39f7` |
| Source-only | UI components | components RPM (로컬 자료: `artifacts/source-only/dali2-ui-components-2.5.39.11414-2.ryucmd.sourceonly.armv7l.rpm`) | `12d62b0283c90fa4e73891d546b5f9b5f980d53825c787e29fd07fa89f4d99f7` |

전달 묶음: reveal-command-trace-armv7l.tar.gz (로컬 자료: `reveal-command-trace-armv7l.tar.gz`).
optional 2개도 manifest와 묶음에 포함하지만 기본 설치 명령에는 포함하지 않는다.

## H. Instrumentation equality

- Core/Adaptor는 하나씩 빌드한 **동일 RPM을 두 variant가 공유**한다.
- instrumentation-equality.json (로컬 자료: `instrumentation-equality.json`): UI4개 trace 삽입 파일의
  instrumentation block hash가 production/Source-only에서 동일하다.
- 두 UI 사본의 파일 내용 차이는 기존 Source-only의 `text-reveal-blur-renderer.h`,
  `text-reveal-runtime-blur.cpp` 두 개뿐이다.
- 이번 UI 사본은 이전 성공한 각 variant 소스와 byte-identical이다.
  source-audit.json (로컬 자료: `source-audit.json`)에 비교 결과를 보존했다.

## I. Target procedure

[TARGET_MEASURE.md](TARGET_MEASURE.md)에 직접 입력할 설치/환경/실행 명령을 정리했다.
추가 .sh 실행이 필요 없다.

Production OFF sanity 1–2회 → Production ON 최소 3회(권장 4회) → 앱 종료 →
UI만 Source-only로 교체 → 같은 조건 ON 반복 → 정상 종료 → 결과 폴더 전달.
PERFORMANCE / Strong / Sync, 동일 창/MSAA/scale/partial-update 조건을 유지한다.
계측 ON에서 현저히 악화되면 먼저 OFF/ON 결과를 보내고 본 비교는 멈춘다.

## J. 보내야 할 것

- OFF의 app.log / FPS 로그 / 첫 실행·warm 메모.
- 두 ON 실행의 app.log / packages.txt / 모든 capture.PID.TID.trace / FPS 로그.
- 입력 직후 멈춤인지 애니메이션 중 지속 frame drop인지와 버튼 설정.

GPU execution은 계속 unknown이다. 다음 target 자료를 받기 전에는 상세 병목이나
새 최적화 방향을 확정하지 않는다. 이번 작업은 RPM/가이드 전달에서 종료한다.

## K. Git / 보호 범위

원본 개발 repository와 기존 source worktree에는 수정하지 않았다.
새 detached source 사본에 diagnostic 변경만 추가했고 history/branch를 변경하지 않았다.
GBS는 기존 build root의 generated build/repository 영역을 갱신한다.
이전 전달 RPM/보고서/target raw trace는 보존한다.

reset/stash/rebase/amend/push 없음. target install/sdb/app 실행/trace 수집 없음.
새 blur variant/production 최적화/public API 변경 없음.
