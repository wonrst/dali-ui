# Text::Reveal Blur — 실제 FHD paragraph native 측정

2026-09-14 · `devel_blur_text` · HEAD `c7f2303e`.

## 결론

**ESTIMATOR TOO CONSERVATIVE FOR NORMAL FHD.**

줄 수를 고정하지 않고 1920×1080을 실제 한글 wrapping으로 채웠을 때:

- font24: **31줄 / 높이1054px / 3208 characters**.
- font32: **23줄 / 높이1058px / 1800 characters**.
- 현재 수식의 전체 candidate estimate는 각각 **190.59 / 175.31 MiB**. 8개 조합 모두 128 MiB guard에서 거부된다.
- guard를 측정용으로만 우회하면 전체 관련 texture payload는 **21.26~46.33 MiB**, CPU prepared payload는 **14.93~15.54 MiB**이다.
- steady RSS는 process 시작 후 Label 생성 전보다 **약63~65 MiB**, ordinary Label layout 이후보다 **약43~45 MiB** 증가했다. RSS는 GPU VRAM/정확한 Reveal allocation이 아니다.
- PERFORMANCE는 HIGH보다 GPU draw 시간이 **30.1~39.4%** 감소했다. 전체 texture 감소는 **5.0~14.5%**, FBO 감소는 **17.2~25.0%**이다.

따라서 현재 결과는 단순히 “full-screen text가 실제로 175~190 MiB를 사용해서 거부된다”는 해석을 지지하지 않는다. 특히 A8까지 동일한 3×RGBA/band allowance로 거부하는 것은 실제 비용 대비 상당히 보수적이다. **threshold를 올리지는 않았다.** 이 측정만으로 모든 target의 allocation peak를 보장하지도 않는다.

**다음 Source batching PoC 우선순위: HIGH.** 이번에는 구현하지 않았다.

## A. Fixture와 환경

Ubuntu native GLES, NVIDIA GeForce GTX1650, driver595.91.07, MSAA4. 실제 window framebuffer는1920×1080이며 desktop은2560×1440/약60Hz이다. TV SoC에서 측정한 값이 아니므로 아래 GPU ms를 TV의 frame time으로 대입하면 안 된다.

공통 설정:

```text
Label requested size: 1920 × 1080, STANDALONE, UI scale 1
MultiLine: true
Line height / wrap: production defaults (line height property -1)
Maximum lines: default unlimited
Overflow: CLIP (하지만 text 자체가 1080 안에 들어가도록 생성)
Unit: PIXEL
Sequence: PER_LINE
Stagger: 0.25
FadeDurationRatio: 0
BlurDurationRatio: 0.5
BlurRadius: 24
Progress: 0.5, steady
Path: Sync
```

별도의 calibration process에서 일상적인 한글 prose를 공백 단위로 반복했다. `GetHeightForWidth(1920)`로 **1080을 넘지 않는 최대 word 수를 binary search**하고, 확정된 텍스트를 파일에 저장했다. 실제 측정 process는 파일을 그대로 읽는다. calibration으로 font cache를 process 내부에서 미리 warm-up하지 않는다. explicit newline, synthetic AAA, compressed line height는 사용하지 않았다.

| Fixture | Font | 기본 실제 line height | Final lines | Text extent W×H | 높이 점유 | Characters / glyphs | line width min / mean / max |
|---|---:|---:|---:|---|---:|---:|---|
| F24 | 24 | 34px (asc28 / desc−6 / spacing0) | 31 | 1916×1054 | 97.59% | 3208 / 3208 | 1836 / 1889.1 / 1916 |
| F32 | 32 | 46px (asc37 / desc−9 / spacing0) | 23 | 1911×1058 | 97.96% | 1800 / 1800 | 1794 / 1869.3 / 1911 |

평균 line width는 Label width의 **98.39% / 97.36%**이다. final model의 모든 line에서 ellipsis=false, line character count의 합도 전체 text와 일치한다. 특정 줄만 길게 만들어 화면을 채운 fixture가 아니다.

A8는 white monochrome이며 gradient/span/image가 없다. RGBA는 **같은 텍스트·font·layout에 가로 text gradient만 추가**했다. actual Source/H/V format이 RGBA8888인 것을 inventory로 확인했다. glyph foreground plane은 이 gradient 구성에서도 L8이다. 두 format의 line count, raster size, characters, per-line CPU payload는 동일하다.

텍스트: F24 (로컬 자료: `corpus-24.txt`), F32 (로컬 자료: `corpus-32.txt`). Calibration: 24 (로컬 자료: `calibrate-24.log`), 32 (로컬 자료: `calibrate-32.log`). Model/line metrics는 `policy-*.log`, `cpu-*.log`에 있다.

## B. 128 MiB guard와 측정용 우회의 구분

현재 production은 이 입력에 blur FBO를 만들지 않으므로, 그대로 측정하면 ordinary Reveal fallback의 비용만 나온다. 이를 blur 비용으로 잘못 보고하지 않기 위해 다음을 분리했다.

1. **Policy ON:** 8개 조합에서 거부와 offscreen task0을 확인했다. 추가로 원래 build library로 F24/F32 A8를 실행해 같은 거부 결과를 확인했다: production24 (로컬 자료: `production-policy-24.log`), production32 (로컬 자료: `production-policy-32.log`).
2. **Cost measurement:** repository 밖의 `preparation-measure.cpp`와 별도 library를 만들었다. 기존 수식의 값을 출력하고, `FHD_MEASURE_OVERLIMIT=1`일 때 **128 MiB memory admission만 우회**했다. dimension/NaN/texture limit/coverage/publication 검증, prepared planes, shader, kernel, Source/H/V/output, packing은 바꾸지 않았다.

중요: production은 합산 도중 128 MiB를 넘으면 즉시 반환한다. 아래 **Admission 열은 동일 수식을 전체 candidate에 끝까지 적용한 값**이다. 실제 early-stop 지점은 F24 약20번째 sequence에서131.18 MiB, F32 약16번째에서129.03 MiB이다(실제 metrics에 동일 수식 적용). 전체175/190 MiB를 끝까지 계산해야 거부한다는 뜻이 아니다.

측정 library는 같은 build의 나머지 object를 사용하고 preparation object 하나만 계측용 복사본으로 교체했다. `LD_LIBRARY_PATH`로 측정 process에서만 사용했다. 저장소/설치 library/128 MiB 상수는 변경하지 않았다. 이 우회본은 target 배포용이 아니다.

## C. 측정 방법과 횟수

각 조합에서 **CPU-only 3 independent processes + GPU-timer 3 independent processes**: 총48개 performance process. 8개 policy check는 별도이다. GPU timer가 process CPU/RSS에 미치는 영향을 피하기 위해 CPU/RSS/setup 표는 CPU-only process를 사용했다.

순서:

```text
process init / 0.5s settle
→ before 메모리
→ ordinary Label 생성·layout / 0.3s settle
→ ordinary 메모리 및 inventory
→ Reveal Blur enable
→ runtime companion/task publication 확인
→ 0.8s warm-up
→ progress 0.5 steady 메모리·inventory
→ 3초 CPU 또는 GPU 측정
→ Label destroy / 0.8s settle / task 회수 확인
→ 같은 Label을 재생성하여 warm publication 시간·inventory 확인
→ destroy / 0.8s settle / task 회수 확인
```

CPU는 `CLOCK_PROCESS_CPUTIME_ID`의 모든 process thread 합계 ms/s이다. GPU는 기존 `gpu-meter`의 Source/H/V/Output draw query를 재사용했다. **clear/upload/present/compositor 시간은 GPU draw 합계에서 제외**된다. 3회 평균이며 각 run은 약3.008초, GPU180~181 frame이다. dropped query/disjoint는 모두0이다.

일부 draw count는 측정 경계 때문에 한두 draw가 덜 들어간다. 아래 Draws는 정상 완전한 frame의 구조상 S/H/V/O 개수이고, raw/평균 카운트는 summary.json (로컬 자료: `summary.json`)에 있다.

progress0.5 정지 화면이므로 animation 전체 평균이나 worst frame 측정이 아니다. 반경 matrix/quality research는 하지 않았다. 캡처는 framebuffer1920×1080과 glyph/gradient/blur 동작 확인에만 사용했다. p0.5라 아래쪽 아직 시작하지 않은 줄이 안 보이는 것은 의도된 상태다.

## D. 8-case 최종 표

메모리는 MiB, GPU는 ms/frame이다. **모든 actual blur 수치는 위 측정용 guard 우회 상태**이다. Total texture는 ordinary resources까지 포함한 이 Label의 전체 unique texture payload이고, 이 앱에는 다른 UI texture가 없어 전체 관련 texture 합계와 같다. RSS Δ는 `steady − before Label creation`이다.

| Font | Lines | Quality | Format | Admission | FBO | Total texture | CPU prepared | RSS Δ | CPU ms/s | Source GPU | H | V | Output | GPU Total | Draws S/H/V/O |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
|24|31|PERFORMANCE|A8|190.59|5.97|23.45|15.54|65.12|188.3|0.2510|0.0480|0.0492|0.2718|**0.6200**|31/6/6/6|
|24|31|PERFORMANCE|RGBA|190.59|23.90|41.37|15.54|64.85|227.5|0.3198|0.0918|0.0719|0.2736|**0.7571**|31/6/6/6|
|24|31|HIGH|A8|190.59|7.21|24.68|15.54|65.37|181.7|0.2546|0.1200|0.3143|0.2479|**0.9367**|31/6/6/6|
|24|31|HIGH|RGBA|190.59|28.86|46.33|15.54|63.34|230.0|0.3137|0.1629|0.3503|0.2568|**1.0837**|31/6/6/6|
|32|23|PERFORMANCE|A8|175.31|4.40|21.26|14.93|64.92|194.2|0.2082|0.0471|0.0427|0.2160|**0.5140**|23/4/4/4|
|32|23|PERFORMANCE|RGBA|175.31|17.60|34.47|14.93|64.71|185.2|0.2637|0.0767|0.0570|0.2172|**0.6145**|23/4/4/4|
|32|23|HIGH|A8|175.31|5.87|22.73|14.93|62.85|224.8|0.2205|0.1308|0.2988|0.1986|**0.8487**|23/4/4/4|
|32|23|HIGH|RGBA|175.31|23.47|40.33|14.93|63.92|177.7|0.2712|0.1719|0.3331|0.2122|**0.9885**|23/4/4/4|

3-run GPU total의 범위는 약0.01~0.06ms 폭이었다. 반면 CPU는 일부 case에서 run 사이 변동이 컸다. 예를 들어 F32 PERFORMANCE A8은156.2~222.1ms/s, HIGH A8은195.3~246.3ms/s였다. **이 CPU 차이를 quality mode에 의한 확정적인 개선/회귀로 판정하지 않는다.** 개별 range/60Hz 환산은 [TABLES.md](TABLES.md), 전체 raw 수치는 summary.json (로컬 자료: `summary.json`)에 저장했다.

## E. 실제 resource breakdown

모든 texture handle을 한 번만 센 `width × height × bytes-per-pixel`이다. driver alignment/compression/physical VRAM, UBO/vertex/command allocations는 포함하지 않는다.

### Ordinary / per-line / CPU prepared

| Resource | F24 MiB | F32 MiB | 설명 |
|---|---:|---:|---|
| ordinary foreground | 1.9299 | 1.9373 | 1920×1054 / 1920×1058, L8 |
| global Reveal metadata | 7.7197 | 7.7490 | 같은 full raster, RGBA8888 |
| per-line foreground 합계 | 1.5641 | 1.4354 | 실제 crop dimensions의 L8, 31/23개 |
| per-line metadata 합계 | 6.2566 | 5.7415 | 대응 crop dimensions의 RGBA8888 |
| gradient LUT | 0.001465 | 0.001465 | RGBA case만,512×1 RGB888; 여러 Source renderer가 같은 handle 참조 |
| mask / image payload | 0 | 0 | 이번 corpus에는 없음 |
| **CPU PreparedRevealBlur planes** | **15.5405** | **14.9259** | line foreground + line metadata + normalized global metadata |

CPU prepared는 preparation 완료 시점에 직접 합산했다. A8/RGBA/HIGH/PERFORMANCE에서 같다. GPU upload 뒤 영구 retained CPU라고 해석하면 안 된다. pending upload 메시지 수명까지 일부 PixelData가 필요할 수 있지만 Prepared result 자체는 publication 후 해제된다.

새 peak profiler는 만들지 않았다. **준비 중 실제 transient logical peak는 측정하지 못했다.** 전체/줄 raster·crop 중간 버퍼, 기존 ordinary metadata와 새 normalized metadata의 overlap은 위 prepared 완료값보다 추가 비용이다. texture+prepared를 더한 값도 정확한 peak는 아니다.

### Source / H / V FBO

| Font | Quality | Format | Source MiB | H MiB | V MiB | 합계 MiB |
|---|---|---|---:|---:|---:|---:|
|24|PERFORMANCE|A8|5.4108|0.2257|0.3385|5.9750|
|24|HIGH|A8|0.9018|0.9018|5.4108|7.2144|
|24|PERFORMANCE|RGBA|21.6431|0.9027|1.3541|23.8998|
|24|HIGH|RGBA|3.6072|3.6072|21.6431|28.8574|
|32|PERFORMANCE|A8|3.9109|0.2444|0.2454|4.4007|
|32|HIGH|A8|0.9777|0.9777|3.9109|5.8663|
|32|PERFORMANCE|RGBA|15.6434|0.9777|0.9815|17.6026|
|32|HIGH|RGBA|3.9109|3.9109|15.6434|23.4651|

F24는 **6 pages**이고 page1970×480이다. PERFORMANCE는 Source6장, shared H493×480 한 장, V493×120 여섯 장이다. HIGH는 full Source/H를 각각 한 장씩 공유하고 V 여섯 장을 유지한다.

F32는 **4 pages**, page1964×522이다. PERFORMANCE H491×522 한 장, V491×131 네 장이며 Source는 네 장이다. HIGH는 full Source/H 각각 한 장과 V 네 장이다. sampler 대상인 Source/V를 빼서 메모리를 줄인 것은 아니다.

따라서 여러 page인 FHD에서는 HIGH도 Source/H sharing의 이익을 받는다. PERFORMANCE의 reduced H/V가 그대로 큰 전체 texture 절감률이 되지 않는다. 두 mode에 공통인 full metadata와 line input textures도 상당한 비중이다.

구조는 F24 Actor51/Renderer50/Geometry19/offscreen tasks18, F32 Actor35/Renderer36/Geometry13/tasks12이다. Actor에는 Label/camera/companion 등이 포함된다. unique textures는 quality/format에 따라54~78개이며, 상세개수와 각 texture dimensions는 [TABLES.md](TABLES.md)와 `TEXTURE`/`TASK` log에 있다.

## F. Process memory / 수명 관측

CPU-only 3개 독립 process의 평균 RSS, 단위MiB:

| Font | Quality | Format | Before | Ordinary | Published | Steady | Destroy+settle | Warm published | Final destroy+settle |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
|24|PERFORMANCE|A8|109.9|130.0|178.5|175.0|167.3|201.5|190.4|
|24|PERFORMANCE|RGBA|110.0|130.2|174.9|174.8|167.1|192.0|200.9|
|24|HIGH|A8|109.9|130.0|172.2|175.3|167.6|195.5|180.0|
|24|HIGH|RGBA|109.9|130.1|176.6|173.2|165.5|194.7|191.8|
|32|PERFORMANCE|A8|109.9|129.7|175.2|174.8|167.1|188.2|181.5|
|32|PERFORMANCE|RGBA|109.9|130.0|169.4|174.6|166.9|192.9|186.4|
|32|HIGH|A8|109.9|129.8|163.3|172.7|165.0|191.0|181.7|
|32|HIGH|RGBA|109.8|129.9|167.1|173.8|166.0|189.4|193.5|

RSS에는 allocator/font/shader/driver/cache가 포함된다. logical texture가 큰 RGBA가 반드시 그 차이만큼 process RSS를 더 쓰는 것도 아니다. final RSS가 baseline으로 돌아오지 않았다고 바로 leak으로 판정하지 않는다. 위 값은 checkpoint이고 continuous peak가 아니다. 전체 checkpoint 중 최대 absolute RSS는204.96MiB였다(다른 row의 mean과 혼동하지 않는다).

`mallinfo2`는 before 약11.3MiB, steady37.3~63.7MiB, 첫 destroy 뒤14.1~14.3MiB, 최종 destroy 뒤14.3~14.5MiB로 관측됐다. 모든 CPU/GPU performance process에서 **첫 destroy 및 warm 재생성 뒤 최종 destroy 모두 추가 RenderTask0**이었다. 결과는 자원 회수를 지지하지만 이번 두 회전만으로 장시간 leak-free를 새로 증명한 것은 아니다.

## G. Setup 비용

CPU-only process의 3회 평균 wall time, ms:

| Font | Quality | Format | ordinary first layout | existing ordinary→first blur publication | warm recreation/layout→blur publication |
|---|---|---|---:|---:|---:|
|24|PERFORMANCE|A8|50.1|141.6|155.6|
|24|PERFORMANCE|RGBA|50.2|143.6|159.2|
|24|HIGH|A8|50.4|142.0|154.6|
|24|HIGH|RGBA|50.9|143.2|158.0|
|32|PERFORMANCE|A8|37.0|119.2|126.4|
|32|PERFORMANCE|RGBA|37.5|122.5|129.1|
|32|HIGH|A8|36.8|118.2|124.8|
|32|HIGH|RGBA|36.9|121.6|127.7|

ordinary 열은 Label 생성 시작→최초 LayoutFinished callback이다. first blur 열은 ordinary layout이 끝난 Label의 Reveal 설정 시작→native companion/task를 event loop에서 관측한 시점이다. warm 열은 새 Label을 scene에 추가한 직후 Reveal 설정 시작→같은 publication 확인이며, 이때는 ordinary layout도 아직 필요한 상태이다. 의도적인 settle 시간은 제외했다.

따라서 warm이 조금 길다는 것을 cache 회귀로 해석하지 않는다. Label::New 단독 시간이나 GPU 최초 draw 완료를 fence로 측정한 값도 아니다. 16ms timer의 관측 단위와 계측 로그 비용이 포함된다. shader compiler의 cold/warm은 엄밀하게 분리하지 않았다.

## H. PERFORMANCE vs HIGH

동일 font/format의 3-run 평균 비교:

| Font | Format | GPU draw time 감소 | FBO payload 감소 | 전체 texture 감소 | PERFORMANCE의 Source GPU 비중 |
|---|---|---:|---:|---:|---:|
|24|A8|33.81%|17.18%|5.02%|40.48%|
|24|RGBA|30.13%|17.18%|10.70%|42.24%|
|32|A8|39.43%|24.98%|6.45%|40.50%|
|32|RGBA|37.83%|24.98%|14.54%|42.90%|

PERFORMANCE의 의미는 FHD에서도 명확하게 확인된다. 주된 GPU 절감은 H/V이며, Source와 Output 비용은 크게 남는다. CPU prepared bytes는 동일하다. process CPU 차이는 noise/range가 겹치는 경우가 많으므로 이번 결과로 일정한 CPU 향상률을 제시하지 않는다.

## I. Admission 대비 실제 비용 / 과대 추정 항목

| Font | Quality | Format | estimate / actual total texture | estimate / (texture + CPU prepared) |
|---|---|---|---:|---:|
|24|PERFORMANCE|A8|8.13×|4.89×|
|24|PERFORMANCE|RGBA|4.61×|3.35×|
|24|HIGH|A8|7.72×|4.74×|
|24|HIGH|RGBA|4.11×|3.08×|
|32|PERFORMANCE|A8|8.24×|4.84×|
|32|PERFORMANCE|RGBA|5.09×|3.55×|
|32|HIGH|A8|7.71×|4.66×|
|32|HIGH|RGBA|4.35×|3.17×|

두 번째 비율은 transient를 완벽히 더한 peak가 아니라 비교를 위한 단순 합이다. 첫 번째 비율도 estimator가 CPU/transient allowance를 포함하는 반면 denominator는 texture만이므로 그 자체가 estimator의 수학적 오류를 의미하지는 않는다.

이번 actual metrics로 계산 항목을 한 번 분해하면 다음과 같다:

| 기존 cheap-model 항목 | F24 MiB | F32 MiB |
|---|---:|---:|
| full-raster 12 bytes/pixel allowance | 23.16 | 23.25 |
| line CPU allowance | 50.41 | 49.53 |
| halo-expanded runtime bands × 15 bytes/pixel | 117.03 | 102.54 |
| 전체 | **190.59** | **175.31** |

큰 차이가 나는 이유:

1. line band가 `2×(ascender+abs(descender))+6`이다. F24는74px, F32는98px의 full-width band를 쓰지만 실제 cropped input은 훨씬 낮다. `controlHeight/rasterHeight` scaling 여유도 더한다.
2. line CPU는12 bytes/pixel을 가정한다. 이번 실제 line planes는 **L8 foreground + RGBA metadata =5 bytes/pixel**, mask0이며 crop도 적용되어 있다. 실제 line CPU 합계는7.82/7.18MiB이다.
3. runtime allowance는 모든 line마다 **3×RGBA +25% page 여유**를 합산한다. A8, PERFORMANCE quarter H/V, HIGH Source/H sharing 모두 계산에서 차감하지 않는다. 실제 FBO4.40~28.86MiB와 allowance102.54~117.03MiB의 차이가 가장 크다.

이전의1500×850 UTC는native FHD와 line metrics/line count가 달랐다. 그 fixture의67.7MiB 통과 또는FHD로 단순 대입한113.4MiB만으로 실제font24/32 full-screen paragraph가 허용된다고 판단할 수 없었다. 이번 측정으로 그 차이를 확인했다.

**Verdict: ESTIMATOR TOO CONSERVATIVE FOR NORMAL FHD.** 요청의 CASE B에 가장 가깝다. 특히 ordinary Label 대비 추가RSS가약43~45MiB이고 A8 전체texture가21~25MiB인 상황에서 동일수식으로175~190MiB를 요구한다. 다만 full-screen blur가 공짜라는 뜻은 아니며, target의 transient peak/여러 Label aggregate 비용은 여전히 별도이다.

128MiB를 즉시256MiB로 올리지 않았다. 다음 policy 변경이 필요하다면 위의 중복된 보수 가정 중 무엇을 줄일지 별도 결정해야 한다. 이 보고서에서 새로운 estimator나 exact memory accounting 체계를 설계하지 않는다.

## J. Next optimization

**A8 text-only Source input atlas batching PoC priority: HIGH.**

- PERFORMANCE Source가 전체 GPU draw 시간의 약40~43%이다.
- F24는31 Source draws/Label, F32는23이다. 여러 page라서 가능한 1차 목표는 각각 **31→6**, **23→4**이지 무조건1 draw가 아니다.
- 이 경우 Source/H/V/Output 전체 draw는49→24,35→16이 될 여지가 있다. task18/12 및 Gaussian 단계는 그대로이다.
- Source fragment 작업은 남으므로 Source GPU 전체0.21~0.25ms(A8)를 모두 없애는 개선은 아니다. atlas 생성 CPU/임시 메모리와 실제 submission 절감을 비교해야 한다.

이번에는 Source batching, quality/phase/UV/kernel 변경, radius40 matrix, production policy 수정, full UTC/regression/sanitizer를 하지 않았다.

## K. Artifacts / worktree

- [전체 수치표](TABLES.md), TSV (로컬 자료: `summary.tsv`), 3회 raw/평균/범위 JSON (로컬 자료: `summary.json`).
- Harness (로컬 자료: `probe.cpp`), 측정용 preparation 복사본 (로컬 자료: `preparation-measure.cpp`), build (로컬 자료: `build.sh`), matrix runner (로컬 자료: `run-matrix.sh`), 실행 완료 목록 (로컬 자료: `matrix-status.log`).
- p0.5 캡처 예: [F24 PERFORMANCE A8](capture/24-performance-a8-Output-0.png), [F32 HIGH gradient](capture/32-high-rgba-Output-0.png). 8개 case의 raw1920×1080 및 축소 preview는 `capture/`에 있다.
- production 소스, HEAD, index, 기존 unstaged diff는 변경하지 않았다. 모든 새 파일은 repository 밖의 이 측정 디렉터리에 있다.
- 원래128MiB guard는 그대로이며 계측 library를 설치하지 않았다. commit/amend/rebase 없음.
