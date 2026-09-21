# Prefilter grid isolation — Y-only / precision / reconstruction

2026-09-16. **Y-ONLY PREFILTER STILL SHOWS GRID — STOP.**

요약: A는 **B1의 추가 세로 격자를 CURRENT 수준으로 되돌린다.** 그러나 Y축 low-resolution 경로에서 가로 band가 남아, 느린 비교에서는 CURRENT보다 더 선명한 가로 격자가 관찰된다. 따라서 **원인 분리에는 성공했지만, 전체 grid를 제거한 품질 후보에는 도달하지 못했다.** 추가 fixture나 성능 측정으로 진행하지 않았다.

## A. Existing regression / 기준 보존

- Repository HEAD: `5aae0d2b389bb9811bed6e21b754cee629f4ecfc`, `devel_blur_text`.
- 시작 시 dali-ui working tree/index clean. 기존 adaptor의 13-line 수정은 그대로 보존.
- 기존 `reveal-prefilter-quality.eYgUhH`와 `reveal-idle-prefilter.o5LPsX` 파일 수정 없음.
- 기존 viewer를 이 디렉터리에 복사하고 별도 mode를 추가했다. B1은 **원래 `PrefilterPoc::Install(window,48,1)`을 그대로 호출**한다.
- Korean24의 HIGH/CURRENT/B1 × 네 progress에서 이전 캡처와 내용 영역 비교: **각 모드 max diff=0**. 하단 HUD만 제외했다.

B1의 문제는 강한 blur 구간의 촘촘한 세로 band/grid다. 특히 느린 왕복에서 CURRENT보다 잘 드러난다. .75에는 획이 더 soft하고, .90에는 기존 sharp handoff로 CURRENT와 동일하다.

## B. Candidate A — 정확한 변경

```
Source       2020×1180   기존 production
Y area       2020×295    Y축만 phase-correct area, X 1:1
H             505×295    production radius48 H shader/kernel 그대로
V             505×295    B1 low-radius12/calibrated V 그대로
Output       full size  기존 Source/V + Late Smooth 그대로
```

H shader **원본 handle**을 보관했다가 재사용하므로 radius48 coefficient/24 positive pairs/offsets/strength/시작 조건은 동일하다. H `uOffsetDirection`도 원래 값(1/2020,0)으로 복원한다. 입력 높이가 바뀌므로 clamp inverse size의 Y만 1/295로 맞췄다. line/source rectangle은 WHOLE_TEXT 전체 사각형 그대로다. horizontal prefilter/tuning은 없다.

Y prefilter는 B1의 pixel-area overlap 식을 Y축으로만 사용한다. 최대 5개 source texel의 coverage를 인접 pair로 합쳐 **3 linear fetch**로 계산한다. X는 full-width pixel center 그대로 읽는다.

V는 B1에서 사용한 sigmaOriginal=15.10251141, sigmaLow=3.50746632, prefilter variance=1.25, 실제 축소비 1180/295=4를 그대로 사용한다. 같은 Y 축소비이므로 calibration 변경이 필요 없었다. sigma/radius 탐색 없음.

공통 fixture: FHD window/Label, 위치(60,60), font24, 기존 한글 corpus 12줄, radius48, WHOLE_TEXT, Unit::LINE, Fade0, Stagger.25, BlurDuration1. Fade0으로 HEAD의 adaptive recipe를 제외한다. Source/Fade/Reveal timing/Output는 바꾸지 않았다.

## C. Candidate A quality

| Progress | CURRENT vs B1 | CURRENT vs A | A verdict |
|---|---|---|---|
|.20|B1의 촘촘한 세로 grid 증가|추가 세로 grid는 줄었지만 가로 band는 CURRENT보다 보임|부분 개선, 전체 grid 조건 미충족|
|.50|큰 blur 형태는 유사, 미세 질감 차이|큰 형태 유사, 미세 Y방향 질감/밝기 차이|SIMILAR에 가까움|
|.75|B1의 한글 획이 더 soft/낮은 대비|B1보다는 CURRENT에 가깝지만 미세한 Y방향 softening 남음|SLIGHTLY WORSE|
|.90|CURRENT와 동일|CURRENT와 동일|SIMILAR, ROI pixel-identical|

큰 halo 확장이나 새로운 위치 오차/명확한 획 누락은 이 fixture에서 확인하지 못했다. 그러나 **강한 blur의 가로 band는 느린 비교에서 보이므로**, 세로 성분만 개선되었다는 이유로 품질 PASS로 올리지 않았다.

고정 ROI의 RGB RMSE/max (8-bit 값; 작은 오차가 band 부재를 뜻하지 않음):

| Progress | B1 vs CURRENT | A vs CURRENT |
|---|---:|---:|
|.20|0.311 / 5|0.210 / 4|
|.50|0.432 / 5|0.344 / 4|
|.75|2.429 / 24|1.609 / 23|
|.90|0 / 0|0 / 0|

## D. Grid origin — 중간 텍스처

Korean24 **p=.20 하나만** CURRENT/B1/A의 Source/Prefilter/H/V를 native FBO에서 읽었다. timer query나 성능 계측 없이 capture-only hook(`stages.cpp`)을 사용했다. **Source는 세 경로 모두 bit-identical(max diff0)** 이다.

1. **Prefilter:** B1은 X/Y 세부 정보가 모두 quarter grid로 모인다. A는 X정보를 유지하고 Y만 축소된다. 이 시점의 블록 모양 자체는 작은 텍스처를 확대한 것이므로 곧바로 blur artifact로 판정하지 않는다.
2. **H:** B1에서 촘촘한 세로 밝기 modulation이 드러난다. A에서는 CURRENT와 유사한 X방향 연속성이 돌아온다. 따라서 최종 Output에서 처음 생긴 현상이 아니다.
3. **V:** B1의 세로 modulation이 그대로 남고 Y band도 보인다. A는 세로 modulation을 낮추지만, 같은 B1 low-res V를 사용하므로 **가로 band는 남는다.**

보조 column-average의 2차 차분 RMS (동일 source-coordinate ROI, 동일 nearest 표시; 절대적인 grid 점수가 아님):

| Stage | CURRENT | B1 | A |
|---|---:|---:|---:|
|H|0.465|0.701|0.464|
|V|0.450|0.678|0.445|

이는 시각적으로 확인한 **X방향 회복**을 뒷받침할 뿐, A 전체 품질 PASS의 근거로 쓰지 않았다. 가로 band는 `comparisons/slow-contact.png`와 `stages-p0.20.png`를 참조한다.

**핵심 질문에 대한 답:** 이번 결과는 B1의 *horizontal 선축소 + 그 저해상도에서의 Gaussian* 경로가 추가 세로 grid의 주요 기여 요인이라는 판단을 지지한다. 다만 A는 X 입력 해상도뿐 아니라 H kernel/offset의 처리 공간도 CURRENT로 복원하므로, **area downsample만 단독 원인이라고 분리해 단정할 수는 없다.** 이 둘의 추가 분해나 새 필터 연구는 하지 않았다.

원시 파일의 `*-H-505x1180.rgba` 중 B1/A 파일은 외부 installer 적용 전의 **obsolete H**다. 비교에는 해당 두 모드의 실제 `H-505x295.rgba`만 사용했다. 표시 이미지는 각 해상도를 동일 source-coordinate로 펼쳐 보여주며 이 확대를 새로운 reconstruction으로 적용한 것은 아니다.

## E. Candidate B — prefilter만 high precision

**구현·확인함.** 이 A8 fixture는 coverage를 red channel에 저장한다. 먼저 공개 Pixel::RGB16F를 요청했지만, 이 GLES backend는 이를 compact **R11G11B10F**로 매핑했다(`gles-graphics-types.h:380–383`). attachment 질의도 redBits=11 / GL_FLOAT였다. 따라서 그 결과를 FP16 증거로 쓰지 않았다.

실제 FP16 비교는 외부 `precision.so`의 좁은 allocation override로 진행했다. **505×295 / R11G11B10F / 빈 allocation 하나**만 RGBA16F로 대체한다. 이 viewer에서는 B의 prefilter target만 그 조건에 해당한다. native attachment 질의에서 **redBits=16 / GL_FLOAT**를 확인했다. H/V는 기존 A8이고 B1 shader/계수/해상도는 모두 동일하다. DALi/adaptor source나 production format support를 수정하지 않았다. 실행 script는 이 진단용 hook을 로드하므로 viewer 안에서 5번 키로 전환해도 실제 FP16이다.

이 경로는 **Korean24/A8 전용 진단**이며 alpha가 필요한 RGBA ImageSpan에 RGB16F를 적용할 수 있다는 뜻이 아니다. viewer의 5번 선택 시 Korean24로 고정한다.

- strong blur의 세로 grid가 그대로 보인다.
- p=.20: B1 대비 RMSE0.054, max1. p=.50: RMSE0.091, max1. .75도 max1이다.
- 따라서 **prefilter output의 8-bit 양자화가 추가 세로 grid의 주원인은 아니다.**
- Stop rule에 따라 여기서 B 종료. H/V 전체 high precision으로 확대하지 않았다. 따라서 H/V를 포함한 모든 양자화 영향이 0이라고 주장하지 않는다.
- 성능/메모리 측정 없음.

## F. Candidate C — Output의 horizontal two-sample

**구현·확인함.** B1 그대로 두고 blurred V read 하나만 아래로 교체했다.

```
0.5 * (V(uv - (0.5/505, 0)) + V(uv + (0.5/505, 0)))
```

half low-texel 후보 하나만 사용했다. Sharp Source read, Late Smooth 식/threshold, 타이밍은 그대로다. delta sweep 없음.

- strong blur의 세로 grid는 조금 눌리지만 제거되지 않는다. 가로 band에는 해결 효과가 없다.
- p=.75에서 획이 추가로 퍼져 부드러움/대비 손실이 증가한다.
- Stop rule에 따라 **reconstruction workaround CLOSED**. production 후보로 승격하지 않음.
- Output의 blurred read는 1→2개가 된다. 실제 GPU 비용은 측정하지 않았으며, Source read 수는 그대로다.

## G. Additional fixtures

**실행하지 않음.** A의 세로 성분은 개선되었지만 Korean24 느린 비교에서 추가 가로 band가 남아 최종 primary 품질 기준을 충족하지 못했다. Korean32/Gradient/ImageSpan으로 검증 matrix를 확대하지 않았다. 기존 corpus/선택 기능은 viewer에 남아 있지만 이번 결과가 그 fixture까지 검증했다는 뜻은 아니다.

## H. Slow animation

CURRENT | A를 **같은 Animation**으로 0→1 4초 + 1→0 4초 Linear 반복. 두 Label 모두 FHD, 화면에서 각각 같은 왼쪽 960px를 clip하므로 글자 크기/reflow 차이는 없다. 8초 lossless RGB 영상 하나를 기록했다. 녹화의 30fps 설정은 실행 FPS 측정값이 아니다.

- 왕복 진행과 sharp 복귀가 정상으로 보이며, A만 검게 남거나 위치가 이탈하는 현상은 확인하지 못했다.
- 영상 +1.3초/+7.3초 부근에서 A의 **가로 band**가 CURRENT보다 더 보인다. 영상 시작은 animation 중간이므로 해당 초를 progress로 읽으면 안 된다.
- large pop이나 별도 sharp layer의 위치 분리는 관찰된 temporal crops에서 확인하지 못했다. 미세 shimmer의 완전한 부재를 보장하지 않는다.
- Unit::LINE/Fade0의 기존 줄별 출현/퇴장은 양쪽 공통이며 blur grid와 구분했다.

## I. Theoretical filtering work — 실측 아님

Q=505×295=148,975. Source/Output 작업, clear/submission, branch, cache는 제외한 texture instruction 추정이다. endpoint에서의 early-out은 이 계산에 포함하지 않았다.

| 경로 | 계산 | texture instructions/frame |
|---|---|---:|
|CURRENT H|505×1180×48|28,603,200|
|CURRENT V|505×295×48|7,150,800|
|**CURRENT 합**||**35,754,000**|
|B1 area + H + V|Q×(9+12+12)|4,916,175 (**−86.25%**)|
|A Y-area|2020×295×3|1,787,700|
|A production H|Q×48|7,150,800|
|A low-res V|Q×12|1,787,700|
|**A 합**|Q×72|**10,726,200 (−70%)**|

A에도 연산량 감소 잠재력은 크다. 하지만 이를 **GPU 시간 70% 감소**, target FPS 또는 CPU 개선이라고 표현하지 않는다. 이번 작업에서 CPU/GPU/FPS benchmark는 실행하지 않았다.

## J. Logical FBO payload — 계산만

full Source S=2,383,600px. single WHOLE_TEXT page, retained sharp Source 동일.

| 경로 | 픽셀 합 | A8 MiB | RGBA MiB |
|---|---:|---:|---:|
|CURRENT: S+H(4Q)+V(Q)|3,128,475|2.984|11.934|
|B1: S+area(Q)+H(Q)+V(Q)|2,830,525|2.699|10.798|
|A: S+Y-area(4Q)+H(Q)+V(Q)|3,277,450|3.126|12.502|
|A−CURRENT|+148,975|**+0.142**|**+0.568**|

A는 CURRENT보다 logical payload **4.76% 증가**, task는 3→4개다. Y-area target은 CURRENT의 큰 H target 크기를 대체하며, 순증가는 Q 하나다. RGB16F 진단 B의 메모리는 평가하지 않았다.

original text/metadata/driver alignment/MSAA/swapchain/CPU RSS/실제 VRAM은 제외한다. 외부 installer는 기존 unused H handle을 즉시 해제하지 않으므로, 위 수치는 **제안 topology의 계산이지 실행 PoC peak memory 측정값이 아니다.**

## K. Verdict / 실행 방법

**Y-ONLY PREFILTER STILL SHOWS GRID — STOP.**

세로 grid가 X방향 저해상도 경로와 관련 있다는 가설은 강화되었다. 하지만 A에는 가로 band가 남아 CURRENT와 동급의 전체 blur 품질로 보기는 어렵다. B의 precision 변경은 주원인을 해결하지 못했고, C는 softness trade-off를 키운다. 이번 범위에서 더 튜닝하거나 성능 PoC로 진행하지 않는다.

```bash
bash /home/bowonryuubuntu/tizen/reveal-yfirst-quality.K64mLI/run-viewer.sh
```

기본: **CURRENT | A(Y-only)**, Korean24, 8초 왕복.

- `1` HIGH / `2` CURRENT / `3` 기존 B1
- `4` A Y-only / `5` B 실제 FP16 prefilter (Korean24 전용) / `6` C two-sample reconstruction
- `7` CURRENT | B1 / `8` CURRENT | A / `9` B1 | A
- `Q/W/E/R`: .20/.50/.75/.90 고정
- `Space`: 8초 왕복 / `C`: 기존 corpus 선택 / `Esc`: 종료

후보 추가는 별도 외부 `candidates.h`에만 있다. shader compile 오류가 있었던 C의 초기 캡처는 폐기하고 수정된 블록 포맷으로 다시 캡처한 것만 비교했다. 필터/계수를 시각 튜닝한 수정은 아니다.

자료:

- `captures/`: Korean24 6모드 × 4 progress, 24개 PNG
- `comparisons/korean24-p0.20-current-b1-yonly.png`: 세로/가로 grid, 무증폭 3×
- `comparisons/korean24-p0.75-current-b1-yonly.png`: 획/handoff
- `comparisons/korean24-p0.20-b1-precision-reconstruction.png`: 보조 B/C
- `comparisons/stages-p0.20.png`: 단계별 native FBO
- `stages/*.rgba`: 중간 텍스처 원본
- `slow-current-yonly.mkv`, `comparisons/slow-contact.png`: 느린 비교
- `metrics-*.json`, `stage-sanity.json`: 보조 수치

## L. Working tree / scope

Production source/HEAD 변경 없음. 기존 두 PoC 폴더도 그대로다. Commit/push 없음. 기존 adaptor 변경 보존.

새로운 파일은 이 외부 diagnostic 디렉터리 안에만 작성했다. 외부 viewer/hook만 빌드했으며 production build, UTC, sanitizer, regression, performance matrix는 실행하지 않았다. PER_LINE/async/lifecycle/API/sigma search/새 필터/generalization 작업 없음. `precision-format.log`는 초기 packed-float 확인 기록이며, 최종 FP16 확인은 `precision-fp16-format.log`이다. 최종 `captures/*-precision-*` 및 metrics는 실제 FP16 재캡처본이다.
