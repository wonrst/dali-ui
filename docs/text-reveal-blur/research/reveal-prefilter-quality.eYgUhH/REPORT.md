# PERFORMANCE prefilter — quick quality PoC

2026-09-16. **PREFILTER QUALITY REGRESSION — STOP HERE.**

후보의 큰 blur 구간에 current보다 촘촘한 세로 줄무늬/격자가 보인다. 특히 느린 왕복 비교에서 차이가 드러난다. 또한 p=.75에서는 한글 획이 조금 더 부드럽고 대비가 낮아진다. 작은 수치 오차만을 근거로 탈락시킨 것이 아니라, 요청한 **grid 증가** 기준으로 이번 후보를 중단한다. 추가 필터/시그마/전환 튜닝은 하지 않았다.

## A. PoC topology

기존 `../reveal-idle-prefilter.o5LPsX/poc.h`와 `kernel.inc`를 그대로 재사용한다. 호출은 **`Install(window, 48, 1)`** 하나뿐이다. 이전 코드에 남은 다른 variant는 사용하지 않는다.

```
CURRENT:   Source 2020×1180 → H 505×1180 → V 505×295 → existing Output
PREFILTER: Source 2020×1180 → area 505×295 → H 505×295 → V 505×295 → existing Output
HIGH:      Source 2020×1180 → H 2020×1180 → V 2020×1180 → existing Output
```

Source/Output renderer와 Late Smooth, Reveal timing은 변경하지 않았다. 후보만 phase-correct 9-fetch area + low radius 12 exact kernel(6 positive pairs)을 사용한다. 기존 calibration의 sigma 15.10251141 / low sigma 3.50746632, area variance 1.25, 실제 축별 축소비 계산을 그대로 사용했다. 새 sigma 탐색 없음.

범위는 radius 48, WHOLE_TEXT뿐이다. 이전 diagnostic과 동일한 Unit::LINE, Fade=0, Stagger=.25, BlurDurationRatio=1을 유지했다. **Fade=0이므로 현재 HEAD의 Fade=1 adaptive recipe는 모든 비교에서 제외된다.** 새 source batching, PER_LINE, async, sampler/reconstruction 정책을 추가하지 않았다.

## B. Test cases / 실행

| Case | Fixture |
|---|---|
|Korean24|기존 `따뜻한 햇살 아래 아름다운 우리들의 이야기가 시작됩니다` corpus, font 24, 12줄|
|Korean32|동일 corpus, font 32, 12줄|
|Gradient|Korean32 + 기존 red/green/blue linear gradient|
|ImageSpan mixed|기존 `The image in the morning 빛나는 장면 [image]` / `A bright day — 안녕하세요 DALi UI`, font 32|

ImageSpan은 이전 diagnostic의 로컬 64×64 procedural RGBA texture를 동일한 80×80 span으로 사용한다. 외부 서버 의존성 없음.

Host: GTX 1650 / driver 595.91.07 / GLES / MSAA 4. Window와 Label의 요청 크기는 각각 **1920×1080**. Label 위치는 (60,60)이며 텍스트/halo가 보이는 부분을 비교한다. 전체 창을 늘려서 Label을 축소하거나 텍스트를 다른 폭으로 reflow하지 않았다.

정적 비교는 **4 cases × 4 progress(.20/.50/.75/.90) × 3 modes = 48 PNG**. X11 client window를 직접 읽으며 GPU timer query, CPU/FPS/memory benchmark, GL 계측 preload는 사용하지 않았다. 내용 영역의 비교에서 하단 모드/키 안내는 제외했다.

실행:

```bash
bash /home/bowonryuubuntu/tizen/reveal-prefilter-quality.eYgUhH/run-viewer.sh
```

기본은 **왼쪽 CURRENT / 오른쪽 PREFILTER**, Korean24, 8초 왕복 애니메이션이다.

- `1`: HIGH 전체 화면
- `2`: CURRENT PERFORMANCE 전체 화면
- `3`: PREFILTER 전체 화면
- `4`: CURRENT / PREFILTER 좌우 비교
- `C`: Korean24 → Korean32 → Gradient → ImageSpan mixed
- `Q / W / E / R`: p=.20 / .50 / .75 / .90 고정, 애니메이션 중지
- `Space`: 0→1→0, 4초+4초 Linear, 반복
- `Esc`: 종료

모드/case 변경은 diagnostic Label을 새로 준비하므로 약 1–2초 뒤 비교 상태가 된다. 이 준비 시간은 production API 성능 측정이 아니다. Split에서도 각 Label은 FHD이며 **글자 크기를 축소하지 않고 같은 왼쪽 960px를 clip**해서 나란히 표시한다. 전체 내용은 1/2/3 모드로 확인한다. 네 case 모두 현재 corpus의 주요 텍스트/이미지가 좌우 비교 영역에 들어온다.

고정 비교로 시작하려면:

```bash
SLOW=0 MODE_INDEX=3 bash /home/bowonryuubuntu/tizen/reveal-prefilter-quality.eYgUhH/run-viewer.sh
```

`CASE_INDEX=0..3`, `MODE_INDEX=0..3`은 각각 위 fixture와 HIGH/CURRENT/PREFILTER/SPLIT이다. Host용 viewer만 준비했으며 target package/spec 작업은 하지 않았다.

## C. Static comparison

평가는 **PREFILTER가 CURRENT에 비해 어떤가**를 뜻한다. HIGH는 reference이며 같아야 한다는 기준으로 판단하지 않았다.

| Case / progress | Current vs Prefilter 관찰 | Verdict |
|---|---|---|
|Korean24 / .20|전체 blur 폭은 유사하나 후보의 미세한 세로 band/grid가 더 드러남. 3× crop에서 명확|SLIGHTLY WORSE|
|Korean24 / .50|큰 형태와 밝기 유사, 뚜렷한 새 halo 없음|SIMILAR|
|Korean24 / .75|후보의 획/글자 내부가 조금 더 무르고 대비가 낮음|SLIGHTLY WORSE|
|Korean24 / .90|내용 ROI pixel-identical|SIMILAR|
|Korean32 / .20|후보의 낮은 대비 격자 차이. blur의 큰 외곽 형태는 유사|SLIGHTLY WORSE|
|Korean32 / .50|큰 형태 유사, 약한 질감 차이|SIMILAR|
|Korean32 / .75|확대 시 획이 더 둔하고 약간 더 두꺼운 blur 성분으로 보임|SLIGHTLY WORSE|
|Korean32 / .90|내용 ROI pixel-identical|SIMILAR|
|Gradient / .20|큰 색 분포/외곽 유사, white case보다 band가 덜 두드러짐|SIMILAR|
|Gradient / .50|새 색 경계/밝은 halo의 뚜렷한 증가 없음|SIMILAR|
|Gradient / .75|색상 배치는 유지되지만 획 대비는 조금 낮아짐|SLIGHTLY WORSE|
|Gradient / .90|내용 ROI pixel-identical|SIMILAR|
|ImageSpan mixed / .20|텍스트와 이미지가 같은 blur 흐름을 유지|SIMILAR|
|ImageSpan mixed / .50|이미지 경계/halo 폭에 큰 차이 없음|SIMILAR|
|ImageSpan mixed / .75|이미지 내부의 미세 무늬는 더 매끈함. 주변 텍스트는 약간 부드러움|SIMILAR — trade-off|
|ImageSpan mixed / .90|내용 ROI pixel-identical|SIMILAR|

정적 한 지점에서의 작은 차이만으로 최종 탈락을 정하지 않았다. **E의 느린 비교에서 강한 blur 동안 후보의 세로 grid가 더 명백하게 나타나는 점**이 최종 판단의 주요 근거다.

## D. Observed artifacts

- **Stroke:** 명확하게 특정 획이 누락되는 사례는 이 네 fixture에서 확인하지 못했다. 다만 .75에서 후보가 더 soft하고 자소 내부 대비가 낮다.
- **Grid:** current도 완전히 매끈하지 않지만 후보에서 더 촘촘한 세로/격자 질감이 드러난다. 확대 crop뿐 아니라 느린 비교의 1:1 temporal crop에서도 보인다. 이번 중단 사유다.
- **Halo:** 기존보다 훨씬 넓거나 밝은 새로운 외곽 halo는 확인하지 못했다. 미세 밝기 차이는 존재한다.
- **Sharp + blur:** Late Smooth가 그대로이므로 양쪽 모두 sharp/blur 혼합 단계가 있다. 후보의 더 soft한 blur 성분은 관찰되지만, 별도 위치의 이중 글자가 확연히 추가되었다고 단정하지는 않는다.
- **Gradient:** 경계 단절이나 새 색띠는 확인하지 못했다. 획의 softening은 white case와 비슷하다.
- **ImageSpan:** 로컬 이미지의 경계/색상/텍스트와의 위치 관계는 유지된다. .75에서 이미지 내부 고주파 checker가 더 뭉개지는 trade-off는 보이나, 이미지에만 비정상적인 halo가 추가되지는 않았다. 이 단일 이미지 결과를 모든 ImageSpan 품질 보장으로 확대하지 않는다.

## E. Slow animation

Korean24를 CURRENT / PREFILTER로 동시에 표시하고 **하나의 Animation**에 두 Label의 progress를 연결했다. 0→1 4초, 1→0 4초, Linear 반복이다. 두 Label에 다른 clock/시작 시간을 사용하지 않는다.

- 왕복 진행과 sharp 복귀가 정상적으로 나타난다. 후보만 멈추거나 검게 남는 현상은 이 관찰에서 없었다.
- 강한 blur 구간에서 후보의 세로 band/grid가 더 두드러진다. 기록 영상 +4.3초 / +5.3초 / +6.3초 부근의 1:1 contact sheet에서 잘 보인다. 영상 시작은 반복 중간이므로 이 시간은 progress 값이 아니다.
- coarse temporal crop에서 후보만의 큰 위치 jump나 별도 layer 이탈은 확인하지 못했다. 미세 shimmer/pop의 완전한 부재를 보장하지 않는다. 실시간 viewer 또는 영상을 보는 것이 최종 UX 판단에 적합하다.
- Unit::LINE + Fade=0의 줄별 나타남/사라짐은 양쪽 공통의 기존 Reveal 동작이다. 그 줄 수 변화 자체를 후보 blur의 pop으로 분류하지 않는다.

영상은 X11 창을 8초 기록한 **lossless RGB** 파일이다. 30fps는 기록 설정일 뿐 실행 성능/FPS 측정값이 아니다. 녹화 당시 HUD의 p 표시는 마지막 수동값(.50)이었으며 실제 애니메이션 값이 아니다. 최종 viewer는 애니메이션 중 `p animated`로 표시하도록 안내 문구만 바로잡았다.

## F. Representative captures

이 디렉터리 기준:

- `captures/`: 지정된 48개 원본 PNG (1920×1080)
- `comparisons/korean24-p0.20.png`: 강한 blur의 grid 비교, 3× nearest
- `comparisons/korean24-p0.75.png`: 작은 한글 획/handoff
- `comparisons/korean32-p0.75.png`: 큰 한글 획/handoff
- `comparisons/gradient-p0.75.png`: gradient 획과 색상
- `comparisons/mixed-p0.50.png`, `mixed-p0.75.png`: ImageSpan 경계/내부 무늬
- `comparisons/*-overview.png`: case별 네 progress, 1:1 배열
- `slow-current-prefilter.mkv`: 8초 좌우 비교 원본 영상
- `comparisons/slow-contact.png`: 영상의 몇 시점만 추린 1:1 비교

정적 crop 배열은 **왼쪽 CURRENT / 가운데 PREFILTER / 오른쪽 HIGH**. 밝기/대비를 증폭하거나 후보에만 다른 resampling을 적용하지 않았다.

## G. Numeric sanity (보조)

8-bit RGB 차이. 고정된 텍스트+halo ROI, 검은 여백 일부 포함. RMSE/max는 perceptual threshold가 아니다. 모든 16개 조합의 결과와 ROI는 `metrics.json`에 있다.

| Case / p | PREFILTER–CURRENT RMSE / max | PREFILTER–HIGH | CURRENT–HIGH |
|---|---:|---:|---:|
|Korean24 / .20|0.311 / 5|0.357 / 6|0.310 / 4|
|Korean24 / .75|2.429 / 24|4.319 / 40|2.740 / 27|
|Korean32 / .75|3.434 / 33|6.784 / 48|4.459 / 32|
|Gradient / .75|1.637 / 25|3.215 / 47|2.121 / 32|
|ImageSpan mixed / .75|1.469 / 27|3.451 / 52|2.418 / 36|
|모든 4 cases / .90|**0 / 0**|HIGH와는 차이 있음|동일한 차이|

강한 blur의 절대 오차가 작아도 어두운 blur 영역의 반복 줄무늬는 보일 수 있다. 반대로 .90에서 current/prefilter가 같다는 것은 기존 sharp Source handoff를 유지한다는 sanity evidence이지, 중간 품질을 보장하는 것은 아니다. .90에서 HIGH와 차이가 남는 것도 기존 PERFORMANCE handoff 정책과의 차이이므로 이번 prefilter의 새 회귀로 계산하지 않는다.

## H. Verdict

**PREFILTER QUALITY REGRESSION — STOP HERE.**

현 후보를 바로 PER_LINE PoC 또는 production 구현으로 올리지 않는다. 정적 강한 blur의 큰 형태는 유사하고 local ImageSpan/gradient에 치명적인 누락은 없었지만, 느린 비교에서 드러난 **추가 grid**는 요청한 실패 조건에 해당한다.

사용자가 영상/실시간 비교로 trade-off를 확인한 뒤 다음 방향을 정할 수 있도록 결과만 남긴다. fixed-phase tent 재시도, tap 증가, sigma search, 새 reconstruction, progress별 tuning으로 확대하지 않았다.

## I. Working tree / 범위 준수

- dali-ui HEAD: `5aae0d2b389bb9811bed6e21b754cee629f4ecfc` (`devel_blur_text`).
- Production source 수정 **없음**, 기존 외부 diagnostic 수정 **없음**.
- 기존 adaptor의 `gles-texture-dependency-checker.cpp` 변경은 그대로 보존.
- commit/push **없음**. 이 디렉터리의 viewer와 보조 파일만 작성.
- 외부 viewer만 빌드. UTC/full regression/sanitizer 실행하지 않음.
- CPU/GPU/FPS/memory benchmark 없음. 기존 impulse/profile matrix 재실행 없음.
- PER_LINE/async/lifecycle/GLES2/Vulkan/small radius 확장 없음.

재현용 파일: `build.sh`, `run-viewer.sh`, `viewer.cpp`, `capture.py`, `compare.py`, `animation-contact.py`. 이전 PoC 폴더의 `poc.h`/`kernel.inc`가 빌드 시 필요하다. 독립된 새 production 클래스나 라이브러리를 추가한 것이 아니다.
