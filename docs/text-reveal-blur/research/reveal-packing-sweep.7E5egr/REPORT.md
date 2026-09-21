# Text Effect Demo cleanup / page-packing threshold local study

2026-09-21 · Ubuntu · UI `83f8a7665a3b5dd3b2cc93a7ccc4d61991d499bf`

## A. Executive verdict

Phase 1 sample cleanup 완료. 별도 per-Label deadline scheduler를 제거하고 기존 shared/single Animation::FinishedSignal cleanup으로 복귀했다.

Phase 2는 외부 디렉터리의 disposable shared libraries로만 수행했다. production threshold는 **1.25 그대로**다. page/task 감소에 실제 command-CPU 이득은 있지만, global ratio 완화는 큰 페이지의 absolute allocation을 충분히 제한하지 못한다. 후속 추천은 **bounded absolute-waste merge** 하나다. 이번에 조건부 heuristic을 구현하지 않았다.

## B. Demo cleanup / validation

제거: RevealCleanup, WeakHandle 목록, chrono deadline, completionSeconds 등록, timer/re-arm, progress endpoint 검사, PlayTextEntrance wrapper. 원래 Animation.Play() 호출을 복원했다.

유지: shared Cards animation 완료 시 전체 group 정리, single Label / scene text FinishedSignal 정리, 기존 lifecycle token/animation identity guard, scene removal/shutdown. ClearTextReveal은 BlurEffect 제거와 already-None guard만 수행한다. progress 0을 None으로 바꾸지 않으며 interrupted entrance는 기존 상태로 reverse한다.

sample build 성공. 외부 native checker의 HIGH/PERFORMANCE/ECONOMY × Sync/Async **6/6, failures=0**. 정상 entrance/exit, 미완료 entrance의 exit, reset/replay, quality switching, removed-scene weak lifetime, pending shutdown을 확인했다. shared 종료 전 일부 Cards만 조기 None이 되는 경우는 없었다(`individual=0`). steady-state offscreen 종료 후 기본 render task 1개로 복귀했다.

동작 assertion과 정상/중단 exit 캡처를 함께 확인했다. 선택한 프레임에서 text reappearance/double draw는 관찰하지 않았다. 모든 subframe/모든 target에 대한 무결함 보증이나 full regression 결과는 아니다.

근거: sample-check.cpp (로컬 자료: `sample-check.cpp`), `sample-{0,1,2}-{0,1}.log`, [async contact](sample-async-contact.png).

## C. Final sample diff

HEAD 대비 `samples/text/text-effect-demo.cpp` **7 insertions / 117 deletions**, 파일 하나. early cleanup 전 HEAD^와 비교하면 already-None guard만 남는다. production rendering 코드는 변경하지 않았다. sample lifecycle이 quality별로 달라지지 않는다.

## D. Current packing policy

`BuildRuntimeRevealBlurBatches`는 line capture를 세로로 쌓는다. page width=max(line widths), height=sum(line heights).

- `pageArea <= 1.25 × occupiedArea`: occupied 대비 25% overhead. **페이지 자체의 빈 면적 비율 25%가 아니다**(상한은 20%).
- whole-Label fast path → 최대 4줄 greedy fallback → O(√N) balanced partitions / 동일 extent scratch sharing 정규화.
- ratio 검사 **네 곳**: whole acceptance, greedy append, balanced page acceptance, common-size normalization.
- max texture extent, 새 combined page의 1,048,576 pixels, publication safety, format grouping, halo를 그대로 유지했다. 이미 큰 단일 target의 기존 처리는 그대로다.
- MAX_LINES_PER_DRAW=64는 renderer/UBO chunk 제한이며 page 수 제한과 별개다.
- balanced search는 기존 greedy storage ceiling을 유지한다. HIGH는 equal-extent Source/H sharing, reduced paths는 Source retained/H sharing/V retained의 기존 allocation semantics를 유지한다.

네 ratio 식만 각 arm의 정수 비율로 바꿨다. shader, tap, radius, curve, clear, refresh rate, task dependency, metadata preparation은 변경하지 않았다. 계측은 외부 파일에만 추가했다.

## E. Recomputed breakpoints

실제 demo 1280×720, content width **339.333343506px**, R24, Sync, 같은 font 환경으로 native inventory를 다시 수집했다. source capture dimensions에는 radius halo가 이미 포함되어 있다.

| Label | line targets | occupied pixels | merged pixels | exact ratio | arm |
|---|---|---:|---:|---:|---|
| 11 | 383×71, 366×71, 155×68 | 63,719 | 80,430 | 1.262260864106 | A=1.2623 |
| 10 | 360×74, 123×69 | 35,127 | 51,480 | 1.465539328721 | B=1.4656 |
| 2 | 362×74, 96×70 | 33,508 | 52,128 | 1.555688193864 | C=1.5557 |

Label ID는 관측 식별자일 뿐 policy에 넣지 않았다. `build-variants.py`는 모든 Label에 같은 ratio를 적용한다.

## F. Actual threshold sweep

아래는 **Cards 12 Labels의 Source/H/V logical FBO payload**다. original text/metadata/CPU buffers, window/MSAA, 다른 scene Labels는 제외했다. RSS/VRAM residency/driver allocation 실측값이 아니다.

| Arm | Pages | Tasks/cameras | ECONOMY bytes | PERFORMANCE bytes | HIGH bytes |
|---|---:|---:|---:|---:|---:|
| BASE 1.25 | 15 | 45 | 517,635 | 603,494 | 1,377,693 |
| A 1.2623 | 14 | 42 | 535,077 | 623,843 | 1,424,205 |
| B 1.4656 | 13 | 39 | 553,374 | 645,239 | 1,473,264 |
| C 1.5557 | 12 | 36 | 574,224 | 669,664 | 1,529,124 |

각 page는 Source/H/V 3 tasks, 3 cameras, 3 FBOs. 이번 Cards 내에서는 scratch extent가 달라 FBO sharing이 없었다. H/V renderer 수는 30→28→26→24, Source renderer는 15→14→13→12. output batching도 page당 하나여서 줄어든다. 실제 60Hz GL trace에서는 page당 대략 draw 4개, clear 3개가 줄었다.

## G. Per-Label merge result

A는 id11, B는 id11+id10, C는 id11+id10+id2가 2→1 page가 된다. 나머지 9개 Label은 동일하다. id5는 RGBA8, 나머지는 A8. 모든 quality에서 page partition은 같다.

각 Label의 모든 Source/H/V extent·format, stage별 bytes는 [TABLES.md](TABLES.md), machine-readable 값은 inventory.json (로컬 자료: `inventory.json`). 총계는 allocation handle을 중복 제거해 계산했다.

## H. Absolute / relative memory waste

| Arm | ECONOMY Δ KiB / % | PERFORMANCE Δ KiB / % | HIGH Δ KiB / % |
|---|---:|---:|---:|
| A | 17.033 / 3.370% | 19.872 / 3.372% | 45.422 / 3.376% |
| B | 34.901 / 6.904% | 40.767 / 6.917% | 93.331 / 6.937% |
| C | 55.263 / 10.932% | 64.619 / 10.964% | 147.882 / 10.992% |

| Merge | merged−occupied pixels | occupied 대비 overhead | extra Source area vs BASE | unused A8/RGBA bytes in merged Source |
|---|---:|---:|---:|---:|
| id11 | 16,711 | 26.226% | 15,504 | 16,711 / 66,844 |
| id10 | 16,353 | 46.554% | 16,353 | 16,353 / 65,412 |
| id2 | 18,620 | 55.569% | 18,620 | 18,620 / 74,480 |

id11 BASE의 앞 두 줄 page에도 1,207 unused pixels가 이미 있었다. 따라서 merged waste와 **추가 allocation**은 다르다. 위의 마지막 열은 Source 한 plane만의 waste이며 pipeline 전체 증가는 quality별 위 표를 사용해야 한다.

## I. Source/H/V pixel workload

전체 target(clear) 면적은 BASE→C:

- Source 399,327→449,804 (+12.64%).
- HIGH H/V 각각 399,327→449,804.
- PERFORMANCE H 100,085→112,722, V 25,362→28,418.
- ECONOMY H/V 각각 25,362→28,418.

반면 draw geometry는 줄별 quad를 유지한다. nominal Source crop 면적 약135,743px, HIGH H band 약107,555px, HIGH V quads 383,415px로 동일하다. PERFORMANCE H 약26,960→26,956 / V 약24,366→24,242, ECONOMY H/V 약24,366→24,242다. 작은 차이는 ceil-reduced target ratio에서 온다.

**clear/allocation 면적 증가를 같은 비율의 Gaussian fragment 증가로 해석하면 안 된다.** H는 HIGH/PERFORMANCE에서 D2 coverage band, ECONOMY에서는 full line quad를 유지한다. page 오른쪽 빈 사각형 전체를 Gaussian으로 채우지 않는다.

draw 면적은 native line crop/band와 target scale로 구한 continuous geometry estimate다. pixel snap/raster sample coverage, fragment discard, tile resolve/cache/bandwidth까지 센 실제 GPU work는 아니다. Source estimate는 fixed control/crop transform 기준이다. [TABLES.md](TABLES.md)에 모든 arm의 clear/draw 면적을 분리했다.

## J. Quality matrix

실제 id2/id10/id11 corpus/font/line height와 소수점 width를 복제했다. 3 rows × A8/gradient RGBA 2 columns, 동일한 화면 위치/scale/MSAA. HIGH/PERFORMANCE/ECONOMY, R16/24/48, p=.20/.50/.75/.90를 비교했다. Unit PIXEL, PER_LINE, stagger .25, fade0, blur time1. 원래 Cards의 preset 길이 분류 대신 줄별 timing 차이가 드러나는 공통 fixture를 사용했다.

BASE/A/B/C의 276개 최종 static PNG는 144개 radius/progress matrix + reverse seek/WHOLE/endpoints/seek smoke를 포함한다. quality-diffs.csv (로컬 자료: `quality-diffs.csv`)는 3 Label × 2 formats ROI 비교. error는 8-bit RGB scale이며 background 포함 RMSE이므로 단독 품질 점수로 사용하지 않는다.

초기 캡처에서 X window map 이전 XGetImage 실패와 첫 clear 화면을 읽는 harness 문제가 있었다. production crash로 집계하지 않았다. 캡처에 viewable check를 넣고 첫 capture를 1초로 늦춰 R16/p=.20을 다시 확인했다. 원래 첫 images는 `initial-map-race/`에 보존했다.

최종 ROI 차이의 최대값(전체 radius/static/reverse/WHOLE/endpoint/seek matrix):

| Quality | Arm | 최대 RMSE /255 | 최대 channel difference /255 | 주요 차이 |
|---|---|---:|---:|---|
| HIGH | A/B/C | 0.0181 | 1 | rounding 수준 |
| PERFORMANCE | A | 0.3097 | 6 | id11 R24 p=.50 |
| PERFORMANCE | B/C | 0.7600 | 23 | id10 R24 p=.75 |
| ECONOMY | A | 0.8776 | 14 | id11 R24 p=.50 |
| ECONOMY | B | 1.5865 | 22 | id10 R24 p=.50 |
| ECONOMY | C | 2.4786 | 42 | id2 R24 p=.50 등의 profile 변화 |

HIGH는 sanity PASS. A의 reduced 경로는 작은 밝기/profile 변화가 있으나 확인한 1:1/확대에서 뚜렷한 새 grid/bleed/cutoff는 없었다. B/C는 짧은 줄의 blur 밝기·선명도 변화가 더 분명하므로 “visual non-regression 확정”으로 승격하지 않는다. 특히 C를 memory/CPU 수치만으로 production 권장하지 않는다. 새로운 품질 보정이나 filter tuning은 하지 않았다.

## K. Split-label zoom inspection

`comparisons/`의 파일은 BASE | A | B | C 순서, **2× nearest**, 색/밝기 보정 없음. 원래 1:1 PNG도 `quality-{arm}-{q}/`에 있다.

대표:

- [ECONOMY R24 p=.50 id11](comparisons/q2-r24-p1-id11.png): A merge에 따른 작은 line-local brightness/profile 변화.
- [ECONOMY R24 p=.50 id2](comparisons/q2-r24-p1-id2.png): C에서 짧은 마지막 줄의 blur 밝기/profile 변화가 더 큼.
- [PERFORMANCE R24 p=.75 id10](comparisons/q1-r24-p2-id10.png): reduced texel phase의 차이.
- [HIGH R24 p=.50 id11](comparisons/q0-r24-p1-id11.png): full-resolution sanity.

HIGH는 거의 동일하지만 reduced 경로는 합친 page의 extent/ceil와 line의 page 내 row 위치가 달라져 bit-identical하지 않다. 선택한 확대/1:1 captures에서 새 line bleed, halo 절단, 오른쪽 blank 영역의 독립 ghost는 관찰하지 않았다. B/C를 “알고리즘이 같으니 무조건 같은 품질”로 판정하지 않았다.

## L. PER_LINE correctness

stagger가 있는 static progress, .90→.75→.50→.20 reverse seeks, 0/1 endpoint와 1→.5 direct seek를 사용했다. 후반 줄이 먼저 밝아지거나 이웃 줄을 잘못 읽는 현상은 관찰하지 않았다. 각 arm/quality에서 같은 progress의 forward/reverse/seek PNG 비교 **60/60 identical**. p0는 모든 arm 동일, p1은 C의 id2에서만 최대1/255 rounding 차이가 있었다.

별도 BASE/A × 세 quality native 4초 0→1→0 KeyFrames animation **6/6 완료**. A8/RGBA를 함께 구동했고 마지막 progress0으로 수렴했다. 200ms 간격 native captures 132장을 저장했다. [ECONOMY animation contact](comparisons/animation-eco.png)는 위가 forward, 아래가 reverse, 왼쪽 BASE/오른쪽 A다. 자동 tick과 render frame이 달라 양쪽 progress가 약.008씩 다르므로 이 contact를 pixel-identical timing 비교로 쓰지 않았다. 검사한 frames에서 새 line bleed/비정상 재등장은 없었지만 매60Hz frame의 shimmer 부재를 증명하는 테스트는 아니다.

줄 texture의 clamp, atlas crop, halo reserve, logical output order를 변경하지 않았다. 이것은 선택 fixture에서의 확인이며 모든 layout의 증명은 아니다.

## M. Qualities / WHOLE / formats

WHOLE_TEXT R24 네 progress는 packing 분기를 쓰지 않으며 모든 arm/quality/format ROI가 BASE와 pixel-identical했다(216 ROI comparisons). A8뿐 아니라 각 split corpus의 gradient RGBA도 별도 column으로 비교했다. HIGH/full source/H/V와 PERFORMANCE/ECONOMY의 kernel/timing/curve는 각각 유지했다.

ImageSpan 확대, target test, full UTC/regression, shader 알고리즘 변경은 이번 Phase 2에서 하지 않았다.

## N. Ubuntu CPU screening

모든 arm×quality의 5회 측정이 완료됐다. 아래는 independent-run means의 평균, 단위 ms/frame. 범위는 표 아래에 별도로 제시했다.

| Quality | Arm | UpdateRender CPU | Δ vs BASE | Command queue CPU | Δ vs BASE | offscreen RenderScene CPU |
|---|---|---:|---:|---:|---:|---:|
| HIGH | BASE | 4.304 | — | 1.893 | — | 1.690 |
| HIGH | A | 4.270 | −0.77% | 1.787 | −5.62% | 1.599 |
| HIGH | B | 4.165 | −3.22% | 1.718 | −9.25% | 1.524 |
| HIGH | C | 4.112 | −4.46% | 1.617 | −14.61% | 1.430 |
| PERFORMANCE | BASE | 4.303 | — | 1.980 | — | 2.152 |
| PERFORMANCE | A | 4.373 | **+1.62%** | 1.931 | −2.46% | 2.193 |
| PERFORMANCE | B | 4.221 | −1.90% | 1.823 | −7.92% | 2.129 |
| PERFORMANCE | C | 4.022 | −6.53% | 1.771 | −10.57% | 2.050 |
| ECONOMY | BASE | 4.340 | — | 2.041 | — | 2.261 |
| ECONOMY | A | 4.186 | −3.55% | 1.940 | −4.94% | 2.182 |
| ECONOMY | B | 4.189 | −3.47% | 1.883 | −7.73% | 2.173 |
| ECONOMY | C | 3.992 | −8.02% | 1.786 | −12.50% | 2.074 |

Command CPU BASE→A의 per-run min–max:

- HIGH 1.850–1.987 → 1.763–1.804.
- PERFORMANCE 1.945–1.999 → 1.861–2.008: **범위가 겹치며 작은 효과**.
- ECONOMY 2.010–2.065 → 1.901–2.005.

PERFORMANCE의 전체 thread CPU는 A에서 4.084–4.598ms, BASE 4.268–4.371ms였다. A를 모든 quality에서 성능 non-regression이 증명된 후보라고 부르지 않는다. Command CPU 감소와 전체 CPU 감소는 같은 주장이 아니다. B/C의 command 방향은 더 명확하지만 quality/memory를 같이 봐야 한다.

전체 process CPU 누적(약2.94초): HIGH BASE775.1→A761.8→B746.5→C734.2ms; PERFORMANCE770.6→780.4→754.4→718.1ms; ECONOMY780.7→746.2→748.5→712.9ms. FPS/cadence는 모든 arm 평균 약16.68ms이고 p95 약17.4–18.0ms라 PC FPS 이득으로 주장할 수 없다.

GL draw 평균 약90→86→82→78, clear 약55.3→52.3→49.3→46.3. 이것은 전체 scene의 실제 trace counter이며 Cards 외 초기 scene-text tasks 때문에 Cards-only inventory보다 크다. 각 run 결과/분산은 bench-summary.txt (로컬 자료: `bench-summary.txt`), bench-aggregate.json (로컬 자료: `bench-aggregate.json`), bench-summary.json (로컬 자료: `bench-summary.json`).

방법: 현재 Core/Adaptor/UI Foundation/Components source를 **외부 디렉터리에 -O2 -g -DNDEBUG로 빌드**, install하지 않았다. Core/Adaptor의 기존 DEBUG_ENABLED 설정은 유지했다. 따라서 production Release 빌드의 절대 시간으로 부르지 않는다. 기존 third-party `glyphy-arcs.cc` object만 원래 빌드 것을 재사용했다. 모든 arms에 동일하다.

GTX1650 / NVIDIA595.91.07 / GLES / MSAA4 / 1280×720. 각 arm×quality **5 independent warm runs**, 총60개 process. 각 process에서 Cards entrance를 3.9초 warm-up한 다음 reset하여 재생하고, 재시작 후 약.25~3.2초를 측정한다. group FinishedSignal(약3.44초) 이전이다. arm/quality 순서를 반복마다 회전했다. screenshot과 GPU readback은 성능 구간에서 하지 않았다.

비교 대상은 실제 demo scene 전체다. Cards 외 scene heading/background/transition도 포함하며 동일하다. Cards-only inventory 45→36 tasks와 전체 scene runtime task count를 혼동하지 않는다. 예를 들어 BASE 측정 시작/끝의 전체 task는61/46이다(기본 window task 포함).

외부 계측 scope:

- `CLOCK_THREAD_CPUTIME_ID`, swap-to-swap: combined UpdateRender thread의 실제 CPU. GPU waiting wall time과 다르다.
- `UpdateManager::Update`: update scope CPU.
- `RenderManager::RenderScene(..., renderToFbo=true)`: offscreen RenderScene scope CPU. 순수 task processing만의 시간은 아니다.
- `EglGraphicsController::ProcessCommandQueues`: GLES command queue drain의 CPU. offscreen+onscreen 명령, driver submission CPU를 포함한다. GPU 실행시간 아님.
- glDraw*/Clear/BindFramebuffer counters 및 swap wall cadence.
- `CLOCK_PROCESS_CPUTIME_ID`: 측정 창의 모든 프로세스 thread CPU 누적.

scope는 서로 포함/중첩될 수 있으므로 합산하지 않는다. frame당 scope duration은 측정 창의 총 CPU/frame 수다. fixed overhead의 방향을 보는 screening이고 target FPS 예측이 아니다. loop 안 logging/file I/O/GPU finish를 넣지 않았다. instrumentation 오버헤드의 절대 제거 측정은 하지 않았고 모든 arms 동일하다.

## O. Synthetic large-page risk

기존 함수 body를 verbatim 추출하고 ratio만 바꿔 720개 deterministic cases를 계산했다. 2/3/4줄, width ratio 1/.75/.5/.25/.1, short-tail/one-wide 패턴, 128×64 / 512×256 / 1024×512 기준 targets, max extent16384/1024. 모든 새 combined page의 extent/1M cap assertion 통과. synthetic.json (로컬 자료: `synthetic.json`), synthetic-summary.txt (로컬 자료: `synthetic-summary.txt`).

이 coarse width grid에서는 A 변화가 0건이다. **A가 일반적으로 비용 증가를 안 만든다는 뜻이 아니다.** 추가 boundary case로 확인했다:

`1024×512 + 604×512` → merged `1024×1024`, ratio1.25799. BASE는 2 pages, A/B/C는 1 page. RGBA:

- Source extra215,040px =840KiB.
- HIGH +2,580,480 bytes =**2.461MiB** (+25.80%).
- PERFORMANCE +1,128,960 bytes =1.077MiB.
- ECONOMY +967,680 bytes =.923MiB.

실제 id11 targets를 3배 scale한 경우도 A에서 HIGH +1.597MiB가 된다. near-breakpoint.json (로컬 자료: `near-breakpoint.json`).

B/C는 기본 synthetic large grid에서도 HIGH +3MiB / PERFORMANCE +1.3125MiB / ECONOMY +1.125MiB인 case를 허용한다. medium 3줄 one-wide, ratio.5의 C는 payload가 +50%다. 이는 한 Label 단위의 logical bytes이며 다중 Label에서는 더해진다.

## P. Global threshold feasibility

1M page cap은 **각 surface의 절대 크기**를 제한하지만 “기존 split 대비 추가한 bytes”는 제한하지 않는다. 따라서 hard cap을 유지했다는 사실만으로 global ratio 완화의 memory trade-off가 충분히 작아지는 것은 아니다.

Cards에서는 A가 가장 작은 비용으로 page 하나를 제거한다. B/C의 command savings도 측정할 수 있지만 더 큰 empty area와 reduced-path phase 차이를 받아들여야 한다. 지금 자료만으로 global 1.47/1.56을 product policy로 올리지 않는다. A도 전체 product의 memory 상한으로 단정하지 않는다.

## Q. Absolute-waste follow-up value

후속 local PoC 가치가 있다. 현재 ratio를 기본으로 두고 **추가 bytes/pixels가 작은 merge만 허용**하는 한 가지 bounded 후보를 보는 것이다. 상대 비율만 쓰는 것보다 이번 목적과 직접 맞는다.

예: 이번 id11 merge의 추가 Source15,504px를 허용하되 위215,040px case는 거부. format/quality/scratch sharing에 따라 pipeline bytes가 다르므로 최종 기준은 그 계산과 일관되어야 한다. 이번 단계에서 새 cache나 복잡한 search/heuristic은 구현하지 않았다.

## R. One recommended candidate

**SIMPLE GLOBAL THRESHOLD IS NOT APPROPRIATE; INVESTIGATE BOUNDED ABSOLUTE-WASTE MERGE.**

현재 production은1.25 유지. 후속 PoC는 A 수준의 작은 merge부터, absolute allocation increase를 제한하는 후보 하나만 검토한다. B/C/global1.56 병행 최적화는 추천하지 않는다.

## S. Exact next local step

새 기능이나 shader 변경 없이 위 bounded 후보를 외부 packing harness에만 넣을 가치가 있다. 우선 이번 Cards의 id11은 허용하고 scaled/near-boundary large case는 거부하는지 확인한다. 그다음 이번과 같은 reduced sampling quality matrix에서 acceptable한지 판정한다. PASS 전 production patch/target 요청으로 넘어가지 않는다.

이번 작업에서 target build/test/사용자 target 확인 요청은 하지 않았다.

## T. Git / artifact state

`devel_blur_text` HEAD unchanged. UI unstaged 변경은 Phase1 sample 파일 하나뿐. Core/Adaptor working trees unchanged. gaussian-blur-algorithm.h/.cpp 및 production packing/shader/API는 변경 없음. commit/amend/rebase/reset/restore/stash/push 없음. 별도 git worktree/branch도 만들지 않았으며 이 외부 directory의 libraries로만 PoC했다.

`git diff --check` 통과. installed library를 교체하지 않았다. 일반 실행은 sample cleanup + 원래1.25 production library를 사용한다. 외부 PoC는 해당 arm directory를 LD_LIBRARY_PATH 앞에 둘 때만 활성화된다.

재현/원자료: build-variants.py (로컬 자료: `build-variants.py`), run-captures.py (로컬 자료: `run-captures.py`), run-bench.py (로컬 자료: `run-bench.py`), build-meter.py (로컬 자료: `build-meter.py`), bench-summary.json (로컬 자료: `bench-summary.json`), [TABLES.md](TABLES.md).
