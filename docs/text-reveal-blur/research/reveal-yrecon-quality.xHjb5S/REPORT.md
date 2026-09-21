# Y-only follow-up: vertical reconstruction → fused Y/2

## 결론

**NEITHER CANDIDATE REMOVES BANDING — STOP PREFILTER OPTIMIZATION**

요청한 순서로 A-R부터 확인하고, 품질 FAIL 뒤에만 Y/2를 구현했다.

- **A-R:** strong blur의 가로 band는 완화되지만 p=.75 한글 획이 A보다 더 흐려진다.
- **Y/2:** p=.50/.75는 CURRENT에 상당히 가까워지지만 p=.20의 가로 band가 A보다도 뚜렷하다.
- 두 후보 모두 이번 Korean24 품질 조건을 통과하지 못했다.
- Korean32/Gradient/ImageSpan 확대, 계수·delta sweep, 추가 필터, 성능 측정은 하지 않았다.
- 이 결론은 **이 두 후보의 Go/No-Go**다. 모든 prefilter 접근이 불가능하다는 증명은 아니다.

## A. Baseline / 보호 범위

- UI HEAD: `5aae0d2b389bb9811bed6e21b754cee629f4ecfc`
- branch: `devel_blur_text`
- 시작/종료 모두 UI working tree clean. 전체 기록: BASELINE.txt (로컬 자료: `BASELINE.txt`)
- 기존 adaptor의 `gles-texture-dependency-checker.cpp` +13줄 변경은 그대로 보존했다.
- 기존 `reveal-yfirst-quality.K64mLI/`, `reveal-prefilter-quality.eYgUhH/`는 수정/덮어쓰기하지 않았다.
- CURRENT/A 정적 이미지와 중간 stage는 기존 결과를 재사용했다. 새 비교 앱의 CURRENT/A 코드 경로도 그대로다.
- 변경/빌드는 이 외부 진단 디렉터리의 viewer와 capture hook에만 한정했다.

실행 환경: Ubuntu / GTX 1650 / NVIDIA 595.91.07 / GLES, MSAA 4.
Korean24, FHD Label 1920×1080, Source의 radius halo 포함 2020×1180.
WHOLE_TEXT, radius48, Fade0, Unit::LINE, Stagger=.25, BlurDuration=1.
기존 한국어 corpus/글꼴 설정/검정 배경/흰 text를 그대로 사용했다.
8초 linear 0→1→0; 양쪽 Label은 같은 Animation으로 구동한다.
분할 화면은 FHD Label의 왼쪽 960px씩을 **원래 픽셀 크기로 clip**한다. 글꼴 크기나 layout을 축소하지 않는다.

| 경로 | Source | prefilter | H | V |
|---|---:|---:|---:|---:|
| CURRENT | 2020×1180 | 없음 | 505×1180 | 505×295 |
| A | 2020×1180 | Y-only 2020×295 | 505×295 | 505×295 |
| A-R | A와 동일 | A와 동일 | A와 동일 | A와 동일 |

## B. Phase 1 — A-R의 정확한 변경

기존 A installer를 그대로 호출한 다음, Output shader의 **blurred V read 한 곳만** 바꿨다.

```glsl
// Before
blurred = texture(V, uv);

// A-R
blurred = 0.5 * (
  texture(V, uv - vec2(0.0, deltaY)) +
  texture(V, uv + vec2(0.0, deltaY)));
```

- `deltaY = 0.5 / VTextureHeight = 0.5 / 295 = 0.001694915...`
- Sharp Source read, Late Smooth 식, Reveal timing/progress/fade는 변경하지 않았다.
- A의 H/V shader·FBO·sampling도 그대로다.
- 이전 B1의 **수평** reconstruction 후보 C와 다르다. 이번 것은 A의 **수직** reconstruction이다.
- delta 후보는 이것 하나만 사용했다.

| p | A vs CURRENT | A-R vs CURRENT | 판단 |
|---|---|---|---|
| .20 | 약한 가로 band | band가 눈에 띄게 완화됨 | strong blur에서는 개선 |
| .50 | 약한 추가 softness | A보다 Y 방향 smoothing이 더해짐 | 선명도 trade-off |
| .75 | CURRENT보다 약간 soft | 획/내부 공간이 A보다 분명히 더 뭉개짐 | **FAIL의 주된 이유** |
| .90 | CURRENT와 동일 | CURRENT와 동일 | sharp handoff 정상 |

대표: [p=.20](comparisons/korean24-p0.20-current-yonly-yrecon.png),
[p=.75](comparisons/korean24-p0.75-current-yonly-yrecon.png).
3× nearest 확대이며 색/명암 보정을 하지 않았다.
1:1 비교 파일도 같은 comparisons 폴더에 있다.

## C. A-R slow animation

slow-comparison.mkv (로컬 자료: `slow-comparison.mkv`)의 **0–8초**가 CURRENT | A-R이다.
[A-R contact sheet](comparisons/slow-contact.png)는 이 구간의 몇 프레임을 1:1 crop한 것이다.

- A의 강한 horizontal band는 완화되지만 전 구간이 CURRENT와 같아지는 것은 아니다.
- 중간 구간의 추가 softness가 정적 p=.75 결과와 일치한다.
- 이 짧은 확인에서는 뚜렷한 halo 외곽 확대나 별도의 이중 윤곽 증가는 관찰하지 못했다.
- p=.90의 native screenshot은 CURRENT와 pixel-identical했다.
- contact sheet만으로 모든 순간의 미세 shimmer/pop이 없다고 보증하지는 않는다.

## D. Phase 1 verdict

**A-R FAIL.** band 완화 효과 자체는 있지만, 요청한
“p=.75 stroke contrast가 A와 거의 동일” 조건을 통과하지 못한다.
이 판단을 PHASE1.txt (로컬 자료: `PHASE1.txt`)에 기록한 뒤 Phase 2를 진행했다.
추가 reconstruction tuning은 하지 않았다.

## E. Phase 2 — fused Y/2

```text
Source 2020×1180
   │ production H Gaussian 그대로, Y 2× reduction
   ▼
H 505×590
   │ half-resolution V Gaussian, Y 2× reduction
   ▼
V 505×295
   │ 기존 Output / Sharp Source / Late Smooth 그대로
   ▼
화면
```

별도 prefilter FBO/Actor/Task는 없다. Source/H/V의 **3 offscreen stages**를 유지한다.

H:

- 기존 shader handle, radius48 kernel, X offset, texture binding, geometry, constraint를 보존했다.
- H framebuffer 높이만 1180→590으로 변경했다.
- 이 fixture의 높이는 정확히 짝수다. 출력 중심 Y는 source row 두 개의 중간에 놓이고,
  LINEAR sampling 한 번으로 두 행의 동일 가중치 평균이 된다.
- H fetch 수는 fragment당 그대로 **48**이다. 추가 Y tap은 없다.
- 실제 캡처한 Y/2 H를 CURRENT H의 두 행 평균과 비교하면 최대 차이 **0.5/255**,
  RMSE **0.04765/255**다. 의도한 두 행 평균이 적용됐음을 확인했다.
- 홀수 크기/generalization은 이번 범위가 아니며, 진단 코드는 정확한 2× 조건을 검사한다.

V:

- production Gaussian constants 계산식을 그대로 재사용했다.
- 원본 positive pair count `48/2 = 24`.
- half-space pair count는 실제 축소율로 `ceil(24 / 2) = 12`를 계산했다.
  임의의 radius24 상수를 고정한 것이 아니다.
- 실제 σ: 원본 **15.10251141**, half kernel **7.37248039**.
- 2-row average의 display-space variance **0.25**를 반영했다:

```text
halfStrength =
  sqrt(max(0, (15.10251141 × productionStrength)² − 0.25))
  / (7.37248039 × 2)
```

- V offset은 half texture의 `1/590` 기준이다.
- positive/negative **12 pairs = 24 texture reads/fragment**.
- V의 output 295 높이에서 평가하므로 추가 reduction도 같은 pass 안에서 수행된다.
- 이론 σ 환산이며 시각적 보정/sweep은 없다. σ가 같아도 이산 샘플링 결과까지
  CURRENT와 동일해지는 것은 아니며, 아래 strong-blur 결과가 그 한계를 보인다.
- sharp read, output reconstruction, Late Smooth, animation/fade는 변경하지 않았다.

## F. Phase 2 quality

| p | A vs CURRENT | Y/2 vs CURRENT | 판단 |
|---|---|---|---|
| .20 | 약한 가로 band | **더 뚜렷한 가로 band** | **FAIL** |
| .50 | 약간 다른 blur profile | 매우 가까움 | 이 구간은 개선 |
| .75 | 추가 softness | 획이 CURRENT에 훨씬 가까움 | 이 구간은 개선 |
| .90 | 동일 | 동일 | sharp handoff 정상 |

대표: [p=.20](comparisons/korean24-p0.20-current-yonly-yhalf.png),
[p=.75](comparisons/korean24-p0.75-current-yonly-yhalf.png).

slow-comparison.mkv (로컬 자료: `slow-comparison.mkv`)의 **8–16초**가 CURRENT | Y/2이다.
[Y/2 contact sheet](comparisons/slow-yhalf-contact.png)에서도 strong-blur 구간의
가로 modulation이 확인된다. late 구간에서는 선명도가 더 잘 보존된다.

Phase 2에서만 p=.20의 H/V 중간 stage를 새로 캡처했다:
[stage comparison](comparisons/stages-p0.20.png).

- H는 수직 Gaussian을 적용하기 전이므로 글자 획의 수평 stripe 자체가 있는 것이 정상이다.
  그 stripe를 곧바로 최종 artifact로 간주하지 않았다.
- H의 2-row average는 위의 pixel check와 일치한다.
- **V output에도** CURRENT보다 강한 가로 modulation이 남는다.
  따라서 Output 확대나 Late Smooth만이 새 band를 만드는 것은 아니다.
- 원인 범위는 half-resolution H를 입력으로 하는 이산 V filtering/reduction까지 좁혀진다.
  이 캡처만으로 aliasing·kernel spacing·quantization의 기여를 각각 확정하지는 않는다.

보조 pixel 차이 (ROI 10,10–1320,730; 8-bit RGB RMSE / max):

| p | A − CURRENT | A-R − CURRENT | Y/2 − CURRENT |
|---|---:|---:|---:|
| .20 | .2096 / 4 | .1949 / 4 | .4556 / 8 |
| .50 | .3443 / 4 | .4868 / 6 | .1180 / 1 |
| .75 | 1.6092 / 23 | 2.3802 / 24 | .3877 / 5 |
| .90 | 0 / 0 | 0 / 0 | 0 / 0 |

이 값은 background를 포함한 참고 수치이지 band의 단독 품질 점수는 아니다.
최종 판단은 확대/1:1 및 느린 애니메이션 비교의 artifact를 기준으로 했다.

## G. Theoretical filtering work — 실제 GPU 측정 아님

`Q = 505×295 = 148,975`.
텍스처 명령 수 추산이며 fetch latency, bandwidth, cache, CPU, FPS와 동일하지 않다.
Source raster, Output, clear, submission 비용은 아래 filtering 합계에서 제외했다.

| 경로 | prefilter | H | V | filtering 합계 | CURRENT 대비 |
|---|---:|---:|---:|---:|---:|
| CURRENT | 0 | 28,603,200 | 7,150,800 | **35,754,000** | 기준 |
| A | 1,787,700 | 7,150,800 | 1,787,700 | **10,726,200** | **−70%** |
| A-R | A와 동일 | A와 동일 | A와 동일 | **10,726,200** | **−70%** |
| Y/2 | 0 | 14,301,600 | 3,575,400 | **17,877,000** | **−50%** |

A-R은 별도로 **Output의 blurred V fetch가 1→2**다.
변경한 expression을 수행하는 출력 fragment마다 **+1 read**이고,
2020×1180 전체 quad를 clip 없이 그린다는 가정에서는 **+2,383,600**이다.
실제 분할 viewer/화면 clipping에서는 그보다 작다.
따라서 A-R의 **전체 GPU 비용도 −70%라는 뜻은 아니다.**

## H. Logical FBO payload — RSS/VRAM 실측 아님

| 경로 | 논리 pixel 합계 | A8 MiB | RGBA8 MiB | CURRENT 대비 | offscreen stages |
|---|---:|---:|---:|---:|---:|
| CURRENT: S+5Q | 3,128,475 | 2.9835 | 11.9342 | 기준 | 3 |
| A / A-R: S+6Q | 3,277,450 | 3.1256 | 12.5025 | +4.76% | 4 |
| Y/2: S+3Q | 2,830,525 | 2.6994 | 10.7976 | **−9.52%** | 3 |

S는 Source 2020×1180이다. RGBA 열은 같은 크기/구성일 때의 bytes 계산이며,
RGBA 품질 fixture를 새로 실행한 결과는 아니다.

driver allocation/alignment, MSAA window, original text/metadata textures,
CPU buffers 등은 포함하지 않았다. 외부 installer는 production 내부가 보유한
교체 전 H handle을 제거하지 않으므로 진단 앱의 실제 live allocation/peak와도 다르다.
**실제 메모리를 측정했다고 해석하면 안 된다.**

## I. 최종 verdict / 재현

**NEITHER CANDIDATE REMOVES BANDING — STOP PREFILTER OPTIMIZATION**

A-R은 band만 보면 개선되지만 선명도 조건에서 실패했고,
Y/2는 late quality가 좋아졌지만 strong band 조건에서 실패했다.
이번 stop rule에 따라 여기서 멈춘다. 품질 PASS 후보가 없으므로
실제 GPU 성능 비교로 넘어가지 않는다.

```bash
# CURRENT | 기존 A
bash /home/bowonryuubuntu/tizen/reveal-yrecon-quality.xHjb5S/run-current-a.sh

# CURRENT | A-R (Output Y ±0.5 low texel)
bash /home/bowonryuubuntu/tizen/reveal-yrecon-quality.xHjb5S/run-current-ar.sh

# CURRENT | Y/2
bash /home/bowonryuubuntu/tizen/reveal-yrecon-quality.xHjb5S/run-current-yhalf.sh
```

- Q/W/E/R: p=.20/.50/.75/.90 정지.
- Space: 8초 왕복 애니메이션.
- 7: CURRENT|A, 8: CURRENT|A-R, 9: A|A-R, 0: CURRENT|Y/2.
- 1 HIGH / 2 CURRENT / 3 B1 / 4 A / 5 A-R / 6 Y/2 단독 화면.
- Esc: 종료.
- C의 추가 corpus 메뉴는 기존 viewer에 남아 있지만 이번에는 실행/검증하지 않았다.

새 정적 캡처는 A-R/Y/2의 네 progress씩 **8장**만 저장했다.
기존 CURRENT/A 이미지는 재사용했고, 비교용 crop/contact sheet는 이 자료에서 생성했다.
8초 비교 두 구간은 **영상 한 개**에 연결했다.

## J. Working tree / 미수행 항목

- production source unchanged.
- UI worktree clean; HEAD와 branch unchanged.
- 기존 adaptor의 사용자 변경 preserved.
- commit / amend / push / reset / restore / stash 없음.
- production build, UTC, sanitizer 없음.
- GPU timer, CPU/FPS/target benchmark, RSS/VRAM 측정 없음.
- PER_LINE/generalization, 다른 radius/delta/filter tuning 없음.
- 새 코드: 외부 diagnostic candidates.h (로컬 자료: `candidates.h`), viewer.cpp (로컬 자료: `viewer.cpp`).
- 계산/구성 검증: sanity-and-theory.json (로컬 자료: `sanity-and-theory.json`), stage-dimensions.json (로컬 자료: `stage-dimensions.json`).
