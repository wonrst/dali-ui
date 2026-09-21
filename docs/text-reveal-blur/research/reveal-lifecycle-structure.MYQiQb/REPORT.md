# Text::Reveal — lifecycle guideline, page/clear audit, single-blur feasibility

2026-09-21. Ubuntu local-only. Production UI/Core/Adaptor source와 commit은 변경하지 않았다.

## A. Executive summary

- **완료된 단발 entrance에 `Reveal::None()`을 설정하는 정책은 권장 가능하다.** 단, 재생이 완전히 끝났고 reverse/seek/loop를 더 하지 않음을 앱이 알아야 한다. Timer 구현 자체를 일반 가이드로 권장하는 것은 아니다.
- 현재 Cards를 다시 확인해 **12 Labels / 15 pages / 45 offscreen tasks**를 재현했다. 세 split 모두 마지막 짧은 줄 때문에 **25% packing overhead 정책**에 걸린다. GPU 최대 크기나 64-line draw limit 때문이 아니다.
- **V clear 일괄 제거는 NOT SAFE.** Garbage-seeded 실험에서 PERFORMANCE/ECONOMY의 줄 경계에 흰색/마젠타 선이 나타났다. p=0에서도 발생한다. 기존 production clear에는 이 문제가 없다.
- 외부 코드에서 **실제로 H Actor/Task/FBO를 만들지 않는** 8-tap single reduced blur를 구현했다. Source, ECONOMY Radius/Composition, gamma1, timing은 유지했다. 그러나 R16/R24/R48의 초기 blur에 획 복제·격자가 뚜렷해 **품질 FAIL**이다.
- 최종: **KEEP CURRENT ECONOMY; FURTHER STAGE REDUCTION FAILS QUALITY**. 이번에 시험한 작은 고정 8-tap 후보에 대한 결론이며, 모든 single-pass 필터의 불가능성을 증명한 것은 아니다.

## B. Reveal::None application guideline verdict

**Recommended, with lifecycle and appearance caveats.**

근거:

- Reveal::None (로컬 자료: `../dali/dali-ui/dali-ui-foundation/public-api/text/style/reveal.h:203`)은 Reveal rendering을 제거하는 public API다.
- Label progress contract (로컬 자료: `../dali/dali-ui/dali-ui-foundation/public-api/views/text-controls/label.h:817`): progress 1은 fully visible / remaining blur strength 0, progress 0은 대상 foreground hidden.
- LabelImpl::SetTextReveal (로컬 자료: `../dali/dali-ui/dali-ui-foundation/integration-api/label-impl.cpp:798`)은 enabled/configuration/revision을 갱신한다. None은 progress property를 없애거나 값을 초기화하지 않는다. 이후 새 Reveal 설정은 지원되는 lifecycle이다.
- HIGH/PERFORMANCE/ECONOMY 모두 같은 disable/publication/retirement 경로를 쓴다. ECONOMY의 endpoint도 sharp Source weight 1 / blur weight 0이다.

Text, text gradient, GradientSpan, multicolor, ImageSpan의 **논리적 최종 콘텐츠와 완전 가시성**은 ordinary rendering으로 돌아가는 것과 맞는다. 그러나 **framebuffer pixel-identical이라는 public guarantee는 아니다.**

- 텍스트도 capture/texture filtering/좌표/8-bit rounding이 개입한다.
- ImageSpan은 offscreen single-sample capture와 window MSAA presentation의 edge coverage가 다를 수 있다는 기존 limitation이 있다. p=1이라고 그 차이가 사라진다는 보장은 없다.
- decorations는 Reveal의 foreground 대상과 별개이며, None이 shadow/underline 등의 authored style을 지우는 것이 아니다.
- texture/glyph resource 자체가 미완료라면 None은 그 resource loading 완료를 보장하지 않는다.

따라서 "일회성 효과가 끝나면 해제"는 문서/샘플 가이드에 넣을 가치가 있지만, "항상 픽셀 변화 없이 즉시 모든 메모리 반환"이라고 쓰면 안 된다.

## C. Safe / unsafe lifecycle matrix

여기서 SAFE는 **해당 앱의 의도에 맞는 사용**이라는 뜻이다. GPU memory의 동기 반환이나 모든 렌더링 경로의 bit identity를 뜻하지 않는다.

| 상태 | 판단 | 조건/이유 |
|---|---|---|
| completed entrance, final progress=1, 단발 효과 종료 | SAFE | 남은 progress writer/reverse/seek/loop가 없고 ordinary final presentation을 받아들이는 경우 |
| completed exit, actor를 먼저 hide/unparent한 뒤 | SAFE | 숨김 상태 유지. 다음 show 전에 필요한 Reveal/progress를 다시 설정 |
| progress=1이지만 곧 reverse 예정 | NOT SAFE | None 이후 progress animation만으로는 숨겨지지 않음. 새 설정 후 reverse를 시작한다면 그 시점에 별도 lifecycle로 가능 |
| progress=0인데 actor는 visible | NOT SAFE | None은 "숨김 유지"가 아니라 ordinary foreground 복원; 글자/이미지가 다시 보일 수 있음 |
| paused animation | NOT SAFE | resume를 기대하는 동안 효과를 제거하지 않음. 명시적으로 종료·폐기한 경우에만 새 정책 적용 |
| looping animation | NOT SAFE | 매 loop의 p=1은 최종 lifecycle 종료가 아님 |
| manual seek | NOT SAFE | seek 가능한 구간에는 유지. seek 세션 종료 후 endpoint 기준으로 판단 |
| quality switching | CONDITIONALLY SAFE | 지원되는 configuration update. cleanup이 필수 아님; 이전 완료 callback이 새 설정을 지우지 않도록 token/ownership 검사 |
| async result pending | CONDITIONALLY SAFE | disable revision이 이전 completion 재공개를 막음. 다만 현재 결과를 보존하다 ordinary publication 시 runtime을 해제할 수 있어 즉시 retirement는 아님 |

Animation이 끝났다는 사실만으로 endpoint=1이라고 가정하지 않는다. 역재생/auto-reverse/부분 종료라면 endpoint와 향후 의도를 별도로 판단한다.

## D. Recommended usage wording

> Reveal can be used as a transient visual effect. After a one-shot entrance has
> fully completed at progress 1, release it with `Label::SetTextReveal(Text::Reveal::None())`
> if the application will no longer reverse, seek, or loop that effect.
> Configure Reveal again before starting another Reveal animation.
> After an exit, hide or remove the Label before disabling Reveal; None restores
> ordinary foreground rendering. Asynchronous publication and graphics resource
> retirement need not complete in the setter call.

일반적인 예시는 앱이 소유한 Animation의 완료 신호에서 lifetime token과 현재 효과를 확인하고 None을 설정하는 정도면 된다. 하나의 Animation에 여러 Label track이 있다면 전체 FinishedSignal까지 기다리는 단순한 가이드도 올바르다. track별 조기 해제가 필요할 때만 더 세분화한다.

None은 공짜 cache eviction API가 아니다. ordinary publication 작업이 발생할 수 있고, 다음 효과 설정 때 setup도 다시 필요하다. 바로 reverse할 효과에는 유지하는 편이 자연스럽다.

## E. Current Text Effect Demo cleanup review / resource lifetime

현재 sample cleanup은 이미 `83f8a766`으로 commit되어 있다. backup (로컬 자료: `sample-cleanup-backup.patch`)을 남겼다.

scheduler (로컬 자료: `../dali/dali-ui/samples/text/text-effect-demo.cpp:1383`)를 직접 검토했다:

- 기존 Animation 생성/track/delay/easing/layout transition을 분할하거나 재구성하지 않는다. Play wrapper는 같은 Animation을 한 번 Play한다.
- deadline 기준 Timer 하나이며 매 frame polling이 아니다. timer가 먼저 왔으면 actual animation elapsed와 current progress=1을 모두 확인한 뒤 해제한다.
- 다음 authored deadline에서 재검토하거나 기존 FinishedSignal에 맡긴다. CPU scheduling 지연이 있으면 조기 해제가 늦어질 수 있으나 미완료 효과를 wall-clock만으로 해제하지 않는다.
- pending Label/Animation은 weak handle. callback은 `ConnectionTracker`, shutdown 및 lifecycle token 검사로 보호된다.
- StopSceneActivity (로컬 자료: `../dali/dali-ui/samples/text/text-effect-demo.cpp:2172`)가 Timer Stop/Reset + vector clear를 수행하고 이전 progress writer를 정지시킨다. 새 exit가 이전 entrance cleanup과 경합하지 않도록 순서가 잡혀 있다.
- 마지막/단일 track은 FinishedSignal fallback. None이면 중복 setter를 생략한다.
- **현재의 forward, one-shot demo에서 적합.** 일반 pause/seek/loop/speed-change scheduler로 그대로 배포할 구현은 아니다. 이번 범위에서 concrete cleanup bug는 찾지 못했다.

기존 [검증 보고서](../reveal-demo-cleanup.a4F90T/REPORT.md)의 3 quality × sync/async lifecycle 결과를 재사용했다. 이번에 그 regression을 다시 실행했다고 주장하지 않는다. 그 보고서에는 최초 async run의 post-None progress exact assertion 2건이 재확인 5회에서 재현되지 않았다는 미확정 관측도 남아 있다. 이를 해결 완료나 특정 플랫폼 버그로 재분류하지 않았다.

### 실제 retirement 순서

```text
SetTextReveal(None)
  → authored disable / revision 증가 / ordinary rendering 요청
  → sync UpdateRenderer 또는 async 새 publication
  → TextVisual::RemoveRenderer
  → RemoveRuntimeRevealBlur: owner의 runtime handle을 먼저 move-out
  → ReleaseForeground: text shader/renderer ownership 복원,
                      ImageSpan capture authority 해제 및 살아 있는 renderer 복원
  → runtime actor Unparent
  → OnSceneDisconnection: Source/H/V(+decoration) tasks 제거, task-list handle 해제
  → 마지막 private handle 해제: actors, cameras, output renderers,
                            constraints, FBO/texture references 소멸
  → Core/render/driver의 지연된 실제 resource retirement
```

근거: RemoveRenderer (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:479`),
RemoveRuntimeRevealBlur (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:2279`),
OnSceneDisconnection (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1852`),
ReleaseBlurCapture (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/replacement/inline-replacement-manager.cpp:1407`).

Runtime owner는 weak이며, ImageSpan 복원도 occurrence/renderer identity가 여전히 맞는 경우만 한다. 같은 manager에 새 capture가 생긴 경우 이전 client가 그것을 해제하지 않는다.

Async에서는 UpdateRenderer (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:832`)가 즉시 return하며, publication (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:1399`)이 이전 renderer/runtime을 제거한다. main thread가 worker를 기다리는 blocking 호출은 아니다. 이전 revision rejection은 기존 `UtcDaliTextVisualRevealNoneRejectsOlderCompletionP`가, 실제 disable/re-enable 및 async convergence는 `UtcDaliTextRevealRuntimeGaussianLifecycleP` / `...AsyncLifecycleP`가 담당한다.

None 후에도 Label의 일반 text textures, progress property/lazy data, 공유 shader/kernel cache까지 전부 없어지는 것은 아니다. 특히 GPU allocator cache/RSS 감소 시점을 API로 보장하지 않는다.

이미 완전히 idle이라 Core가 frame 생성을 멈춘 scene에서는 지속 frame 비용 절감을 주장할 수 없다. 다른 animation으로 계속 frame을 생성할 때 남아 있는 REFRESH_ALWAYS tasks의 clear/copy/filter/submission을 없애는 가치가 있다. [기존 task-gating source audit](../reveal-task-gating-analysis.T3s39Z/REPORT.md)을 함께 참고한다.

## F. Actual Cards page inventory

현재 sample를 include한 외부 inventory.cpp (로컬 자료: `inventory.cpp`)로 Cards를 생성했다. Window **1280×720**, UI scale 1, MSAA 4, sync, entrance radius24. 세 quality를 각각 실행했으며 page geometry는 같았다. cleanup 전 snapshot이다.

runtime의 현재 page/line 계산을 복사한 외부 Foundation에 **로그만** 추가해 line coverage/target을 얻었다. 이후 같은 외부 파일에 Phase C 분기만 추가했으며 production 설치 파일은 교체하지 않았다.

`content`는 Label text content 영역, `Source pages`는 각 offscreen Source texture 크기다. 원본 text/metadata atlas와 혼동하지 않는다.

| id / Label text | lines | content | Source pages | format |
|---|---:|---:|---|---|
| 0 / DAY 1 | 1 | 339×30 | 95×66 | A8 |
| 1 / Forest & Oreum | 1 | 339×54 | 290×78 | A8 |
| 2 / Bijarim Forest · Abu Oreum · Local Cafe | 2 | 339×64 | **362×74 + 96×70** | A8 |
| 3 / Walk beneath ancient cedars, climb a quiet oreum, and end with coffee among Jeju's green landscapes. | 3 | 339×82 | 380×212 | A8 |
| 4 / DAY 2 | 1 | 339×30 | 95×66 | A8 |
| 5 / Sea & Sunset | 1 | 339×54 | 256×78 | RGBA8 |
| 6 / Woljeongri · Sehwa · Hamdeok | 1 | 339×64 | 331×74 | A8 |
| 7 / Follow the eastern coastline slowly, leaving time for ocean views, village walks, and an unhurried sunset. | 3 | 339×82 | 344×212 | A8 |
| 8 / DAY 3 | 1 | 339×30 | 95×66 | A8 |
| 9 / Market & Old Town | 1 | 339×54 | 334×79 | A8 |
| 10 / Dongmun Market · Old Jeju · Local Dessert | 2 | 339×64 | **360×74 + 123×69** | A8 |
| 11 / Browse the morning market, discover old alleyways, and finish the trip with Jeju's local flavors. | 3 | 339×82 | **383×142 + 155×68** | A8 |

Logs: HIGH (로컬 자료: `inventory-0.log`), PERFORMANCE (로컬 자료: `inventory-1.log`), ECONOMY (로컬 자료: `inventory-2.log`).
전체 line coverage, halo 포함 target 및 H/V 크기는 로그에 기록했다.

| quality | Source bytes | H bytes | V bytes | logical FBO payload | FBOs/tasks |
|---|---:|---:|---:|---:|---:|
| HIGH | 459,231 | 459,231 | 459,231 | 1.31387 MiB | 45 / 45 |
| PERFORMANCE | 459,231 | 115,061 | 29,202 | 0.57554 MiB | 45 / 45 |
| ECONOMY | 459,231 | 29,202 | 29,202 | 0.49366 MiB | 45 / 45 |

**Cards 12개만의 logical FBO bytes**다. 원본 text/metadata, CPU data, driver alignment, window MSAA, 다른 scene Label은 제외한다. 실제 VRAM peak/RSS나 앱 전체 메모리 측정이 아니다. 다른 Label끼리 scratch를 공유하지 않으며 이 fixture의 같은 Label 내 page 크기도 달라 45개 모두 별도 FBO였다.

## G. Why the three splits occur

BuildRuntimeRevealBlurBatches (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1915`)는 우선 전체를 한 page에 넣어 본다. 조건은 max texture extent, 1,048,576 pixels/page, 그리고 `pageArea <= 1.25 × sum(lineTargetArea)`다. 실패하면 최대 4줄 greedy를 ceiling으로 balanced partition/common-size 후보를 평가한다.

| id | line capture sizes | 한 page 크기 | page / line-area 합 | 원인 |
|---|---|---|---:|---|
| 2 | 362×74, 96×70 | 362×144 | **1.55569** | 짧은 마지막 줄의 오른쪽 빈 공간 |
| 10 | 360×74, 123×69 | 360×143 | **1.46554** | 같은 이유 |
| 11 | 383×71, 366×71, 155×68 | 383×210 | **1.26226** | 1.25 한도를 조금 초과 |

- 모두 maxTexture=16384 및 1M pixel 제한보다 훨씬 작다. **hard GPU limit로 필수인 split은 아니다.**
- 25%는 메모리/RT area를 방어하는 보수적 정책이다. 4줄 limit 때문도 아니고 greedy prefix만 보고 잘못 거절한 것도 아니다. 전체 1-page 검사를 먼저 한다.
- `MAX_LINES_PER_DRAW=64`는 geometry/uniform draw chunk limit이지 page limit이 아니다. 이 fixture는 최대 3줄이다.
- 실제 coverage에서 radius24 halo 및 filtering guard를 붙인 target이다. 단순 glyph bounding box와 같지 않다. 예를 들어 id2 coverage는 306×18 / 40×14, target은 362×74 / 96×70이다.
- line-local sampling isolation용 halo다. 이를 줄이면 blur edge/bleed 계약을 다시 검증해야 한다. 이번 조사에서 과도한 halo라는 증거는 없다.
- Cards에 ImageSpan은 없다. id5의 RGBA는 색 표현 때문이며 split을 일으키지 않는다. 일반 image-bearing sequence와 A8/RGBA grouping을 통합하는 변경은 하지 않았다.
- 이 split에 추가 공통-size normalization padding은 없다. 2D atlas packing으로 바꾸면 빈 공간을 줄일 여지는 있지만 새 geometry/UV/packing 설계라 이번의 안전한 제거에 해당하지 않는다.

## H. Can page count be reduced?

**현재 pixel-equivalence/packing/memory 정책을 그대로 유지하며 바로 적용할 수 있는 reduction은 확인하지 못했다.**

가장 작은 연구 후보는 id11만 합쳐 **15→14**다. 1.25→1.26226을 허용하면 크기 자체는 충분히 작다. 그러나:

1. 추가 메모리가 필요하며 현재의 no-worse-storage 기준도 바뀐다.
2. reduced H/V의 ceil dimension 및 source-to-target texel-center phase가 달라진다. 예: height142/36 → height210/53. 같은 line rectangle을 옮기는 것만으로 pixel identity가 보장되지 않는다.
3. sync/async/ImageSpan/general packing 및 타겟 large-RT 정책까지 안전하다고 이 작은 fixture로 인증할 수 없다.

따라서 **물리적으로 불가능한 것은 아니지만 safe-to-implement로 판정하지 않는다.** merge PoC나 threshold 변경은 하지 않았다.

## I. Conditional structural savings

아래는 **합쳤다고 가정한 계산**이지 적용/측정 결과가 아니다. 합치는 대상은 전부 A8이다.

| 합칠 Label | pages 감소 | tasks/cameras/passes 감소 | HIGH bytes 증가 | PERFORMANCE bytes 증가 | ECONOMY bytes 증가 |
|---|---:|---:|---:|---:|---:|
| id11 | 1 | 3 | 46,512 | 20,349 | 17,442 |
| id10 | 1 | 3 | 49,059 | 21,396 | 18,297 |
| id2 | 1 | 3 | 55,860 | 24,425 | 20,850 |

id11만: 15→14 / 45→42 tasks. id11+id10: 15→13 / 45→39. 전부: 15→12 / 45→36, ECONOMY +56,589 bytes. 현재 fixture에는 scratch sharing을 잃는 추가 항목이 없다. draw chunk가 합쳐지는 만큼 H/V/Source draw도 줄 수 있으나 ImageSpan/64-line 일반 case의 draw 감소를 3N으로 일반화하지 않는다. CPU/FPS %로 환산하지 않았다.

## J. Source clear verdict

**NOT SAFE to remove generically.** Glyph/gradient/image capture는 page 전체를 쓰지 않는다. halo, line gaps, transparent background가 남고 premultiplied blending의 destination도 깨끗해야 한다. Source는 최종 sharp composition에도 사용된다. V의 제한적 overwrite 성질을 Source에 적용하지 않는다. Source clear는 PoC에서도 그대로다.

## K. H clear verdict

**NOT SAFE to remove generically.** HIGH/PERFORMANCE는 D2 horizontal coverage-band geometry를 사용하며 page의 일부만 쓴다. 같은 크기 page의 H는 scratch 공유도 한다. ECONOMY H는 full line rectangles지만 full page overwrite와 같지 않고 fractional reduced texel coverage/unused padding이 남는다. H clear도 그대로 유지했다.

## L. V clear verdict

**일괄 제거: NOT SAFE.**

- V는 shared scratch가 아니라 page별 persistent output이다. 이것만으로 미기록 texel이 안전해지지는 않는다.
- shader는 discard하지 않고 BlendMode OFF로 covered fragments를 덮지만, PER_LINE quad는 각 줄의 padded rectangle까지만 차지한다. 좁은 줄 오른쪽/normalized tail이 page 전체를 채우지 않는다.
- H/V read의 half-texel clamp는 **입력** rectangle을 보호한다. V가 쓰지 않은 texel을 Output이 읽는 것까지 막는 clamp가 아니다.
- Output (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:579`)은 `rectangle.xy + uv * rectangle.zw`를 LINEAR sample한다. reduced target에서 line edge가 texel boundary와 맞지 않으면 바깥 미기록 영역과 섞인다.

WHOLE_TEXT full quad 및 이번 HIGH integer-aligned fixture에서는 차이가 없었다. 하지만 이 부분 결과를 전체 policy로 확장하지 않는다. narrow fast path는 task/FBO/dependency를 하나도 없애지 않으며 충분한 일반화 검증 없이 분기를 늘릴 근거가 없다. 이번 단계의 좁은 subset 제안은 **NOT WORTH COMPLEXITY**로 보류한다. clear GPU 시간이 실제로 negligible하다고 측정한 것은 아니다.

## M. Garbage-seeded clear tests

외부 quality.cpp (로컬 자료: `quality.cpp`), 현재 production reference paths. Clear 실험에는 `single-` 후보 Label을 만들지 않는다.

1. R24 / 361×151 odd Label / MSAA4 / sync / Unit LINE / Fade0 / Stagger0 / BlurDuration1.
2. HIGH/PERFORMANCE/ECONOMY 세 열; Latin(얇은 IIII 포함), Korean24, bold Korean32, gradient 네 행.
3. p=0/.20/.50/.75/.90/1 baseline 캡처.
4. 모든 V의 clear color를 **magenta / alpha1**로 바꾸고 실제 여러 frame render. 덮지 않은 영역이 garbage로 남는 seed다.
5. V clear만 OFF. Source/H, shaders, dimensions, samplers 모두 그대로.
6. p=1/.90/.75/.50/.20/0으로 역순 direct seek, current progress 확인 후 캡처. baseline과 같은 화면 위치별 RGB 비교.

| PER_LINE | p0 | p.20 | p.50 | p.75 | p.90 | p1 |
|---|---:|---:|---:|---:|---:|---:|
| HIGH max RGB difference /255 | 0 | 0 | 0 | 0 | 0 | 0 |
| PERFORMANCE | **215** | **215** | **215** | 33 | 0 | 0 |
| ECONOMY | **194** | **194** | **167** | 28 | 1 | 0 |

p.20에서는 PERFORMANCE/ECONOMY 모두 네 행 합계 **1,326 pixels**가 달라졌다. White A8의 흰 선과 RGBA gradient의 마젠타 선을 직접 확인했다.

- [baseline p.20](clear-perline-validated/base-1.png)
- [V clear 제거 p.20](clear-perline-validated/noclear-1.png)
- [V clear 제거 p0](clear-perline-validated/noclear-0.png)
- WHOLE_TEXT log (로컬 자료: `clear-whole-validated.log`): 같은 네 내용·세 quality·여섯 progress에서 차이 0.
- 전체 수치: measurements.json (로컬 자료: `measurements.json`), 계산: analyze.py (로컬 자료: `analyze.py`). 각 470×215 panel의 native RGB pixels를 명도 보정 없이 비교했다.

PER_LINE은 multiple pages를 포함한다. **clear 일괄 제거에 대한 구체적 반례를 얻었으므로** 64/65 lines, 큰 line gap, ImageSpan/multicolor, 다른 radius, async, fractional UI scale까지 acceptance matrix를 확대하지 않았다. 연속 reverse animation 대신 역순 seek를 확인했다. 미수행 항목을 PASS로 처리하지 않으며, 전체 조건 통과를 근거로 한 clear 최적화 인증도 아니다.

초기 harness는 event flush가 부족해 오래된 화면을 캡처했다. 변경마다 `KeepRendering`을 요청하고 current progress를 로그로 확인한 `*-validated`만 판정에 사용했다. `clear-perline/`은 제외한다.

## N. Safe no-quality-loss optimizations

- 명확한 실용책은 **필요 없어진 단발 Reveal runtime을 앱이 None으로 해제하는 것**이다. ImageSpan 등의 presentation 차이에 대한 위 caveat가 있으며 bit identity 보장은 아니다.
- page/clear에서는 현재 일반 조건을 유지하면서 즉시 적용할 production 변경을 제안하지 않는다.
- 단순 H/V tap 절감이나 cache 추가로 논점을 바꾸지 않는다.

## O. Single-blur architecture

```text
Current ECONOMY: full Source → reduced H → reduced V → Output(Source, V)
Disposable:      full Source ───────────→ reduced 8-tap → same Output(Source, blur)
```

quarter target은 각 축 ceil(W/4), ceil(H/4)다. Source capture, foreground data, per-line isolation bounds, halo, output geometry, reveal progress/sequence schedule은 current와 같다.

외부 runtime copy에서 candidate owner에 한해 H Actor/FBO/Task/renderer 생성을 생략하고 기존 V slot에 Source를 직접 입력했다. **H를 실행하면서 V shader만 교체한 비교가 아니다.** task reorder는 기존처럼 빈 slot을 건너뛰며 Source→blur 의존 순서를 유지한다.

## P. Candidate families reviewed

| family | 판단 |
|---|---|
| 4-tap box / rotated box | 가장 가볍지만 넓은 full Source를 평균하기에는 footprint 부족 우려. 미구현 |
| 5-tap cross | center residual과 축 방향 bias 우려. 미구현 |
| **8-tap symmetric ring** | center tap 없이 양 축과 대각선을 균등하게 다루는 최소 후보로 한 가지만 구현 |
| single-pass Kawase-like | 직접 Source를 읽는 small-tap의 sparse footprint 문제가 동일하게 우려됨. multi-pass 누적을 추가하면 범위 밖 |

과거 [dense9](../reveal-dense2d-quality.ECWtnl/REPORT.md), [frequency9](../reveal-frequency-quality.HryP4o/REPORT.md), [independent-strength9](../reveal-independent-quality.On1JuQ/REPORT.md), [1/3 7×7 / 1/2 5×5](../reveal-resolution-tradeoff.OHZfwT/REPORT.md)를 확인했다. Gaussian fidelity를 새로 fitting하거나 기존 실패 후보를 재실행하지 않았다.

## Q. Selected disposable PoC

single-filter.h (로컬 자료: `single-filter.h`) / runtime-audit.cpp (로컬 자료: `runtime-audit.cpp`).

- 8방향(수평/수직/45°), 동일 가중치 1/8. offset distance = **0.42 × current ECONOMY effective radius**.
- 0.42는 축 방향 표준편차가 약 0.297R이 되는 단순한 초기 support다. 화면을 보고 sweep/tuning하지 않았으며 기존 Gaussian과 정확히 일치시킨 것도 아니다.
- offset만 uniform scale로 처리. 현재 `Radius()`, `Composition()`, gamma1, native timeline은 유지한다.
- full Source를 LINEAR sample하며 기존 line-local rectangle + Source half texel로 clamp한다. blur 출력 한 장 외에 prefilter/texture/temporal history는 없다.
- sequence 시작 전/progress0은 투명, strength0은 현재 Source copy. Output의 sharp transition도 그대로다.
- candidate shader는 scalar/batch 두 개의 thread-local cache다. radius/layout별 kernel/UBO 생성은 없다.

PoC는 geometry를 얻기 위해 기존 renderer factory를 사용한 뒤 shader를 교체하므로 기본 Gaussian shader cache가 warm될 수 있다. setup 최적화까지 마친 production 후보가 아니라 품질 PoC다. candidate는 layout 의존 reduced-V kernel을 생성하지 않는다.

## R. Quality comparison

1410×890 window의 세 열은 **PERFORMANCE | current ECONOMY | single8**이다. 동일한 361×151 text area, text/style/position phase(470px 정수 평행 이동), PER_LINE/stagger0, native font size를 사용했다. 네 행은 English24 / Korean24 / bold Korean32 / gradient다.

R16/R24/R48 × p0/.20/.50/.75/.90/1을 정지 화면으로 확인했다. p.20/.50에서 candidate가 current ECONOMY보다 명확히 나쁘다. 얇은 glyph 잔상, 두꺼운 glyph의 규칙적 modulation, gradient의 격자가 나타났다. p.75 이후 sharp Source 가중치가 커지면서 차이가 약해지지만 초기 품질 FAIL을 해소하지 못한다.

- [R24 p.20](quality24-final/base-1.png)
- [R24 p.50](quality24-final/base-2.png)
- [R24 p.75](quality24-final/base-3.png)
- [R48 p.20](quality48-final/base-1.png)
- [R48 p.50](quality48-final/base-2.png)
- [R16 p.20](quality16-final/base-1.png)
- [Korean R24: ECONOMY | single8, nearest 2×](quality24-korean-p20-2x.png)

기존 ECONOMY에도 Gaussian fidelity의 trade-off가 있지만 candidate의 격자가 더 뚜렷하다. 단순한 밝기 차이가 아니다. quarter target을 만드는 것만으로 full Source의 넓은 영역이 평균되지는 않는다. 소수의 떨어진 LINEAR reads가 고주파 문자 획 일부를 선택적으로 읽는다는 설명과 결과가 일치한다. 다만 이번에 모든 주파수 성분을 새로 측정한 것은 아니다.

Shader compile 실패가 있었던 초기 `quality24-validated/`와 flush 미검증 `quality24/`는 판정에서 제외했다. 최종 `quality16/24/48-final`에는 candidate compile/link failure가 없다. R48에는 미생성 shader-cache 파일 읽기 miss 로그가 있지만 이후 compile/link failure는 없다.

## S. Radius findings

| authored radius | 결과 |
|---|---|
| 16 | p.20부터 윤곽/점 형태의 잔상이 강하다. 작은 radius도 PASS가 아님 |
| 24 | entrance 초기 격자와 복제된 듯한 가는 선이 뚜렷함. STOP 조건 A/B |
| 48 | strong blur에서 듬성듬성한 샘플 무늬가 확대. p.50에도 stroke ghosting |

R24/R48에서 가시적 격자가 발생했으므로 tap 추가, resolution 1/3·1/2, radius floor/composition curve 재조정, slow animation 추가 평가로 진행하지 않았다. FPS/CPU/GPU benchmark도 하지 않았다.

## T. A8 / RGBA / PER_LINE

- A8는 같은 Source channel을 평균하고 기존 Output에서 text color를 복원한다.
- RGBA는 premultiplied 네 성분을 동일한 비음수·합1 weights로 평균한다. 별도 color reconstruction은 없다.
- Gradient RGBA source도 native 실행했지만 품질 FAIL이다. ImageSpan/multicolor도 공통 Source 입력으로 구조상 처리 가능하지만 candidate 개별 검증은 품질 gate에서 중단했다.
- PER_LINE은 같은 line rectangle/half-texel clamp를 사용하며 인접 line/page를 읽도록 설계하지 않았다. 큰 line gap, 64/65 lines, async/scale/tiling, WHOLE scalar candidate의 전면 검증은 하지 않았다.
- Halo는 유지했다. ring 최대 offset이 current radius보다 작아 추가 halo는 필요하지 않다. halo 축소나 page 재packing을 이득에 포함하지 않는다.

## U. Structural accounting — not a performance benchmark

1 page / ≤64 lines / 단순 text source 기준:

| 항목 | ECONOMY | single8 |
|---|---:|---:|
| offscreen tasks / passes | 3 | 2 |
| offscreen cameras | 3 | 2 |
| Source/H/V FBO attachments | 3 | 2 |
| filtering renderers/draws | 2 | 1 |
| Source draw | 1 | 1 |
| onscreen Output | 동일 | 동일 |
| filtering sample expressions / reduced fragment | H: R reads + V: reduced-kernel reads | 8 reads |
| full-size Source | 유지 | 유지 |

큰 draw chunk와 ImageSpan 추가 draw는 별도다. 여러 page가 H scratch를 공유한다면 FBO 감소 수는 page 수가 아닌 distinct scratch 수다. tasks/cameras는 page당 하나씩 감소한다.

**실제 PoC의 R24 네 내용 합계**는 5 pages다. 로그에서 PERFORMANCE15 / ECONOMY15 / candidate10 offscreen tasks를 확인했다. 각 task subtree에 draw renderer가 하나씩 있어 offscreen renderers도 15→10이다. Source 10개가 아니라 Source5 + blur5다. 입력 문자 내용이나 줄 분할은 줄이지 않았다.

| 4-Label fixture | PERFORMANCE logical bytes | ECONOMY | candidate |
|---|---:|---:|---:|
| R16 | 416,831 | 357,097 | 337,002 |
| R24 | 555,887 | 476,209 | 449,454 |
| R48 | 1,053,695 | 902,665 | 852,090 |

실제로 얻은 texture 크기×Pixel bpp 계산이다. RSS/VRAM 실측이나 Source/Output/clear/driver allocation 시간을 뜻하지 않는다. R24의 ECONOMY→candidate payload 차이는 26,755 bytes, 약 5.6%에 그친다. full Source가 남기 때문이다.

Cards 15 pages에 같은 방식을 적용한다고 가정한 **미적용 산술**은 45→30 tasks/cameras, logical FBO 517,635→488,433 bytes(−29,202)다. 품질 FAIL이므로 production 개선 결과로 보고하지 않는다.

R24, 축소율 약4라면 current ECONOMY의 H24 + V약6 reads에 비해 candidate는8이다. 실제 V kernel은 실제 크기와 sigma를 반영하므로 끝수/작은 radius에서 이 개수를 고정값으로 취급하지 않는다. sample/task 수를 target FPS %로 환산하지 않는다.

## V. Resource / shader ownership

소수 고정 tap은 radius uniform으로 처리할 수 있어 layout-dependent V UBO가 필요 없다는 설계상 이점이 있다. page-local Source/blur 두 단계만 소유하며 async publication/replacement ownership도 바꾸지 않을 가능성이 있다.

외부 구현은 기존 runtime owner가 두 단계를 보유하며 reorder/disconnect는 빈 H slot을 건너뛴다. page별 unique shader, 거대한 UBO, 추가 texture/geometry rebuild를 만들지 않았다. 새로운 generic factory나 Core/Adaptor extension도 없다.

단, 품질 확인용 외부 코드이며 reentrant creation/retirement, 모든 fallback/async/ImageSpan, memory-admission 산술까지 새 topology로 상품화 감사한 구현은 아니다. 일반화 전에 품질 gate에서 중단했다.

## W. Rejected / deferred approaches

- Source/H/V clear 일괄 제거: V 반례가 있으며 Source/H도 coverage상 부적합.
- 15→12를 메모리 부담 없는 최적화로 취급: 추가 payload/texel phase 변화를 무시하므로 불가.
- 좁은 WHOLE-only clear 분기: stage 수는 줄이지 않으며 일반화 검증 미완료. 보류.
- 8-tap single blur: current ECONOMY보다 명확한 품질 저하.
- 다른 tap/계수 sweep, 추가 prefilter, pyramid, temporal accumulation: STOP 이후 진행하지 않음.
- 과거 dense Gaussian 연구 재실행, generic Gaussian/Core/Adaptor 변경: 범위 밖.
- production docs/API 실제 변경: 이번에는 제안만 작성.

## X. Production risk

production branch는 변경하지 않았으므로 새 page/clear/single-blur 로직에 따른 production 동작 리스크는 추가하지 않았다. 진단 라이브러리는 이 디렉터리의 `lib/`에만 두고 프로세스 한정 LD_LIBRARY_PATH로 사용했다.

None guideline의 주요 리스크는 종료 판정, 오래된 callback의 새 효과 제거, visible progress0 오해제, 짧은 주기의 재setup, async retirement 시차다. 현재 demo 범위에서는 기존 token/정지 순서/endpoint guard로 처리하고 있다.

이번 조사는 lifecycle 전체 증명, 새로운 full UTC regression, 장시간 메모리 누수 시험, GPU performance 인증이 아니다.

## Y. Recommended next action — one verdict

**KEEP CURRENT ECONOMY; FURTHER STAGE REDUCTION FAILS QUALITY**

single8 후보는 채택하지 않는다. page/clear에도 즉시 채택을 보장할 no-quality-loss 변경은 없다. 현재 production ECONOMY와 sample early cleanup을 유지한다. 가벼운 후속 결과물로는 D의 조건부 None usage guideline 문서화가 적절하지만, 이번에는 문서/API source를 변경하지 않았다.

id11의 15→14 page 후보는 작은 개선 여지로 기록했다. 하지만 새 메모리 허용/packing 정책과 pixel 비교가 필요하므로 이번 채택 결론에는 포함하지 않는다.

## Z. Git state / artifacts / reproducibility

- UI: `devel_blur_text`, **83f8a7665a3b5dd3b2cc93a7ccc4d61991d499bf**, 시작/종료 clean.
- Core: `f43e95be477ad301f84ecc772c753f9357821ecc`, clean.
- Adaptor: `dcadcfdc3e0d1abdce20767bee2858cb9e1851a4`, clean.
- commit/amend/rebase/reset/restore/stash/push 없음. target build/install/profile 없음.
- production build/install, UTC, sanitizer, CPU/FPS/GPU timer/RSS/VRAM 측정 없음.
- 외부 진단 runtime object/라이브러리와 harness만 compile했다. 이번에 새로 만든 파일은 모두 이 디렉터리에 있다.
- sample backup (로컬 자료: `sample-cleanup-backup.patch`), build (로컬 자료: `build.mk`), private link helper (로컬 자료: `link-private.py`), inventory (로컬 자료: `inventory.cpp`), quality/clear probe (로컬 자료: `quality.cpp`), raw results (로컬 자료: `measurements.json`).

위 R24 이미지 링크에서 품질을 확인할 수 있다. 짧은 자동 재생/capture를 다시 실행하려면:

```bash
bash /home/bowonryuubuntu/tizen/reveal-lifecycle-structure.MYQiQb/run-quality.sh 24
```

PERFORMANCE | ECONOMY | rejected single8을 정지 progress별로 표시하고 자동 종료한다. 새 capture 디렉터리를 만들며 기존 자료를 덮어쓰지 않는다. target용이 아니다.
