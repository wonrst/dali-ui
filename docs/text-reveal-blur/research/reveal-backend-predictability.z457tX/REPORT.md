# Reveal Blur: backend overhead audit / PC→TV predictability

2026-09-18. Production code 변경 없음. 새 target 계측/build/install 없음.

## A. Executive verdict

**SAME-CONTEXT WAIT FAST PATH ALREADY EXISTS.**
**BOUNDED FLUSH COALESCING IS A REVIEW CANDIDATE — NOT YET VALIDATED.**

Source/H/V의 ordinary FBO 작업은 같은 resource context에서 실행되고, 같은 context의
texture read에서는 이미 wait를 하지 않는다. 따라서 단순 same-context `return` 추가로
현재 DependencySync 비용이 사라지는 것은 아니다.

다만 pass마다 fence를 만든 뒤 `glFlush`하는 부분은 **ordinary 연속 구간의 마지막 제출로
합칠 여지**가 있다. fence/tracking은 그대로 두고 producer-context 경계에서 flush하는
작은 generic 후보가 우선이다. 이번에는 안전 조건/위치/검증 계획만 정리했다.

**PC→TARGET: C. DIRECTIONAL SCREENING ONLY.**

- Production/Source-only의 command CPU 방향은 PC와 TV가 일치한다.
- 단일 `TV = K × PC` 모델은 부적합하다. 신규 동일 상세 계측에서 Update의 K는
  약1.2, Draw는17–19, FBO initialization은12–20이다.
- 독립적인 정량 paired variant는2개뿐이다. 반복 cycle을 architecture 표본으로 늘려
  회귀계수를 추정할 수 없다. 절대 시간/FPS와 일반적인 개선률을 예측하지 않는다.
- A-R는 host draw GPU **−35.2%**인데 TV에서 더 느렸다는 반례다.
  GPU draw timer 단독은 이 target workload의 primary predictor에서 제외한다.
- 구조 감소 + PC command CPU + setup 비악화 + 품질/수명 검증을 조합해 후보를 거르는
  workflow는 가능하다. 이것도 target 성공 보증이나 검증된 확률 모델은 아니다.

상세 source audit: [BACKEND_AUDIT.md](BACKEND_AUDIT.md).
모든 새 수치/분포: [METRICS.md](METRICS.md), RESULTS.json (로컬 자료: `RESULTS.json`),
cycle-metrics.csv (로컬 자료: `cycle-metrics.csv`).

## B. Exact GLES dependency graph

```text
resource context R
  S write → H reads S / writes H → V reads H / writes V
       │                                      │
       └──────────── R → W_i sync ────────────┤
                                              ▼
surface context W_i                 Output reads S + V
                                              │
                         post-present backward fence
                                              ▼
resource context R                 next-frame S/V overwrite
```

Core가 정렬된 RenderTask/instruction을 command buffer로 만들고,
Adaptor가 offscreen submission을 resource context에서 처리한다. 위 그림은 resource/surfaceless
context 지원 분기이며, 그 뒤 같은 scene의
window output으로 넘어간다. 다중 window는 이 쌍이 반복된다.
CPU의 `EglGraphicsController::Flush`는 queue drain이며 GL `glFlush`와 같은 함수가 아니다.
[실제 함수 순서와 링크](BACKEND_AUDIT.md#1-실제-실행-순서).

## C. Context ownership

| 작업 | Context | 판단 |
|---|---|---|
| ordinary S/H/V | 동일 resource R | 중간 edge마다 cross-context wait 불필요 |
| resource upload | R | upload queue와 command queue 순서 유지 |
| onscreen output | 별도 surface W_i | S 및 V 둘 다 cross-context consumer가 있음 |
| 여러 window | R→W_1→R→W_2 가능 | window별 경계 유지 |
| native/external / own-context callback | ordinary chain과 별도 계약 | 첫 후보에서는 보수적 기존 경로 |
| 다음 frame overwrite | W_i read→R write | backward fence/check 제거 금지 |

같은 RenderThread라는 이유로 같은 context라고 판단하지 않았다.
`MakeContextCurrent`의 동일-context skip도 이미 존재한다.
Resource-context 미지원 fallback도 존재하므로 distinct R/W를 모든 GLES 환경에 일반화하지 않는다.
기존 TV trace는 actual EGLContext handle이나 capability를 기록하지 않았다.
따라서 위 내용은 실행되는 **코드 분기 계약**이며 runtime handle을 직접 계측했다는 뜻은 아니다.
첫 후보는 resource-context 지원이 확인되는 ordinary 경로만 대상으로 하고 다른 경로는 보존한다.

## D. Sync/fence audit

핵심 확인:

1. `AddTextures`는 FBO end마다 forward attachment/producer를 기록하고 fence를 생성한다.
2. `CheckNeedsSync`는 producer/consumer context가 같으면 **이미 wait와 backward read 기록을 생략**한다.
3. `CheckFramebufferNeedsSync`는 current/previous backward maps를 검사한다.
4. surface output 후 `MarkFramebufferTextureRead`는 읽은 texture들을 한 backward fence로 묶는다.
5. `FreeSyncObject`/Reset/age/discard는 수명 관리와 결합된다. sync 개수를 줄이는 것과
   wait ID를 안전하게 공유하는 것은 별개의 변경이다.

Source는 H뿐 아니라 최종 Output에서도 읽힌다. H consumer만 보고 S fence를 삭제하는
방식은 안전하지 않다. H-only fence가 보수적으로 생성되는 것은 맞지만, generic backend에는
“이 texture는 앞으로 절대로 cross-context 소비되지 않는다”는 확정 정보가 없다.
[same/cross, current/previous 함수 행렬](BACKEND_AUDIT.md#4-함수별-감사).

## E. RenderPass / glFlush audit

`Context::EndRenderPass`: texture unbind → AddTextures/fence → **glFlush** →
depth/stencil invalidate → FBO unbind/cache reset.

Ordinary same-context S→H→V에는 pass마다 flush할 필요가 없을 수 있다. 그러나 cross-context
wait 전에 producer stream을 제출해야 한다. consumer context의 flush로 이를 대신할 수 없다.
이 구분은 [Khronos fence contract](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_fence_sync.txt)와
현재 DALi context 전환 구조 양쪽에 근거한다.

첫 후보는 fence/수명/상태 reset을 건드리지 않고 **bounded ordinary command-drain**에서
flush만 합치는 것이다. context/native/completion 경계 및 drain 종료 전에 반드시 제출한다.
Offscreen-only/REFRESH_ONCE에서도 진행되어야 한다. `RenderTracker` sync 생성 이후의 제출,
readback, native callback도 별도 boundary다. “glFlush 전부 삭제”가 아니다.

명세상 중복 가능성과 성능 이득은 구분한다. 제출을 미루면 GPU와 CPU의 overlap이 줄거나
driver batching 양상이 바뀔 수 있어 **더 느려질 가능성도 측정해야 한다**.

## F. Safe optimization candidates

아래의 safe는 *명시한 경계를 지키는 설계 후보*를 뜻한다. 구현 승인/회귀 통과를 뜻하지 않는다.

| Candidate | Structural reduction | Correctness risk | Scope | Recommendation |
|---|---|---|---|---|
| ordinary R-chain flush coalescing | eligible drain의 pass-end flush N→1; fence/check/pass 수 동일 | 경계 누락, FBO-only 진행, native release | generic Adaptor, 제한된 context/drain 상태 | **우선 PC PoC 하나** |
| boundary-lazy/shared forward fence | 조건부 N fences→epoch당1 가능 | 여러 소비자, last-write generation, shared ID 수명 | dependency ownership까지 변경 | 후순위; 간단한 if로 구현 불가 |
| same-context read에 early return | 이미 구현 | 새 이득 불명 | 현재 CheckNeedsSync | 추가 작업 불필요 |
| FBO name-only recycling | Gen/Delete 일부 | attachment/state/cache/inflight | generic resource lifecycle | 시간 근거 부족, 보류 |
| same-owner full attachment reuse | hit 때 초기화/texture allocation 회피 가능 | async overlap, size/radius 변화, memory retention | owner bounded resources | hit rate/비용 미확인, 보류 |
| unbind/state reset 생략 | 일부 GL/state calls | native/feedback/cache coherence | draw state 전체 계약 | 이번 우선순위 아님 |

기존 TV steady 평균은 RenderPass102.42 blocks/frame = 약51.21 passes/frame이다.
단일 ordinary drain이라는 조건에서 pass-end flush는 약51.21→1로 줄일 **잠재력**이 있다.
fence allocations, dependency checks, RenderPass bookkeeping, FBO 생성은 **0개 감소**다.
이는 코드를 통한 환산이며 glFlush counter 실측이 아니다. flush 효과를 해당 category의
현재 ms에 비례시켜 계산하지 않는다.
[correctness matrix와 구조적 상한](BACKEND_AUDIT.md#5-flush-후보-무엇을-합칠-수-있는가).

## G. Framebuffer initialization analysis

`InitializeResource`는 Gen/Bind/AttachTexture/DrawBuffers/optional attachments/unbind/cache를
포함한다. `glCheckFramebufferStatus` 직접 호출은 이 함수에 없다. driver validation의
내부 비용까지 독립 계측한 것은 아니다. 반복된 동일 live object는 이미 `mInitialized`로 skip한다.

FBO name만 재활용하면 Gen/Delete는 줄일 수 있지만 새 texture를 붙이는 비용은 남는다.
같은 attachment까지 유지해야 큰 초기화 부분을 생략할 수 있고, 그때는 retained texture memory와
publication overlap이 문제가 된다. Generic `TryRecycle` 기본 false, Framebuffer override 없음.
기존 old-resource 인자를 전달하는 것만으로 재활용되지 않는다.

이번 새 PC 측정에서도 정상 퇴장 생성량은 **57 vs19**, 기존 TV와 동일했다.
그러나 FBO init batch CPU의 PC/TV 배율은 매우 다르다. Gen 하나가 원인이라는 근거가 없으므로
pool을 먼저 만드는 것은 권하지 않는다. [수명/compatibility 행렬](BACKEND_AUDIT.md#7-framebuffer-initialization--recycling).

## H. Existing PC↔Target paired dataset + new PC runs

### 기존 자료 분류

| Variant | PC evidence | TV evidence | paired 해석 |
|---|---|---|---|
| Production | coarse trace + **이번 detailed5 warm runs** | coarse4 + detailed4 cycles | 정량 category 비교 가능; 기기/profile/layout 완전 동일은 아님 |
| Source-only | 위와 동일 | 위와 동일, 대부분60fps/동시제거약52 사용자 관찰 | 정량 비교 가능; 품질 동일 optimization 후보는 아님 |
| V-only | native inventory/동작 smoke | production보다 소폭 개선 관찰 | 정성 paired만; command CPU 숫자 없음 |
| H/V1tap | native inventory/동작 smoke | V-only와 거의 비슷 | 정성 paired만; tap만 줄여도 target 좋아진다는 근거 없음 |
| A-R/prefilter | native3-run GPU .716068→.464271ms; CPU neutral | 이전 PERFORMANCE보다 느림 관찰 | **GPU predictor 반례**, TV 정량 CPU/오차 산출 불가 |
| BlurEffect | native inventory, 앞선 PC 비용 비교 | Reveal보다 낫지만 상당한 frame drop | 정성 paired; 같은 상세 CPU trace 없음 |
| earlier Source batching | host 다수 CPU/GPU/quality runs | 해당 report에서 target 검증 미완료 | paired quantitative sample로 채택하지 않음 |

재사용한 주요 자료:
[coarse PC](../reveal-critical-path.CFOTks/REPORT.md),
[coarse TV](../reveal-target-attribution.01tPcM/REPORT.md),
[detailed TV](../reveal-command-results.BftCT1/REPORT.md),
[direction review](../reveal-direction-review.QxzXQ5/REPORT.md),
[inventory](../reveal-pipeline-count.a9GqcW/REPORT.md),
[A-R native](../reveal-ar-production.FY6dsQ/REPORT.md),
[Source batching](../reveal-source-production.hWQHqE/REPORT.md).
외부 A-R feasibility의 −45.9%와 native −35.2%는 서로 다른 실험이므로 섞지 않았다.

### 이번 PC 방법

- Ubuntu / GTX1650 / NVIDIA595.91.07 / GLES / window1280×720 / MSAA4.
- 기존 Text Effect Demo, PERFORMANCE / Strong / Sync. 자동화는 기존 `OnPrimaryAction`과
  `OnBackAction` 호출만 수행. 렌더링/animation 설정이나 source shader는 변경하지 않음.
- Production5 + Source-only5 **독립 process**. 실행 순서는 반복마다 교대.
- 각 process에서 Cards 등장→ready→2초 유지→정상 퇴장 두 cycle.
  첫 cycle은 warmup으로 제외, **두 번째 cycle을 표본1개**로 사용.
- steady: CardsStart+1초부터+3초까지 시작한 frame. 각 run120개, variant당600개.
  frame별 CPU 평균을 run마다 구한 뒤5개 평균의 median/p25/p75/p95를 산출했다.
  600frames를600개의 독립 실험으로 취급하지 않는다.
- target과 **동일 detailed collector/category scopes/parser**. CPU는 combined Update/Render
  thread CPU이고 process 전체 CPU나 GPU 시간이 아니다. nested scope 합산 금지.
- offscreen commands는 parent interval union; category는 exclusive + Other residual.
- exit tail: ExitStart+300ms 이후. exit create: 동일 정상 exit 중 FBO init items가 가장 많은
  frame. 기존 max-wall spike 정의도 별도 보존했다.
- **PC의 max-wall frame은 대부분 FBO 생성 frame이 아니다.** 이를0ms 생성으로 해석하면
  틀리므로 실제 생성 frame의57/19 items를 확인해 비교했다.
- Core329 / Adaptor380 / Foundation471 objects를 private directory에서 새로 빌드.
  기존 CMake flags 사용: PC Core/Adaptor Debug, UI build type empty, **-O 최적화 flag 없음**.
  Source-only는 동일 objects 중 기존 variant 차이가 있는 runtime object만 교체한다.
  Components는 설치된 동일 라이브러리를 공유한다. `/proc/PID/maps`로 로딩 경로를 저장했다.
- PC/TV는 같은 ABI·optimization·폰트·window를 완전히 복제한 빌드가 아니다.
  TV에는 window/font 설정 기록이 없고 PC 평균 pass53.375 vs TV51.231처럼 실제 작업량도 다르다.
  따라서 K는 순수 hardware calibration이 아니라 **이 두 환경/빌드/부하 사이의 관측 배율**이다.
- PC/TV trace dropped/unmatched/open/allocation_failed/clock_failed 모두0.
  PC p95는5개 run 사이 descriptive quantile이지 target prediction CI가 아니다.
- 새 target run, GPU timer, framebuffer readback, sanitizer, broad UTC는 하지 않았다.
  Optional V-only/1tap/BlurEffect의 새 PC category 측정은 수행하지 않았다. 대응 TV quantitative
  자료가 없으므로 현재 transfer model 표본 수를 늘리지 못한다.

수집/빌드 재현: build_pc.py (로컬 자료: `build_pc.py`), run_pc.py (로컬 자료: `run_pc.py`),
summarize.py (로컬 자료: `summarize.py`), build commands (로컬 자료: `build-commands.json`),
run environment/return codes (로컬 자료: `run-records.json`), raw captures (로컬 자료: `captures/`).

### PC warm5: median [p25, p75], p95 (ms/frame)

| Scope | Production | Source-only |
|---|---:|---:|
| thread total | 7.370 [7.339,7.429],7.470 | 3.688 [3.684,3.709],4.017 |
| Update | 1.000 [.986,1.020],1.024 | .710 [.699,.710],.735 |
| offscreen render | 4.630 [4.613,4.659],4.659 | 1.608 [1.602,1.616],1.897 |
| offscreen commands | 3.289 [3.270,3.290],3.342 | 1.155 [1.142,1.157],1.407 |
| exit FBO creation batch | 2.264 [2.251,2.290],2.356 | .850 [.845,.860],.904 |

Source-only 일부 run에서 CPU가 높게 관측되었으며, 해당 값을 임의로 제외하지 않았다.
모든 category와 exit-tail 분포는 [METRICS.md](METRICS.md)에 있다.

## I. Transferability by metric

| Metric | 분류 | 근거/제약 |
|---|---|---|
| logical task/FBO/graph topology | HIGH, 같은 resolved layout 조건 | architecture 정의; 페이지/tiling/font 변화 시 절대 count 달라짐 |
| RenderPass/Draw/state block count | HIGH conditional | PC RP106.75/35.58 vs TV102.46/34.03; P/S 약3배 관계 보존 |
| dependency graph edges | HIGH conditional | 같은 resource graph라면 보존; **DependencySync block count와 edge 수는 다름** |
| FBO create attempts | HIGH, 같은 publication | 실제 exit 양쪽57/19 |
| logical target pixels / texture bytes | HIGH arithmetic, MEDIUM 실제 전이 | format/scale/packing 같을 때; VRAM/RSS/driver allocation 보증 아님 |
| shader sample expressions | HIGH code count, LOW latency predictor | 1tap 및 A-R 반례 |
| PC command/thread CPU ratio | MEDIUM, screening 보조 | P/S 방향 일치; category/variant별 K 다름 |
| category absolute CPU / fence/Bind/Flush/FBO latency | LOW | driver, ABI, frequency, instrumentation 영향; GL별 독립 실측 없음 |
| GPU draw time / FPS | LOW for this target | A-R 방향 반대; vsync ceiling와 CPU bottleneck |

작업량 벡터에는 counts뿐 아니라 pixels/format/새 allocation도 포함해야 한다. 다만 이번 TV
trace에는 per-stage dimensions/bytes가 없어 전체 W를 정량 회귀에 사용하지 않았다.

## J. Ratio / scaling analysis

새 표는 **PC5개 warm-cycle 평균의 median**, TV기존4개 cycle 평균의 median이다.
이전 detailed report의 frame-weighted mean22.275/7.089와 요약 방식이 달라 TV median은
23.051/7.255다. 원본 자료를 다시 측정하거나 수치를 수정한 것이 아니다.

| Scope | PC P/S ms | TV P/S ms | PC P/S ratio | TV P/S ratio | K=P, S |
|---|---:|---:|---:|---:|---:|
| Update | 1.000/.710 | 1.202/.902 | 1.409 | 1.332 | 1.20,1.27 |
| thread total | 7.370/3.688 | 32.716/14.072 | 1.998 | 2.325 | 4.44,3.82 |
| offscreen | 4.630/1.608 | 24.719/7.886 | 2.879 | 3.134 | 5.34,4.90 |
| offscreen commands | 3.289/1.155 | 23.051/7.255 | 2.847 | 3.177 | 7.01,6.28 |
| exit create queue | 2.538/1.045 | 46.531/11.529 | 2.428 | 4.036 | 18.33,11.03 |
| exit FBO batch | 2.264/.850 | 44.541/10.250 | 2.663 | 4.345 | 19.68,12.06 |

Category별 K (TV/PC):

| Category | Production | Source-only |
|---|---:|---:|
| DependencySync | 7.20 | 6.09 |
| RenderPass | 4.91 | 4.33 |
| Draw | 18.54 | 16.75 |
| BufferUniform | 3.40 | 3.62 |
| TextureSampler | 6.36 | 6.92 |
| PipelineState | 5.45 | 6.24 |
| Other residual | 11.78 | 11.68 |

**Global K는 기각한다. Category별 K도 새 variant의 absolute predictor로는 미검증이다.**
TV 첫 cycle을 제외한 warm3에서도 command median23.283/7.313, ratio3.184로 결론은 같다.
추가로 PC count가 약4% 더 많은데도 TV CPU는 크게 높다. count mismatch만으로
이 배율 차이를 모두 설명할 수 없다.

### 요청한 기존 coarse 숫자

| Scope | PC P/S ratio | TV P/S ratio | K production | K source-only |
|---|---:|---:|---:|---:|
| Update (1.00/.70 vs1.20/.87) | 1.429 | 1.379 | 1.200 | 1.243 |
| commands (3.91/1.54 vs16.62/4.81) | 2.539 | 3.455 | 4.251 | 3.123 |

두 번째 ratio 오차는 약−26.5%다. 하지만 이전 PC는 full entrance median/host smoke이며
TV는 entrance+1–3초 평균이고, PC command scope는 window 포함 값이다.
**이 표는 주어진 역사적 수치의 산술 확인이지 엄밀한 paired calibration이 아니다.**
이번 detailed 동일 offscreen scope 표를 주 비교로 삼았다.

## K. Mathematical model feasibility

선택된 target steady4cycles×2variants, 열은
`[1, N_RP, N_dep, N_draw, N_buffer, N_texture, N_pipeline, N_FBOcreate]`.

- 8행×8열이지만 **rank3**. independent architecture는2개다.
- variant별 median만 사용하면2행×8열, rank2.
- RP=2Draw, Buffer=RP, Pipeline=RP, Texture=Draw가 데이터에서 정확히 성립한다.
- 이 다섯 count의 correlation≈1. Dependency와도0.999999606.
- steady FBOcreate열은 모두0. 당연히 생성 계수를 식별할 수 없다.
- constant 제외 표준화 matrix도 rank3. singular values는
  `6.9282, 2.8284, .002291, ~7.7e-16, ...`.
  full-column-rank가 아니므로 condition number는 **무한대**.
- 작은 nonzero 축은 cycle 경계/활성 pass 차이를 반영할 뿐 independent optimization이 아니다.
- source/H/V pixels까지 넣으면 관측 없는 열과 parameter 수만 늘어난다.

따라서 `C_rp/C_dep/C_draw/C_fb/C_pixel`를 fit하지 않았다.
CPU/category를 해당 count로 나눈 값도 driver work의 causal marginal cost가 아니다.
steady와setup을 한 회귀식에 섞지 않았다. 원 matrix/SVD/correlation (로컬 자료: `RESULTS.json`).

## L. PC predictor

**수치 모델은 없음.** 대신 다음의 비확률적 screening vector를 제안한다:

```text
architecture: pass / FBO / cross-context edges / exact flush+fence calls / pixels
setup: actual create attempts / publication overlap / retained bytes
PC execution: command CPU / thread CPU / update CPU / actual create CPU
correctness: exact output / timing / ownership / native & multi-window lifecycle
```

weights를 임의로 주어 하나의 점수로 합치지 않는다. Pareto-style로
“어떤 비용이 줄고 어떤 비용이 늘었는가”를 보며, 비용 증가가 있으면 별도 근거를 요구한다.
FBOflush 후보처럼 pass/check count가 그대로여도 **실제 redundant GL call** 감소는 vector에
포함할 수 있다. 기존 category count만으로 그 개선을 검증했다고 주장하면 안 된다.

ordering 검증 범위:

- Source-only vs Production: PC command/total CPU와 target 방향 일치.
- V-only/1tap: topology 거의동일, target 개선 작음. count-screen은 큰 승리를 예고하지 않음.
  이것은 PC CPU ordering을 새로 측정했다는 뜻은 아니다.
- BlurEffect: 등장 cards36 vs45tasks는 설명 일부지만 퇴장36vs36이다.
  pass count **하나**로 BlurEffect 우위를 전부 설명하지 못한다.
- A-R: offscreen3→4, GPU win/target loss. host GPU 단독 predictor 명확히 부적합.
- earlier batching: 대상 paired target 정량 결과 부족; 성공 사례로 model에 넣지 않음.

## M. Prediction validation error

일반화 가능한 CI/relative speedup band는 만들지 않는다. 가능한 것은 **2점 K의 실패 진단**뿐이다.
Production의 K로 Source-only를 예측하고 반대로도 계산하면:

| Scope | train P→predict S error | train S→predict P error |
|---|---:|---:|
| Update | −5.5% | +5.8% |
| thread total | +16.3% | −14.0% |
| offscreen commands | +11.6% | −10.4% |
| create queue | +66.2% | −39.8% |
| FBO init batch | +63.2% | −38.7% |

이는 충분한 variant를 가진 leave-one-out validation이 아니라 두 점 사이의 불일치 확인이다.
이 observed error를 앞으로의±오차 범위로 쓰지 않는다. A-R의 정량 TV latency가 없으므로
반대 방향이라는 사실 이상으로 오차값을 발명하지 않는다.
FPS=1000/CPUms 환산도 하지 않는다. Vsync/pacing/GPU/compositor/system load가 남는다.

## N. Future PC screening rules

**우선 탈락/보류 조건** — 이 TV의 현재 bottleneck에 대한 규칙이지 모든 제품의 일반 법칙은 아님:

- 품질/Reveal semantics/ownership이 달라지는 후보는 performance 숫자와 무관하게 탈락.
- pass/FBO/dependency/create가 증가하고 근거가 host GPU win뿐이면 보류: A-R 재발 방지.
- pass bundle 그대로 tap만 줄이는 후보는 현재 TV의 주요 최적화 후보에서 후순위.
- PC command/create CPU가 반복 변동을 넘어 일관되게 악화되면 target build 전 재검토.
- valid case가 섞인 평균으로 특정UX/setup regression을 가리지 않는다.

**승격 조건**:

1. 변경 대상 work가 실제 줄어든다. pass/count 또는 정확한 GL call 등 해당 지표로 입증한다.
2. 다른 pass/FBO/edge/create/retained memory가 늘지 않거나 명확한 bounded trade-off를 제시한다.
3. 동일 품질과 semantics, lifecycle correctness를 PC에서 통과한다.
4. PC command/thread CPU는 비악화, 목표scope에는 반복 가능한 개선이 보인다.
5. creation/warm exit를 별도 검사한다. GPU timing만 좋아진 것으로 승격하지 않는다.

본 기준은 generic flush 후보의 **다음 PC PoC gate**이지 이번에 후보가 통과했다는 결과가 아니다.

## O. Target escalation rule

Static audit → private PC correctness/count check → repeated PC CPU/setup gate →
가장 근거가 강한1~2개만 최종 target 확인.

PC에서 위 gate를 통과하더라도 실제 상품화 승인에는 TV 확인이 필요하다.
특히 flush는 tile GPU/driver 제출 시점과 concurrency를 바꿀 수 있다. target 검증을 없애는
모델이 아니라 후보10개를 전부 target에 올리지 않도록 줄이는 절차다.
**이번 작업에서는 추가 target 측정/설치/GBS를 요청하거나 수행하지 않았다.**

## P. Recommended next engineering action — 하나

**Generic GLES의 bounded ordinary-resource-chain glFlush coalescing을 private PC PoC로 검증한다.**

fence allocation/dependency tracking/RenderTask/FBO/shader는 유지하고 flush 제출 경계만 검토한다.
첫 검증에는 one-page chain, multi-page/scratch reuse, next frame, multi-window, FBO-only,
REFRESH_ONCE, native/external fallback, readback/RenderTracker, disconnect/reconnect/None/destroy,
async replacement를 포함한다. 기존 image/blur clients에도 똑같은 generic 규칙이 적용되는지 본다.
정확한 출력 비교와 **실제 flush count**, command CPU, setup/frame tail을 확인한다.

구현이 예외 branch를 과도하게 늘리거나 CPU/GPU overlap 손해가 나오면 이 후보도 폐기한다.
fence sharing 및 FBO pooling으로 바로 범위를 넓히지 않는다.

## Q. Explicitly rejected directions

- fence/flush/dependency 무조건 제거, Reveal 이름 기반 backend 예외.
- consumer flush가 producer를 제출할 것이라는 가정.
- SyncPool 수명/현재·이전 frame map 생략.
- 새 blur algorithm/2-stage/Vogel/Kawase/mipmap/tap 조정.
- 새 camera/constraint 최적화, global FBO pool, cross-Label live sharing.
- category ms를 없앨 수 있는 비용으로 간주, count×ms로 speedup 예측.
- PC GPU 개선=TV FPS 개선, 임의 global K, rank-deficient regression/가짜 CI.
- Source-only를 동일 품질의 production 대안으로 제시하는 것.

## R. Git / preservation

원래 repo의 source/index/HEAD/branch에 쓰지 않았다. 기존 dirty worktree는 그대로다.

| Repo | HEAD | 기존 수정 보존 |
|---|---|---|
| Core | 8228720460a4910151f4eb4ad36976816b13a102 | trace/core-impl/update-manager 및 untracked collector |
| Adaptor | a3ab9b8db9637fda4c973b5d59c4d4b95a9d5286 | combined controller / program / EGL의 계측3파일 |
| UI / devel_blur_text | 05087317cac8ea9600bba498f00ccf8086a79d3f | preparation/runtime/text-visual/sample 계측4파일 |

현재 UI HEAD는 Source-only diagnostic이다. 이를 production으로 측정하지 않았고,
production overlay는 기존 b54bb666 기반 별도 사본을 사용했다.
이전 report/trace/RPM/source snapshots는 읽기만 했다.
새 외부 폴더에만 private objects/libraries/runner/captures/보고서를 생성했다.
원래 세 repo의 `git diff --check` 통과. commit/amend/rebase/push/reset/restore/stash 없음.
소스 상품화 최적화 구현, 공용 라이브러리 install, target 접근 없음.
