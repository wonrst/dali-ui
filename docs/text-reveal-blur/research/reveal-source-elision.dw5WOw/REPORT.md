# Demo cleanup / bounded packing / A8 direct Source study

2026-09-21 · `devel_blur_text` · Ubuntu / GTX 1650 / NVIDIA 595.91.07 / GLES

## A. Executive verdict

- Phase 1 완료: **`5e313021 Demo clean up`**. author/signoff 보존, push 없음.
- Phase 2: **BOUNDED MERGE SAVINGS TOO SMALL**. 작은 allocation 상한은 잘 작동하지만 Cards에서 1 page만 줄고, PERFORMANCE/ECONOMY 전체 UpdateRender CPU는 개선을 확인하지 못했다. production 1.25 정책은 그대로 둔다.
- Phase 3의 구현 전 판정은 **FEASIBLE FOR SIMPLE A8 FAST PATH**였다. 외부 복사본에서 Source actor/FBO/task/camera를 할당 전에 제외했고, 기존 Gaussian H/V를 유지한 채 기본 품질 행렬을 통과했다. 다만 bit-exact 결과는 아니며 production-ready 판정도 아니다.
- 최종 추천: **NEED ONE MORE NARROW POC**. Source 제거 자체는 가능하다. 다음 판단 항목은 virtual Source 재구성의 추가 H sampling 비용과 production binding/취소 안전성이다. 넓은 feature 지원이나 새로운 scene/cache manager로 확대하지 않는다.

Phase 2와 Phase 3는 서로 섞지 않았다. Direct Source는 원래 **1.25 packing** 위에서 측정했다.

## B. Phase 1 — demo commit

시작 HEAD `83f8a7665a3b5dd3b2cc93a7ccc4d61991d499bf`의 `demo optimize test` 및 unstaged sample diff를 확인했다. index는 비어 있었고 다른 UI 수정은 없었다.

복잡한 per-Label deadline/timer/progress polling을 제거한 기존 cleanup을 검토했다. 기존 `Animation.Play()`와 group/single `FinishedSignal` cleanup으로 돌아갔으며 `ClearTextReveal()`의 already-None guard만 유지했다.

- `cmake --build . --target text-effect-demo.example -j8`: PASS.
- 기존 lightweight native checker를 다시 실행: HIGH/PERFORMANCE/ECONOMY × Sync/Async **6/6 PASS**.
- normal entrance/exit, interrupted entrance→exit, reset/replay, quality switch, removed-scene weak lifetime, pending shutdown 확인. 모두 `failures=0 individual=0`; 종료 후 기본 task 1개.
- 이번 재실행은 상태 assertion 중심이다. 선택한 화면의 시각 검증은 이전 checker 자료도 사용했다. 모든 subframe 무결함/full regression을 보증하는 결과는 아니다.
- 최종 commit: `5e3130213bfaed3cf4436b5a048b01a6ed652314` / **Demo clean up**.
- parent 대비 sample **4 insertions / 1 deletion**. 기존 author/date 및 `Signed-off-by: Bowon Ryu <bowon.ryu@samsung.com>` 보존.

근거: phase1-check.py (로컬 자료: `phase1-check.py`), `phase1-{quality}-{async}.log`.

## C. Phase 2 — bounded absolute-waste merge

### 정책과 상한

원래 planner를 먼저 실행한다. 이미 한 page면 그대로 반환한다. 예외는 whole-Label merge 하나이며 다음을 모두 만족해야 한다.

1. 기존 texture extent와 combined page **1,048,576 pixels** 제한.
2. `mergedArea / occupiedArea <= 1.27`.
3. **merged의 실제 S/H/V allocation − 원래 split의 실제 allocation**이 상한 이하.

추가 bytes는 RGBA8을 가정하고 HIGH/PERFORMANCE/ECONOMY 중 가장 큰 증가량을 택한다. format/quality에 따라 partition을 바꾸지 않는다. 동일 extent의 scratch sharing과 quarter 크기의 ceil rounding도 반영한다.

`F=전체 page full-area 합`, `U=unique extent full-area 합`, `HQ=unique ceil(W/4)×H 합`, `Q=전체 ceil(W/4)×ceil(H/4) 합`, `UQ=unique Q 합`이면:

- HIGH: `4 × (F + 2U)` — Source/H scratch sharing 포함.
- PERFORMANCE: `4 × (F + HQ + Q)`.
- ECONOMY: `4 × (F + UQ + Q)`.

선택한 최소 cap은 **196,608 bytes = 192 KiB / merge**다. 16K Source-equivalent pixels × 3 full-resolution RGBA planes에 해당한다. Source만 192 KiB 늘려도 된다는 뜻이 아니며 scene 전체의 memory budget도 아니다.

이전 A의 breakpoint 1.26226을 조금 넘는 1.27로 제한했다. 1.30까지 전역 완화하지 않았다. ratio guard/format grouping/halo/MAX_LINES_PER_DRAW/기존 fallback은 보존했다.

### Sweep

195 fixtures × 4 caps = **780 결과**, 반복 실행 partition 동일. 실제 Cards 12개, scaled id11 2개, large counterexample 1개, 기존 synthetic 180개. 범위 및 allocation assertion PASS.

| cap 개념 | worst RGBA pipeline cap | merge 승인 | 비고 |
|---|---:|---:|---|
| 16K | 192 KiB | 1/195 | actual id11 |
| 24K | 288 KiB | 1/195 | 추가 이득 없음 |
| 32K | 384 KiB | 1/195 | 추가 이득 없음 |
| 64K | 768 KiB | 2/195 | id11 ×2도 승인 |

| fixture | 기존→선택 후보 page | 추가 Source pixels | worst RGBA pipeline 증가 | 판단 |
|---|---:|---:|---:|---|
| id11 | 2→1 | 15,504 | 186,048 B | 승인 |
| id10 | 2→2 | 0 | 0 | ratio 1.46554 거부 |
| id2 | 2→2 | 0 | 0 | ratio 1.55569 거부 |
| id11 ×2 | 2→2 | 0 | 승인 시 744,192 B | cap 거부 |
| id11 ×3 | 2→2 | 0 | 승인 시 1,674,432 B | cap 거부 |
| 1024×512 + 604×512 | 2→2 | 0 | 승인 시 2,580,480 B | cap 거부 |

id11의 merged−occupied는 16,711 pixels지만 **merged−current split은 15,504**다. 후자를 사용했다. 승인 page는 383×210 = 80,430 pixels. Synthetic의 큰/불균형 merge는 허용하지 않았다.

### 실제 Cards native inventory

1280×720, 12 Labels, R24. **15→14 pages, 45→42 offscreen tasks/FBOs/cameras**. GPU backing allocation을 재는 도구가 아니라 실제 Texture dimensions/format의 논리 payload 합이다.

| quality | BASE bytes | bounded bytes | 증가 |
|---|---:|---:|---:|
| HIGH | 1,377,693 | 1,424,205 | +46,512 B / +3.38% |
| PERFORMANCE | 603,494 | 623,843 | +20,349 B / +3.37% |
| ECONOMY | 517,635 | 535,077 | +17,442 B / +3.37% |

### 품질

HIGH/PERFORMANCE/ECONOMY, R16/24/48, p=.20/.50/.75/.90, A8+gradient, reverse/seek/WHOLE_TEXT/endpoints: **새 native captures 69장**.

모두 이전 A(1.2623) 캡처와 텍스트 ROI에서 **pixel-identical**. 이전 A의 BASE 대비 차이 및 시각 검토도 재사용했다. A 대비 추가 line bleed/halo cutoff/brightness/texel-phase 악화 없음. 이것은 A와 동등하다는 뜻이지 BASE와 bit-identical하다는 뜻은 아니다.

근거: bounded-quality-summary.json (로컬 자료: `bounded-quality-summary.json`), [이전 품질 분석](../reveal-packing-sweep.7E5egr/REPORT.md).

### CPU — 5 independent warm processes / arm / quality

총 30 runs. 같은 optimized private libraries(-O2, -DNDEBUG), 같은 기존 meter. Core/Adaptor는 기존 DEBUG_ENABLED 빌드 특성이 남아 있으므로 완전한 production Release 수치가 아니다. 이전 build의 glyphy-arcs 단일 object 예외도 동일하다.

각 process에서 Cards를 한 번 만들고 3.9초 warm-up, 다시 생성한 뒤 **250ms→3.2s** 구간을 측정했다. creation synchronous call과 전체 first-frame latency는 이 수치에 포함하지 않는다. heading 등도 포함된 demo 전체 active 구간이며 task가 변하는 장면이다.

아래는 5개 run의 frame당 평균(ms). UpdateRender 전체와 내부 scope들은 중첩되므로 더하지 않는다. GPU execution time이 아니다.

| quality | UpdateRender BASE→bounded | Update scope | offscreen RenderScene | command queues |
|---|---:|---:|---:|---:|
| HIGH | 4.329→4.083 | .282→.279 | 1.694→1.604 | 1.876→1.861 |
| PERFORMANCE | 4.239→4.282 | .301→.294 | 2.232→2.200 | 1.991→1.917 |
| ECONOMY | 4.259→4.265 | .301→.300 | 2.268→2.243 | 2.016→1.966 |

PERFORMANCE command −3.70%, ECONOMY −2.52%; 전체 thread CPU는 +1.02% / +0.14%로 neutral/noise. HIGH command의 후보 run 범위는 1.766–2.162ms로 outlier도 있다. draw 평균은 약90→86, clear 약55.3→52.3. frame cadence는 대체로16.68ms로 vsync 제한 상태다. PC FPS로 target 개선율을 추정하지 않는다.

**BOUNDED MERGE SAVINGS TOO SMALL.** 안전한 작은 후보를 정의하는 데는 성공했지만 이 fixture에서는 복잡도를 추가할 만큼 큰 전체 CPU 이득을 확인하지 못했다. 생산 코드로 가져오지 않았다.

근거: bounded-wrapper.h (로컬 자료: `bounded-wrapper.h`), 780-case 결과 (로컬 자료: `bounded-sweep.json`), CPU raw aggregate (로컬 자료: `bench-aggregate.json`), CPU summary/ranges (로컬 자료: `bench-summary.txt`).

## D. Source capture semantics

```text
현재: CPU bitmap + metadata → Source raster(R8) → H → V → Output(Source,V)
PoC:  CPU bitmap + metadata ───────────────────→ H → V → Output(bitmap,metadata,V)
```

Source는 단순 복사본이 아니다. text visual의 transform/crop, 선형 glyph coverage sampling, nearest metadata, Reveal opacity, text alpha를 평가한 뒤 8-bit R8 FBO에 저장한다. PER_LINE에서는 먼저 CPU에서 line-isolated raster를 만들고, GPU capture에서 halo/page 위치로 배치한다.

| 기능 | Source capture의 책임 | H에서 fuse | direct sharp Output |
|---|---|---|---|
| simple A8 monochrome | coverage×Reveal×text alpha, R8 저장 | 가능, virtual Source grid 필요 | 동일 함수 필요 |
| animated text RGB | RGB는 A8에 저장하지 않음 | RGB 불필요 | 기존 color constraint/unpremultiply 유지 |
| animated text alpha | Source alpha에 포함 | update-side alpha mirror 필요 | 같은 alpha, owner alpha 이중 적용 금지 |
| owner opacity/color/position | capture actors USE_OWN_COLOR; 최종 output에서 owner color | capture 입력에 owner alpha 넣지 않음 | 기존 USE_PARENT_COLOR 유지 |
| renderer opacity/visibility | source draw/skip 정책 | uniform multiplication만으로 모두 대체 불가 | 생산 guard/동기화 필요 |
| WHOLE_TEXT Reveal | full bitmap의 per-pixel metadata 평가 | 가능 | 가능, normalized metadata schedule 사용 |
| PER_LINE Reveal | 분리된 line bitmap, 독립 start, 페이지/halo 배치 | line-local 매핑·clamp 필요 | line 순서/source-over 유지 |
| gradient / GradientSpan | mask·lookup·gradient coordinates·preserved color | 첫 후보 제외 | capture fallback |
| RGBA/emoji/multicolor | premultiplied color plane 조합 | 첫 후보 제외 | capture fallback |
| ImageSpan | ready image proxy와 text를 합성 | 첫 후보 제외 | capture/ownership 그대로 |
| replacement glyph | synthetic glyph는 text raster에서 제외, image는 별도 renderer | text bitmap만으로 재현 불가 | image 포함 publication 전체 fallback |
| transform / fitting / UI/render scale | texture→display affine mapping 및 sampling phase | fixed affine는 역매핑 가능 | dynamic binding/rounding 추가 검증 필요 |
| decoration/cutout/emboss | 별도 planes/neighbor reads/합성 | 첫 후보 제외 | 기존 composition fallback |

근거: source shader (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/graphics/shaders/text-visual-shader.frag`), source atlas vertex shader (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/graphics/shaders/text-visual-shader.vert`), runtime (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`).

## E. Simple A8 eligibility

진단 구현의 representational guard:

- PERFORMANCE/ECONOMY, alpha-capable GLES3 path, L8 foreground + RGBA8 metadata 두 texture.
- `mSingleColor`이며 ImageSpan/decorations 없음. 이 flag는 gradient/emoji/multicolor/cutout을 제외한 publication에서 설정된다.
- WHOLE_TEXT는 원래 uploaded foreground TextureSet 사용.
- PER_LINE은 page 내 모든 sequence에 text가 있고 같은 prepared source atlas TextureSet을 공유해야 함.
- 첫 진단은 page당 최대8 sequences. non-atlased isolated source는 기존 capture로 fallback; source atlas를 새로 생성하지 않는다.
- HIGH는 무조건 기존 capture.

**상용 eligibility를 완성한 것은 아니다.** 이번 fixture는 scale1, 고정 capture transform이다. 단순 predicate만으로 arbitrary transform animation/custom renderer opacity/reentrant publication을 안전하다고 보장할 수 없다. 이 후보를 production으로 옮긴다면 fail-closed transform 정책 또는 기존 transform constraints와 동일한 update-side binding이 필요하다. 그 전에는 샘플/일반 앱용 대체 library로 쓰지 않는다.

## F. Original texture의 실제 형태

glyph atlas가 아니다.

- WHOLE_TEXT: Typesetter가 완성한 Label coverage bitmap을 `Texture::Upload`한 L8 texture. metadata는 같은 raster domain의 RGBA8 texture이며 nearest sampler다.
- PER_LINE: `RenderRuntimeBlurLine`이 해당 line의 glyph range만 렌더링한다. foreground/metadata를 crop한 후 `PrepareRevealBlurSourceAtlases`에서 여러 line을 세로로 합친다. 각 entry 주위에 복제된 1-texel border가 있다.
- source atlas와 runtime blur page는 서로 다른 packing이다. `textureRect`는 전체 raster에 대한 crop, `atlasRectangle`은 업로드 atlas의 interior UV다.
- 이 atlas는 static uploaded CPU data이며 다른 RenderTask가 생산한 FBO가 아니다. 따라서 eligible Source producer dependency 하나는 정말 없앨 수 있다.
- whole Label bitmap으로 모든 PER_LINE을 대체하면 겹치는 line glyph/line-local metadata ownership을 잃을 수 있다. PoC는 그렇게 하지 않았다.

근거: preparation (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp`), line raster (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/rendering/text-typesetter.cpp`), publication/upload (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp`).

## G. 정확히 옮겨야 하는 source shader 연산

현재 Source pixel center j에 대해:

`S[j] = Q8(Linear(Coverage, uv(j)) × Reveal(Nearest(Metadata, uv(j)), p) × textAlpha)`

H의 기존 read는 `Linear(S, sampleUV)`다. 다음 두 식은 일반적으로 다르다.

`Linear(Coverage × Reveal)` ≠ `Linear(Coverage) × Reveal(Nearest(metadata))`.

예: 이웃 coverage=(1,1), Reveal=(0,1), 중간 위치이면 기존 Source read는 .5지만 단순 fused read는0 또는1이다. 따라서 단순 두 texture read 대체는 STEP/PIXEL 경계에서 정확하지 않다.

PoC는 sample 주변 Source grid의 네 pixel center를 평가하고, R8 quantization을 재현한 다음 bilinear interpolation한다. 원래 metadata의 **mediump load → highp encodedStart → highp normalization** 계산 순서도 보존한다. 첫 진단에서 이 정밀도 순서를 달리했을 때 경계 차이가 생겼고, 동일하게 맞춘 뒤 큰 차이가 사라졌다. production의 버그를 수정한 것이 아니라 외부 PoC의 불일치를 바로잡은 것이다.

## H. H direct sampling

**가능하나 비용이 공짜는 아니다.** kernel factory, weights, offsets, radius, strength, Gaussian loop, quarter dimensions, D2 geometry는 그대로다. `ReadRevealBlurTexture`의 Source read만 virtual Source evaluator로 교체한다.

한 Source lookup당 coverage+metadata 최대8 texture instructions(4 centers×2)가 생긴다. crop 밖은 fetch 없이0을 반환하므로 실제 수는 이 상한보다 적을 수 있다. 기존은 materialized Source의 linear read1개였다. 이론상 R24의 H Gaussian24 reads는 최대192 input reads, R48은48→384가 된다. 이것을 실제 GPU 시간의8배로 환산하면 안 된다.

PERFORMANCE의 D2 band/full H clear 유지. ECONOMY는 원래대로 full padded H coverage 유지. H/V scratch sharing과 V→Output dependency도 변경하지 않았다. Source만 제거한 topology다.

## I. Output sharp-source sampling

**Source FBO 없이 가능했다.** 같은 virtual Source evaluator를 sharp read에 사용하고, 기존 PERFORMANCE Late Smooth / ECONOMY composition weights는 그대로 사용했다. blurred V read는 변경하지 않았다.

alpha-only output은 기존처럼 `uTextColorAnimatable.rgb / alpha`로 RGB를 복원하고 coverage를 곱한다. owner color는 마지막에 한 번 적용한다. Source alpha를 빼거나 text alpha를 두 번 곱하지 않는다.

Output은 V 외에 uploaded coverage+metadata 두 sampler를 보유한다. 새 offscreen texture는 없다. sharp branch도 추가 read 비용이 있으므로 Source task 감소만으로 GPU 승리를 단정할 수 없다.

## J. PER_LINE feasibility

- page별 source pixel grid와 각 line source quad의 rectangle/atlas UV를 H 및 Output에 전달한다.
- H의 padded line-local clamp, sequence-start early transparent return, 현재 update-side progress/strength 유지.
- 아직 시작하지 않은/진행 중/완료 line이 같은 page에 있어도 metadata와 isolated raster는 섞이지 않는다.
- output quad 순서와 halo source-over order 유지.
- eligibility가 atlas 하나/page로 제한되어 sampler array나 새 texture packer는 추가하지 않았다.
- 다른 atlas/legacy isolated draw/mixed image page는 capture fallback.

## K. Async / lifecycle implications

기존 publication은 prepared metadata의 revision/source/layout/renderer identity를 확인하고, detached companion 생성→소유권 등록→activate→metadata upload 순서로 진행한다. Source 제거는 이 revision architecture를 바꿀 이유가 없다.

PoC도 기존 companion과 ownership 흐름을 유지한다. hidden foreground actor는 **남긴다**. Renderer의 update-side constraints를 살려 두고 None 시 원래 renderer를 되돌리는 책임은 Source task와 별개다. original texture/metadata handle도 유지하므로 원본 bitmap을 해제하는 최적화가 아니다.

Source task slot은 비워 두며 GetOffScreenRenderTasks/scene disconnect는 원래의 null-handle guard를 사용한다. H/V task/camera는 기존 scene connection/disconnection에서 생성/제거한다. direct→fallback quality/style/text 교체는 기존 companion 교체가 소유해야 한다.

생산 작업 전에 필요한 검증:

- 새 H/Output binding 중 ObjectCreated reentry/cancel guard. 현재 진단 helper는 이 부분을 production 수준으로 강화하지 않았다.
- fixed snapshot 대신 dynamic capture transform의 update-side 일관성 또는 명시적 fallback.
- text-color alpha animation과 renderer draw-skip/opacity의 일관성.
- async stale completion, None, scene disconnect/reconnect, pending destroy. **Phase 1 demo 6/6이 direct path의 lifecycle test를 대신하지 않는다.**

ImageSpan path는 변경하지 않았고, Core/Adaptor sync/fence/dependency checker도 변경하지 않았다. uploaded static source이므로 Source→H/Output FBO dependency는 제거 가능하지만 H→V, V→Output ordering은 그대로 필요하다.

## L. Actual Cards eligibility

원래 Cards: **12 Labels / 15 pages = A8 14, RGBA 1, ImageSpan 0**.

| 분류 | page 수 | 이번 PoC |
|---|---:|---|
| 같은 isolated source atlas를 쓰는 A8 multiline (id2/3/7/10/11) | 8 | direct 적용 |
| non-atlased A8 single-line | 6 | 보수적으로 capture 유지 |
| gradient RGBA (id5) | 1 | capture 유지 |

따라서 **실제 eligible N=8**이다. A8가14개라고14개 모두 제거했다고 쓰면 틀리다. non-atlased A8 지원까지 별도 검증하면 N=14가 될 잠재력은 있으나 이번 결과가 아니다.

## M. Task/FBO/creation/memory

native inventory에서 PERFORMANCE/ECONOMY 모두 확인:

- Source tasks7 + H15 + V15 = **37 tasks**, BASE45 대비8개 감소(−17.8%).
- unique FBOs45→37, task cameras45→37.
- eligible8개 page의 Source Actor/FBO/texture/task/camera를 **생성하지 않는다**. PER_LINE Source child foreground actor도 만들지 않는다. 생성 후 disable하는 방식이 아니다.
- H/V/output objects와 original hidden foreground는 유지.
- Source FBO attachment8회와 Source color texture8개도 구성상 사라진다. setup latency를 별도로 실측한 것은 아니다.

| quality | BASE logical FBO bytes | direct PoC bytes | 절감 |
|---|---:|---:|---:|
| PERFORMANCE | 603,494 | 316,445 | 287,049 B / 47.56% |
| ECONOMY | 517,635 | 230,586 | 287,049 B / 55.45% |

이는 Cards의 **S/H/V 논리 payload**다. driver allocation/alignments, original Label texture, source CPU atlas, metadata, window/MSAA, RSS/VRAM peak는 포함하지 않는다. 실제 process memory가 이 비율로 줄었다는 뜻이 아니다.

모든 A8 14 pages가 가능하다고 가정한 상한은 tasks45→31, Source bytes379,359 제거다. 현재 구현/측정값과 구분한다.

근거: native counts (로컬 자료: `direct-structure.json`), `direct-inventory-{1,2}.log`.

## N. Disposable implementation

[구현 전 gate](PHASE3-GATE.md) 기록 후 build-direct.py (로컬 자료: `build-direct.py`)로 외부 runtime copy만 생성했다. direct-helper.h (로컬 자료: `direct-helper.h`)에 virtual Source GLSL과 H/Output binding을 모았다. 일반 source shader 전체를 복제하지 않았고 generic Gaussian factory/kernel 파일은 손대지 않았다.

- publication/CPU preparation/packing은 BASE 그대로.
- native quality/CPU 실행에만 `LD_LIBRARY_PATH`로 외부 foundation을 선택.
- 생산 레포 또는 설치 prefix에 복사/install하지 않았다.
- 기존 private optimized Core/Adaptor 및 CPU-meter binaries는 재사용했으며 해당 repo/file들을 수정하지 않았다.
- HIGH는 소스 수준에서 기존 분기로 유지했다. direct HIGH prototype/성능 일반화는 하지 않았다.

## O. Quality gate

PERFORMANCE/ECONOMY × WHOLE_TEXT/PER_LINE. English/Korean/thin glyph/bold glyph, 여러 줄, R16/24/48, p0/.20/.50/.75/.90/1. PIXEL, Fade0, PER_LINE stagger.25. reverse .90→.75→.50→.20 및 seek .50도 확인했다.

- BASE/direct 각각23 screenshots×2qualities =92장, **368 paired text ROIs**.
- 최대 channel difference **1/255**. worst per-ROI RMSE 약 .061/255. 새로운 grid/halo cutoff/line bleed는 관찰하지 못했다.
- 같은 arm에서 reverse/seek가 같은 progress의 정적 결과와 **20/20 pixel-identical**.
- 별도 actual fractional-width Cards fixture(A8+gradient fallback)46장: BASE 대비 최대 **2/255**, worst whole-ROI RMSE 약 .044/255. p0는 동일하지만 fractional fixture p1에도 sparse2/255 차이가 남는다.
- 같은 fractional fixture의 gradient fallback 영역은 두 quality 모두 BASE와 pixel-identical했다.
- 이 작은 차이를 숨기지 않는다. **visual screening PASS이지 bit-exact equality PASS가 아니다.** scope를 확대해 tolerance를 높이거나 근사 Gaussian을 넣지 않았다.

이 확인은 고정 progress/역순 seek 캡처다. 모든 연속 애니메이션 frame의 shimmer, 다른 GPU, UI/render scale, asynchronous lifecycle까지 검증한 결과가 아니다.

근거: matrix metrics (로컬 자료: `direct-quality-summary.json`), fractional metrics (로컬 자료: `direct-fractional-summary.json`), [PERFORMANCE strong blur](direct-comparison-1-r48-p1.png), [ECONOMY late handoff](direct-comparison-2-r24-p4.png). 비교 이미지는 왼쪽 BASE, 오른쪽 direct이고 각 패널 안에서 왼쪽 WHOLE_TEXT/오른쪽 PER_LINE이다.

## P. Structural / CPU screening

품질 gate를 통과한 최종 외부 library만 비교했다. 조건은 Phase2와 동일한 warm Cards 구간이며 BASE/DIRECT × PERFORMANCE/ECONOMY 각5 independent processes, 총20 runs다. BASE도 다시 측정했고 실행 순서를 교차했다. Phase2 결과와 서로 다른 시점의 숫자를 섞어 개선율을 계산하지 않았다.

| quality | UpdateRender CPU ms/frame | Update scope | offscreen RenderScene | command queues |
|---|---:|---:|---:|---:|
| PERFORMANCE | 4.417→4.365 (−1.2%) | .298→.266 | 2.357→1.895 (−19.6%) | 2.024→1.785 (−11.8%) |
| ECONOMY | 4.549→4.258 (−6.4%) | .311→.273 | 2.467→1.891 (−23.3%) | 2.116→1.797 (−15.1%) |

UpdateRender run-mean 범위: PERFORMANCE BASE4.258–4.564 / direct4.265–4.622ms, ECONOMY BASE4.399–4.690 / direct3.838–4.619ms. 따라서 전체 CPU 개선은 command/offscreen scope 감소보다 작고 편차도 크다. PERFORMANCE 전체는 neutral/noise로 본다.

native GL counts는 demo 구간 평균 draw 약90→81.9, clear 약55.3→47.3이다. Cards의 Source8개 제거와 일치한다. total task는 측정 초기에 heading 등을 포함해61→53, Cards만 남은 시점에는 기본 task 포함46→38이다. structural inventory의 Cards offscreen45→37과 구분한다.

Source stage를 없애면 command/offscreen CPU 감소가 가능한 것은 확인했다. 그러나 남은 driver work, shader sampling, 다른 scene 비용까지 포함한 전체 CPU/GPU 승리를 입증한 것은 아니다. scope들은 중첩되며 감소량을 합산하면 안 된다. frame cadence는 양쪽 약16.68ms였고 FPS 향상으로 해석하지 않는다.

Source elimination의 GPU read 증가 때문에 CPU 측정만으로 target 성능 개선을 확정하지 않는다. GPU timer, target build/test, RSS/VRAM 실측은 하지 않았다.

## Q. Complexity / remaining risk

장점은 실제 stage/object/FBO 제거다. Gaussian approximation은 없다. 원본 bitmap/metadata를 재사용하므로 새로운 CPU raster가 필요 없다.

비용은 source-grid 재구성 함수, H/Output 두 shader variant, mapping/atlas uniforms, color/fade/progress bindings다. shader/scene 전체 복제나 cache manager는 추가하지 않았지만, 단순 sampler 치환보다 분명히 복잡하다.

특히 H의 입력 fetch 증가가 가장 큰 미확정 trade-off다. 페이지 수가 줄지 않아도 source submission/clear/objects는 줄지만, GPU가 충분히 느린 타겟에서는 반대 결과일 수 있다. quality가 유지된다는 결과를 성능 승리로 바꾸어 해석하지 않는다.

현재 diagnostic의 transform snapshot과 reentry helper를 그대로 shipping할 수 없다. 이 한계를 해결하기 위해 many-feature pipeline을 복제해야 한다면 여기서 STOP하는 것이 맞다.

## R. Final recommendation

**NEED ONE MORE NARROW POC**.

다음 범위는 지금의 단순 A8 virtual Source를 그대로 두고, 추가 H/Output sampling GPU 비용과 최소 update-side binding/cancellation 안전성을 판정하는 것 하나다. non-atlased/gradient/ImageSpan/scale/HIGH로 확대하지 않는다. 이것이 작고 안전하게 끝나며 cost도 합리적일 때만 production fast path 작업으로 진행한다.

bounded merge는 보류한다. 이번 연구 결과의 핵심은 더 큰 구조적 절감 후보가 실제로 가능함을 확인했다는 점이며, 바로 production 구현을 변경했다는 뜻은 아니다.

## S. Git / artifacts

- UI HEAD: `5e3130213bfaed3cf4436b5a048b01a6ed652314 Demo clean up`.
- UI production source 변경 없음. UI worktree clean. `git diff --check` PASS.
- Core/Adaptor worktree clean, 이번 작업에서 변경 없음.
- Phase2/3 파일은 이 외부 디렉터리에만 존재. 기존 연구 디렉터리 덮어쓰기 없음.
- Phase1 amend 외 commit/amend/rebase/reset/restore/stash/push 없음.
- target/GBS build/test, production library install, full UTC regression 없음.

최종 CPU 데이터: bounded (로컬 자료: `bench-aggregate.json`), direct (로컬 자료: `direct-bench/bench-aggregate.json`), direct runs와 scope summary (로컬 자료: `direct-bench-run.log`).
