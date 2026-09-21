# Reveal Blur: memory admission / PERFORMANCE quality review

대상: `devel_blur_text`, HEAD `c7f2303e` (2026-09-14).

## 결론

- **MEMORY GUARD CLEANUP READY**: threshold 256 MiB는 유지하고, PER_LINE의 `줄 수 × 전체 Label 면적` 중복 추정만 정리했다. 정상 fixture의 estimate는 **433.428 → 67.745 MiB**, sync/async × HIGH/PERFORMANCE 모두 publication 성공.
- **PERFORMANCE QUALITY: NO SAFE ZERO-COST FIX FOUND**: 아래 후보에서는 모든 경우에 일관된 개선을 찾지 못했다. 품질 관련 production 수정은 하지 않았다. 이는 더 나은 필터가 불가능하다는 결론은 아니다.
- production 변경은 memory admission 파일 1개, UTC 변경은 전용 테스트 2개다. 모두 **unstaged**. commit/index/기존 adaptor 변경은 건드리지 않았다.

## PART I — Memory Guard

### A. Previous guard

기존 계산은 PER_LINE의 각 sequence가 full raster와 full blur target을 각각 소유한다고 가정했다.

```
old = N × (12 × fullRasterPixels + 12 × haloExpandedControlPixels)
      + (decorations ? 4 × haloExpandedControlPixels : 0)
```

WHOLE_TEXT는 N=1. 12 bytes/pixel은 nominal CPU planes 및 3개의 RGBA target을 넉넉하게 잡는 기존 조합이다. **정확한 GPU peak/CPU peak를 따로 재는 quota가 아니다.**

위치: `PrepareRevealBlur()`에서 final sequences/visible replacement placements를 선택한 뒤, 줄별 bitmap bounds, foreground/mask/metadata raster, crop 및 normalized metadata 생성 **이전**이다. 일반 text/metadata 버퍼는 호출 시점에 이미 존재할 수 있다. runtime upload와 FBO 생성은 complete publication validation 이후 event-side 작업이다.

### B. Problem

현재는 line crop, page packing, scratch sharing, PERFORMANCE reduced H/V를 사용한다. 줄마다 전체 높이를 곱하면 정상 paragraph도 쉽게 거부된다.

sync에서도 previous/current/candidate가 겹칠 수 있다. async는 worker의 ordinary/prepared payload와 이전 publication, 여러 in-flight 요청이 추가로 겹칠 수 있다. 기존 guard도 전체 Label 수/worker queue 동시성/driver allocation까지 보장하지 않았다. 이번 변경 역시 그 역할을 확대하지 않는다.

### C. New cheap estimate

O(1) `전체 면적 + N×halo` 후보는 compressed spacing/큰 font overlap을 놓치므로 사용하지 않았다. 반대로 quality/format별 packing 재현은 불필요한 비용과 결합을 만든다. **기존 line metrics로 generous full-width band를 합산**하는 방법을 선택했다.

```
RW, RH = ceil(raster width/height)
halo = 2 × (radius + 2)
TW, TH = ceil(control width/height) + halo
scaleY = max(1, controlHeight / RH)

band_i = min(RH, 2 × ceil(abs(ascender_i) + abs(descender_i)) + 6)

estimate = 12 × RW × RH                         // full-raster 여유분 1회
         + Σ(text-bearing i) 12 × RW × band_i  // retained line planes
         + Σ(i) 15 × TW × min(TH, band_i × scaleY + halo + 6)
         + 15 × visible-image target-band pixels
         + (decorations ? 4 × TW × TH : 0)
```

15는 `3 × RGBA 4 bytes × page 여유 1.25`. A8, quarter, shared scratch 절감은 차감하지 않는 공통 보수 추정이다. 이미지 bounds는 **이미 수행하던 placement visit에서** 합산하며 text와의 overlap은 보수적으로 중복 계산한다. 새 index/추가 traversal은 없다.

WHOLE_TEXT는 기존 `12 × raster + 12 × target + decorations` 형태를 유지한다. 면적 계산 시 ceil을 사용한다.

모든 면적 연산은 bounded dimension 검사 이후 double로 수행한다. `radius + 2`를 double 변환 **후** 더해 uint32 wrap도 막는다. non-finite metrics/estimate는 거부한다.

주의: font metrics의 2배 band는 generous allowance이지 임의 font bitmap의 수학적 상한은 아니다. one-full-raster allowance도 concurrent requests 전체의 상한이 아니다. 실제 bitmap/crop/max texture/page/allocation failure guards는 그대로 남는다. threshold의 정책적 변경은 하지 않았다.

### D. Computational cost

- incremental O(visible final sequences), 초과 시 early exit.
- estimator 전용 allocation 0, bitmap traversal 0, coverage scan 0, raster 0, packing simulation 0, per-frame 비용 0.
- 기존 sequence/placement 준비 자체의 allocation까지 없다고 주장하는 것은 아니다.

### E. Normal case acceptance

1500×850 Label, actual raster 1500×750, visible lines 15, font32/line height50, PER_LINE, radius24.

| 조건 | 기존 estimate | 새 estimate | 결과 |
|---|---:|---:|---|
| Sync HIGH | 433.428 MiB | 67.745 MiB | reject → blur publish |
| Sync PERFORMANCE | 433.428 MiB | 67.745 MiB | reject → blur publish |
| Async HIGH | 433.428 MiB | 67.745 MiB | reject → blur publish |
| Async PERFORMANCE | 433.428 MiB | 67.745 MiB | reject → blur publish |

이 수치는 **guard estimate**이며 실제 메모리 사용량 측정치가 아니다. old production + 새 UTC에서 실패를 먼저 재현했다. 기존 6-line case는 기존 focused suite로 유지 확인했다.

### F. Extreme case rejection

실제 작은 model로 다수의 final lines를 만들고, 2048²/radius64의 dimension-valid but over-budget preparation 요청을 검증했다. 5120²(large max texture), huge/nonfinite/negative size, UINT32_MAX radius도 거부했다.

debugger function breakpoints로 전용 rejection UTC의 expensive 작업 진입을 확인했다:

```
REJECTION_EXPENSIVE_CALLS bounds=0 lineRaster=0 metadataRaster=0
```

테스트 fixture를 만드는 기존 ordinary raster는 이 카운트의 대상이 아니다. pathological 크기의 실제 texture/bitmap을 만들어 시험한 것도 아니다.

### G. Tests

- 새 UTC: `UtcDaliTextRevealBlurMemoryAdmissionP`, `UtcDaliTextRevealBlurMemoryRejectionP`.
- final focused runtime suite 81/81 + async/publication/failure targeted 5/5 = **86/86**.
- foundation/components build 및 UTC build 성공.
- `git diff --check` 성공.
- 전체 3000+ regression/sanitizer는 요청대로 실행하지 않았다.

로그: final UTC summary (로컬 자료: `phase1-final-summary.tsv`), build (로컬 자료: `phase1-final-build.log`), old failure (로컬 자료: `admission-before.log`), acceptance trace (로컬 자료: `admission-trace.log`), rejection trace (로컬 자료: `rejection-final-trace.log`), [설계 검토](PHASE1-DESIGN.md).

### H. Verdict

**MEMORY GUARD CLEANUP READY.** cheap admission policy를 현실화한 것이며, pixel/timing/ownership/API/quality pipeline을 변경하지 않았다.

## PART II — PERFORMANCE Quality

### A. Existing GaussianBlurEffect architecture

현재 branch의 구현을 기준으로 확인했다. 과거 구현/일반적인 Gaussian이라는 이름만으로 추정하지 않았다.

**고정 blur, 기본 downscale .25:**

```
owner → Source (floor(W/2), floor(H/2))
      → linear downsample (floor(W/4), floor(H/4))
      → H quarter → V quarter → linear output
```

Source를 최소 .5에서 생성하고, .25로 한 번 더 줄인다. source/H/V 외 downsample FBO/task가 하나 더 있다. `gaussian-blur-effect-impl.cpp:416`, `:610`, `:679` 참조.

**strength 애니메이션:** `AddBlurStrengthAnimation()`은 `ApplyInternalDownscaleFactor(1.0f)`를 호출한다. 현재 애니메이션 경로는 quarter가 아니다. 끝이 strength1이면 configured scale 복원, strength0이면 bypass한다 (`:287`, `:854`).

동일 uploaded Source의 native 확인, 467×161 기준:

- static: Source233×80, downsample/H/V 각각116×40, 추가 task4.
- active animation: Source/H/V 각각467×161, 추가 task3.
- animation strength .5의 V와 같은 Source의 full kernel replay 차이: max1/255, p99=0.

기존 effect의 animation이 더 자연스러웠던 것을 “동일 비용 quarter의 우수한 복원법”이라고 볼 수 없다.

### B. Reveal PERFORMANCE architecture

```
full-resolution foreground+Reveal Source
  → H (ceil(W/4), H), full-space exact Gaussian offsets
  → V (ceil(W/4), ceil(H/4)), full-space exact Gaussian offsets
  → linear V + retained sharp Source, Late Smooth
```

H는 Y 해상도를 유지한다. V는 아직 full-height인 H에서 세로 convolution을 수행하며 Y를 줄인다. shader tap 수/weight는 HIGH와 동일한 Gaussian factory를 사용한다. output batching/sequence strength/Reveal timing은 기존대로다.

### C. Difference table

W/H는 각 구현의 **실제 target/page 크기**다. 기존 effect의 view target과 Reveal의 halo/cropped page 크기는 일반적으로 다르므로 같은 Label만 만들고 직접 bytes를 비교하면 입력 범위부터 달라진다.

| 항목 | Existing effect static .25 | Reveal PERFORMANCE |
|---|---|---|
| Source | floor(W/2)×floor(H/2), RGBA | W×H, R8 또는 RGBA |
| Initial downsample | source capture .5 + 별도 linear .25 | 별도 pass 없음 |
| H target | floor(W/4)×floor(H/4) | ceil(W/4)×H |
| V target | H target과 같음 | ceil(W/4)×ceil(H/4) |
| 순서 | 양 축 축소 후 convolution | 해당 축 convolution과 축소를 H→V 순으로 수행 |
| Kernel | 공통 Gaussian, scaled integer radius | 공통 Gaussian, full display-pixel kernel |
| Radius / taps | floor(radius×.25), pairs=radiusDown>>1, 최소4 | 기존 validated even/min4 radius, pairs=radius>>1 |
| r40에서 H 또는 V fetch | 10 | 40 (HIGH와 동일) |
| Offset | offsets×strength/downsampledSize | offsets×strength/full line extent |
| H/V min/mag | DEFAULT→실제 GL_LINEAR | 명시적 LINEAR |
| H/V wrap | MIRRORED_REPEAT | CLAMP_TO_EDGE + tile half-texel clamp |
| UV | full quad 0..1 | line-local UV→normalized page rectangle |
| Pixel center | normalized center mapping | 같은 normalized center mapping |
| Rounding | floor, 최소1 | ceil, 최소1; crop floor/ceil 별도 |
| Output | 1×LINEAR fetch, corner 처리, 기본 dither .1 | V 1×LINEAR + Source 1×LINEAR, Late Smooth |
| Final wrap/filter | default CLAMP / LINEAR | 명시적 CLAMP / LINEAR |
| Premultiply | premultiplied RGBA | RGBA premultiplied / R8 coverage + text color |
| A8/R8 | 없음 | plain white/solid text fast path |
| RGBA | 공통 경로 | gradient/color/image 등 |
| Crop/page | view 전체 effect target | per-line crop + halo + format pages + shared scratch |
| Effective scale | viewEffectiveScale + target scale/RENDERED_SCALE_FACTOR | validated display radius + raster→control crop transform, reduced target 비율 |
| strength animation | **full resolution으로 전환** | reduced 유지 + sharp handoff |

공통 kernel formula라도 scaled radius를 재생성하므로 normalized Gaussian shape가 수학적으로 정확히 같지는 않다. r16의 paired-offset RMS 거리(보간 kernel 폭은 제외)는 Reveal4.737px, 기존 effect3.213px; r40은12.406 vs11.232px. 같은 nominal radius에서 기존 effect가 더 선명한 것이 더 좋은 reconstruction을 뜻하지 않는다.

### D. First stage where grid appears

실제 native capture에서 HIGH/PERFORMANCE의 **Source는 24개 대응 상태 모두 byte-identical**이었다. glyph raster/Reveal Source 자체가 원인은 아니었다.

1. H에서 X를 낮추면서 샘플 lattice 차이가 시작된다. 복원한 H는 이미 X 방향 detail/slope 차이를 가진다. H에 원래 남아 있는 글자의 가로 획(Y detail)을 “grid bug”라고 판정하지 않았다.
2. V에서 Y도 낮춰 2D coarse lattice가 된다. filter와 decimation의 fractional phase/paired-linear sampling 차이도 여기 포함된다.
3. 최종 bilinear 확대가 coarse lattice를 piecewise bilinear surface로 보여준다. 낮아지는 effective radius에서 작은 획이 충분히 low-pass되지 않으면 cell 형태가 더 보인다.
4. Late Smooth는 lattice를 만들지 않는다. r40×strength.5=20px처럼 **sharp mix가 0인 구간에서도 차이가 있다**. handoff 구간은 sharp mix가 error를 줄이거나 별도의 HIGH 차이를 더하므로 따로 해석했다.

원본이 같은 6-line Korean, 숫자는 0..255 coverage 값, H/V는 full page로 LINEAR 복원해 HIGH와 비교:

| radius / strength | H RMSE | V MAE | V RMSE | V max | V p99 |
|---|---:|---:|---:|---:|---:|
| 16 / .5 | 4.448 | 2.087 | 4.212 | 27 | 16 |
| 24 / .5 | 1.553 | .650 | 1.325 | 10 | 5 |
| 40 / .5 | .474 | .189 | .480 | 4 | 2 |
| 64 / .5 | .195 | .084 | .290 | 2 | 1 |

r24/.5에서 native V lattice의 HIGH sample 대비 RMSE는 .323이다. **HIGH 결과 자체를 quarter로 줄였다 다시 확대해도 RMSE1.372**, 현재 V 복원은1.312(float interpolation 기준)다. 이 상태는 잘못된 V 계산보다 reconstruction 손실의 비중이 크다는 근거다. 모든 radius/strength에 같은 비율이라고 단정하지 않는다.

### E. Texel-center / phase findings

S개 원본 texel, L=ceil(S/4)인 target에서 j번째 center는 다음이다.

```
normalized u_j = (j + .5) / L
source texel-index coordinate = (j + .5) × S/L - .5

출력 x번째 center에서 low texture 좌표:
lowIndex(x) = (x + .5) × L/S - .5
```

둘은 서로 역인 normalized mapping이다. S가4의 배수일 때만 첫 식이 `4j+1.5`다. odd S에 무조건 `4j+1.5`를 강요하면 오히려 다른 sample footprint가 된다.

H offset은 `offset×strength/sourceWidth`, V는 `offset×strength/fullHeight`. V 입력의 width가 줄었어도 **Y는 full height**여서 이 계산은 맞다. quarter height로 나누면 반경을4배로 키우는 오류가 된다.

batched geometry에서는 `local uv × rect.zw`가 line inverse extent를 page inverse extent로 바꾼다. `uRevealBatchInvSize`는 입력 texture의 실제 size를 사용하여 tile 경계를 half texel 안쪽으로 clamp한다. output 역시 같은 rectangle을 사용한다.

native Source를 GLES replay에 입력해 같은 formula를 검증한 24상태의 차이는 H max1/255, V max2/255. UBO float/CPU coefficient/8bit 단계 반올림이 포함된 수치다. 큰 반-texel 위치 오차를 나타내는 결과는 없었다.

### F. Crop phase findings

현재 lattice는 **page-local**, full Label에 고정된 4-pixel lattice는 아니다. crop 시작점/packing/ceil 때문에 global 좌표에서 phase와 pitch가 조금씩 바뀐다. 이것은 실제 특성이지만 그 자체로 잘못된 UV mapping은 아니다.

Korean/Latin의 crop origin x=y=0,1,2,3을 독립된 image replay로 만들었다. 첫 low-res center를 공통 위치로 맞추고 output에서 역보정하는 constant-offset 후보도 비교했다. 일부 phase에서 개선, 일부에서 악화했다. 모든 cell의 pitch까지 고정하려면 단순 half-texel 보정이 아니라 crop rect/edge coverage와 packing의 관계까지 바뀐다. 이번 결과만으로 production에 넣지 않았다.

이 시험은 diagonal mod4 phase 4개씩이다. 16개의 x/y Cartesian product나 모든 font/crop 조합을 exhaustive 검증한 것은 아니다.

### G. Sampler findings

Source→H, H→V, V/Source→Output은 실제 GL에서도 min/mag **9729=GL_LINEAR**였다. Reveal wrap33071=CLAMP_TO_EDGE, 기존 effect H/V33648=MIRRORED_REPEAT 확인.

Source text shader의 metadata texture에는9728=NEAREST가 있다. 이는 unit ownership/timing metadata용 기존 계약이며 **blur texture의 reconstruction sampler가 아니다**. 바꾸지 않았다.

기존 effect의 final reconstruction도 한 번의 bilinear fetch다. 재사용할 숨은 bicubic/multi-sample filter는 없다. 기본 dither는 .1이지만 이번 동일-source 비교는0으로 설정했다. noise로 grid를 가리는 변경은 하지 않았다.

### H. A8 vs RGBA

white text의 A8 경로와 **양 끝 stop을 모두 WHITE로 둔 RGBA gradient 경로**를 같은 조건으로 비교했다. Korean/r40, strong/medium/near-handoff 세 상태의 V alpha는 **byte-identical**이었다. canonical Korean/AVATAR replay도 동일했다.

RGBA8888의 alpha도8bit이므로 이것은 “8bit quantization이 전혀 없다”는 증명은 아니다. A8만의 precision 저하/포맷 오류라는 가설을 배제하는 결과다.

별도 float-intermediate 수치 모델에서도 quarter와 full 차이가 남았다. r40/.5 Korean에서 quarter-vs-full RMSE .392, R8-vs-float-quarter RMSE .233(max1.13/255). quantization은 어두운 halo 계단을 일부 더하지만 공간 sampling/reconstruction 차이를 없애지는 못한다. 이 float 모델은 진단용이며 format 변경 후보가 아니다.

### I. Candidates tested

기본 비교는 Korean dense line / `AVATAR ffi office in sunlight`, r16/24/40/64, strength1/.5/effective radius4(near handoff)의24조건. GLES replay에서 현재 구현 포함9종, **216 결과**. native6-line capture에는 WHOLE_TEXT reference, PER_LINE stagger0/.25, 551×303 odd-size layout도 포함했다.

| 후보 | 실행 내용 | 결과 / 채택 여부 |
|---|---|---|
| 1: texel phase | ±.5 source pixel shift, output inverse compensation | +.5는19/24조건 RMSE 감소지만 평균은5.24594로 현재5.24468보다 미세 악화. pattern 위치만 바뀌는 경우 포함. 미채택 |
| 2: crop anchor | origin0..3 첫 center를 기준 lattice로 정렬 | corpus/phase별 혼합 결과. 전역 pitch/edge invariant까지 공짜로 보장하지 않음. 미채택 |
| 3: 기존 effect | 실제 Source .5→.25→H/V 및 final LINEAR 비교 | 더 좋은 same-cost upsampler 없음. static 축소 경로는 현재보다1pass 추가. dynamic public path는full. 미채택 |
| 4: same taps | offsets×.97 / ×1.03, 기존 positive normalized weights 유지 | .97의 평균RMSE5.197로 약.9% 감소하지만11/24에서만 개선. blur 폭 변경, 일부회귀. 미채택 |
| 5: same fetch UV | f'=f+k f(1-f)(2f-1), k=-.5/.5/1 | k=.5 평균5.275, k=1 평균5.345. cell/획 강조가 악화되는 경우. 미채택 |

Candidate3의 strength<1 quarter 결과는 **기존 static kernel을 강제로 그 strength에서 평가한 진단값**이다. 기존 public strength animation의 실제 결과라고 부르지 않는다. static strength1 직접 native readback과 replay의 V 차이는8조건 모두 max1/255 이하였다.

전체 평균에는 의도적인 Late Smooth-vs-HIGH 차이가 큰 handoff 상태도 포함된다. 평균 수치만으로 채택하지 않았다. clear improvement가 없어서 candidate를 product binary에 넣은 GPU benchmark/64·65-boundary/async regression 단계로 올리지 않았다.

### J. Quality comparisons

- [동일 Source: HIGH / PERFORMANCE / 실제 기존 effect, r40 strong](analysis/native-effect-korean-r40.png)
- [r24 medium: HIGH / PERFORMANCE / static-effect-kernel 진단 / UV shaping](analysis/compare-korean-r24-s0.5.png)
- [r40 medium 비교](analysis/compare-korean-r40-s0.5.png)
- [float intermediate와 R8의 차이](analysis/precision-r40.png)

모두 crop을 **4× nearest 확대**, brightness/gamma 증폭 없음. enlargement 자체의 pixel 확대와 원래 저해상도 cell의 차이를 구분해야 한다. source/crop 위치는 script에 기록했다.

기존 effect의 static 결과도 낮은 공간 해상도 특성이 있으며 nominal radius/kernel shape부터 다르다. native animations의 full 품질과 PERFORMANCE를 동일한 quarter 품질이라고 비교하지 않는 것이 중요하다.

### K. GPU / performance

현재 production에 품질 수정을 하지 않았으므로 새 최적화 전/후의 성능향상 수치가 아니다. 아래는 **대표 비용 기준선 확인**이다.

GTX1650 / NVIDIA595.91.07, GLES, 현재 build config, MSAA4. Korean6줄, Label551×303, radius40, PER_LINE/stagger.25, PIXEL, Label8개. 기존 probe의 2초 progress loop, warm setup1회 후 약3초/180~181frames, 각 조합1회. texture upload/clear/swap/compositor는 GPU draw query 합계에 포함되지 않는다. CPU는 전체 process CPU ms/s이며 setup을 제외했다.

| Format / quality | GPU Source | H | V | Output | GPU 합계 ms/frame | CPU ms/s | FBO color payload MiB (8개 합) |
|---|---:|---:|---:|---:|---:|---:|---:|
| A8 HIGH | .240 | .167 | .436 | .122 | .964 | 289.4 | 6.429 |
| A8 PERFORMANCE | .251 | .115 | .113 | .153 | .633 | 296.9 | 2.816 |
| Gradient RGBA HIGH | .317 | .215 | .483 | .133 | 1.148 | 301.5 | 25.717 |
| Gradient RGBA PERFORMANCE | .299 | .130 | .122 | .139 | .690 | 301.2 | 11.264 |

이 단일 trace에서 GPU draw 합계는 PERFORMANCE가 약34%/40% 낮고 FBO payload는56.2% 작다. 통계적 regression 판정/TV 성능 보장은 아니다. GPU query dropped/disjoint 모두0. 각 Label의 Source418×672, H105×672, V105×168. HIGH는3개 모두418×672. runtime task는둘다8×3=24, output8draw/frame이다.

FBO payload는 `width×height×bytesPerPixel` 합, **물리 VRAM/RSS가 아니다**. 전체 retained texture payload는 A8 HIGH13.470/PERFORMANCE9.857MiB, RGBA32.769/18.316MiB다.

후보의 비용 분류:

- phase/crop shift/offset tuning: fetch+0, FBO+0, draw+0, bytes+0. UV offset/상수 산술만 추가 가능.
- UV shaping: output fetch 수2 유지, FBO/draw/bytes 그대로; fract/floor/polynomial ALU 추가. true bicubic이 아니다.
- 기존 effect의 sampling 순서를 그대로 옮기는 것은 downsample pass/FBO가 늘어 **same-cost 후보가 아니다**. 기존 final LINEAR는 이미 사용 중.
- 채택 후보가 없으므로 candidate의 target GPU/frame “무손실”을 주장하지 않는다. 위 timing은 현 production baseline이다.

### L. Recommendation

1. memory guard 변경은 유지할 수 있다.
2. 품질 변경은 이번에 합치지 않는다. 기존 HIGH/PERFORMANCE 계약·timing·sharp handoff·exact tap count를 유지한다.
3. TV에서 특정 phase/strength의 cell이 여전히 UX를 해치면 지금은 HIGH를 선택하는 것이 이미 제공되는 확실한 대응이다.
4. 더 개선하려면 이 report의 crop/strength를 기준으로 quality/cost trade-off를 별도로 승인해 조사해야 한다. 이번 검토가 zero-cost 최적화의 수학적 불가능 증명은 아니지만, 단순 sampler/half-texel 수정으로 해결할 문제라는 근거는 나오지 않았다.

## 상태 / 재현 자료

- native matrix: `native-quality.py`, `native/` (22 app cases, 각3시점).
- 기존 effect 직접 비교: `effect-check.py`, `effect/` (8 static cases + active animation reference).
- GLES same-source replay: `replay-gl.cpp`, `quality-analysis.py`; production sample 변경 없음.
- numerical results: `analysis/native-metrics.json`, `replay-validation.json`, `effect-replay-validation.json`, `candidate-metrics.json`, `crop-phase-metrics.json`, `format-metrics.json`, `grid-study.json`.
- 대표 timing: `phase2-timing.sh`, `timing-*.log`.
- phase2는 full regression/sanitizer 없이 capture/diagnostic/timing만 수행했다.
- 최종 production diff는 memory guard와 UTC뿐. 작업 디렉터리 밖의 자료는 commit 대상이 아니다.
