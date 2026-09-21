# WHOLE_TEXT + Fade=1 퇴장 Blur: 다음 최적화 후보 분석

2026-09-15 · baseline `46d0ea182d77af03b8b75ab6e5480bf5d591624c` (`Batch text reveal blur sources`)

## 결론 요약

- WHOLE_TEXT + explicit Fade=1은 CHARACTER/WORD/LINE/PIXEL 모두 동일한 foreground opacity `a(p)=p`를 갖는다. reverse와 seek에서도 같다.
- 단일 foreground의 fade를 마지막으로 이동하는 것은 **실수 연산상 동등**하다. 그러나 현재 8-bit Source/H/V 저장까지 포함하면 byte-exact하지 않다. 독립 진단에서 A8/RGBA 모두 최대 **1/255**의 최종 RGB 차이를 확인했다. 겹치는 ImageSpan의 separate source-over draw에는 이보다 근본적인 비동등성도 있다.
- 따라서 **1+3으로 Source task/FBO를 제거하는 구조는 유망하지만, 이번의 엄격한 framebuffer equivalence 조건으로 즉시 Go라고 결론낼 수 없다.** 정적인 plain A8, 1:1 좌표/zero-border 조건까지 제한해야 한다.
- 가장 작은 다음 후보는 **4: PERFORMANCE Output의 exact-interval fetch 분기**다. 대표 캡처 12/12의 Output이 byte-identical했다. Output GPU 비용은 A8 약 9.7%, RGBA 약 6.1% 감소했지만, 전체 파이프라인의 큰 개선으로 확대 해석할 수는 없다.
- **5A: WHOLE_TEXT H-band**는 차순위다. 추가 bitmap scan 없이 ordinary raster bounds만으로 FHD H fragment 영역 10.34%를 제외할 수 있다. 다만 외부 geometry 진단에서 일부 픽셀에 1 LSB 차이가 남았다. 이 상태를 그대로 production으로 넣는 추천은 아니다.
- 화면을 거의 채운 이번 FHD 문단에서 **2: coverage FBO crop은 약 2.37%의 FBO payload 감소**에 그친다. 빈 공간이 많은 Label에는 더 유효하지만, 이번 타겟 문제의 최우선 해법은 아니다.
- **NO PRODUCTION CHANGE.** repository 밖 진단 앱과 보고서만 작성했다. commit/staging/build 설정/production lifecycle은 변경하지 않았다.

## A. Problem Workload

타겟 관찰: Source batching 적용 후 PER_LINE은 개선되었지만, WHOLE_TEXT에서 전체 foreground가 동시에 blur/fade되며 사라지는 구간에 큰 frame drop이 남는다. 타겟 frame time을 이 호스트 분석에서 직접 측정한 것은 아니다.

| 항목 | 이번 primary fixture |
|---|---|
| Label / window | 1920×1080 / 1920×1080 |
| 실제 text raster | 1920×1054, 31줄, 한글 중심의 자연스러운 문단 |
| Font | 24px, 기존 로컬 기본 font/fallback 구성 유지 |
| Unit / Sequence | PIXEL / WHOLE_TEXT; 다른 Unit의 uniform성은 코드 감사 |
| Fade / Stagger | explicit 1 / 0 |
| Radius | 48px — 사용자 확인값 |
| BlurDurationRatio | 1.0 — 현재 샘플 퇴장 preset 기준. 사용자는 radius만 별도로 확인 |
| Animation | 1초 Linear, progress 1→0, 6회 loop |
| Scale | UI scale=1, render scale=1 |
| A8 | plain white foreground |
| RGBA | ForegroundColorSpan + linear gradient, RGBA foreground + L8 mask + LUT |
| 호스트 | GTX 1650 / NVIDIA 595.91.07 / GLES / X11 / window MSAA=4 |

기존 corpus 파일을 재사용했다. 대표 fixture에는 ImageSpan/decoration을 넣지 않았다. 이들의 correctness는 코드 경로를 별도로 분류했다. 목표 workload를 바꾸지 않기 위해 품질 모드·radius·kernel·curve·quarter 비율은 그대로 유지했다.

### 측정 방법과 한계

1. 기존 native foundation build를 사용했다. CMake build type/최적화 옵션은 기존 상태 그대로이며, 새 full build는 하지 않았다. 설치된 다른 foundation 대신 repository build library를 `LD_LIBRARY_PATH`로 지정했다.
2. publication이 생성되고 0.8초 warm-up한 뒤 측정을 시작했다. setup/첫 생성 비용 측정이 아니다.
3. GPU: draw 앞뒤의 `EXT_disjoint_timer_query`로 Source/H/V/Output을 구분했다. 실제 reverse 6초 구간을 독립 process 2회씩 측정했다. 각 stage의 누적 query 시간을 관측 window frame 수로 나눈 ms/frame이다.
4. CPU: GPU query preload 없이 별도 process에서 `CLOCK_PROCESS_CPUTIME_ID` 증가량 / 경과 시간. 같은 reverse를 2회씩 측정했다. process 전체 비용이며 UI event thread만의 비용이 아니다.
5. static checkpoint 1/.8/.6/.4/.2/0도 각 1회, 1.5초씩 GPU 측정했다. endpoint 한 번의 outlier를 개선 근거로 사용하지 않았다.
6. GPU query는 **draw execution 비용**이다. FBO clear/store, 전체 submission/present, VSync 대기, driver allocation 비용을 모두 포함한 frame time이 아니다. 이 수치를 FPS나 TV frame time으로 환산하지 않는다.
7. 독립 후보 진단은 외부 앱에서 renderer shader 또는 H geometry만 교체했다. production code, task refresh policy, resource ownership은 수정하지 않았다. 후보를 동시에 켜지 않았다.
8. timing 유효 기록에서 dropped/disjoint query는 모두 0이었다. 소수 반복으로 CPU/전체 GPU의 작은 차이를 통계적으로 확정하지 않는다.

원자료: metrics.json (로컬 자료: `metrics.json`), baseline logs (로컬 자료: `baseline`), diagnostic GPU logs (로컬 자료: `diagnostic-gpu`), capture 비교 (로컬 자료: `capture-analysis-final.txt`). 기존 성능 보고서의 다른 matrix를 다시 수행하지 않았다.

## B. Current WHOLE_TEXT Pipeline

```text
ordinary foreground + metadata (+ mask/LUT)
          │ 현재 progress로 foreground fade
          ▼
Source FBO ──► H FBO ──► V FBO ──► foreground Output ──► window
     └───────────────────────────────▲
                         PERFORMANCE sharp handoff만 사용
```

WHOLE_TEXT는 Source batching 대상이 아닌 기존 simple path다. 모든 Source/H/V task가 REFRESH_ALWAYS이고, transparent clear를 사용한다. decoration이 없는 이 fixture는 offscreen task 3개 + 공유 window task 1개, offscreen FBO 3개, frame당 draw 4개다. Output 전용 FBO는 없다.

48px kernel의 halo는 각 변 `radius+2=50px`이므로 full target은 **2020×1180**이다. target 계산 기준은 actual nonzero glyph coverage가 아니라 control 크기다.

| Stage | PERFORMANCE | HIGH | draw / texture fetch |
|---|---:|---:|---|
| Source FBO | 2020×1180 | 2020×1180 | 1 draw; A8: foreground+metadata 2 fetch, 이번 RGBA: foreground+mask+LUT+metadata 4 fetch |
| H FBO / full quad | 505×1180 | 2020×1180 | 1 draw; active kernel에서는 24 paired samples = 48 texture instructions/fragment |
| V FBO / full quad | 505×295 | 2020×1180 | 1 draw; 동일 48 texture instructions/fragment |
| Output quad | 2020×1180 | 2020×1180 | 1 draw; PERFORMANCE V+Source 2 fetch, HIGH V 1 fetch |

Source의 clear target은 2020×1180이지만 **ordinary foreground의 draw quad는 1920×1054 raster에 대응**한다. FBO 전체 면적을 Source draw fragment 수라고 부르면 안 된다. Output은 halo를 포함한 quad지만 이 fixture에서는 window에서 잘려 실제 화면 영역이 1920×1080이다.

strength=0이면 H/V는 각각 1 fetch copy, progress=0이면 shader가 zero를 반환한다. 중간 strength에서는 radius가 작아져도 kernel loop의 sample 개수는 감소하지 않는다.

### 실제 reverse 평균 draw GPU 비용

단위 ms/frame, 각 6초×2회 평균.

| Quality / format | Source | H | V | Output | 합계 | Source 비중 | H+V 비중 |
|---|---:|---:|---:|---:|---:|---:|---:|
| PERFORMANCE A8 | 0.1384 | 0.4320 | 0.1182 | 0.1238 | **0.8124** | 17.0% | 67.7% |
| PERFORMANCE RGBA | 0.2425 | 0.5324 | 0.1506 | 0.1409 | **1.0663** | 22.7% | 64.1% |
| HIGH A8 | 0.1148 | 1.5429 | 1.4703 | 0.0977 | **3.2257** | 3.6% | 93.4% |
| HIGH RGBA | 0.1861 | 1.5066 | 1.4888 | 0.1080 | **3.2895** | 5.7% | 91.1% |

GPU 클럭/부하가 완전히 고정된 시험은 아니다. 같은 Source 코드라도 quality별로 GPU 부하가 달라 stage 시간이 같지 않다. 이 표에서 Source 자체가 PERFORMANCE에서 더 복잡해졌다는 결론을 내리면 안 된다.

### CPU와 texture payload

| Quality / format | CPU ms/s: run 1 / run 2 | 평균 | Source/H/V payload | 연결된 unique texture payload 전체 |
|---|---:|---:|---:|---:|
| PERFORMANCE A8 | 219.5 / 196.1 | 207.8 | 2.984 MiB | 12.633 MiB |
| PERFORMANCE RGBA | 155.9 / 201.6 | 178.8 | 11.934 MiB | 29.305 MiB |
| HIGH A8 | 323.4 / 325.1 | 324.2 | 6.820 MiB | 16.469 MiB |
| HIGH RGBA | 323.3 / 345.6 | 334.4 | 27.278 MiB | 44.649 MiB |

payload는 texture width×height×format bytes를 handle 중복 없이 합산한 **논리 저장량**이다. 실제 driver VRAM/RSS, allocation alignment, window MSAA buffer, cache, 과거 publication의 순간 중첩, CPU bitmap을 포함하지 않는다. A8는 이 GLES3 경로에서 R8_UNORM으로 저장된다. GLES2/Vulkan에는 같은 A8 수치를 적용하지 않는다.

A8 전체에는 ordinary foreground 2,023,680B와 metadata 8,094,720B가 포함된다. RGBA 전체에는 ordinary RGBA foreground, L8 mask, 512×1 RGB LUT, metadata가 포함된다. 따라서 blur FBO의 절감률과 Label 전체 texture 절감률은 다르다.

### Progress별 실측

단위 ms/frame. `S/H/V/O` 순서다. 모든 checkpoint에서 실제 progress를 고정하고 별도 process로 측정했다.

| p | PERFORMANCE A8: S/H/V/O | 합계 | PERFORMANCE RGBA: S/H/V/O | 합계 |
|---:|---|---:|---|---:|
| 1.0 | .1124 / .0379 / .0124 / .1163 | .2790 | .1934 / .0577 / .0233 / .1195 | .3939 |
| .8 | .1177 / .3952 / .1075 / .1119 | .7324 | .1869 / .4057 / .1160 / .1203 | .8289 |
| .6 | .1192 / .4104 / .1107 / .1056 | .7459 | .1860 / .3876 / .1142 / .1140 | .8018 |
| .4 | .1142 / .4107 / .1097 / .1047 | .7393 | .1926 / .4084 / .1156 / .1164 | .8329 |
| .2 | .1127 / .3790 / .1055 / .1043 | .7015 | .1927 / .4188 / .1146 / .1125 | .8387 |
| 0 | .1009 / .0156 / .0065 / .0904 | .2134 | .1709 / .0205 / .0106 / .0784 | .2804 |

| p | HIGH A8: S/H/V/O | 합계 | HIGH RGBA: S/H/V/O | 합계 |
|---:|---|---:|---|---:|
| 1.0 | .1148 / .1089 / .1095 / .1055 | .4387 | .1882 / .1229 / .1362 / .1126 | .5599 |
| .8 | .1110 / 1.4498 / 1.4355 / .1028 | 3.0991 | .1858 / 1.5735 / 1.4658 / .1210 | 3.3461 |
| .6 | .1116 / 1.4656 / 1.4536 / .0960 | 3.1268 | .1832 / 1.4606 / 1.4547 / .1061 | 3.2045 |
| .4 | .1159 / 1.5559 / 1.4499 / .0958 | 3.2174 | .1842 / 1.4746 / 1.4862 / .1067 | 3.2518 |
| .2 | .1150 / 1.4612 / 1.4540 / .0951 | 3.1253 | .1855 / 1.4864 / 1.4962 / .1031 | 3.2713 |
| 0 | .3117 / .0659 / .0648 / .0946 | .5371 | .1714 / .0640 / .0633 / .0828 | .3816 |

HIGH A8 p=0 Source 값은 단일 관측 outlier다. endpoint의 비용이 실제로 더 높다는 주장에는 사용하지 않는다. 중요한 것은 **중간 progress 전반에서 H/V의 전체 tap loop가 지속**된다는 점이다. 거의 투명해진 p=.2에서도 blur 비용이 자동으로 줄지 않는다.

## C. Fade=1 Mathematical Contract

### Unit별 uniform opacity

| Unit | WHOLE_TEXT + explicit Fade=1의 실제 schedule |
|---|---|
| CHARACTER / WORD | `PopulateSchedule`: start interval `(1−fade)/(N−1)=0`, 모든 start=0, fade duration=1 |
| LINE | final visible line unit 생성 후 같은 `PopulateSchedule`, 따라서 동일 |
| PIXEL | unit start와 progressionSpan 모두 `(1−fade)`가 곱해져 0. WHOLE_TEXT totalDuration=1, fade duration=1 |

visible foreground를 포함하는 정상 plan에서는 sequenceDuration=1, reveal end=1, 마지막 sequence start=0이다. blur 비율을 B(0<B≤1)라고 하면 `PrepareRevealBlur`의 전체 종료점은 `max(1, 0+B)=1`이므로 blur 정규화 때문에 이 uniform fade가 달라지지 않는다. B=0은 runtime blur가 없는 경로다.

metadata의 start=0은 16bit에도 정확히 표현된다. 현재 shader는 valid ownership에 대해 `clamp((p−0)/1)=p`를 적용한다. p는 기존 `ResolveRenderProgress`의 clamp와 완료 epsilon 보정을 거친 값이다. metadata가 없거나 잘못된 픽셀을 이 증명의 대상으로 삼지 않는다.

sync와 async가 같은 `PrepareRevealBlur`를 사용한다. 기존 `TextVisual::SetTextReveal()`의 async pending fallback에도 full fade + simultaneous starts에서는 glyph metadata 없이 `opacity==progress`로 이전 foreground를 표시하는 분기가 이미 있다. **그것은 pending fallback이며 regular Source static화가 이미 구현되어 있다는 뜻은 아니다.**

근거: schedule (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal.cpp:136`), LINE (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal.cpp:687`), PIXEL (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal.cpp:1284`), 공통 정규화 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/reveal/text-reveal-blur-preparation.cpp:475`), async worker (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/async-text/async-text-loader-impl.cpp:1831`), 기존 pending fallback (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:2187`).

### Reverse / seek / alpha function

현재 blur는 별도 wall clock이 아니라 동일한 progress를 입력으로 받는다.

```text
a(p) = p
q(p) = clamp(p / B, 0, 1)
s(p) = 1 − q²(3 − 2q)          // Gaussian offset strength
effectiveRadius = 48 × s(p)   // Late Smooth 판정용
t = (effectiveRadius − 2) / 6
w = smootherstep(clamp(t, 0, 1))
```

PERFORMANCE output은 `(1−w)×sharp + w×blurred`다. Linear reverse에서 `p=1−elapsed/T`; 다른 alpha function에서는 애니메이션이 제공하는 `p=1−A(elapsed/T)`를 그대로 쓴다. direct seek도 같은 함수다. 따라서 scalar relocation의 수학은 forward/reverse/seek와 무관하게 각 p에서 성립해야 한다. 실제 elapsed-time 구간 비율만 alpha function에 따라 달라진다.

| p (reverse 순서) | fade a | blur strength s | effective radius | Output |
|---:|---:|---:|---:|---|
| 1 | 1 | 0 | 0 | sharp-only |
| .8 | .8 | .104 | 4.992 | handoff |
| .6 | .6 | .352 | 16.896 | blur-only |
| .4 | .4 | .648 | 31.104 | blur-only |
| .2 | .2 | .896 | 43.008 | blur-only |
| 0 | 0 | 1 | 48 | 완전히 hidden; H/V zero 반환 |

### Premultiplied linearity와 한계

고정 p에서 선형 Gaussian 연산을 `G_s`, full-opacity premultiplied foreground를 S라고 두면:

```text
G_s(aS) = a G_s(S)
(1−w) aS + w G_s(aS) = a [(1−w)S + w G_s(S)]
```

RGB와 alpha를 함께 곱하므로 premultiplied A8 coverage 복원과 RGBA 모두 실수 연산상 성립한다. 단, S 자체를 만드는 과정이 p-independent여야 한다.

separate draw A over B의 경우:

```text
(aA) over (aB) = aA + aB − a² αA B
a (A over B)   = aA + aB − a  αA B
차이           = a(1−a) αA B
```

따라서 ImageSpan/text 또는 여러 이미지의 filtered support가 겹치면 global fade relocation은 원래 합성과 다르다. opacity가 같은 unit이라는 사실만으로 해결되지 않는다.

마지막으로 현재 저장은 R8/RGBA8이다. 양자화 Q를 포함하면:

```text
Qv(Gv(Qh(Gh(Qs(aS)))))  ≠  a · Qv(Gv(Qh(Gh(Qs(S)))))
```

가 일반적이다. 8bit 변환/보간/최종 framebuffer rounding 순서가 달라진다. 이 한계는 실수상의 Gaussian 선형성과 모순되지 않는다. [OpenGL ES 2.0 사양의 fixed-point 변환·blending·filtering 정의](https://registry.khronos.org/OpenGL/specs/es/2.0/es_full_spec_2.0.pdf)에 따른 구분이다.

## D. Candidate 1 — Fade relocation / static Source

### 적용 범위 분류

아래의 '동등'은 **양자화 전의 수학적 동등성**이다. actual byte parity는 별도다.

| Source 구성 | 수학적 판단 | production 적용 조건 |
|---|---|---|
| plain text one foreground draw | 동등 | static source 내용, valid metadata, color alpha 포함 검토 |
| colored text | 동등 | 이미 하나의 texture/shader 결과로 합성된 뒤 fade 적용 |
| emoji / multicolor / mask | 동등 | 현재 foreground shader 내부 합성 후 동일 scalar 적용하는 지원 경로 |
| gradient / mixed gradient / overlay | 동등 | LUT/좌표/색이 S 안에서 처리되고 마지막 fade 이전에 완료되는 지원 경로 |
| ImageSpan separate draw | 조건부 | 모든 filtered support가 서로 disjoint라는 보장이 필요 |
| ImageSpan/text 또는 images overlap | 비동등 | 현재 source-over 순서를 보존해야 함; 첫 fast path에서 제외 권장 |
| decorations | foreground에만 적용하면 가능 | Label 전체 opacity 또는 decorated composite 전체에 곱하면 계약 위반 |
| emboss 등 현재 blur unsupported 경로 | 대상 제외 | 이 작업으로 fallback/지원 범위를 확장하지 않음 |

현재 foreground Output과 decoration composition은 분리되어 있다. 따라서 blur foreground Output에만 fade를 곱하는 위치는 존재한다. shadow/outline/underline/strikethrough/background까지 같이 fade하는 방식은 아니다.

### 실제 픽셀 진단

외부 앱에서 Source의 `ResolveTextRevealOpacity` 곱만 제거하고 Output에 동일 progress scalar를 곱했다. Source는 계속 REFRESH_ALWAYS로 유지해 **relocation 자체만** 검사했다. Shader, strength, H/V resolution, animation timing은 원래와 같다.

550×300의 6줄 문단, A8 / RGBA, p=1/.8/.6/.4/.2/0:

| format | p=1 / p=0 | 중간 progress 최종 RGB max error | 변경 pixel 수 범위 |
|---|---|---:|---:|
| A8 | 동일 | 1 LSB (=1/255) | 5,192–11,262 |
| RGBA | 동일 | 1 LSB (=1/255) | 14,244–27,483 |

검정 window에 합성된 최종 RGB 기준이다. 투명 output alpha 전체의 동등성이나 모든 backend에 대한 검증은 아니다. 매우 작은 수치 차이지만 **'모든 결과가 정확히 같다'는 주장은 반증**된다. 이 diagnostic은 static Source lifecycle 구현/검증이 아니다.

### REFRESH_ONCE 및 invalidation

DALi는 `RenderTask::REFRESH_ONCE`를 지원한다. 다시 SetRefreshRate(REFRESH_ONCE)하면 one-shot 렌더 요청 상태로 진입한다. 자동으로 source의 모든 dirty property를 추적해 재실행하는 캐시는 아니다. core 구현 (로컬 자료: `../dali/dali-core/dali/internal/update/render-tasks/scene-graph-render-task.cpp:220`)

| 변경 | 현재 publication 재생성만으로 충분한가? |
|---|---|
| text / font / StyledText / layout / UI·render scale / 새 async 결과 | 기존 새 publication 경로를 유지하면 source 재생성 기회가 있음 |
| scene reconnect | 새 task의 첫 snapshot 및 H/V 소비 순서를 보장해야 함 |
| progress | relocation이 성립하는 범위에서는 Source dirty일 필요 없음 |
| animatable text color alpha / live foreground property | **새 text publication만으로 충분하지 않음**. 현재 renderer constraint가 update-side에서 변경 가능 |
| gradient property / LUT 교체 | setter와 live uniform 갱신을 모두 감사해야 함 |
| ImageSpan ready / replacement / proxy renderer 교체 | 기존 `RefreshImages`는 renderer를 바꾸지만 one-shot Source를 자동 재요청하지 않음 |

예: uTextColorAnimatable constraint (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-visual.cpp:433`), foreground property mirroring (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:147`), ImageSpan refresh (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:807`).

즉, 단순히 REFRESH_ALWAYS 한 줄을 REFRESH_ONCE로 바꾸는 것은 안전하지 않다. 모든 관련 live property를 연결하거나 eligibility를 제한해야 한다. app callback/메인 thread 대기는 필요하지 않지만, 내부 snapshot publication 수명과 첫 render 의존성 검증은 필요하다.

### 이득

성립할 경우 active frame마다 Source draw/clear/write를 제외한다. 최초 source 생성은 남는다. host draw-time 기준 제거 가능한 비중은 PERFORMANCE A8 약 17%, RGBA 약 23%, HIGH 약 4–6%다. output scalar 비용은 추가된다. source clear/store의 추가 절감은 이 query가 따로 측정하지 않았으므로 수치화하지 않는다.

Source FBO를 유지하므로 **texture memory 절감은 0**이다. CPU draw submission은 줄 수 있지만 dirty 관리가 추가되며 이번에 CPU 개선을 측정하지 않았다.

판정: 수학적으로 유효한 제한적 후보. **엄격한 byte-equivalence / 전체 feature static화에는 현재 No-Go.** 시각적으로 허용 가능한 1 LSB 기준으로 요구를 바꾸었다고 임의로 간주하지 않는다.

## E. Candidate 2 — Content coverage crop

### 현재 bounds와 재사용 가능한 정보

WHOLE_TEXT의 Source/H/V는 `ceil(control)+2(radius+2)` full bounds를 사용한다. PER_LINE은 `ResolveRuntimeRevealBlurTarget`로 coverage를 transform한 뒤 halo를 확장한다. 현재 분기 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1084`)

WHOLE_TEXT의 PreparedRevealBlur에는 현재 aggregate text coverage rectangle이 없다. `lines`는 비어 있다. 그러므로 '이미 준비된 line coverage를 꺼내기만 하면 된다'는 설명은 정확하지 않다.

사용 가능한 방향:

- 가장 단순한 보수적 bounds: ordinary raster의 전체 사각형 + 기존 foreground transform. 추가 scan이 없다.
- 더 tight한 aggregate: 기존 metadata ownership 생성 pass의 row extrema를 누적하여 전달. **이미 진행 중인 pass에 min/max를 합치는 방법**이며, 새 bitmap scan/readback을 추가할 필요는 없다. 현재 그 aggregate를 결과로 저장하고 있지는 않으므로 작은 payload 전달 수정은 필요하다.
- ImageSpan은 이미 preparation에서 보관하는 visible placement coverage를 union. remote image 도착 여부가 아니라 예약된 placement 전체를 포함한다. image 픽셀 readback은 필요 없다.

measurement에서 glReadPixels로 ROI를 읽은 것은 분석용이다. 이를 production coverage 계산법으로 제안하는 것이 아니다. 실제 최적화 bounds는 현재 글자색/alpha가 0이라는 이유로 미래에 보일 glyph를 제외해서도 안 된다.

### Correctness 조건

- glyph metrics나 line advance 대신 actual raster/ownership 범위: negative bearing, overhang, 압축된 line spacing, bidi를 포함한다.
- `ResolveRuntimeRevealBlurTarget`의 displayed-extent 변환과 radius+4 guard를 재사용한다. UI scale와 async render scale를 혼동하지 않는다.
- source target만 옮기고 원래 text-local 좌표계는 유지한다. USER_SPACE / OBJECT_BOUNDING_BOX gradient를 새 crop 크기로 재정의하지 않는다.
- padding, fitting offset/pivot, clipping, output 위치, 기존 visible halo를 모두 유지한다. crop으로 unsupported tiling이나 admission fallback을 새롭게 우회하지 않는다.
- **quarter sampling grid를 보존해야 한다.** 예를 들어 높이 450의 V는 113 texel이므로 간격은 450/113이다. 이를 300/75로 단순 변경하면 간격이 4가 되어 보간 결과가 바뀐다. 정수 crop만으로는 충분하지 않다. 기존 sampling phase/spacing 유지 또는 안전한 fallback이 필요하다.

### 실제 ROI

p=1 Source의 nonzero coverage를 측정한 후 radius+4, 바깥 방향 4px 정렬, 기존 target clamp를 적용한 **면적 추정**이다.

| Label / corpus | 기존 Source | nonzero Source bbox 크기 | 후보 Source | Source/H/V payload 감소 |
|---|---:|---:|---:|---:|
| FHD, 31줄 | 2020×1180 | 1916×1044 | 2020×1152 | **2.37%** |
| 500×350, 6줄 | 600×450 | 487×194 | 592×300 | **34.24%** — grid 문제 해결 전 잠재치 |
| 500×500, 14줄 | 600×600 | 494×466 | 600×572 | **4.67%** |

FHD는 A8 약 **0.071 MiB**, RGBA 약 **0.283 MiB**의 FBO payload 감소에 그친다. ordinary foreground/metadata는 이 후보만으로 줄지 않는다. source glyph draw 자체도 거의 그대로이며 clear target만 작아질 수 있다. 이번 FHD crop의 Output quad는 여전히 window 전체를 덮으므로 Output fragment 감소는 0이다.

FHD H+V 면적 2.37%를 비용에 단순 비례시키면 전체 draw GPU의 약 1.5–1.6%에 해당한다. 실제 측정 개선값은 아니다. sparse Label에서의 가치는 있지만 이번 primary workload에서는 우선순위가 낮다.

## F. Candidate 3 — Plain A8 direct source

### 제거하려는 pass가 실제로 하는 일

현재 Source는 ordinary L8를 단순 참조하는 것 이상을 한다: foreground shader의 color alpha/fade, renderer transform, pixel sampling, halo 안의 배치, R8 quantization을 결과 texture로 확정한다.

plain A8, static content, Fade=1 relocation이 성립한다고 가정하면 다음 구조는 가능하다.

```text
ordinary L8 ──► H ──► V ──► Output × global fade
       └───────────────────▲
                 PERFORMANCE sharp-only/handoff
```

HIGH도 H가 ordinary source를 직접 읽도록 바꿀 수 있으며, HIGH Output은 V만 읽는다.

### 단순 texture 교체로는 불가능한 이유

| 항목 | ordinary L8 | Source FBO |
|---|---|---|
| 이번 크기 | 1920×1054 | 2020×1180 |
| 좌표 | text raster local | halo 포함 display/capture local |
| Color alpha | ordinary coverage에는 미적용일 수 있음 | capture shader에서 반영 |
| Fade | 일반 metadata shader에서 계산 | 이미 반영됨 |
| 영역 밖 | sampler CLAMP_TO_EDGE | 실제 transparent halo texel |
| Fractional transform | 원래 texture | display grid에 한 번 raster/resample된 결과 |

H와 PERFORMANCE sharp output 모두 source rect/origin/scale 매핑이 필요하다. Gaussian offset의 기준은 계속 display/full target 공간이어야 하며 L8 texture 크기로 무작정 바꾸면 radius가 달라진다.

**UV가 [0,1] 밖이면 0이라는 단순 분기만으로도 부족할 수 있다.** 경계에 걸친 bilinear footprint에는 inside texel과 outside zero의 가중합이 필요하다. edge clamp는 마지막 texel을 halo로 연장한다. half-texel support를 포함한 zero-border reconstruction이 필요하다. 현재 sampler는 LINEAR + CLAMP_TO_EDGE (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:281`)다.

또한 fractional UI/render scale 또는 translation에서 기존 경로는 `ordinary → Source grid → H`의 두 보간을 거친다. direct는 이를 하나로 줄이므로 같은 함수가 아니다. 초기 후보는 **1:1 texel-center 정렬**, 정상 opacity/color, no gradient/mask/emoji/ImageSpan으로 제한해야 한다. 이것도 Candidate 1의 quantization 차이를 자동으로 해결하지 않는다.

### Lifetime / ownership

현재 original foreground Renderer는 Label에서 companion으로 borrow되어 숨겨진 capture actor에 연결된다. `HasCurrentForeground`는 borrowed renderer의 shader/texture identity를 검사하며, None/교체/해제 시 원래 renderer를 복구한다.

Source task를 없애더라도 ordinary texture와 progress/color property source의 수명은 계속 유지해야 한다. 원본 renderer를 Label에 그대로 다시 붙이면 double draw가 된다. 반대로 renderer/actor를 무작정 제거하면 constraint input/ownership 검증을 잃을 수 있다. borrow/identity 경로 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:772`)

따라서 이는 sampler 변경만의 작업이 아니다. Source 없는 pass의 생성/연결/해제 분기, renderer 보관·constraint 연결을 제한적으로 정리해야 한다. 새 app callback이나 ImageVisual/adaptor 변경은 필요하지 않지만, async old/new publication, reconnect, None/destroy 검증이 선행되어야 한다.

### 최대 구조 이득 — plain A8에만 해당

| 항목 | 현재 PERFORMANCE | direct PERFORMANCE | 현재 HIGH | direct HIGH |
|---|---:|---:|---:|---:|
| Offscreen task | 3 | 2 | 3 | 2 |
| FBO | 3 | 2 | 3 | 2 |
| Draw/frame, Output 포함 | 4 | 3 | 4 | 3 |
| Blur FBO payload | 2.984 MiB | 0.710 MiB | 6.820 MiB | 4.546 MiB |
| 전체 texture payload | 12.633 MiB | 10.360 MiB | 16.469 MiB | 14.196 MiB |

양쪽 모두 Source **2.273 MiB/Label** 제거다. PERFORMANCE blur FBO만 보면 76.2%, 전체 texture는 18.0% 감소다. HIGH 전체 texture는 13.8% 감소다. existing ordinary metadata를 추가로 제거하는 최적화는 계산에 넣지 않았다.

GPU의 이상적 Source 제거 몫은 Candidate 1과 같다. direct zero-border/좌표 변환은 **H의 매 tap**에 비용을 추가할 수 있으므로 실현 가능한 순이득은 그보다 작을 수 있다. CPU bitmap 메모리는 이 GPU Source FBO 제거만으로 자동 감소하지 않는다. 실제 driver allocation 절감은 타겟 확인이 필요하다.

판정: 구조적으로 가능하지만, 지금 상태에서 **bit-exact fast path가 증명된 것은 아니다.** 모든 A8/scale로 일반화하거나 바로 구현하지 않는다.

## G. Candidate 4 — Output dual-fetch short-circuit

현재 PERFORMANCE shader는 Source와 V를 먼저 모두 sample한 뒤 `t<=0` 또는 mix로 결과를 고른다. compiler가 어떤 최적화를 하는지는 별개지만 source-level에서는 blur-only 구간도 명시적 fetch 회피가 없다. Output shader (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:506`)

안전한 후보는 shader 하나에서 기존 uniform을 그대로 사용하는 다음 분기다.

```text
effectiveRadius <= 2 : Source만 fetch
effectiveRadius >= 8 : V만 fetch
그 사이             : Source와 V fetch, 기존 SmootherStep 그대로 mix
```

radius=48, B=1, Linear reverse에서:

| 구간 | progress 범위 | 1초 reverse 중 비중 | fetch/fragment |
|---|---|---:|---:|
| sharp-only | p≥.87699784 | 시작 12.30% | 2→1 |
| handoff | .74085099<p<.87699784 | 13.61% | 2 유지 |
| blur-only | p≤.74085099 | 나머지 74.09% | 2→1 |

p=0은 이미 hidden endpoint다. 연속 active 시간의 **86.39%에서 1 fetch를 줄일 여지**, 평균 2→1.1361 fetch/fragment다. A8/RGBA 모두 Output에 LUT/mask fetch는 없으므로 동일하다. 평균 Output fetch instruction 수의 이론적 감소는 43.19%지만 이것이 Output GPU time 43.19% 감소를 뜻하지 않는다.

B가 .5라면 두 경계 p는 각각 절반, handoff 시간은 6.81%, sharp-only는 56.15%다. Fade=1만 보고 B=1이라고 가정해서는 안 된다. alpha function이 달라지면 시간 비중은 달라지되 p 구간은 같다.

### 구현 방식 비교

| 방식 | 평가 |
|---|---|
| Fragment uniform branch | **우선 추천**. WHOLE_TEXT의 모든 fragment가 같은 strength를 사용. 기존 uniform 재사용, 추가 task/actor/CPU animation 없음 |
| Vertex mode + varying | WHOLE_TEXT에는 이득이 불분명. GLES2의 flat qualifier 제약과 불필요한 varying 추가를 피할 수 있음 |
| Shader variant/renderer switching | 전환 state/resource churn, async 연결 검증 증가. 이득 대비 불필요 |

현재 FBO sampler는 mipmap이 아닌 LINEAR다. uniform 조건의 분기를 추가하기 위해 sampler array 동적 indexing이나 derivative/LOD 계산을 도입할 필요가 없다. GLES2에서는 mipmapped texture의 non-uniform conditional access 제한도 있으므로 이 단순 non-mipmapped 구조를 유지하는 편이 적절하다. [GLSL ES 1.00 사양](https://registry.khronos.org/OpenGL/specs/es/2.0/GLSL_ES_Specification_1.00.pdf)

Vulkan에는 동일 의미의 uniform branch를 사용할 수 있으나 이번 native 진단은 GLES만 수행했다. GPU별 compiler가 branch를 flatten할 수 있으므로 타겟에서 실제 속도 이득은 따로 확인해야 한다.

### 독립 진단 결과

- A8/RGBA × 6 checkpoint의 **Output 12/12 byte-identical**. 저장된 upstream pass 비교 33/33도 동일했다. RGBA p=.2의 Source/H/V 3개 capture 파일은 없으므로 36/36이라고 세지 않았다. 해당 Output 비교는 존재하며 동일했다.
- 실제 1→0 reverse, 각 format 2회, baseline/candidate 순서를 교차한 결과:

| format | Output GPU baseline → branch | 변화 | 파이프라인 전체 관측 |
|---|---:|---:|---|
| A8 | .1239 → .1119 ms | **−9.7%** | .7958 → .8000 ms, neutral/noise |
| RGBA | .1329 → .1249 ms | **−6.1%** | .934 → .916 ms, 작은 감소이나 2회뿐 |

변경하지 않은 Source/H/V도 run별로 흔들렸다. 따라서 확실한 표현은 **Output stage 감소가 관측되었다**이고, 전체 1–2% 향상을 보장한다는 표현은 아니다. 기존 전체 비용에 stage 절감만 대입하면 약 0.008–0.012ms/frame, 대략 0.8–1.5% 규모다.

전체 비용에 대한 매우 느슨한 상한은 Output stage 전체인 약 13–15%다. fetch 비율만 비용에 비례시키는 낙관적 계산도 전체 약 5.7–6.6% 수준이다. 실제 진단 결과는 훨씬 작다.

Memory/task/renderer count는 변하지 않는다. Source/H/V lifecycle도 바꾸지 않아 5개 후보 중 가장 작은 production 변경으로 이어질 수 있다.

## H. Candidate 5 — H-band / dynamic support

### 5A. Static WHOLE_TEXT H-band

현재 D2는 PER_LINE batch에 H geometry를 따로 선택한다. WHOLE_TEXT는 H/V 모두 full quad다. H는 X 방향만 convolution하므로 source의 Y support 밖에서는 48번의 fetch 결과가 모두 0이다.

처음부터 tight coverage를 새로 만들 필요는 없다. **ordinary raster 전체 높이 + 현재 transform + 기존 D2 filter guard**를 쓰면 no-image text-only 범위에서 보수적인 H-band를 얻는다. 색상/gradient/emoji가 raster 안에서 어떻게 바뀌어도 full raster rectangle은 포함한다.

1:1에서 guard는 각 변 2줄이다. FHD ordinary raster가 y=50에서 시작해 높이1054이므로 H band는 `[48,1106)`, 높이1058이다. shader/kernel/radius/clear/FBO/카메라 크기/Output은 바꾸지 않는다. source와 같은 UV 공간을 유지하며, 단순히 작은 quad를 새 UV 0→1로 늘려 그리면 안 된다.

| fixture | 기존 H pixels | full raster 기반 H-band | H 영역 감소 |
|---|---:|---:|---:|
| FHD | 595,900 | 534,290 | **10.34%** |
| 500×350 | 67,500 | 31,200 | 53.78% |
| 500×500 | 90,000 | 72,000 | 20.00% |

FHD tight glyph coverage까지 쓰면 H=529,240 pixels, 11.19% 감소다. full raster만 쓰는 방법과 차이가 **0.85%p의 H 면적**뿐이므로, 이 fixture에서는 추가 coverage 전달을 강요할 이유가 약하다.

### 5A의 외부 geometry 진단

production shader 그대로 두고, 외부 앱에서 H renderer의 geometry만 만들었다. text-only plain/gradient fixture이며 raster 영역 밖을 제외했다.

- Source capture는 동일했다.
- 550×300 A8: Output 6 checkpoint 중 p=.6에서만 19 pixels가 1 LSB 다름.
- RGBA: p=.8/.6/.2에서 각각 39/15/18 pixels가 1 LSB 다름. 나머지 checkpoint는 동일.
- FBO의 투명 행을 제외하는 수학은 같지만, 다른 triangle에서 UV interpolation/저정밀 저장이 이뤄지는 수치 경로까지 완전히 같다고 보장할 수 없다. 이 차이가 없는 production 구현은 아직 증명하지 않았다.

실제 FHD reverse 진단의 H 비용:

| format | H baseline → H-band | 변화 | 전체 관측 |
|---|---:|---:|---|
| A8 | .4160 → .3933 ms | −5.46% | .7888 → .7696 ms, 약 −2.43% |
| RGBA | .5163 → .4991 ms | −3.34% | 1.0362 → 1.0418 ms, neutral/noise |

각 2회이며 GPU clock 고정 시험이 아니다. **10.34%의 H 면적 감소가 그대로 10.34%의 H time 감소로 이어지지는 않았다.** 이득은 제한적이며, strict parity 해결 없이 바로 production에 넣지 않는다.

Shader/GPU interpolation 정확도 검증 후에만 차순위 후보로 삼는다. 비교하면 fade relocation은 수천~수만 pixels에 1 LSB 차이, 이 H geometry 진단은 수십 pixels의 차이였지만, 두 경우 모두 byte-exact 통과로 기록하지 않았다.

FBO allocation/clear/store는 그대로라 GPU texture memory 감소는 없다. setup 시 작은 geometry 1개(진단 inventory geometry 2→3), 고정 4 vertices가 추가되며 CPU frame별 geometry 재생성은 필요하지 않다. ImageSpan은 placement union까지 증명하거나 우선 기존 full geometry로 남겨야 한다.

### 5B. Dynamic effective support

현재 shader는 kernel offset에 `strength`를 곱한다. 반경48의 NUM_SAMPLES=24에서 최대 paired offset K는 47 미만이다. 따라서 convolution support가 strength에 따라 줄어드는 것은 실제 코드에 근거한 사실이다. radius를 바꾸거나 tap 수를 줄이는 것이 아니다.

C를 source의 nonzero pixel-cell support, δ를 보간/rounding guard라고 하면 개념적인 포함 관계는 다음과 같다.

```text
H.x ⊇ C.x expanded by ±K·s, plus Source/H sampling guard
H.y ⊇ C.y plus Source filtering guard

V.x ⊇ H.x plus H texture's X interpolation support
V.y ⊇ C.y expanded by ±K·s, plus Source/H/V sampling guard
```

PERFORMANCE H texel의 X 간격은 약4 display pixels다. V의 X guard에 1 display pixel만 더하는 등 full-resolution 기준을 그대로 쓰면 안 된다. odd dimensions에서는 정확히4도 아니다. GPU가 실제 읽는 sample footprint와 outward rounding을 사용해야 한다.

구현한다면 immutable rectangle attributes + 기존 strength uniform으로 vertex에서 bounds를 확장해야 한다. 매 frame CPU vertex buffer 갱신/Geometry 재생성은 제외한다. `quad × strength`로 축소하면 text 본체의 위치/UV까지 변하므로 잘못이다. FBO는 유지하고 geometry/UV 매핑만 조절하며 매 frame transparent clear도 유지해야 seek/reverse에서 이전 halo가 남지 않는다.

FHD처럼 content가 거의 가득 찬 경우 줄일 수 있는 것은 주로 약50px halo와 작은 외곽 공백이다. 5A와 겹치는 부분이 크고, p가 내려가 blur가 커질수록 추가 절감은 줄어든다. 반대로 sparse content에서는 더 유효할 수 있다. 아직 5B를 구현/측정하지 않았으므로 숫자를 실측 개선으로 제시하지 않는다.

이 FHD의 단일 사각형 support 모델에서 halo/보간 guard를 아예 무시해도 content bbox는 1916×1044=2,000,304 pixels다. full target 2,383,600 pixels에 비해 H/V 각각의 영역 감소는 최대 약16.1%다. 이를 GPU 비용에 단순 비례시키는 매우 낙관적인 계산도 PERFORMANCE 전체 draw의 약10–11%이며, **5A의 몫을 이미 포함**한다. 실제 nonzero strength의 halo/guard를 포함하면 이보다 작다. 아래의 sharp-only 구간 전체 pass 작업 생략은 이 rectangle 모델과 별도다.

### strength=0 / pre-start collapse

- WHOLE_TEXT는 start=0이므로 정상 active 구간에서 별도의 pre-start 구간은 없다. p=0은 이미 hidden이다.
- **HIGH에서 strength=0이라고 H/V를 없애면 안 된다.** V가 sharp foreground의 공급자이며 Fade가 아직 진행 중일 수 있다. 현재 copy branch가 필요한 이유다.
- PERFORMANCE에서 Candidate 4가 정확히 sharp-only를 선택하는 구간에는 Output이 H/V를 읽지 않으므로 geometry를 collapse할 여지가 있다. task clear는 남고, source는 계속 유효해야 한다. 이것은 4와 결합된 별도 검증 대상으로 남긴다.
- B=1에서 정확한 strength=0은 p=1 endpoint뿐이다. radius48의 sharp-only 구간은 reverse 처음12.30%지만, 이때 full tap shader도 곧 실행되고 있음을 구별해야 한다.

이 단계에서는 새 dynamic-support logic을 구현하지 않았다. 5A보다 훨씬 많은 sampling/clear/partial-update 검증이 필요하므로 후순위다.

## I. Combined Candidates

| 조합 | 가능 여부 / 중복 |
|---|---|
| 1+3 | 제한적 plain A8 topology 가능. 1의 fade/quantization 조건과 3의 좌표·sampling·ownership 조건을 모두 통과해야 함. Source 절감은 한 번만 계산 |
| 1+4 | Output global fade와 기존 handoff 분기 공존 가능. 1의 비동등성을 4가 해결해 주지는 않음 |
| 3+4 | PERFORMANCE sharp fetch를 ordinary L8로 대체한 뒤 동일 구간 분기. direct source zero-border가 필요 |
| 2+5 | 가능. crop된 target 안에서 H의 Y 여백을 다시 줄이지만 영역이 겹치므로 절감률을 더하면 안 됨 |
| 4+5A | 서로 독립적인 Output/H 작업. FBO/task/lifecycle을 유지하는 작은 방향. 5A의 수치 parity 이슈는 별도 |

FHD의 2와 5 중복 계산, PERFORMANCE 기준:

| 선택 | Source FBO pixels | H FBO pixels | H draw 영역 pixels | V FBO/draw pixels |
|---|---:|---:|---:|---:|
| 현재 | 2,383,600 | 595,900 | 595,900 | 148,975 |
| 2만 | 2,327,040 | 581,760 | 581,760 | 145,440 |
| 5A tight만 | 2,383,600 | 595,900 | 529,240 | 148,975 |
| 2+5A tight | 2,327,040 | 581,760 | 529,240 | 145,440 |

2+5A에서도 Source ordinary draw는 거의 유지되고, FHD Output 화면 영역도 그대로다. 5A로 줄인 H 영역은 FBO 메모리 감소로 합산하지 않았다. 2+5A의 H 절감은 11.19%이지 2.37%+11.19%가 아니다.

## J. Expected Best Topology

장기적으로 가장 큰 구조 절감 후보는 다음이다. **현재 exactness가 해결된 완료 설계라는 뜻은 아니다.**

```text
WHOLE_TEXT + Fade1 + eligible plain A8

ordinary foreground L8
        │ display-space mapping + correct zero border
        ▼
H (quarter X / full Y) ──► V (quarter X/Y)
        │                            │
        │      ordinary sharp ───────┤
        │                            ▼
        └─────────────────── Output selection × p

Source task / Source FBO 없음
```

실현되면 3→2 offscreen tasks/FBOs, 4→3 draws, FHD A8 Source 2.273MiB 제거다. 하지만 지금 가장 안전하게 진행할 수 있는 topology는 **기존 Source→H→V를 그대로 유지하고 Output fetch만 분기하는 것**이다. 큰 구조 절감과 즉시 안전한 작은 개선을 구별해야 한다.

## K. Estimated Benefit

| 후보 | 이번 FHD에서 기대할 수 있는 몫 | Memory | 근거 수준 |
|---|---|---|---|
| 1 static Source | PERF Source draw 약17–23% 제거가 이상적 몫. clear/store 추가 절감은 미계측 | 변화 없음 | stage 측정 + 수식. actual exactness 미충족 |
| 2 FBO crop | H/V 면적 2.37%, 전체 draw 약1.5–1.6%의 낙관적 면적 환산 | A8 .071 / RGBA .283 MiB | 실제 ROI, 성능 미측정 |
| 3 direct A8 | Source draw 제거에서 per-tap mapping/zero-border 비용 차감 | Source 2.273MiB, PERF 전체 texture18.0% | 구조 계산, direct prototype 미구현 |
| 4 Output branch | Output .008–.012ms 감소 관측. 전체로는 작은 몫 | 없음 | 독립 native 진단, 12 Output parity |
| 5A H-band | H 영역10.34%; host H time 약3–5% 감소 관측 | 없음 | 독립 진단. 일부 1LSB 차이 남음 |
| 5B dynamic support | rectangle만으로는 H/V 영역 최대 약16.1%의 느슨한 면적 상한(5A 포함); sharp-only 생략은 별도 | FBO 유지 시 없음 | 식/경계 분석만, 실측 개선 아님 |

48개의 sample instructions 자체를 보존하는 한 H/V는 여전히 주 비용이다. **이 후보들만으로 타겟의 큰 frame drop을 반드시 해결할 수 있다는 증거는 없다.** Source 최적화는 분명 유용한 몫이지만 H/V 대부분을 없애는 작업은 아니다. target memory bandwidth/driver/동시 Label 수가 달라 호스트의 stage 비율도 그대로 옮길 수 없다.

## L. Risk Matrix

| Candidate | Exact correctness condition | GPU gain upper bound / observed | Memory gain | CPU impact | Lifecycle risk | Backend risk | Complexity | Recommendation |
|---|---|---|---|---|---|---|---|---|
| 1 | one p-linear foreground, static inputs; 8bit reorder 동등성 필요 | PERF Source17–23%가 이상적 몫 | 0 | submission 감소 가능, dirty 처리 추가 | 중~높음: live color/async/reconnect | fixed-point precision, one-shot producer 순서 | 중~높음 | strict equivalence 상태로 보류 |
| 2 | coverage+halo+gradient 좌표+quarter grid phase 보존 | 이번 FHD FBO면적2.37% | 작음; sparse에서 커짐 | 기존 scan에 min/max 또는 raster bounds만 | 중: payload/placement/update 반영 | odd dims/scale/texel grid | 중 | primary FHD 우선순위 낮음 |
| 3 | 1 조건 + plain A8 1:1/zero-border/ownership | Source 몫에서 새 sampling 비용 차감 | FHD2.273MiB | draw 감소 가능; shader ALU 증가 | 높음: original renderer/texture 수명 | 현 A8는 GLES3만, fractional resampling | 중~높음 | 장기 제한적 후보 |
| 4 | exact t≤0 / t≥1, 기존 premultiplied mix 유지 | Output stage 6–10% 감소 관측 | 0 | 추가 frame별 CPU 작업 불필요 | 낮음: resource 교체 없음 | uniform branch compiler 효율, GLES2/Vulkan 확인 | 낮음 | **1순위** |
| 5A | 보수적 Y support와 동일 UV mapping, numeric parity | H면적10.34%; H time3–5% 감소 관측 | 0 | publication geometry O(1) | 낮음: task/ownership 유지 | interpolation rounding 남음 | 낮음~중 | **parity closure 후 2순위** |
| 5B | H/V 각각의 convolution+filter support, clear 유지 | halo/구간 의존, 미측정 | 0 | CPU rebuild 없이 vertex 계산 | 중: stale halo/seek/clear | quarter grid/filter/partial-update | 중~높음 | 후순위 |

공통 기능별:

- **sync/async:** 4는 이미 accepted된 renderer의 shader 식만 바뀐다. 5A는 그 publication의 raster/transform에만 기반해야 한다. 1/3은 live property와 pending/retired publication까지 확인해야 한다.
- **ImageSpan:** 1/3의 초기 eligibility에서 제외. 2/5는 예약 placement의 union을 포함하거나 full bounds fallback. 4는 이미 합성된 Source/V를 선택하므로 별도 이미지 I/O나 ownership 변경이 없다.
- **Gradient:** 1은 shader 내부 합성 후 fade인 경우에 한정. 2/3은 좌표계 보존 중요. 4는 LUT를 건드리지 않는다. 5A는 원래 Source raster를 유지한다.
- **Decoration:** 항상 blurred foreground에만 적용. decorated final output/Label opacity로 대체하지 않는다.
- **GLES2/Vulkan:** 이번 GL host에서 parity/성능이 확인된 것과 모든 backend가 검증된 것은 다르다. 현재 A8 capability gate는 변경하지 않는다.

## M. Recommended Next Step

### 1순위: Candidate 4만 먼저 production 후보로 만들기

기존 Output shader 안에서 exact sharp-only/blur-only 구간의 불필요한 fetch만 생략한다. kernel, curve, uniform binding, RenderTask, lifetime은 유지한다. 이번 범위 중 최종 pixel 동일성과 작은 stage 이득을 함께 확인한 가장 단순한 후보다.

구현 후에는 WHOLE_TEXT A8/RGBA의 구간 경계(p≈.741/.877), reverse/seek, B≠1, ImageSpan/decoration composition, HIGH/ordinary/일반 Reveal 무변경을 targeted 검증하고 타겟 측정한다. 작은 개선이므로 이것으로 타겟 frame drop이 해결됐다고 가정하지 않는다.

### 2순위: Candidate 5A의 수치 동등성 closure

추가 bitmap scan 없이 ordinary raster full bounds부터 사용한다. 현재 외부 geometry 진단의 1LSB 차이가 남는 채로 채택하지 않는다. UV interpolation 경로를 보존하는 구현을 먼저 검증하고, 동일 결과를 유지할 수 있을 때만 production으로 진행한다. 더 tight한 coverage와 5B dynamic geometry는 처음부터 섞지 않는다.

**Candidate 1+3은 가장 큰 구조적 절감 가능성이 있지만, 이번 strict framebuffer 기준으로 우선 구현 대상으로 지정하지 않는다.** live input invalidation, source-over, quantization, resampling 네 조건을 생략하고 'Gaussian은 선형이므로 안전하다'고 결론내리는 것이 가장 큰 위험이다.

최종 두 질문에 대한 답:

1. **Source를 animation-independent하게 만들 수 있는가?** 단일 static foreground에는 수학적으로 가능하다. 다만 현재 R8/RGBA8 framebuffer까지 완전히 동일하게 만들 수 있다는 증명은 없고, 단순 relocation에서는 1LSB 차이를 재현했다. 모든 ImageSpan 혼합에 적용하는 것은 수학적으로도 틀리다.
2. **plain A8에서 Source RenderTask/FBO를 제거할 수 있는가?** 제한된 1:1 plain A8에는 가능한 구조다. 그러나 단순 texture substitution이 아니며, 위 exactness 조건과 zero-border/transform/borrowed-renderer 수명까지 해결해야 한다. 이번 단계에서는 가능성을 구조적으로 확인했을 뿐 production-ready exact fast path로 승인하지 않았다.

## N. Production Source / 작업 상태

**NO PRODUCTION CHANGE.**

- dali-ui HEAD: `46d0ea182d77af03b8b75ab6e5480bf5d591624c` 유지.
- dali-ui working tree와 index는 작업 전후 clean.
- 기존 dali-adaptor의 별도 13-line 변경을 건드리지 않았다.
- commit/amend/stage/reset/restore/stash/rebase/push를 하지 않았다.
- production full build/full UTC/sanitizer matrix를 실행하지 않았다. 외부 진단 harness만 컴파일/실행했다.
- 현재 kernel/quality/quarter/fade/blur curve/public API/source batching은 유지했다.
- 사용 foundation library SHA-256: `aaee5bd2599f297e88a9098f70b09a5b01ec67cfbd4daa0f21d6f46328811f43` 유지.

### 진단 데이터 감사 메모

- 최초 fade diagnostic은 외부 shader uniform block 작성 형식 오류로 shader link가 실패했다. `captures/fade-*`의 최초 결과와 `capture-analysis.txt`는 **무효**다. 수정된 `captures-corrected/fade-*`와 `capture-analysis-final.txt`만 결론에 사용했다. production shader 오류가 아니다.
- 최초 FHD RGBA H-band timing은 새 actor의 update-side SIZE를 첫 update 전에 읽어 잘못된 band를 만들었다. `hband-pair-hband-rgba-*`는 **무효**다. production과 같은 event-side creation size를 사용한 `hband-corrected-*` 결과로 대체했다. 유효 band는 `[48,1106)`임을 로그에서 확인했다. A8 timing과 small RGBA capture의 band는 정상이다.
- 이 잘못된 진단에서 나타난 큰 '개선'을 보고서에 성능 이득으로 포함하지 않았다.
- `probe.cpp`, `diagnostic.inc`, 실행 스크립트, raw logs/captures는 이 보고서와 같은 repository 외부 디렉터리에 남겼다. 독립 후보를 함께 활성화한 결과는 없다.

추가 코드 근거: runtime task 구성 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1692`), target/guard 변환 (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp:1996`), Gaussian sampling (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/graphics/shaders/text-reveal-blur.frag:88`), kernel offsets (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/render-effects/gaussian-blur-algorithm.cpp:99`), metadata ownership pass (로컬 자료: `../dali/dali-ui/dali-ui-foundation/internal/text/rendering/text-typesetter-impl.cpp:1930`).
