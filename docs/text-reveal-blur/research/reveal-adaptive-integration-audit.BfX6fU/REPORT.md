# Adaptive Gaussian integration — Phase A audit

2026-09-15 · source audit, not a runtime/performance measurement

## A. Initial Repository State

| Item | Value |
|---|---|
| Repository | `/home/bowonryuubuntu/tizen/dali/dali-ui` |
| Branch | `devel_blur_text` |
| HEAD | `46d0ea182d77af03b8b75ab6e5480bf5d591624c` — Batch text reveal blur sources |
| Worktree / index | Both clean |
| Backup ref created | `backup/reveal-before-adaptive-poc-20260915` |
| Backup target | Same HEAD, `46d0ea182d77af03b8b75ab6e5480bf5d591624c` |

Initial command results (로컬 자료: `INITIAL_STATE.txt`). Backup ref만 추가했다. 기존 commit, branch HEAD, source, sample, tests, index는 변경하지 않았다. 별도 dali-adaptor의 기존 dirty file도 보존했다.

기존 PoC의 adaptive.inc (로컬 자료: `/home/bowonryuubuntu/tizen/reveal-adaptive-study.8A0C8e/adaptive.inc`), kernels.h (로컬 자료: `/home/bowonryuubuntu/tizen/reveal-adaptive-study.8A0C8e/kernels.h`), [REPORT.md](../reveal-adaptive-study.8A0C8e/REPORT.md)를 읽었다. Coefficients/threshold를 새로 계산하거나 조정하지 않았다. Middle-range 후속 후보도 사용하지 않았다.

## B. text-effect-demo Audit

### 1. Strong / Performance는 실제로 무엇인가

샘플의 BLUR_PRESETS (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:66`)는 Strong = entrance 24 / exit 48이다. 초기 상태는 Strong(index 0), PERFORMANCE, blur enabled, Sync다. quality toggle (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:753`)은 표시 문자열만 바꾸는 것이 아니라 실제 `mBlurQuality`를 변경한다.

ConfigureEntranceReveal (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:303`):

| Recipe | Quality | Sequence | Fade | Stagger | Radius | Blur duration ratio |
|---|---|---|---:|---:|---:|---:|
| Short entrance | PERFORMANCE | PER_LINE | 0 | 0 | 24 | 1 |
| Long entrance | PERFORMANCE | PER_LINE | 0 | .25 | 24 | .5 |
| Completed-text exit | PERFORMANCE | WHOLE_TEXT | 1 | 0 | 48 | 1 |

Long/short는 final line count가 아니라 UTF-8 code-point 수 64 이상인지로 구분한다. 일반 entrance는 long 2초 / short 1초, EASE_OUT_SQUARE, Reveal lead .06초다. 카드/상태 문구는 아래의 별도 duration을 사용한다.

중요: 마지막 표의 exit recipe는 **모든 퇴장에 무조건 적용되지 않는다.** ConfigureExitReveal (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:319`)는 기존 Reveal이 None이 아니고 현재 progress < 1이면 설정을 바꾸지 않고 return한다.

### 2. 실제 scene / Label 목록

`BuildScene → StartSceneTextEntrance → StartSceneActivity`를 포함해 sample 2,401줄 전체를 확인했다. CollectSceneLabels (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:1279`)는 scene 내부 Label을 수집하며 Markdown subtree는 의도적으로 건너뛴다.

아래는 **각 entrance가 완료된 뒤**, blur ON / Performance / Strong을 유지하고 `TransitionTo()`로 퇴장할 때의 표다. `Yes*`는 authored recipe가 일치한다는 뜻이며, 아직 adaptive shader를 통합하거나 실제 선택을 테스트했다는 뜻이 아니다. 미완료 entrance에서의 반례는 다음 절에 따로 적었다.

모든 표 행의 일반 scene exit는 `progress 1 → 0`, **.40초 / LINEAR**, Stagger=0이다. Completed callback에서 `None()`으로 정리된 Label은 exit를 만들 때 Unit=PIXEL이 된다. Callback 직전 progress=1인 기존 Reveal은 Unit을 유지할 수 있지만 Fade=1 whole-text 수식에는 영향이 없다.

형식 분류: `A8*`는 일반 monochrome foreground의 A8 경로다. 실제 FBO A8 사용에는 GLES3+와 source L8 등 기존 조건이 필요하다. 임의 플랫폼/폰트의 실제 Pixel::Format을 읽어 확인했다는 뜻이 아니다. `RGBA`는 gradient/overlay가 들어가 A8-only 조건에서 제외되는 경로다. 기호 glyph의 color fallback 가능성도 별도로 적었다.

| Scene / Text or Label | Disappear path | Quality | Sequence | Fade | Radius | BlurDuration | Unit | Foreground class | Eligible? |
|---|---|---|---|---:|---:|---:|---|---|---|
| Intro / brand | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Intro / Plan your next escape | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Intro / JEJU | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | RGBA gradient | Yes* |
| Intro / 3 DAYS · NATURE · RELAX | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Intro / Create My Trip → | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* / symbol fallback | Yes* recipe |
| Intro / Personalized itinerary powered by AI | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Generating / brand | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Generating / JEJU · 3 DAYS | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Generating / Creating your perfect Jeju escape | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | RGBA SCREEN overlay | Yes* recipe |
| Generating / current status | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL or CHARACTER | A8* | Yes* |
| Generating / skeleton DAY 1 | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Generating / skeleton DAY 2 | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Generating / skeleton DAY 3 | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Results / brand | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Results / JEJU · 3 DAYS · RELAXED | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Results / Your personalized escape | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Results / A slower route through forests… | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Results / Select the best match… hint | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 1 / DAY 1 | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 1 / Forest & Oreum | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 1 / Bijarim Forest · Abu Oreum · Local Cafe | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 1 / Walk beneath ancient cedars… | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 2 / DAY 2 | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 2 / Sea & Sunset | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | RGBA gradient / temporary overlay | Yes* recipe |
| Card 2 / Woljeongri · Sehwa · Hamdeok | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 2 / Follow the eastern coastline slowly… | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 2 / BEST MATCH badge | Scene exit after badge entrance | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* foreground | Yes* |
| Card 2 / View day plan → | Scene exit after action entrance | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | RGBA gradient / symbol | Yes* recipe |
| Card 3 / DAY 3 | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 3 / Market & Old Town | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 3 / Dongmun Market · Old Jeju · Local Dessert | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Card 3 / Browse the morning market… | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Detail / ← Back | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* / symbol fallback | Yes* recipe |
| Detail / brand | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Detail / DAY 2 · BEST MATCH | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Detail / Sea & Sunset | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | RGBA gradient | Yes* |
| Detail / Woljeongri · Sehwa · Hamdeok | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |
| Detail / current status | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL / CHARACTER / WORD | A8* or RGBA overlay/symbol | Yes* recipe |
| Detail / Scroll to explore the complete day plan | Scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL | A8* | Yes* |

Scene별 Label slot은 6 / 7 / 19 / 7개다. 동시에 39개라는 뜻은 아니다. Badge/action이 아직 mount되지 않은 Results는 17개다. `CollectSceneLabels()`는 ScrollView viewport 안에 현재 보이는 glyph만 세는 것이 아니라 scene의 Label 객체를 수집한다.

Sample 생성 위치: Intro (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:850`), Generating (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:956`), Cards (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:987`), Results (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:1094`), Detail (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:1132`).

### 3. Scene 내부 문자열 교체에 의한 퇴장

단순 scene exit 이외에 동일 Label의 이전 문구가 사라지는 경로도 있다.

| Previous text | Next state / disappear path | Quality | Sequence | Fade | Radius | BlurDuration | Unit at completed exit | Direction / time / alpha | Eligible? |
|---|---|---|---|---:|---:|---:|---|---|---|
| Understanding your travel style… | TransitionGeneratingStatus → next string | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL, or retained CHARACTER at p=1 | 1→0 / .28s / LINEAR | Yes* |
| Finding quiet places away from the crowd… | Same | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | Same | Same | Yes* |
| Balancing travel time and experiences… | Same | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | Same | Same | Yes* |
| Building your personalized itinerary… | Automatic generating → results scene exit | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | Same | 1→0 / .40s / LINEAR | Yes* |
| Planning your day…✦ | StartWritingStatus | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL, or retained CHARACTER at p=1 | 1→0 / .28s / LINEAR | Yes* recipe |
| Writing your plan…✦ | FinishMarkdownStreaming | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | Same | Same | Yes* if entrance finished |
| Your day is ready ✓ | Back → results | PERFORMANCE | WHOLE_TEXT | 1 | 48 | 1 | PIXEL, or retained WORD at p=1 | 1→0 / .40s / LINEAR | Yes* if entrance finished |

Generating status는 1.3초 간격 timer, exit .28초, 다음 entrance .52초 + lead .06초다. 정상적으로 진행되면 entrance가 완료된 뒤 다음 퇴장 설정을 한다. Detail의 Planning은 entrance 1.00초 + lead .06초 완료 후 Markdown delay 3초와 panel delay .18초를 거쳐 Writing으로 교체된다. Writing 완료는 Markdown timer 진행에 의존하므로 상태 이름만으로 progress=1을 단정하지 않는다.

Detail status 자체 교체는 `StopShimmer()`로 overlay를 정리한다. 반면 scene exit의 `StopSceneActivity(true)`는 gradient/shimmer를 보존한다. 따라서 animated SCREEN overlay가 포함된 퇴장은 recipe는 같아도 과거 static gradient capture의 pixel 오차 보증을 그대로 확장할 수 없다.

근거: Generating status (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:1487`), Planning→Writing (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:1692`), Writing→Completed (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:2310`).

### 4. 실제 불일치: interrupted entrance → exit

이 항목 때문에 strict한 ALL 판정은 할 수 없다. 단순히 blur를 사용하지 않는 로딩/Markdown을 포함시켜 얻은 결론이 아니다.

공통 경로:

```text
TransitionTo()
  → StopSceneActivity(true): 진행 중인 progress animator 정지
  → ConfigureExitReveal(label)
      current Reveal != None && current progress < 1
      → return (새 WHOLE_TEXT recipe로 교체하지 않음)
  → 현재 progress에서 0까지 .40s LINEAR
```

| Reachable situation | Affected text | Actual retained quality / sequence / fade / radius / blur time | Eligible? |
|---|---|---|---|
| Intro entrance가 끝나기 전에 Create/Enter | 아직 나타나는 Intro Labels | PERFORMANCE / PER_LINE / 0 / 24 / 1 (long이면 .5) | No |
| Generating 중 Back | 아직 등장 중인 scene/status Labels | PERFORMANCE / PER_LINE / 0 / 24 / 1 또는 .5 | No |
| Results 카드 등장 중 Back | day/title/places/subtitle 중 미완료 항목 | PERFORMANCE / PER_LINE / 0 / 24 / 1 또는 .5 | No |
| RESULTS_READY 직후 상세 화면으로 이동 | 새로 등장 중인 BEST MATCH, View day plan → | PERFORMANCE / PER_LINE / 0 / 24 / 1 | No |
| Detail status 등장/교체 중 Back | Planning / Writing / Completed 등 미완료 항목 | PERFORMANCE / PER_LINE / 0 / 24 / 1 | No |

특히 네 번째 반례는 보통의 forward navigation으로 가능하다. StartResultReveal completion (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:1557`)은 카드 본문이 끝나면 즉시 `mState=RESULTS_READY`로 바꾸고 `StartDay2Highlight()`를 부른다. 그때 badge/action은 **.60초 + .06초 lead** entrance를 새로 시작한다. OpenDay2Detail (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/samples/text/text-effect-demo.cpp:1646`)은 RESULTS_READY인지 검사할 뿐 badge/action 완료를 기다리지 않는다. 이 약 .66초 구간 중 이미 보이기 시작한 두 Label도 퇴장할 수 있다.

카드 본문의 마지막 subtitle은 delay .96 + .42 + lead .06 + duration 2.00 = **3.44초**로 설정된다. `RESULTS_READY`는 모든 affordance entrance까지 완료되었다는 의미가 아니다. 이 시간은 sample 설정값의 합이며, 실제 frame presentation/async publication 완료시간 측정값은 아니다.

이 guard는 correctness 보호다. 미완료 PER_LINE을 강제로 WHOLE_TEXT Fade1 / p=1로 바꾸면 아직 숨겨진 글자를 노출할 수 있으므로, PoC eligibility를 맞추려고 제거하면 안 된다. Label progress getter (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/integration-api/label-impl.cpp:906`)는 on-scene에서 current property를 읽는다. Animation::Stop (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-core/dali/internal/event/animation/animation-impl.cpp:586`)도 기본 BAKE에서 현재 값을 보존하며 강제로 최종값에 도달시키는 방식이 아니다.

### 5. Reveal 대상이 아닌 disappearance

| Text / object | How it disappears | PoC 대상 여부 |
|---|---|---|
| Preparing your escape | 2초 loading-view opacity keyframes; 마지막 .18초 parent fade | No Reveal; exclude |
| Markdown 내부 모든 heading/body/list/quote/emoji | Scene parent fade 또는 streaming 재구성, scroll clipping, immediate removal | CollectSceneLabels에서 제외; exclude |
| 현재 scene 전체, 0 restart / DETAIL_READY에서 Enter restart | ShowLoading → RemoveCurrentSceneImmediately | Reveal exit를 호출하지 않는 즉시 제거; exclude |
| Blur ON / Strong / Performance control labels | Scene 밖에 유지; shutdown에서는 unparent | Animated text exit 대상 아님 |
| Skeleton shimmer bars / loading dots | View background / parent opacity | Text/Label 아님 |

Markdown source의 🌊/🍜/🌅/✨️는 Reveal 대상이 아니다. 샘플에는 별도의 ImageSpan/Replacement construction이 없다. `✦`, `✓`, 화살표는 text glyph로 입력되며 font fallback에 따라 color glyph가 될 가능성까지 source audit만으로 배제하지 않는다. 이번 보고서는 이를 새로 framebuffer 검증하지 않았다.

### 6. Shared instance / recipe mutation / Sync vs Async

- Entrance와 exit helper는 각 Label마다 지역 `Text::Reveal`을 새로 만든다. Reveal copy constructor (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/public-api/text/style/reveal.cpp:74`)는 Impl deep copy다. Label Set/Get (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/integration-api/label-impl.cpp:784`)는 authored fields를 저장/재구성한다. 모든 Label이 하나의 mutable Reveal instance를 공유하는 구조가 아니다.
- `UpdateSceneBlur()`는 현재 active Reveal의 radius/quality를 업데이트하지만 unit/fade/stagger/blur time/progress animator는 유지한다. Toggle 중이면 recipe도 변경된다. Performance+Strong 고정 A/B라면 애니메이션 도중 control을 바꾸지 않아야 한다.
- `None()` 상태의 완료 문구는 toggle 시 그대로 두고 다음 entrance/exit에서 설정을 받는다.
- 1/2번 키는 `SetAsyncRendering(false/true)`를 바꾼다. Recipe 분기나 exit alpha/duration은 Sync/Async 공통이다. 내부 publication 지연이 없다고 증명한 것은 아니다.
- Scene exit는 .40초 Reveal와 parent .22초 EASE_IN opacity fade(.18초 delay)가 함께 진행된다. Parent opacity를 포함한 최종 합성은 이전 검증용 FHD Label의 단독 reverse와 다르다. H/V recipe 대응과 타겟 전체 FPS 보장은 별개다.
- Runtime crop/odd sizes, font, animated gradient overlay, async 교체 및 타겟 compiler는 과거 90 captures의 보증 범위와 동일하지 않다. 이번 audit에서 새로 통과했다고 주장하지 않는다.

### 7. Unit / float / resolved kernel 확인

**Unit**: PopulateSchedule (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal.cpp:136`)에서 Fade=1이면 `(1-fade)/(count-1)=0`, 모든 unit start=0이다. CHARACTER/WORD 및 final-visible LINE은 이 unit schedule을 사용한다. PIXEL final timing (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal.cpp:1301`)도 WHOLE_TEXT에서 totalDuration=1, Fade=1이면 모든 unitStart와 spatial progressionSpan이 0, fade duration=1이다. Unit이 CHARACTER/WORD라는 이유만으로 reject할 필요는 없다.

**Blur timing**: GetRevealBlurPlanTiming / normalization (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp:315`)에서 text-backed WHOLE_TEXT start=0, above fade end=1, sequenceDuration=1이다. BlurTime=1이면 `max(revealEnd, start+blur)=1`, resolved blur duration도 1이다. BlurStrength (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:62`)는 `1-p*p*(3-2*p)`가 되어 기존 PoC와 맞는다.

**Authored float**: public setters (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/public-api/text/style/reveal.cpp:192`)는 유효한 1.0/48.0을 그대로 저장한다. 이진 float로 정확히 표현되는 상수이며 여기에는 넓은 epsilon이 필요 없다. Label/Visual은 일부 no-change 판단에 기존 `Dali::Equals`를 쓰지만 저장되는 상수 자체를 재튜닝하지 않는다. 47.9나 .99를 포함하도록 eligibility를 넓힐 근거는 없다.

**Radius / samples**: ResolveRevealBlurRadius (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp:32`)는 최소4와 even rounding을 적용한다. Authored 48, effective UI scale=1이면 resolved kernel radius=48이고 Gaussian factory (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/render-effects/gaussian-blur-algorithm.cpp:317`)의 `radius >> 1`은 **24 positive pairs**다. PERFORMANCE는 axis-aware quarter (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:2068`)를 선택하나 Gaussian pair count를 1/4로 바꾸지 않는다.

중요한 범위: sample은 UI/render scale을 직접 설정하지 않는다. Production은 authored radius × effective scale (로컬 자료: `/home/bowonryuubuntu/tizen/dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:3165`)을 사용한다. 과거 PoC는 UI/render scale=1 fixture였고 진단 eligibility에 UI scale=1 검사도 있다. 따라서 Strong 문자열만으로 모든 외부 scale 환경에서 24-pair/동일 Late Smooth 반경을 보장할 수 없다. 후속 통합이 승인되더라도 resolved display radius, kernel pair count, 검증된 scale 조건을 좁게 확인해야 한다. Sample상 모든 Label의 authored 값이 같다는 주장과 runtime eligibility를 구분한다.

## C. Audit Verdict

**PARTIAL ELIGIBILITY**

- Strong mode 자체가 다른 recipe인 것은 아니다. 완료된 텍스트의 정상 scene exit와 일반 status replacement는 기존 PoC의 의도한 수혜 대상이다.
- 하지만 실제로 퇴장하는 Reveal text 중에서도 interrupted entrance는 PER_LINE / Fade0 / radius24를 유지한다. 특히 RESULTS_READY 직후 badge/action 퇴장은 일상적인 forward navigation에서도 발생 가능하다.
- No-Reveal loading/Markdown/immediate reset은 별도 제외다. 이들만을 이유로 partial 판정을 내린 것이 아니다.
- Animated overlay/기호 color fallback 등은 recipe 일치와 기존 품질 검증 범위를 구분해야 한다.

요청의 Phase A CASE B와 마지막 원칙에 따라 **이 mismatch를 먼저 보고하고 production integration 전에 멈췄다.** 모든 relevant text가 통과했다고 재해석하거나 helper guard/sample preset을 바꾸지 않았다.

## D. Adaptive Integration

**Not performed — Phase A gate did not pass unconditionally.**

Source 변경 파일 없음. 기존 5-tier는 읽기만 했다:

| Progress | Validated pairs |
|---|---:|
| .86 ≤ p < 1 | 2 |
| .82 ≤ p < .86 | 12 |
| .60 < p < .82 | 20 |
| .20 < p ≤ .60 | 24 exact |
| 0 < p ≤ .20 | 16 |

현재 production은 여전히 이전 exact H/V path다. Private coefficients, tier predicates, new shader/cache/property/env switch를 repository에 추가하지 않았다.

## E. Unchanged Architecture

Source, Source batching, H/V FBO, Output/Late Smooth, HIGH, PER_LINE, lifecycle, async publication, ImageSpan ownership, public API 전부 변경 없음. Sample도 변경 없음.

## F. Focused Validation

| Item | Result |
|---|---|
| Full sample source / recipe / navigation audit | Completed, concrete partial eligibility found |
| Production schedule / float / radius / shader path read | Completed |
| Foundation/components build | Not run: integration gate stop |
| Eligibility UTC | Not added/run |
| Framebuffer parent-vs-PoC spot-check | Not run: no candidate build |
| Lifecycle smoke | Not run: no runtime change |
| Performance/memory remeasurement | Not run |
| `git diff --check` | Passed; diff empty |

과거 90 framebuffer comparisons, GPU/CPU 결과는 reference 자료이며 이번 integration 검증 실적으로 중복 계산하지 않았다.

## G. Demo Build

Not run. Demo source 및 installed/build artifacts를 변경하지 않았다.

## H. Target Test Procedure / Next decision

아직 parent-vs-new-commit 비교본은 없다. 현재 HEAD로 실행하면 adaptive를 사용하지 않는다.

제안은 sample을 수정하는 것이 아니라, **완료된 텍스트의 조건에 맞는 퇴장만 adaptive 대상으로 삼고, interrupted entrance / 기타 제외 항목은 원래 exact 경로를 유지하는 제한적 통합**이다. 이것은 부분 수혜를 전제로 하므로 사용자의 확인 후 진행한다. PoC 조건을 넓히거나 미완료 entrance 보호를 삭제할 이유는 없다.

승인 후 타겟 A/B에서는 Performance + Strong을 선택하고 각 entrance, 특히 badge/action까지 완료된 뒤 동일한 navigation을 반복한다. 동시에 early Back/바로 상세 이동은 exact fallback을 검증하는 별도 케이스로 남긴다. Average/min-like FPS, visible frame drop, WHOLE_TEXT 퇴장 중간 .2~.6 구간, brightness/grid/halo jump를 기록한다. 현재 단계에서는 타겟 개선율이나 품질 판정을 새로 제공하지 않는다.

## I. Commit

| Item | Value |
|---|---|
| Existing HEAD / intended parent | `46d0ea182d77af03b8b75ab6e5480bf5d591624c` |
| New Adaptive PoC commit | None |
| Commit title / sign-off | Not applicable; no commit made |
| Backup ref | `backup/reveal-before-adaptive-poc-20260915` |
| Index / working tree | Clean, unchanged |
| Push / rewrite / amend / squash / stash / reset | None |

## J. Revert

Adaptive commit이 없으므로 revert할 대상도 없다. 기존 Source batching commit은 그대로다. `git revert 46d0ea18`을 실행하면 안 된다 — 그것은 이번 PoC가 아니라 기존 Source batching을 되돌리는 작업이다.

## K. Final Verdict

**PARTIAL ELIGIBILITY — AWAIT SCOPE CONFIRMATION BEFORE INTEGRATION.**

**POC COMMIT NOT CREATED. PRODUCTION ADOPTION PENDING TARGET RESULT.**

이 보고서와 초기 상태 기록만 repository 밖에 저장했다. 추가 runtime 작업은 수행하지 않았다.
