# Text::Reveal Blur — 최종 개발 보고서

## 최종 결론

**Text::Reveal Blur 상품 적용 중단**

약 3주간 품질·성능·메모리 최적화를 검토했으나 현재 구조에서는 목표 품질과 타겟 성능을 동시에 만족하지 못해 이번 개발을 종료한다. Blur 관련 public API도 이번 작업에서는 출시하지 않는다.

| 비교 항목 | HIGH | PERFORMANCE | ECONOMY |
|---|---:|---:|---:|
| 처리 구조 (Source / H / V) | Full / Full / Full | Full / X¼ / XY¼ | Full / XY¼ / XY¼ |
| 페이지당 offscreen 단계 | 3 | 3 | 3 |
| 논리 텍스처 메모리 | 1.31 MiB | 0.58 MiB | 0.49 MiB |
| 평균 FPS | 50.21 | 48.19 | 48.87 |
| 중앙값 FPS | 53.98 | 52.66 | 52.59 |
| 59 FPS 미만 구간 평균 | 42.76 | 42.29 | 42.98 |
| 최저 측정값 | 25.41 | 27.92 | 30.21 |
| 샘플 수 | 23 | 24 | 26 |
| 품질·비용 | 가장 안정적인 품질 / 높은 비용 | 균형적인 전환 / 성능 목표 미달 | 일부 격자 완화 / 한글 전환 품질 저하 |

## 적용을 중단한 이유

- HIGH는 품질을 만족했지만 비용이 컸다.
- PERFORMANCE는 filtering·메모리를 줄였지만 목표 FPS를 확보하지 못했다.
- ECONOMY는 더 저렴했지만 FPS 차이가 작고 한글 전환 품질이 더 떨어졌다.
- stage 제거 후보는 품질·정확성·GPU 비용 문제로 채택하지 못했다.

## 1. 목표

앱이 줄마다 Label·BlurEffect·Animation을 따로 구성하지 않아도 Reveal 시퀀스에 맞는 blur를 제공하려 했다.
기존 progress·fade·stagger, 등장/퇴장 및 reverse 동작과 사용자 Animation 시간을 유지하는 것이 목표였다.
한글·gradient·컬러 glyph·ImageSpan 품질과 async·자원 수명 안전성을 함께 검증했다.
HIGH는 최상의 품질 reference, PERFORMANCE는 저사양 타겟에서 실용적인 경로를 지향했다.
ECONOMY는 PERFORMANCE의 타겟 성능이 부족한 뒤 추가한 연구 경로였다.
판단 기준은 여러 Label이 동시에 전환되는 Text Effect Demo에서의 안정적인 60 FPS 수준 UX였다.

## 2. 최종 구현 구조

W/H는 halo와 packing을 포함한 Source page 크기이며 축소 시 `ceil`을 적용한다. Output은 일반 scene renderer로, 네 번째 offscreen stage가 아니다. Source는 현재 Reveal progress가 반영된 foreground다.

### HIGH

```text
Full Source → Full H → Full V → Output
```

- 장점: Gaussian 품질 reference. Strong blur와 한글 획·전환이 가장 안정적이다.
- 비용: full-resolution filtering과 intermediate texture가 크다.

### PERFORMANCE

```text
Full Source → H (X/4, Y 유지) → V (X/4, Y/4) → V + Source / Late Smooth
```

- 장점: H의 Y를 보존해 early Y downsample artifact를 줄이고 full Source로 선명하게 복귀한다.
- 비용: Source 유지와 Output 합성이 필요하며 3-stage 제출·자원 비용도 남는다.

### ECONOMY

```text
Full Source → H (X/4, Y/4) → V (동일 크기) → Source × sharpAlpha + V × blurAlpha
```

- 장점: H/V가 가장 작고 추가 prefilter/pass가 없다. 최종은 authored radius와 gamma=1/native timing이다.
- 절충: 일부 fine-grid는 덜 거슬리지만 한글 전체 fade와 sharp 전환은 더 부자연스럽다. 단순한 단조 품질 등급이 아니다.

## 3. 핵심 교훈

1. 타겟의 1-tap 관측은 Gaussian tap/ALU만 줄여서는 큰 병목이 해결되지 않음을 보여줬다.
2. page별 RenderTask/FBO/RenderPass와 dependency 비용이 중요하다. task 수만이 원인은 아니다.
3. PERFORMANCE/ECONOMY의 메모리 절감은 유효하지만 full Source와 3 stages/page는 남는다.
4. stage 제거는 구조적 이득이 있으나 시험한 후보들은 품질 또는 GPU 비용과 충돌했다.
5. 앱의 `Reveal::None()` cleanup은 활성 수명을 줄일 수 있지만 범용 플랫폼 최적화는 아니다.
6. PC는 품질·correctness·비용 방향의 screening에 유용하며 타겟 FPS를 직접 예측하지 못한다.
7. 배치·coverage·ownership 검증의 성과는 보존하되, 다음 큰 개선은 architecture 수준에서 다뤄야 한다.

BlurRadius / BlurDurationRatio / BlurQuality 및 HIGH / PERFORMANCE / ECONOMY는 연구 구현에 남지만 제품 contract로 출시하지 않는다. 기존 blur 없는 Reveal과는 별개다.

## 측정 정보

메모리는 Cards 12 Labels / 15 pages / R24의 Source/H/V 논리 texture payload다. FPS는 제공된 로그를 mode별 동일 방식으로 집계했다.

각 FPS 값의 비가중 평균, 정렬 중앙값, 59 미만 값의 평균, 최솟값을 사용했다. 각 값은 약 1초 구간으로 최저 측정값은 순간 최저 FPS나 1% low가 아니다.

초기 PERFORMANCE 수동 요약은 49.10 / 53.23이었으나 raw 24개 재집계는 **48.19 / 52.66**이다. 최종 비교에는 재집계값을 사용한다.

### HIGH

N=23 · 평균 50.21 · 중앙값 53.98 · 59 미만 평균 42.76 · 최저 25.41

```text
59.99, 60.00, 53.98, 53.72, 39.93, 60.00,
60.00, 60.00, 60.00, 34.40, 36.36, 39.38,
54.32, 59.02, 33.08, 48.75, 60.00, 60.00,
50.22, 25.41, 36.40, 49.97, 60.00
```

### PERFORMANCE

N=24 · 평균 48.19 · 중앙값 52.66 · 59 미만 평균 42.29 · 최저 27.92

```text
60.00, 39.41, 55.59, 44.98, 41.94, 60.01,
60.00, 60.00, 54.01, 29.52, 34.79, 34.37,
38.98, 60.00, 52.67, 33.42, 52.64, 60.00,
60.00, 53.78, 27.92, 31.27, 51.35, 60.00
```

### ECONOMY

N=26 · 평균 48.87 · 중앙값 52.59 · 59 미만 평균 42.98 · 최저 30.21

```text
60.00, 47.39, 47.64, 60.00, 52.70, 39.73,
57.29, 60.01, 60.00, 60.00, 34.37, 31.40,
34.21, 30.92, 55.16, 60.00, 52.06, 33.90,
52.47, 60.00, 60.00, 55.33, 30.34, 30.21,
45.56, 60.00
```


정확한 메모리: HIGH 1,377,693 bytes / 1.31387 MiB, PERFORMANCE 603,494 bytes / 0.57554 MiB, ECONOMY 517,635 bytes / 0.49366 MiB.

Ubuntu/GLES, 1280×720, Sync, MSAA4, Cards 등장 R24의 고유 S/H/V attachment 합이다. 이 fixture는 45 offscreen tasks/FBOs이며 원본 text/metadata·CPU·window/MSAA·driver allocation을 포함한 실제 VRAM/RSS/peak는 아니다. 실험별 PC·타겟 조건은 해당 연구에 따른다. 타겟 계측 ON 오버헤드는 정량 확인되지 않았다.

## 연구 기록

각 실험의 자세한 목적·방법·결과·판단은 단일 HTML의 상세 패널에 포함했다. 유지/검증 통과는 국소 변경·검증 결과이며 제품 출시 승인이 아니다.

### A. 타겟 성능 분석

| 실험 | 확인한 내용 | 핵심 결과 | 판단 |
|---|---|---|---|
| [타겟 병목과 개발 방향 재평가](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-direction-review.QxzXQ5/REPORT.md) | 타겟 관측과 병목 가설 | 샘플링 계산보다 H/V 처리 경로 전체를 먼저 조사할 필요가 있었다. | 참고 |
| [BlurEffect / Reveal 자원 구성 비교](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-pipeline-count.a9GqcW/REPORT.md) | 실제 데모의 task/FBO | 정상 퇴장은 양쪽 task 수가 같았다. 개수만으로 성능 차이를 설명할 수 없었다. | 참고 |
| [V-only 최종 합성 진단](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-vonly-local.WmHrK8/REPORT.md) | 최종 Source 읽기 제외 | 원본 Source 읽기를 빼도 Source/H/V 자원은 그대로 남았다. | 참고 |
| [H/V 1-tap 필터 진단](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-onetap-local.xAtm4Y/REPORT.md) | 필터만 중앙 1회 읽기 | Gaussian 연산을 크게 줄여도 같은 처리 구조의 타겟 동작은 거의 그대로였다. | 참고 |
| [Source-only 처리 단계 제거](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-sourceonly-local.l6cJM9/REPORT.md) | H/V 자원 생성 자체 제외 | H/V 전체를 실제로 없애자 타겟 동작이 크게 좋아졌다. 다만 blur도 함께 사라진다. | 참고 |
| [타겟 프레임 비용 분리](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-target-attribution.01tPcM/REPORT.md) | 지속 비용과 setup 피크 분리 | 지속 비용은 offscreen 명령 처리에, 전환 직후 피크는 자원 생성에도 집중됐다. | 참고 |
| [타겟 명령·FBO 상세 계측](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-command-results.BftCT1/REPORT.md) | 명령·FBO 생성 범주 계측 | DependencySync와 RenderPass가 지속 비용 차이의 큰 부분을 차지했다. | 참고 |
| [PC 결과의 타겟 예측 가능성](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-backend-predictability.z457tX/REPORT.md) | PC→TV 외삽 가능성 | PC는 후보를 거르는 데 유용하지만 하나의 배율로 TV FPS를 예측할 수 없었다. | 참고 |

### B. Blur 품질·필터 후보

| 실험 | 확인한 내용 | 핵심 결과 | 판단 |
|---|---|---|---|
| [Gaussian tap 수 축소](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-perf-opt.upcgEj/REPORT.md) | CAP_12 / 8 / 6 근사 | tap을 줄이면 GPU 비용은 줄었지만 촘촘한 한글에서 격자가 늘었다. | 품질 실패 |
| [Fade-aware Adaptive Gaussian](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-adaptive-study.8A0C8e/REPORT.md) | Fade1 퇴장 커널 축소 | 보이지 않는 구간의 계산은 줄였지만 중간 구간의 최대 비용은 남았다. | 미적용 |
| [Adaptive 중간 구간 세분화](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-midrange-study.Uxvyif/REPORT.md) | 촘촘한 progress 품질 | 거친 checkpoint에서는 통과했지만 촘촘한 progress 검사에서 오차가 다시 나타났다. | 품질 실패 |
| [Prefilter B1](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-prefilter-quality.eYgUhH/REPORT.md) | 사전 축소 후 작은 H/V | 낮은 해상도에서 필터링하기 전 평균을 냈지만 strong blur의 세로 격자가 늘었다. | 품질 실패 |
| [Y-only prefilter A](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-yfirst-quality.K64mLI/REPORT.md) | Y축만 먼저 축소 | X 정보를 보존하자 세로 격자는 줄었지만 가로 band가 남았다. | 품질 실패 |
| [A-R / Y-half 품질 비교](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-yrecon-quality.xHjb5S/REPORT.md) | 수직 보간 / H 높이 절반 | 가로 band 완화와 중간 획 선명도를 동시에 만족하지 못했다. | 품질 실패 |
| [A-R 실제 경로 후속 검증](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-ar-production.FY6dsQ/REPORT.md) | 실제 4-stage A-R | PC GPU 이득은 컸지만 처리 단계가 늘었고 후속 타겟 결과는 좋지 않았다. | 미적용 |
| [Vogel12 직접 blur](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-vogel12-quality.0TCvfD/REPORT.md) | Source 직접 12회 읽기 | 12개의 흩어진 샘플로는 한글 획이 복제되는 무늬를 피하지 못했다. | 품질 실패 |
| [Dense 9×9 직접 blur](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-dense2d-quality.ECWtnl/REPORT.md) | 81-read 2D 커널 | 81회 읽기로 늘려도 strong blur에서 획 모양의 격자가 남았다. | 품질 실패 |
| [주파수 최적화 9×9](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-frequency-quality.HryP4o/REPORT.md) | 주파수 응답 최적화 | 커널 응답을 직접 최적화해도 Strong24의 반복 밝기 무늬가 남았다. | 품질 실패 |
| [Strength별 독립 9×9](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-independent-quality.On1JuQ/REPORT.md) | strength별 독립 커널 | 고정 scaling 제약을 풀어도 제한된 81-read 후보군은 품질 기준에 도달하지 못했다. | 품질 실패 |
| [해상도·tap 절충: 7×7 / 5×5](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-resolution-tradeoff.OHZfwT/REPORT.md) | 해상도와 샘플 수 절충 | 출력 해상도를 높이고 tap을 줄이는 조합도 최종 누설을 충분히 낮추지 못했다. | 품질 실패 |
| [Single8 실제 2-stage](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-lifecycle-structure.MYQiQb/REPORT.md) | H 없는 실제 8-tap 경로 | H를 실제로 없앤 구조는 성립했지만 초기 blur의 격자와 획 복제가 컸다. | 품질 실패 |

### C. ECONOMY 개발

| 실험 | 확인한 내용 | 핵심 결과 | 판단 |
|---|---|---|---|
| [ECONOMY 1단계: blur 단독](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-economy-quality.fxgYr6/REPORT.md) | quarter H/V blur 단독 | 최대 blur에서도 같은 문장이 줄마다 다르게 흐려져 안전한 표현 구간을 정하지 못했다. | 품질 실패 |
| [ECONOMY 2단계: 원본·blur 혼합](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-economy-phase2.2zaqod/REPORT.md) | sharp와 blur opacity 분리 | 전환은 부드러워졌지만 sharp가 사라진 뒤의 구조적 무늬와 이중 layer 느낌은 남았다. | 미적용 |
| [ECONOMY radius / gamma 조정](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-economy-refine.eIvjxu/REPORT.md) | 반경과 시간 곡선 | R32·gamma1.5는 시각적으로 나아졌지만 전체 데모 성능의 일관된 이득은 없었다. | 미적용 |
| [ECONOMY 최종 gamma1 구현](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-economy-isolation.PJQV3U/REPORT.md) | gamma1·원래 반경·커널 격리 | 구현과 수명 검증은 통과했지만 최종 타겟 성능·전환 품질은 제품 기준을 충족하지 못했다. | 미적용 |

### D. RenderTask·자원 실험

| 실험 | 확인한 내용 | 핵심 결과 | 판단 |
|---|---|---|---|
| [Source draw batching](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-source-production.hWQHqE/REPORT.md) | 입력 atlas와 Source draw | 같은 publication의 입력 텍스처를 atlas로 묶어 줄마다 반복하던 draw를 줄였다. | 유지 |
| [Output draw batching](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-production-cleanup.iVbXZv/REPORT.md) | 줄별 최종 합성 draw | 줄별 strength는 유지하면서 호환되는 최종 합성 renderer를 묶었다. | 유지 |
| [D2: H의 세로 coverage 제한](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-cost-audit.0jBjH2/REPORT.md) | H의 불필요한 세로 raster | H에서 필요 없는 세로 halo raster를 줄이되 clear와 최종 blur 범위는 유지했다. | 유지 |
| [Half-rate RenderTask 실험](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-blur-quality.pbOxDT/report.ko.md) | offscreen 갱신 간격 | offscreen 갱신을 줄이면 blur뿐 아니라 Source에 포함된 Reveal 움직임도 함께 느려진다. | 미적용 |
| [H/V task sleep / wake](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-task-gating-analysis.T3s39Z/REPORT.md) | 미사용 H/V 중지·재개 | blur가 안 보이는 구간은 있지만 reverse·seek와 같은 프레임에 깨우는 계약이 없었다. | 중단 |
| [glFlush 제출 묶기](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-flush-poc.2JyKvS/REPORT.md) | GLES 제출 시점 묶기 | 호출 수는 크게 줄었지만 전체 command CPU의 반복 가능한 순이득은 작았다. | 성능 한계 |
| [V clear 제거](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-lifecycle-structure.MYQiQb/REPORT.md) | 이전 FBO 내용이 남는 경우 | 이전 텍스처 내용을 남기자 줄 경계에 오염이 나타나 correctness 기준을 위반했다. | 중단 |
| [페이지 packing 한도 sweep](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-packing-sweep.7E5egr/REPORT.md) | ratio 한도와 페이지 수 | 작은 페이지 수 감소는 가능했지만 메모리와 sampling phase의 절충이 있었다. 기존 1.25를 유지했다. | 유지 |
| [절대 메모리 상한을 둔 merge](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-source-elision.dw5WOw/REPORT.md) | 추가 allocation 상한 | 메모리 증가는 제한했지만 Cards 한 페이지 감소만으로 전체 CPU 이득을 확인하지 못했다. | 성능 한계 |
| [메모리 admission / FHD 검토](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-source-atlas.3igp9v/REPORT.md) | 정상/병적 요청 admission | 256 MiB는 실제 사용량이 아니라 큰 요청을 미리 거르는 publication 단위 추정 한도다. | 유지 |
| [후속 렌더링 구조 후보 분석](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-topology-analysis.Thk4x4/REPORT.md) | Source 유지·공유 구조 | PERFORMANCE는 sharp Source를 유지해야 해 여러 페이지에서 추가 소유 비용이 생긴다. | 참고 |

### E. 수명 관리·앱 정리

| 실험 | 확인한 내용 | 핵심 결과 | 판단 |
|---|---|---|---|
| [소유권·수명·누수 감사](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-lifecycle-audit.ZfWQqF/REPORT.md) | async·ImageSpan·파괴 | async 교체와 ImageSpan borrow/restore, 직접 파괴·shutdown에서 자원 소유권을 검증했다. | 검증 통과 |
| [Reveal::None 사용 조건](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-lifecycle-structure.MYQiQb/REPORT.md) | 효과 해제의 의미와 조건 | 완료된 단발 등장은 해제할 수 있지만, progress 0에서 None은 숨김 유지가 아니다. | 조건부 |
| [Label별 조기 cleanup](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-demo-cleanup.a4F90T/REPORT.md) | Timer 기반 조기 None | 일찍 끝난 Label의 수명은 줄일 수 있었지만 데모의 scheduler 복잡도가 커졌다. | 미적용 |
| [단순 FinishedSignal cleanup](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-packing-sweep.7E5egr/REPORT.md) | 기존 완료 신호로 정리 | 기존 Animation의 완료 신호에 정리를 맡기고 별도 deadline scheduler를 제거했다. | 유지 |

### F. Source 제거

| 실험 | 확인한 내용 | 핵심 결과 | 판단 |
|---|---|---|---|
| [A8 Direct Source 가능성](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-source-elision.dw5WOw/REPORT.md) | coverage+metadata 직접 읽기 | Source FBO 없이 coverage와 metadata로 같은 foreground를 재구성할 수는 있었다. | 참고 |
| [Direct Source GPU 최종 판정](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-direct-gpu.VMDaVs/REPORT.md) | Source 절감과 H 추가 비용 | Source pass 절감보다 H의 반복 재구성 비용이 더 커져 후보를 중단했다. | 성능 한계 |

## 전체 연구 자료 경로

아래는 lookup용 전체 인덱스다. 상대 경로는 이 최종 폴더의 상위 연구 루트 기준이다. 초기 D2/half-rate 단독 report는 미복원으로 후속 감사 자료에 연결했다. 측정 원문과 기존 장문 기록은 그대로 보존한다.

- [축약 전 최종 보고서](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/TEXT_REVEAL_BLUR_FINAL_REPORT.md)
- [reveal-blur-quality.pbOxDT/report.ko.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-blur-quality.pbOxDT/report.ko.md)
- [reveal-perf-opt.upcgEj/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-perf-opt.upcgEj/REPORT.md)
- [reveal-production-cleanup.iVbXZv/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-production-cleanup.iVbXZv/REPORT.md)
- [reveal-lifecycle-audit.ZfWQqF/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-lifecycle-audit.ZfWQqF/REPORT.md)
- [reveal-memory-quality.InrGZf/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-memory-quality.InrGZf/REPORT.md)
- [reveal-cost-audit.0jBjH2/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-cost-audit.0jBjH2/REPORT.md)
- [reveal-fhd-native.dHnAFY/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-fhd-native.dHnAFY/REPORT.md)
- [reveal-source-atlas.3igp9v/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-source-atlas.3igp9v/REPORT.md)
- [reveal-atlas-followup.y6Uy33/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-atlas-followup.y6Uy33/REPORT.md)
- [reveal-source-production.hWQHqE/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-source-production.hWQHqE/REPORT.md)
- [reveal-whole-analysis.buD2NQ/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-whole-analysis.buD2NQ/REPORT.md)
- [reveal-adaptive-study.8A0C8e/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-adaptive-study.8A0C8e/REPORT.md)
- [reveal-midrange-study.Uxvyif/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-midrange-study.Uxvyif/REPORT.md)
- [reveal-adaptive-integration-audit.BfX6fU/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-adaptive-integration-audit.BfX6fU/REPORT.md)
- [reveal-adaptive-integration.VWDc21/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-adaptive-integration.VWDc21/REPORT.md)
- [reveal-idle-prefilter.o5LPsX/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-idle-prefilter.o5LPsX/REPORT.md)
- [reveal-prefilter-quality.eYgUhH/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-prefilter-quality.eYgUhH/REPORT.md)
- [reveal-yfirst-quality.K64mLI/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-yfirst-quality.K64mLI/REPORT.md)
- [reveal-yrecon-quality.xHjb5S/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-yrecon-quality.xHjb5S/REPORT.md)
- [reveal-ar-feasibility.wIhiCC/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-ar-feasibility.wIhiCC/REPORT.md)
- [reveal-ar-production.FY6dsQ/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-ar-production.FY6dsQ/REPORT.md)
- [reveal-demo-compare.jsbHIi/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-demo-compare.jsbHIi/REPORT.md)
- [reveal-exit-cpu.VyPki6/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-exit-cpu.VyPki6/REPORT.md)
- [reveal-upstream-rebase.JeAqys/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-upstream-rebase.JeAqys/REPORT.md)
- [reveal-topology-analysis.Thk4x4/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-topology-analysis.Thk4x4/REPORT.md)
- [reveal-onepage-analysis.RepMKt/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-onepage-analysis.RepMKt/REPORT.md)
- [reveal-vonly-diagnostic.kVClQY/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-vonly-diagnostic.kVClQY/REPORT.md)
- [reveal-vonly-local.WmHrK8/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-vonly-local.WmHrK8/REPORT.md)
- [reveal-onetap-local.xAtm4Y/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-onetap-local.xAtm4Y/REPORT.md)
- [reveal-sourceonly-local.l6cJM9/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-sourceonly-local.l6cJM9/REPORT.md)
- [reveal-pipeline-count.a9GqcW/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-pipeline-count.a9GqcW/REPORT.md)
- [reveal-direction-review.QxzXQ5/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-direction-review.QxzXQ5/REPORT.md)
- [reveal-critical-path.CFOTks/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-critical-path.CFOTks/REPORT.md)
- [reveal-trace-tizen101.prtWoU/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-trace-tizen101.prtWoU/REPORT.md)
- [reveal-target-attribution.01tPcM/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-target-attribution.01tPcM/REPORT.md)
- [reveal-command-attribution.O3eOCq/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-command-attribution.O3eOCq/REPORT.md)
- [reveal-command-results.BftCT1/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-command-results.BftCT1/REPORT.md)
- [reveal-backend-predictability.z457tX/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-backend-predictability.z457tX/REPORT.md)
- [reveal-flush-poc.2JyKvS/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-flush-poc.2JyKvS/REPORT.md)
- [reveal-vogel12-quality.0TCvfD/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-vogel12-quality.0TCvfD/REPORT.md)
- [reveal-dense2d-quality.ECWtnl/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-dense2d-quality.ECWtnl/REPORT.md)
- [reveal-frequency-quality.HryP4o/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-frequency-quality.HryP4o/REPORT.md)
- [reveal-independent-quality.On1JuQ/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-independent-quality.On1JuQ/REPORT.md)
- [reveal-resolution-tradeoff.OHZfwT/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-resolution-tradeoff.OHZfwT/REPORT.md)
- [reveal-economy-quality.fxgYr6/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-economy-quality.fxgYr6/REPORT.md)
- [reveal-economy-phase2.2zaqod/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-economy-phase2.2zaqod/REPORT.md)
- [reveal-economy-refine.eIvjxu/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-economy-refine.eIvjxu/REPORT.md)
- [reveal-economy-production.yd7Ikq/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-economy-production.yd7Ikq/REPORT.md)
- [reveal-economy-isolation.PJQV3U/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-economy-isolation.PJQV3U/REPORT.md)
- [reveal-task-gating-analysis.T3s39Z/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-task-gating-analysis.T3s39Z/REPORT.md)
- [reveal-demo-cleanup.a4F90T/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-demo-cleanup.a4F90T/REPORT.md)
- [reveal-lifecycle-structure.MYQiQb/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-lifecycle-structure.MYQiQb/REPORT.md)
- [reveal-packing-sweep.7E5egr/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-packing-sweep.7E5egr/REPORT.md)
- [reveal-source-elision.dw5WOw/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-source-elision.dw5WOw/REPORT.md)
- [reveal-direct-gpu.VMDaVs/REPORT.md](https://github.com/wonrst/dali-ui/blob/devel_blur_text_result/docs/text-reveal-blur/research/reveal-direct-gpu.VMDaVs/REPORT.md)

2026-09-21 · 약 3주 연구의 종료 기록 · UI 기준 `5e313021` (`Demo clean up`). Production source / git 변경 없음. 새 측정·commit·push 없음.
