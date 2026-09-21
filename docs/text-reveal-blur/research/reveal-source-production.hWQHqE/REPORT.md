# Text Reveal Blur — Production Source Batching

판정: **HOST VERIFIED PRODUCTION CANDIDATE / TARGET PERFORMANCE VERIFICATION PENDING**.

Host에서 Source draw 감소, GPU 이득, atlas replacement 및 focused correctness/lifecycle 검증을 완료했다. FHD CPU 개선률은 편차 때문에 확정하지 않는다. Tizen target FPS와 build-server 전체 회귀 검증은 아직 남아 있다.

전체 수치·범위·객체 수·FBO 크기는 [METRICS.md](METRICS.md), 원시 집계는 matrix.json (로컬 자료: `matrix.json`)에 있다.

## A. Production architecture

```text
PrepareRevealBlur (sync: 기존 preparation thread / async: worker)
  isolated foreground + metadata + optional mask
    → O(lines) vertical atlas packing / one-texel replicated guards
    → CPU atlas + line entry rectangles; isolated CPU planes release

Publication (event thread, 기존 revision/owner transaction)
  atlas당 TextureSet 생성 및 plane별 upload
    → eligible line의 개별 GPU texture는 생성하지 않음

Runtime (GPU handles only)
  compatible contiguous source quads, 최대 64개/draw
    → Source FBO → 기존 HIGH/PERFORMANCE H/V → 기존 output
```

Prepared layer에 Actor/Renderer/Texture가 없고 runtime sequence에 PixelData가 없다. Atlas는 publication마다 소유하며 global/shared atlas cache를 만들지 않았다. Shader cache만 기존 factory 방식으로 재사용한다.

Atlas는 format/mask 유무/최대 texture dimension 경계에서 분할한다. Single-entry 또는 guard를 넣을 여유가 없는 line은 기존 isolated path를 유지한다. 한 page에 여러 atlas가 걸리면 draw를 나눈다. Shader/TextureSet identity는 같은 publication의 shared LUT, sampler 및 feature variant를 포함한다.

ImageSpan의 기존 A8/RGBA page packing과 output 순서는 유지한다. 기존 packing으로 서로 떨어진 논리적 줄이 같은 page에 배치되어도 새 Source draw는 그 경계를 넘어 합치지 않는다. 이미지가 있는 줄 앞뒤도 별도 group이다. 이미지의 proxy/capture/readiness/borrow/restore 코드는 변경하지 않았다.

## B. Supported paths

| Case | Source 처리 |
|---|---|
| Plain A8/L8 | foreground + metadata atlas, compatible page/group batching |
| RGBA colored StyledText | RGBA foreground + metadata atlas |
| Global gradient + colored text / emoji | foreground + mask + metadata atlas; LUT는 기존 shared resource |
| Linear / Radial / Conic, overlay | local text UV / gradient bounds 유지; atlas UV는 plane sampling에만 사용 |
| ImageSpan | compatible text sub-batches + 기존 image capture draw |
| Shadow / underline / background 등 기존 supported decorations | foreground batching + 기존 별도 decoration composition |
| Sync / async | 동일 prepared payload / GPU publication; async atlas assembly는 worker |
| WHOLE_TEXT | 기존 simple Source; atlas assembly/buffer reserve 없음 |
| 기존 emboss/cutout/tiling fallback | 변경 없음; blur eligibility 확장 없음 |

Source shader는 명시적인 `TEXT_REVEAL_SOURCE_ATLAS` compile-time variant이다. 문자열 find/replace가 없다. `aSourceCrop`, `aSourceOffset`, `aSourceAtlas`는 각 quad의 기존 crop, page 배치 및 atlas 주소를 분리한다. Attribute 기반 배치이므로 shader의 `MODIFIES_GEOMETRY` hint로 원본 visual bounds를 잘못된 culling bound로 사용하지 않게 한다. 일반 shader의 hint는 변경하지 않는다.

Sampler 수는 line 수와 무관하다. 기존 foreground/mask의 LINEAR, metadata의 NEAREST, shared LUT sampler를 그대로 사용한다. DALi의 DEFAULT wrap은 CLAMP_TO_EDGE이다. Local UV를 [0,1]로 제한하고 네 모서리까지 edge texel을 한 칸 복제하므로, non-mipmapped linear footprint가 다른 entry를 읽지 않는다.

## C. Ownership

- Atlas entry는 CPU preparation 후 isolated foreground/metadata/mask handle을 비운다.
- Event publication은 atlas당 각 plane을 한 번만 upload하고 TextureSet을 공유한다. 기존 line GPU textures와 atlas를 동시에 유지하지 않는다.
- `Texture::Upload`는 `PixelDataPtr`를 `MessageValue2`의 값으로 queue에 보관한다 (`dali-core/internal/render/renderers/render-texture-messages.h`). 수동 buffer free를 하지 않는다.
- Sync의 local prepared object는 publication 뒤 scope를 벗어나며, async는 기존 `renderInfo.revealBlur.reset()`/result 폐기 경로로 CPU payload가 해제된다. Stale request는 기존 revision 검사로 폐기한다.
- Publication의 allocation/reentry checks, candidate activation 및 ImageSpan ownership transaction을 재사용한다.
- GPU memory profiling이 켜진 빌드의 `TextureUploadWithContent` 경로도 유지했다.
- 기존 hidden ordinary foreground/global metadata는 이 작업의 제거 대상이 아니다. 제거하는 것은 **atlas에 들어간 개별 line input textures**다.

## D. Correctness

최종 후보 실제 GL framebuffer 비교: **472 pairs**, Source/H/V/output 및 progress `0 → .4 → 1 → .4`.

- **468 pairs byte-identical**.
- Radial USER_SPACE/REFLECT와 Conic OBJECT_BOUNDING_BOX/REPEAT에서 완료 시 각각 Source 한 채널, output 한 채널만 **1/255** 차이. Alpha, H/V 및 다른 progress에는 차이 없음.
- Radial: Source `(252,207)`, red `249 → 250`.
- Conic: Source `(285,277)`, red `87 → 88`.
- CPU에서 line별 gradient bounds를 나누던 계산을 per-quad GPU 계산으로 옮긴 데 따른 부동소수점/최종 8-bit 색상 양자화 차이와 부합한다. Atlas 좌표를 gradient 좌표로 사용하는 변경은 없다. 1 LSB를 없애기 위한 추가 per-line resource/복잡한 보정은 넣지 않았다.
- Reverse/seek는 모든 case에서 같은 progress의 forward 결과와 일치한다.
- Plain/gradient/emoji/ImageSpan text Source는 p0에서 clear, p1에서 실제 내용이 존재함을 확인했다. Decoration composition은 기존 contract대로 p0에서도 남는다.
- Coverage: A8 두 quality, 실제 RGBA+gradient-mask 두 quality, colored StyledText, emoji/mask, Linear/Radial/Conic/overlay, ImageSpan text-image-text, shadow+underline, render scale 1.25, async, FHD multi-page, 64/65 lines, CHARACTER/WORD/LINE/PIXEL, WHOLE_TEXT.

상세 raw file 및 결과: `capture/`, `capture-extra/`, `parity.json`. Shader hash만 비교한 검증이 아니라 실제 FBO bytes를 비교했다.

## E. Lifecycle

Focused UTC는 None, quality 변경, text 변경, scene disconnect/reconnect, direct destruction, async stale/destruction, ImageSpan readiness/replacement, Adaptor stop 및 construction reentry를 포함한다. 새 atlas packing/splitting/stride/guard/format/grouping/lifetime/ImageSpan-boundary 검증을 추가했다. 기존 valid→invalid→valid publication 테스트에 atlas index/rectangle/metadata invalid cases를 추가했다.

Native FHD 및 ImageSpan capture harness의 teardown에서도 tracked atlas Texture/Renderer/Geometry weak handles가 0으로 복귀하고, 최종 offscreen tasks가 0으로 복귀했다. Source Geometry가 VertexBuffer를 소유하는 기존 경로를 사용하며 별도 raw-pointer ownership이 없다.

## F. Performance

12 cases × baseline/candidate × CPU/GPU separate × 3 independent runs = **144 process runs**. 추가 FHD A8 CPU 확인 20 runs는 아래에서 따로 설명한다.

| Workload | Source draws | Total Source/H/V/output draws | PERFORMANCE GPU A8 / RGBA 감소 | HIGH GPU A8 / RGBA 감소 |
|---|---:|---:|---:|---:|
| 500×350, 6 lines, 8 Labels | 48 → 8 | 72 → 32 | 32.4% / 26.0% | 14.2% / 15.5% |
| 500×500, 14 lines, 8 Labels | 112 → 8 | 136 → 32 | 39.7% / 34.5% | 26.4% / 20.8% |
| FHD, 31 lines, 1 Label | 31 → 6 | 49 → 24 | 21.0% / 19.7% | 11.2% / 16.1% |

Source 외 H/V/output draw structure와 FBO 크기는 동일하다. GPU 시간이 수치상 완전히 같은 것은 아니며, stage별 측정값은 METRICS에 모두 남겼다. 위 draw count는 steady frame의 구조이고, 계측 시작/끝이 프레임 경계에 정확히 일치하지 않아 raw interval 평균에는 약간의 소수 차이가 있다.

PERFORMANCE medium CPU는 A8 **20.2% / 34.0%**, RGBA **25.3% / 42.8%** 감소했고 baseline/candidate 범위가 겹치지 않았다. HIGH는 medium RGBA 500×500에서 +1.0%/넓은 겹침, 나머지는 mean 감소였다. 따라서 모든 case의 CPU가 일정한 비율로 개선된다는 주장은 하지 않는다.

초기 FHD A8 HIGH mean은 213.3→254.3 ms/s(+19.2%)였다. 같은 코드/조건으로 순서를 교대하여 **각 quality baseline/candidate 5회씩 추가**했다. 추가 HIGH는 211.2→173.2 ms/s, PERFORMANCE는 209.2→180.7 ms/s였으며 둘 다 paired 5회 중 4회 감소/1회 증가였다. 처음 3회도 제외하지 않은 전체 8회 평균은 HIGH 212.0→203.6, PERFORMANCE 212.4→199.3 ms/s다. 범위가 넓고 순위가 바뀌므로 **지속적 CPU 회귀는 확인되지 않았지만, FHD CPU benefit 역시 미확정**으로 남긴다. 요청한 repeated/non-overlapping/>10% stable regression gate를 위반하지 않는다.

방법: 같은 native public-API probe, baseline frozen library와 candidate library를 교대로 로드한다. Case당 CPU 3개 process와 GPU 3개 process를 따로 실행한다. 두 번째 반복은 baseline/candidate 순서를 뒤집는다. 빌드/UTC/sanitizer와 동시에 측정하지 않는다.

Ordinary label 생성 후 blur를 설정하고 모든 runtime companion publication을 확인한다. **Progress=0.5 고정 + KeepRendering** 상태에서 약 0.8초 warmup 후 약 3초 구간의 process CPU ms/s를 측정한다. GPU는 별도 실행에서 GL timer query로 Source/H/V/output **draw time**을 측정하며 frame 수로 나눈다. FBO clear/upload/present 비용 및 실제 애니메이션 FPS와 동일한 지표는 아니다. 종료 전 pending query를 drain한다.

환경: GTX 1650 / NVIDIA 595.91.07, X11, MSAA4, font24. 기존 host foundation 빌드 설정을 그대로 사용했다(CMake build type/추가 CXX flags 비어 있음, GCC11 기본 O0). Baseline도 같은 설정이다. Release TV의 절대 CPU 시간이나 FPS로 외삽하지 않는다. Baseline frozen library는 `reveal-source-atlas.3igp9v/baseline/libdali2-ui-foundation.so.2`, candidate는 현재 repository의 `build/tizen/dali-ui-foundation`이다.

A8는 실제 L8 foreground + RGBA metadata이다. 이번 RGBA workload는 단순 L8 gradient가 아니라 **ForegroundColorSpan + global gradient**, 즉 RGBA foreground + L8 mask + RGBA metadata와 LUT를 사용한다. 이전 PoC 수치는 참고이며 이번 표의 baseline/candidate는 동일 corpus/format의 재측정이다.

## G. Setup

`ordinary → blur 설정 직전`부터 `모든 companion publication 확인`까지의 wall/process CPU를 별도로 기록한다. Timer polling/frame scheduling을 포함하므로 순수 atlas copy 시간이나 GPU upload 완료 latency와 동일하지 않다.

- 500×350×8 wall: baseline 81.27~92.31 ms → candidate 74.46~81.07 ms (각 quality/format 평균들의 범위).
- 500×500×8 wall: 155.84~179.39 ms → 134.64~148.42 ms.
- FHD wall: 약 141~157 ms → 138~156 ms, 대체로 중립. Process CPU에는 약 0.9~4.7 ms의 증가가 있으며 감소한 Source renderer setup과 추가 atlas copy를 함께 포함한다.
- 별도 atlas construction microbench: 한 Label의 실제 plane dimensions, 입력 생성은 구간 밖, 5 warmup+20 repeats. A8 mean은 medium 약 0.08~0.11 ms/FHD 1.07 ms, RGBA+mask는 약 0.21~0.52 ms/FHD 5.12 ms. Allocator/cache 편차가 있으므로 cold-start 또는 크기별 선형 예측으로 사용하지 않는다. 범위/방법은 METRICS 참조.
- Async에서는 이 CPU assembly가 기존 worker preparation 함수 안에서 수행된다. Event thread에는 upload request/binding만 남겼다. Async event-thread 시간을 독립적으로 계측한 결과라고 주장하지는 않는다.

## H. Memory

실제 runtime Texture handle inventory에서 unique texture의 dimensions/format으로 payload를 합산한다. **Projected가 아니라 실제 생성된 texture들의 payload**이며, driver allocation alignment/compression/VRAM residency까지 측정한 숫자는 아니다. FBO payload는 포함 부분집합으로 별도 표기한다.

| Workload / PERFORMANCE | A8 total MiB | RGBA total MiB | Texture objects A8 / RGBA |
|---|---:|---:|---:|
| 500×350×8 | 9.537 → 9.877 | 22.915 → 23.526 | 136→56 / 200→80 |
| 500×500×8 | 22.148 → 23.239 | 53.442 → 55.405 | 264→56 / 392→80 |
| FHD×1 | 23.445 → 24.139 | 55.348 → 56.597 | 77→17 / 110→20 |

전체 12 cases의 total payload 증가는 **1.68~4.92%**다. Atlas guard와 vertical stack의 폭 차이에서 오는 증가이며 PoC의 중복 보관이 아니다. HIGH 및 FBO별 수치도 METRICS에 포함했다. FBO payload 자체는 모든 case에서 baseline과 같다.

한 Label의 input texture 수는 A8 12/28/62 → **2**, RGBA+mask 18/42/93 → **3**이다. Source renderer 감소에 따라 전체 Renderer는 medium6줄 80→40, medium14줄 144→40, FHD 50→25. Actor/offscreen task 수는 그대로이고, 기존 shared quad를 대체하는 정적 Source batch geometry 때문에 unique Geometry는 medium 25→33/FHD 19→25다.

TextureSet/Actor traversal과 RenderTask framebuffer inventory를 합쳐 중복을 제거한다. Atlas로 교체된 line의 별도 GPU Texture 생성 분기가 없음을 코드와 object count 양쪽에서 확인한다. RSS/heap checkpoint는 driver/allocator caching을 포함하므로 live texture payload와 구분한다. Preparation 순간에는 line CPU planes와 작성 중인 atlas가 겹치는 transient peak가 있으며, 256 MiB admission threshold/estimator는 변경하지 않았다.

기존 admission은 cropped width가 아니라 전체 raster width와 넉넉한 metric band를 사용하며 A8/quarter/shared-scratch 절감도 credit하지 않는다. 새 atlas 역시 전체 raster width(+2 guard) 이하의 단순 stack이라 기존 conservative 정책을 교체할 근거는 없었다. 실제 peak/RSS가 256 MiB 이하라는 보장은 원래도 아니며 이번에도 추가하지 않았다.

## I. Tests

- Foundation/components build: PASS.
- Focused internal Runtime Blur + async publication: **117/117 PASS**.
- Targeted ASan/LSan: **34/34 PASS**, 새 suppression 없음.
- Sanitizer 범위: foundation 및 UTC/test utilities. 설치된 core/adaptor 자체는 ASan 재빌드하지 않았으므로 그 내부 전체의 sanitizer 검증을 의미하지 않는다.
- Native framebuffer 비교: 위 D 참조.
- 새 perf sample build 및 native key 전환/종료 smoke: PASS.
- Full 3000+ regression/Windows UTC: 요청대로 이번 작업에서 실행하지 않음, build-server/merge gate.
- `git diff --check`: PASS.

## J. Target test

`samples/text/text-reveal-perf-example.cpp` / `text-reveal-perf.example` 추가. 기존 gradient perf는 이 workload와 다르고 Reveal guide는 단일 preview이므로, 기존 앱을 복잡하게 바꾸지 않고 작은 고정 workload 샘플로 분리했다.

```sh
cmake -S samples/text -B samples/text
cmake --build samples/text --target text-reveal-perf.example
cd samples/text
LD_LIBRARY_PATH=../../build/tizen/dali-ui-foundation:/home/bowonryuubuntu/dali-env/opt/lib \
  DALI_WINDOW_WIDTH=1920 DALI_WINDOW_HEIGHT=1080 ./bin/text-reveal-perf.example
```

| Key | 기능 |
|---|---|
| 1 | 500×350, 8 Labels, host corpus 6 lines |
| 2 | 500×500, 8 Labels, host corpus 14 lines |
| 3 | 1920×1080, single Label, host corpus 31 lines |
| H | PERFORMANCE / HIGH |
| G | A8 white / RGBA + gradient |
| A | Sync / async |
| Space | progress replay |
| Escape/Back | 종료 |

24px, PIXEL/PER_LINE, fade0, stagger.25, blur24, blurTime.5. Animation은 2초 AUTO_REVERSE 무한 반복. 폰트 metrics가 다르면 target의 실제 line count는 달라질 수 있다. Label의 requested size는 고정하며 창 크기에 맞춰 texture workload를 축소하지 않는다. 8개 4×2 grid는 가로2000px이므로 FHD에서는 좌우40px씩 화면 밖이다. Host matrix는2000×1000 창을 사용했다. 동일 창 조건의 baseline/candidate를 비교해야 한다.

Runtime optimization OFF switch는 없다. Baseline library/build 또는 사용자가 보존한 baseline branch와 candidate build를 비교한다. 새 sample 파일은 baseline에도 동일하게 적용해서 public API-only workload를 동일하게 유지할 수 있다. FPS는 target의 기존 관측 도구를 사용한다.

위 LD_LIBRARY_PATH는 **host의 아직 install하지 않은 candidate**를 명시하는 명령이다. Target에서는 각 baseline/candidate package를 정상 build/install하여 비교한다. 설치된 host library는 이번 작업에서 덮어쓰지 않았다. 기존 guide/sample의 설정을 변경하지 않았다.

## K. Remaining gates

**TIZEN TARGET FPS / PERFORMANCE VERIFICATION REQUIRED.** Host GPU ms/frame 개선을 target FPS 개선률로 그대로 환산하지 않는다. Build-server full regression/Windows 및 target 확인 후 merge 판단이 필요하다.

## L. Working tree

HEAD `47be37cfeab40afb36382574ebed464b19697623` 유지. Commit/staging/rebase/reset/stash/push 없음. 모든 후보 변경은 unstaged. Backup branch는 수정하지 않았다. 기존 dali-adaptor의 별도 13-line 변경은 그대로 보존했다.

PoC CPU PixelData runtime retention, env switch, shader 문자열 치환 및 진단 계측은 제거했다. Public API, blur quality/timing/kernel/H/V packing/refresh 정책에는 변경이 없다.

## M. Verdict

**HOST VERIFIED PRODUCTION CANDIDATE**

**READY FOR TARGET PERFORMANCE TEST / TARGET PERFORMANCE VERIFICATION PENDING**

판정 근거: supported Source variants의 focused parity, ownership/sanitizer 검증, 모든 primary case의 GPU draw 이득, medium PERFORMANCE CPU/설정 비용 이득, actual atlas replacement 및 작은 total payload 증가. FHD CPU는 미확정이고 target FPS는 측정하지 않았으므로 merge-ready/target 성능 보장으로 해석하지 않는다.
