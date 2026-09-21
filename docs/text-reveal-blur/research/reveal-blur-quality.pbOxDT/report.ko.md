# Text::Reveal BlurQuality 구현 및 비용 비교

측정일: 2026-09-14

## 구현 결과

- 공개 API: `BlurQuality : uint8_t { HIGH, PERFORMANCE }`, `SetBlurQuality()`, `GetBlurQuality()`.
- 기본값: **PERFORMANCE**. 값 복사·이동·비교, Label 보관/복원, sync/async publication에 연결했다.
- HIGH: 기존 Full-resolution Source/H/V 경로를 유지한다.
- PERFORMANCE: Source는 원본 해상도, H는 X만 1/4, V는 X/Y 각각 1/4. 기존 Late Smooth로 sharp source와 합성한다.
- 두 모드 모두 매 프레임 갱신한다. Reveal/blur의 시퀀스 계산과 progress, 사용자 Animation duration은 바꾸지 않았다.
- 작은 radius의 자동 HIGH 전환은 적용하지 않았다. PERFORMANCE의 현재 sharp handoff는 effective radius 2~8px 구간이다. 따라서 작은 radius에서 blur가 약해질 수 있으며, display radius가 2px 이하이면 현재 handoff는 sharp source만 선택한다. 작은 반경에서도 기존 Full의 표현이 필요하면 HIGH를 명시해야 한다. 이 작업에서 handoff 곡선을 새로 설계하지 않았다.
- 환경변수로 선택하던 full/quarter, early downsample, blur-only, binary/soft curve 비교 코드는 제거했다. 내부 Full 구현은 보존했다.
- Reveal example은 진단용 초기 설정/HUD를 제거하고 기존 일반 샘플 설정으로 복구했다. 기존 sliders/presets를 유지하며 High/Performance 선택 행을 추가했다.
- Effect demo에는 기존 버튼과 같은 스타일의 High/Performance 토글을 추가했다. Blur OFF에서는 radius/quality 버튼을 비활성화한다. 현재 Reveal과 이후 생성되는 entrance/exit 설정에 모두 반영한다.
- CPU raster/preparation 데이터는 공통으로 유지한다. quality만을 위한 worker 복사본이나 cache/revision 체계를 추가하지 않았다.

## 측정 방법과 범위

환경:

- Intel Core i7-11700, NVIDIA GTX 1650, driver **595.91.07**.
- Linux 6.8.0-138-generic, X11, GLES, MSAA 4x.
- 현재 일반 foundation/components 빌드 사용. UI의 CMake build type은 비어 있고 명시적인 `-O` 최적화 옵션이 없다. UTC의 Release 빌드 결과를 성능 수치로 사용하지 않았다.
- 설치된 adaptor는 기존 원본이다. 별도 worktree의 fence-lifetime 테스트 수정은 빌드/설치/커밋에 포함하지 않았다.

공통 fixture:

- Label **550×300**, font 24, absolute line height 40, 명시적 줄바꿈 6줄의 같은 Latin corpus.
- 실제 원본 text texture는 **550×240**이다. Label의 requested height와 texture height를 혼동하면 안 된다.
- White text와 좌→우 text gradient를 각각 측정했다. 여기서 A8/RGBA는 **blur capture 포맷**이다. Gradient도 일반 text raster는 A8일 수 있고 gradient lookup texture가 추가된다.
- Label 1개 및 16개. 16개는 같은 680×440 창 안에 24px씩 오프셋하여 모두 들어오도록 배치했다. 겹쳐 그리는 부하 fixture이며, 일반 앱의 실제 화면 구성을 재현한 것은 아니다.
- Reveal: PIXEL / PER_LINE / Fade 0 / Stagger 0.25. Blur: radius 24 / duration ratio 0.5. UI scale 1, sync.
- 실행 중 Reveal은 2초 linear progress 0→1 반복. 일반 Label도 매 프레임 그리도록 actor alpha를 0→1로 애니메이션했다. **일반 Label에는 Reveal API를 호출하지 않는다.**
- 새 프로세스 3회, 프로세스마다 최초 생성 1회와 재생성 5회. 아래 warm setup 평균은 각 조합 **15회**, cold 값은 **3회**이다. 실행 중 CPU는 안정화 후 약 2초씩 **3회** 측정했다. 실행 순서는 반복마다 순환했다.

수치의 정의:

1. **생성 시간**: 첫 `Label::New` 직전부터 모든 Label의 설정 및 scene Add 반환까지. 아직 다음 layout/render 작업은 포함하지 않는다.
2. **첫 layout 완료 시간**: 같은 시작점부터 마지막 Label의 첫 `LayoutFinishedSignal`까지. sync layout/raster/resource 준비와 event-loop 지연을 포함한다. **GPU 완료·화면 표시 시간은 아니다.** 이 X11 환경의 frame-rendered fence 콜백은 지원되지 않아 그 시간을 추정해서 대신 쓰지 않았다.
3. **실행 CPU**: `CLOCK_PROCESS_CPUTIME_ID` 차이. main/render/worker/driver CPU를 합한 ms/초이며 GPU 대기 wall time과 다르다. 1000ms/초는 CPU 코어 하나에 해당한다.
4. **CPU heap 증가**: 생성 전과 안정화 후 `mallinfo2`의 사용 heap + mmap allocation 차이. DALi의 CPU 자원뿐 아니라 driver host allocation도 포함하는 관측값이다. 순수 text-engine 전용 메모리나 peak 임시 buffer 수치가 아니다.
5. **텍스처 저장량**: Label/blur actor/renderer와 RenderTask FBO가 참조하는 고유 Texture를 중복 제거하고 `width×height×bytesPerPixel`로 합산했다. hidden 원본, line raster, metadata, gradient lookup, Source/H/V를 포함한다. **driver의 실제 VRAM 할당량은 아니다.** window swapchain/MSAA, alignment/compression, shader/UBO/vertex buffer는 제외한다.
6. **GPU draw 시간**: 별도 실행에서 `GL_EXT_disjoint_timer_query`로 draw별 GPU 실행 시간을 비동기 수집하여 합산한 뒤 window frame 수로 나눴다. Source/H/V/output 및 일반 text draw를 포함하지만 clear, upload, swap/vsync와 CPU 작업은 제외한다. GPU 계측을 사용한 실행의 CPU 시간은 CPU 표에 섞지 않았다. 조합별 3회, dropped/disjoint는 모두 0이었다.

이전 580 드라이버의 수치와 직접 비교하지 않고, 네 모드를 같은 새 드라이버에서 비교했다.

## Label 1개 — 초기 비용과 메모리

Warm 평균, 시간 ms / 메모리 MiB.

### White text

| 모드 | 생성 | 첫 layout 완료 | CPU heap 증가 | 전체 텍스처 | 그중 blur FBO |
|---|---:|---:|---:|---:|---:|
| 일반 Label | 0.469 | 2.095 | 0.158 | 0.126 | 0 |
| 일반 Reveal | 0.564 | 3.362 | 0.667 | 0.629 | 0 |
| PERFORMANCE | 0.515 | 7.946 | 1.575 | 1.128 | 0.239 |
| HIGH | 0.523 | 7.761 | 1.830 | 1.434 | 0.545 |

### Text gradient

| 모드 | 생성 | 첫 layout 완료 | CPU heap 증가 | 전체 텍스처 | 그중 blur FBO |
|---|---:|---:|---:|---:|---:|
| 일반 Label | 0.498 | 2.100 | 0.168 | 0.127 | 0 |
| 일반 Reveal | 0.549 | 3.262 | 0.677 | 0.631 | 0 |
| PERFORMANCE | 0.555 | 8.810 | 2.387 | 1.846 | 0.955 |
| HIGH | 0.522 | 8.117 | 3.569 | 3.071 | 2.181 |

새 프로세스에서의 첫 생성은 font/기반 자원의 first-use 비용이 포함된다:

| 모드 | White: 생성 / 첫 layout | Gradient: 생성 / 첫 layout |
|---|---:|---:|
| 일반 Label | 9.222 / 12.745 | 9.464 / 13.240 |
| 일반 Reveal | 9.243 / 14.538 | 9.708 / 14.993 |
| PERFORMANCE | 9.343 / 20.792 | 9.574 / 21.659 |
| HIGH | 9.777 / 20.956 | 9.885 / 21.193 |

따라서 PERFORMANCE를 **Label 생성 자체나 CPU 초기 준비를 더 빠르게 하는 모드**로 해석하면 안 된다. 핵심은 GPU filtering과 텍스처 저장량이다.

## Label 16개 — 초기 CPU / 실행 CPU / GPU

전체 16개의 합계. GPU 시간만 프레임당 값이다.

### White text

| 모드 | 첫 layout 완료 ms | CPU heap MiB | 전체 텍스처 MiB | 실행 CPU ms/초 | GPU draw ms/frame |
|---|---:|---:|---:|---:|---:|
| 일반 Label | 24.371 | 2.659 | 2.014 | 106.984 | 0.140 |
| 일반 Reveal | 41.302 | 10.816 | 10.071 | 105.959 | 0.167 |
| PERFORMANCE | 118.798 | 26.733 | 18.047 | 524.335 | 0.704 |
| HIGH | 110.033 | 30.464 | 22.948 | 416.407 | 1.073 |

### Text gradient

| 모드 | 첫 layout 완료 ms | CPU heap MiB | 전체 텍스처 MiB | 실행 CPU ms/초 | GPU draw ms/frame |
|---|---:|---:|---:|---:|---:|
| 일반 Label | 25.249 | 2.819 | 2.038 | 96.956 | 0.167 |
| 일반 Reveal | 42.571 | 10.978 | 10.094 | 96.826 | 0.211 |
| PERFORMANCE | 125.881 | 39.633 | 29.535 | 578.060 | 0.828 |
| HIGH | 119.053 | 58.514 | 49.140 | 464.121 | 1.372 |

실행 중 FPS는 이 PC에서 약 59~60이었다. 16개 PERFORMANCE 평균은 white 59.62, gradient 59.46; HIGH는 각각 59.97, 59.85였다. Vsync에 도달한 PC 결과만으로 타겟 FPS를 예측할 수 없다.

GPU draw 측정 3회의 범위:

- White: PERFORMANCE 0.694~0.714ms, HIGH 1.038~1.093ms.
- Gradient: PERFORMANCE 0.796~0.860ms, HIGH 1.337~1.410ms.

### PERFORMANCE vs HIGH 정리

| 항목 | White | Gradient |
|---|---:|---:|
| GPU draw 시간 감소 | 34.4% | 39.6% |
| 전체 텍스처 저장량 감소 | 21.4% | 39.9% |
| blur FBO 저장량 감소 | 56.2% | 56.2% |
| 관측 CPU heap 증가분 감소 | 12.3% | 32.3% |
| 첫 layout 완료 시간 증가 | 8.0% | 5.7% |
| 실행 CPU 비용 증가 | 25.9% | 24.5% |

CPU가 증가하는 이유는 PERFORMANCE가 줄별 sharp handoff를 위해 output draw를 분리하기 때문이다. 이 fixture에서 계측된 draw는 프레임당 약 215회, HIGH는 약 150~153회였다. PERFORMANCE는 **GPU와 메모리를 줄이는 대신 CPU submission/constraint 비용을 일부 더 쓰는 선택**이다. 이 차이를 숨기고 모든 비용이 개선되었다고 결론내리면 안 된다.

## 텍스처 크기와 메모리 해석

이 fixture는 한 Label에 blur page 하나이며, 두 모드 모두 추가 RenderTask 3개이다.

| 모드 | Source | H | V |
|---|---:|---:|---:|
| HIGH | 402×474 | 402×474 | 402×474 |
| PERFORMANCE | 402×474 | 101×474 | 101×119 |

PERFORMANCE의 V 면적은 대략 1/16이지만, Source는 full이고 H는 Y를 보존한다. 기존 text/metadata도 유지하므로 전체 메모리가 1/16이 되는 것은 아니다. Source 높이가 Label의 실제 text 높이보다 큰 것은 줄별 blur margin을 포함한 atlas 배치 때문이다.

16개에서 FBO 합계는 white HIGH 8.723→PERFORMANCE 3.821MiB, gradient 34.890→15.286MiB이다. blur를 사용하지 않는 일반 Reveal에는 추가 FBO/RenderTask가 없다.

모든 native 측정 실행에서 Label 제거 후 추가 task는 0으로 돌아왔다. 장기적인 process RSS leak 판정은 이 짧은 성능 probe의 목적이 아니며, 자원 수명 검증은 아래 UTC와 기존 lifecycle 검증에 의존한다. cold RSS/heap은 driver 및 shader cache 초기화가 섞여 warm texture footprint와 직접 비교하지 않았다. 원시 RSS/heap 값은 summary.json과 로그에 보존했다.

## 검증

- Full foundation/components 빌드 및 두 sample 빌드 성공.
- Internal UTC **683/683**, public foundation **2389/2389**, components **400/400**: 합계 **3472/3472**.
- 추가 quality publication UTC: sync/async × Whole Text/Per Line × A8/gradient, PERFORMANCE→HIGH→PERFORMANCE, 빠른 async 설정 교체, progress/index/timing 보존, None/disconnect 자원 회수.
- 최소 kernel/radius UTC는 HIGH와 PERFORMANCE 모두 확인. 작은 radius의 PERFORMANCE도 Quarter texture 경로를 사용함을 확인했다.
- 기존 Reveal, ImageSpan/replacement, async publication, scale/decoration/tiling fallback, lifecycle UTC를 포함한 전체 suite 통과.
- 실제 샘플: High/Performance 전환, presets, sync/async, demo 장면 이동, Blur OFF 시 controls 비활성화 확인.
- `git diff 1f794aa3 --check` 통과.
- Windows 빌드와 실제 TV 타겟 성능은 이번 환경에서 실행하지 않았다.

## 커밋 정리

최종 stack:

1. `a78547fa` Add custom Gaussian blur shader factory
2. `daed0088` Add blur to text reveal
3. `70fe0355` Add samples for testing text reveal blur

모두 기존 사용자 Signed-off-by를 유지했다. half-rate/D2/Quarter test 커밋을 별도로 남기지 않고 해당 production/sample 변경에 통합했다. 최종 tree는 정리 직전 tree와 동일하다. UI working tree는 clean이다.

복구용: `backup/reveal-quality-before-cleanup-20260914`.

adaptor의 기존 13줄 테스트 수정은 그대로 두었으며 이번 commit stack에 포함하지 않았다.

## 재현 자료

- `quality-probe.cpp`, `run-matrix.py`: native CPU/메모리 probe.
- `gpu-meter.cpp`: 별도 GPU draw timer 계측. product patch에 포함하지 않음.
- `native-*.log`, `gpu-*.log`, `summary.json`: 원시 및 집계 결과.
- `sample-smoke.py`, `*-initial.png`, `*-high.png`, `*-off.png`: 샘플 확인 자료.
- 각 build/UTC log 및 `consolidation.log`.

이 폴더의 측정용 코드·보고서는 모두 제품 repository 밖에 보관했다.
