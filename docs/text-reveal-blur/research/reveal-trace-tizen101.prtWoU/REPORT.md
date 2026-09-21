# Tizen 10.1 ARMv7l critical-path trace package

**TARGET RPM BUILD COMPLETE — 타겟 실행/측정은 사용자 검증 단계.**

## 요청 기준

| Repository | 기준 commit |
|---|---|
| Core | `ecf444c414543b1e629f53afe1340beda2ee5f9c` |
| Adaptor | `f451029c14a7bda7175254ffd3aa4c73bb8252ba` |
| UI | `4d74bbf9d4169f808c0e11cc5e806d2e506f1d95` |

UI에는 Gaussian shader factory → Reveal Blur → samples → Source batching의
네 커밋을 적용했다. production은 `b54bb666`까지의 blur 구현이고,
Source-only는 기존 `b54bb666..05087317` 진단 변경을 추가한 비교본이다.

원본 checkout 대신 별도 detached worktree 네 개에서 작업했다.
원본 repository의 작업 파일·index를 수정하지 않았고 commit/push도 하지 않았다.
사용자의 `.gbs.conf`는 그대로 사용했다. 빌드 옵션은 `-A armv7l -P tizen_10.1`.

## 빌드 상태

| 항목 | 상태 |
|---|---|
| Core | GBS 성공, `1.ryutrace` |
| Adaptor | GBS 성공, TV profile 포함, `1.ryutrace` |
| production UI foundation/components | GBS 성공, `1.ryutrace.production` |
| Text samples / Text Effect Demo | GBS 성공, `1.ryutrace` |
| Source-only UI foundation/components | GBS 성공, `1.ryutrace.sourceonly` |

Core/Adaptor의 로컬 `1.ryutrace` development 패키지를 UI 빌드에서 사용하는
것을 GBS 로그로 확인했다. 샘플은 production UI development 패키지로 빌드했다.
비교 시에는 같은 샘플과 Core/Adaptor를 사용하고 UI만 교체한다.

## 이식 중 해결한 사항

- Typesetter 충돌 두 곳: metadata pointer 초기화와 cropped metadata의
  reference-height clipping. 기준 버전의 좌표 계산/누적 방식은 보존했다.
- Core의 Extents가 아직 public API인 차이: include, enum 및 getter를
  지정 Core의 동등 API로 맞췄다.
- Adaptor의 Application이 아직 public API인 차이: Text 샘플의 include
  경로만 맞췄다.
- 샘플 RPM: trace용 Core integration header와 기존 localization 샘플이
  사용하는 UI integration header의 BuildRequires를 명시했다.
- 샘플 RPM file list에 이미 빌드/설치되는 `text-reveal-perf.example`를 포함했다.

두 UI 사본의 차이는 기존 Source-only diagnostic의 두 파일뿐이다.
계측/호환성 처리/샘플은 동일하며, 새 최적화나 새 rendering variant는 없다.

## 확인한 내용 / 한계

- 실제 RPM 내부에 Core collector, Adaptor offscreen/window/EGL/Shader marker,
  UI preparation/publication/attachment marker, demo phase marker가 포함됨을 확인.
- ARMv7l RPM 및 TV Adaptor의 ARM ELF32/EABI 형식을 확인.
- `git diff HEAD --check`, 실행 wrapper의 `bash -n` 확인.
- 타겟 설치/실행, target trace 수집, UTC, 새로운 성능/메모리 측정은 하지 않았다.
- GPU duration은 계속 unknown이다. thread CPU time과 wall time만 수집한다.
- 정상 종료 시 trace를 파일로 저장한다. kill/abort로 유실될 수 있다.
- 진단 버퍼는 traced thread당 약 10 MiB다. 타겟 OFF/ON sanity가 먼저 필요하다.

## 전달 / 재현

- [타겟 설치·실행 가이드](delivery/README.md)
- 타겟 전달 압축파일 (로컬 자료: `reveal-trace-armv7l.tar.gz`)
- [정확한 빌드 명령 및 port 설명](BUILD.md)
- [기존 trace 분석 보고서](../reveal-critical-path.CFOTks/REPORT.md)
- `patches/`: 지정 기준에 적용할 전체 변경. Core는 tracked+collector 두 patch.
- `delivery/`: 실제로 설치할 runtime RPM, wrapper, offline parser.
- `gbs-root/local/repos/tizen_10.1/armv7l/RPMS/`: 전체 RPM/devel/debuginfo 결과.
- `ui-production-rpmbuild.log`, `ui-source-only-rpmbuild.log`,
  `sample-rpmbuild.log`: 개별 성공 빌드의 전체 로그.

타겟 결과 폴더 production/Source-only 두 개를 받은 뒤 늦은 프레임의
Event/Update/Render/Swap 구간을 비교한다. 이 빌드 결과만으로 target 병목이나
성능 개선을 결론내리지 않는다.
