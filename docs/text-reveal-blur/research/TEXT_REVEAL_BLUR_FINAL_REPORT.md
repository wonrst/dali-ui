# Text::Reveal Blur — Final Development Report

작성일: 2026-09-21 · 최종 engineering decision record

**최종 판정: NOT SUITABLE FOR PRODUCT INTEGRATION — 적용 불가 / development stopped.**

이 문서는 약 3주간의 구현·품질 비교·최적화·타겟 검증을 기존 자료로 정리한 것이다. 새 실험이나 재측정 결과가 아니다. 본문은 최종 구조와 제품 판단을, 부록은 실험별 근거와 중단 이유를 설명한다.

기록 기준은 `dali-ui/devel_blur_text`, HEAD `5e3130213bfaed3cf4436b5a048b01a6ed652314` (`Demo clean up`)이다. 보고서는 저장소 밖에 작성했으며 UI/Core/Adaptor, 샘플, shader, API 및 commit을 변경하지 않았다.

## 1. Executive Summary

Text::Reveal과 시퀀스별 runtime Gaussian blur를 결합하는 기능은 구현할 수 있었다. HIGH에서는 원하는 시각적 품질을 확보했고, PERFORMANCE와 ECONOMY에서는 해상도·배치·메모리 구성을 바꾸어 filtering work와 논리적 텍스처 저장량을 상당히 줄였다. Sync/Async, ImageSpan, 자원 교체·회수까지 구현과 검증을 진행했다.

그러나 **시험한 Tizen TV 타겟의 Text Effect Demo에서 요구한 안정적인 성능과 품질을 동시에 확보하지 못했다.** PERFORMANCE/ECONOMY original 비교의 사용자 기록은 평균 약 **49.10 / 48.87 FPS**로, ECONOMY의 추가 계산량 절감이 뚜렷한 타겟 성능 향상으로 이어졌음을 보여주지 않는다. 이 FPS 기록의 원본 로그·집계 window는 이번 자료 조사에서 복원하지 못했으므로, 독립 재계산한 benchmark로 취급하지 않는다. 상세 신뢰도는 §6에 명시한다.

HIGH는 품질 reference이지만 저사양 타겟에 부담이 큰 경로다. PERFORMANCE는 가장 균형 잡힌 reduced-resolution 결과였으나 목표 성능이 부족했다. ECONOMY는 filtering/memory를 더 낮췄지만, 한글 전체 fade 및 blur→sharp 전환에서 추가적인 품질 손해가 있었고 이를 정당화할 성능 차이는 확인되지 않았다.

후속 pass 제거·prefilter·cache/gating·packing·direct-source 연구도 품질, correctness, 메모리 또는 GPU 비용의 trade-off를 남겼다. 일부 국소 최적화는 유효했지만 제품 조건을 충족하는 해법으로 이어지지 않았다. 현재 증거에서 다음 큰 개선은 추가적인 Gaussian 계수 조정보다 **renderer/backend architecture 수준의 재설계**에 가깝다.

따라서 이번 Blur 기능의 상품 적용을 중단한다. 아직 출시하지 않은 `BlurRadius`, `BlurDurationRatio`, `BlurQuality`를 제품 public contract로 확정하지 않는다. 이는 구현 불가능 판정이 아니라 **현재 구현·시험 타겟·요구 UX에 대한 적용 중단 결정**이다.

## 2. Goal and Product Requirement

출발점은 글자가 나타나고 사라지는 Reveal 진행에 고품질 blur를 자연스럽게 결합하는 것이었다. 앱이 줄마다 Label, BlurEffect, Animation을 따로 만들고 서로 다른 시작 시각을 맞추지 않아도, Text::Reveal 내부의 최종 시퀀스와 일치하는 효과를 제공하려 했다.

필요한 조건은 다음과 같았다.

- Reveal의 unit/sequence/fade/stagger 계산과 일관된 등장·퇴장, reverse 및 직접 progress 변경.
- 사용자 Animation 시간 안에 끝나는 효과. 별도 clock이나 앱의 비동기 완료 대기를 요구하지 않는 동작.
- 한글·영문·bidi·컬러 glyph·gradient·ImageSpan에서 수용 가능한 품질. 한글 획의 grid, 밝기 변조, 중간 glow와 부자연스러운 sharp 전환을 특히 점검.
- 여러 Label이 동시에 전환되는 실제 Text Effect Demo에서 안정적인 60 FPS 수준의 사용자 경험.
- 일반 Label/blur 없는 Reveal의 경로를 보호하고, async 교체·None·scene disconnect·파괴·shutdown에서 안전하게 자원을 회수.
- 일반 BlurEffect보다 가능하면 낮은 비용. 다만 줄별 타이밍과 foreground 처리 범위가 다르므로 완전히 같은 효과라고 가정하지 않음.

처음의 품질 구분은 **HIGH = 품질 reference**, **PERFORMANCE = 일부 품질을 양보한 저사양 실사용 경로**였다. ECONOMY는 처음부터 독립적인 제품 요구사항이 아니라, PERFORMANCE의 타겟 성능이 충분하지 않아 H/V 비용을 더 낮추기 위해 추가한 연구 경로다.

대표 데모 UX는 긴 텍스트의 PER_LINE stagger 등장, 짧은 텍스트의 동시 등장, WHOLE_TEXT/Fade=1의 동시 퇴장이다. Strong 설정의 authored radius는 등장 24px, 퇴장 48px였다. 특정 샘플 한 장면만 빠른 것이 아니라 이 전환 과정에서 품질과 성능을 함께 만족하는 것이 목표였다.

## 3. Final Implementations

세 경로는 공통 Reveal publication, Source capture, page 배치 및 lifecycle을 사용한다. 여기서 Source는 **현재 progress가 반영된 foreground**를 담는 offscreen 결과이며, 변경되지 않는 원본 글자 bitmap이나 CPU 입력 atlas와는 다르다. 일반 progress 애니메이션마다 CPU text raster를 다시 생성하는 방식도 아니다.

### 3.1 HIGH

```text
Full-resolution Source → Full-resolution H Gaussian → Full-resolution V Gaussian → Output
```

Source/H/V 모두 full resolution이다. 구현한 경로 중 원래 Gaussian reference에 가장 가까우며, strong blur와 촘촘한 한글, blur→sharp 전환의 품질 기준점으로 사용했다. 축소 해상도에서 추가되는 sampling artifact를 피하지만, Gaussian 자체나 offscreen capture의 모든 오차가 없다는 뜻은 아니다.

장점은 효과의 해석과 품질 reference가 명확하다는 것이다. 단점은 큰 filtering 영역과 intermediate texture 비용이다. page당 Source/H/V의 세 offscreen stage가 필요하다. 저사양 제품의 기본 효과로 선택하기에는 비용 부담이 크다. 추가로 제공된 HIGH 로그 23개 sample의 평균은 50.21 FPS이며, PERFORMANCE/ECONOMY original과 동일 조건·window인지 확인되지 않아 §6.2의 별도 실행 결과로 기록한다.

### 3.2 PERFORMANCE

```text
Source W×H → H ceil(W/4)×H → V ceil(W/4)×ceil(H/4)
                                      ↓
                         V blur + full Source, Late Smooth
```

H에서 X를 줄이되 **Y는 full resolution으로 유지**하고, V에서 Y를 줄인다. 초기에 양축을 모두 줄이는 방식보다 수직 방향의 획·줄 sampling을 보존하기 위한 선택이다. 마지막에는 reduced blur와 full-resolution Source를 Late Smooth 곡선으로 합성하여 선명한 글자로 돌아온다.

HIGH보다 H/V 저장량과 filtering 영역은 작다. Source/Output batching, A8 입력 처리, line coverage와 page packing 최적화도 사용한다. 연구한 저비용 경로 중 전체적인 품질 균형은 가장 나았다.

그러나 **세 offscreen stage는 그대로**이며, sharp handoff에 쓰는 full Source를 유지해야 한다. Output의 Source read 및 합성도 필요하다. 여러 Label/page의 task 준비, RenderPass, dependency와 자원 처리 비용은 필터 면적을 줄인 비율만큼 사라지지 않는다. 타겟에서는 이 구조가 기대한 performance mode의 충분한 성능으로 이어지지 않았다.

### 3.3 ECONOMY

```text
Source W×H → H ceil(W/4)×ceil(H/4) → V 동일 크기
                                      ↓
                         full Source × sharpAlpha + V × blurAlpha
```

H부터 X/Y를 모두 줄인다. V 커널과 좌표는 실제 축소 비율에 맞추되, 추가 prefilter task/FBO를 넣지 않는다. Output은 Gaussian 결과만 점점 약하게 만드는 대신, 현재 Source와 reduced blur의 비중을 따로 조절하는 perceptual composition이다.

최종 경로는 **authored BlurRadius, gamma=1/native Reveal timing**을 사용한다. 중간 연구에서 사용했던 radius32/gamma1.5를 모든 UX에 강제로 적용하지 않는다. blur 강도와 sharp/blur 비중은 기존 시퀀스 진행으로 결정하며 별도 애니메이션 시간을 추가하지 않는다.

H/V의 저장량과 filtering 비용은 PERFORMANCE보다 더 작지만 Source와 세 stage는 남는다. 타겟에서는 blur 자체의 미세 grid가 PERFORMANCE보다 덜 거슬리는 경우가 있었고, 최저 FPS가 조금 나은 기록도 있었다. 반면 한글 전체 fade-in/out 및 blur→sharp 전환은 더 부자연스러웠다. **항상 더 좋거나 항상 더 나쁜 품질 단계가 아니라 artifact의 종류가 달라지는 절충**이다.

구현·격리 근거: [ECONOMY 구현](reveal-economy-production.yd7Ikq/REPORT.md), [generic Gaussian 보존 및 최종 격리](reveal-economy-isolation.PJQV3U/REPORT.md). 이 보고서들의 당시 “TARGET-READY”는 PC 검증 후 타겟 시험에 넘길 상태라는 의미였으며, 상품 승인과 다르다.

## 4. Architecture Comparison

| Mode | Source | H | V | Output | Offscreen stages/page |
|---|---|---|---|---|---:|
| HIGH | W×H | W×H | W×H | V blur | 3 |
| PERFORMANCE | W×H | W/4×H | W/4×H/4 | V + Source, Late Smooth | 3 |
| ECONOMY | W×H | W/4×H/4 | W/4×H/4 | Source + V, perceptual mix | 3 |

표의 축소 크기는 실제로 `ceil`을 적용한다. W/H는 halo·guard·packing을 포함한 **page의 Source extent**이지 반드시 Label의 width/height와 같지는 않다. quarter-X/Y는 각 축 1/4, 즉 면적으로는 약 1/16이다.

Output은 일반 scene에 붙는 renderer이며, 이 표의 **네 번째 offscreen stage가 아니다.** 기본 window task나 decoration용 별도 composition, 여러 page는 별도로 센다. 같은 extent의 scratch sharing은 고유 texture/FBO 수를 줄일 수 있지만, 논리적 실행 stage 자체를 없애지는 않는다.

PER_LINE도 무조건 한 줄마다 세 task를 만드는 것은 아니다. line capture를 page에 묶고 호환되는 Source/Output draw를 batch한다. 실제 Cards entrance의 12 Labels/20 visible lines는 15 pages, 세 quality 모두 45 offscreen tasks였다. draw 수·renderer 수·page 수·task 수는 같은 지표가 아니다.

Async는 CPU text preparation을 worker로 옮긴다. event publication, GPU resource realization 및 Source/H/V 실행까지 없애는 기능은 아니다. 따라서 async만으로 runtime blur의 stage 비용이 해소되지는 않는다. [구조 감사](reveal-topology-analysis.Thk4x4/REPORT.md), [실제 Cards inventory](reveal-lifecycle-structure.MYQiQb/REPORT.md).

## 5. Memory Comparison

아래를 최종 메모리의 canonical example로 사용한다. **동일 Text Effect Demo Cards 12 Labels / 15 blur pages / Strong entrance R24**에서 관측한 고유 Source/H/V attachment의 `width × height × bytesPerPixel` 합이다.

| Mode | Logical Source/H/V payload | MiB | HIGH 대비 감소 |
|---|---:|---:|---:|
| HIGH | 1,377,693 bytes | 1.31387 | 기준 |
| PERFORMANCE | 603,494 bytes | 0.57554 | 56.20% |
| ECONOMY | 517,635 bytes | 0.49366 | 62.43% |

ECONOMY는 PERFORMANCE보다 **85,859 bytes, 14.23%** 작다. 세 경로 모두 Source는 459,231 bytes로 같고, H/V가 줄어든 결과다. 따라서 ECONOMY의 H/V 절감률을 전체 텍스처 절감률로 읽으면 안 된다. 이 fixture에서 full Source가 차지하는 몫은 PERFORMANCE 약 76.1%, ECONOMY 약 88.7%다.

조건: Ubuntu/GLES native inventory, window 1280×720, UI scale 1, MSAA 4, Sync, 같은 font/layout. 이 Cards fixture는 page extent가 서로 달라 scratch 공유 없이 45개 FBO가 각각 존재했다. A8와 gradient RGBA가 섞인 **실제 format**으로 계산했으며, 모든 plane을 일괄 RGBA 또는 A8로 가정하지 않았다. [원자료·inventory](reveal-lifecycle-structure.MYQiQb/REPORT.md), [동일 baseline 재확인](reveal-packing-sweep.7E5egr/REPORT.md).

이 수치는 **driver allocation, 실제 VRAM, RSS 또는 전체 앱의 peak가 아니다.** 원본 text/metadata, 입력 atlas, CPU raster/prepared buffers, window/MSAA, driver alignment/cache, 이전·다음 publication의 순간 중첩과 Cards 외 Label을 제외한다. 퇴장 R48이나 FHD 전체 텍스트의 peak에도 그대로 적용할 수 없다.

논리적 texture 절감은 실제 구조상 이득이다. 다만 Source는 선명한 handoff 및 현재 Reveal 결과를 보존하는 역할이 있어 쉽게 제거할 수 없었다. 이를 없애는 마지막 direct-source PoC는 메모리를 줄였지만 H의 반복 재구성 GPU 비용이 더 커져 중단했다(부록 G).

## 6. Target Performance Results

### 6.1 HIGH / PERFORMANCE / ECONOMY — 타겟 FPS 요약

HIGH는 후속 제공된 로그 23개를 집계했고, PERFORMANCE/ECONOMY는 사용자가 제공한 original 비교 요약을 보존했다. **HIGH는 별도 실행이며 동일 조건·sample window는 확인되지 않았다.** 세 mode를 한 표에 표시하되, 서로 다른 run의 sample을 합산하거나 mode 간 개선율을 계산하지 않는다. PERFORMANCE/ECONOMY의 canonical raw log와 sample window는 복원하지 못했다. HIGH의 집계 방법은 §6.2에 기록한다.

| Mode | Mean FPS | Median FPS | Avg below 59 FPS | Minimum FPS |
|---|---:|---:|---:|---:|
| HIGH | 50.21 | 53.98 | 42.76 | 25.41 |
| PERFORMANCE | 약 49.10 | 53.23 | 42.29 | 27.92 |
| ECONOMY | 48.87 | 52.59 | 42.98 | 30.21 |

PERFORMANCE/ECONOMY 출처: 당시 사용자 요약 원문 (로컬 자료: `/home/bowonryuubuntu/.codex/attachments/f680f8c7-de0c-45fb-851c-5201867e05f5/pasted-text.txt`), 이를 인용한 [task gating 분석](reveal-task-gating-analysis.T3s39Z/REPORT.md), 이번 종료 요청 (로컬 자료: `/home/bowonryuubuntu/.codex/attachments/00cfc413-4d21-4313-9fce-c1c6af4284d3/pasted-text.txt`). 여러 문서에 인용되어 있다는 사실은 독립된 여러 측정이 있다는 뜻이 아니다. HIGH의 제공 FPS 목록은 부록 A.1에 보존한다.

PERFORMANCE/ECONOMY 요약에서 확인 가능한 범위와 남는 제한:

- 대상은 사용자 타겟의 Text Effect Demo, **demo optimize와 구분된 original 비교**다.
- 해당 요약의 정확한 타겟 모델/SoC, 해상도, 설치 revision, 반복 수, sample 수, 시작·끝 timestamp, Sync/Async 선택 기록은 확보되지 않았다. 같은 조건으로 시험한 비교라는 사용자 설명은 보존하되, 로그로 검증했다고 쓰지 않는다.
- 샘플의 Strong 기본값은 entrance R24/exit R48이지만, **이 FPS run의 실제 버튼 상태와 동일하다고 단정하지 않는다.** 별도 Tizen 10.1 ARMv7l trace의 조건도 이 표로 옮겨 붙이지 않는다.
- “전체 평균”의 전체가 Cards entrance만인지, 수동 장면 전환/대기를 포함하는지와 frame/time weighting은 확인되지 않았다. `Avg below 59`도 표기된 FPS sample의 조건부 평균이지 느린 개별 frame time이나 1% low로 해석하지 않는다.
- 이후 raw 재계산에서 PERFORMANCE 평균이 달랐다는 기록은 종료 요청에 명시되어 있다. 그러나 **그 수치와 선택 window를 입증할 원본을 찾지 못했다.** 차이를 숨기거나 임의로 한 값을 canonical 재계산값으로 선택하지 않는다. 해당 결과는 별도 run/window의 미확인 자료로 남긴다.

이번 조사는 기존 연구 보고서, 첨부 원문, Downloads의 trace/app.log를 확인했다. 발견한 `ryu-cmd-*` 및 `trace/old/ryu-*`는 Production/Source-only 계측 자료이며, 이 original PERFORMANCE/ECONOMY FPS 원본이 아니다. 그 로그에 없는 FPS를 trace iteration 수로 만들어 대체하지 않았다. 원본 경로를 추가 요청했지만 보고서 작성 시점까지 확보하지 못했다.

**해석:** 보존된 요약에서는 ECONOMY의 평균·중앙값이 개선되지 않았고, below-59 평균·최저값은 조금 나았다. 이것만으로 통계적 동등성이나 유의한 악화를 증명할 수는 없다. 그러나 기대했던 “뚜렷하게 더 빠른 low-cost mode”를 뒷받침하는 결과도 아니다. 양쪽 모두 안정적 60 FPS라는 요구와 거리가 있다는 현장 관찰에 부합한다. 정확한 개선율·신뢰구간은 제시하지 않는다.

### 6.2 HIGH 로그 집계 방법 — 별도 실행

사용자가 HIGH FPS라고 지정하여 추가 제공한 PID 18964 로그의 **23개 `FPS:` 값 전체**를 사용했다. 선택 window는 제공된 첫 sample 59.99부터 마지막 sample 60.00까지이며, 60 FPS 및 1.1초로 표시된 구간도 제외하지 않았다. 원본 전체 실행에서 어느 장면·구간을 발췌했는지는 확인되지 않았다.

평균은 로그에 인쇄된 FPS 23개의 **비가중 산술평균**(합 1,154.93 / 23), 중앙값은 53.98이다. 59 미만 sample은 13개이며 그 평균은 42.7630769…이다. 소수 둘째 자리로 반올림했다. 최소값은 로그 구간 평균의 최솟값이지 순간 FPS나 1% low가 아니다. 반올림된 `elapsed time`과 frame count로 전체 실행의 시간 가중 FPS를 재구성하지 않았다. 복사 과정의 PID/`Frame count` 문자열 누락이 있는 행도 `FPS:` 값은 명확하므로 포함했다. 재계산용 값은 부록 A.1에 보존한다.

**PERFORMANCE/ECONOMY와는 별도 실행 자료다.** HIGH라는 mode는 확인됐지만 해상도·기기 상세·revision·Strong 설정·Sync/Async·scene window의 일치는 확인되지 않았다. 따라서 50.21 대 49.10/48.87을 근거로 HIGH가 더 빠르다고 순위를 매기거나 mode 간 개선율을 계산하지 않는다. 이 HIGH 로그에서도 낮은 FPS 구간이 있었다는 사실까지만 확인한다.

### 6.3 다른 관측과 계측은 별도 근거로 유지

이전 타겟의 V-only는 소폭 개선, H/V 1-tap은 V-only와 거의 비슷, Source-only는 대부분 60 FPS에 가까우나 간헐적 하락이라는 사용자 관측이었다. 이는 서로 다른 diagnostic 실행이며 위 표의 sample에 합치지 않는다. Source-only는 실제 blur를 제거한 진단으로, 60 FPS의 대체 제품 구현이 아니다.

HIGH의 집계 방법과 별도 실행 조건은 §6.2에 기록했으며, 일반 BlurEffect의 직접 비교 가능한 original FPS 통계는 찾지 못했다. BlurEffect가 더 잘 동작했다는 타겟 관찰은 있으나 항상 60 FPS였던 것도 아니다. 또한 Label 전체의 BlurEffect와 시퀀스별 Reveal blur는 처리 영역과 동작이 같지 않다.

별도 Tizen 10.1/ARMv7l trace는 지속 offscreen command CPU와 전환 시 FBO 초기화 부담을 확인하는 데 유효했다. 하지만 계측 ON이 약간 느리다는 사용자 관찰이 있었고 OFF/ON 비용을 정량 검증하지 못했다. **그 CPU ms를 위 FPS와 합치거나 무계측 production 비용으로 제시하지 않는다.** PC GPU timer 결과도 타겟 FPS로 환산하지 않는다. 구체적인 환경·window·수치는 부록 A에 분리한다.

## 7. Visual Quality Comparison

| Mode | 확보한 가치 | 남은 trade-off |
|---|---|---|
| HIGH | strong blur, 한글 획, 전환의 가장 안정적인 reference | 비용이 큼. offscreen capture/MSAA 차이까지 없애는 것은 아님 |
| PERFORMANCE | reduced 경로 중 전체적인 균형이 좋고 한글 fade/blur→sharp가 ECONOMY보다 자연스러움 | 일부 strong blur의 미세 grid와 축소 sampling 한계 |
| ECONOMY | 일부 타겟 장면에서 blur 자체의 fine-grid가 덜 거슬림 | 한글 전체 fade/전환이 더 부자연스럽고 줄·획 modulation, sharp/blur 합성의 별도 인상 |

품질 판단은 “얼마나 흐린가” 하나만 보지 않았다. blur의 공간적 균일성, 획이 복제된 듯한 패턴, 줄 간 밝기 차이, animation 중 shimmer/pop, 선명해지는 시점과 fade를 함께 봤다. 같은 sigma나 에너지 합을 맞췄다고 같은 지각 품질이 되지는 않았다.

ECONOMY는 더 싼 PERFORMANCE와 동일한 외형을 보장하는 모드가 아니다. 특히 radius/gamma 조정은 blur가 남아 보이는 시점도 바꾸므로 비용 외에 UX 인상까지 달라졌다. 최종 gamma=1은 기존 native timing에 가까운 비교를 위한 선택이며, quarter-Y에서 잃는 공간 정보를 복원한 것은 아니다.

Source/Output batching처럼 기존 의미를 보존하는 최적화와, prefilter/direct 2D/perceptual mix처럼 품질 contract가 달라지는 연구를 구분했다. 외부 품질 PoC에서 탈락한 후보를 FPS가 좋다는 이유로 무조건 채택하지 않았다. 한편 A-R은 초기 strict 품질 gate 뒤 사용자 검토를 거쳐 성능·production 후보 단계까지 다시 평가했으나, 최종 구조로 남지 않았다. 각 단계의 판단을 부록에 시간 순서대로 보존한다.

ImageSpan에는 일반 화면 MSAA와 single-sample offscreen capture 사이의 경계 coverage 차이가 남았다. 이를 좌표 오류나 종료 타이밍 오류로 일반화하지 않았고, 무리한 보정이나 MSAA 정책 변경도 최종 해법으로 삼지 않았다. None으로 ordinary rendering으로 돌아갈 때도 모든 pixel이 동일하다고 약속하지 않는다. [lifecycle/appearance 한계](reveal-lifecycle-structure.MYQiQb/REPORT.md).

## 8. Why the Feature Is Not Applied

### 필터 계산량 감소와 타겟 frame time 감소는 같은 문제가 아니었다

**확인된 사실:** PERFORMANCE/ECONOMY의 texture extent와 filtering workload는 작아졌지만 page당 Source/H/V 세 stage는 남는다. 여러 Label이 전환될 때 task/FBO 생성과 실행, RenderPass, command-buffer 처리, dependency synchronization 같은 비용도 누적된다. 타겟 trace는 그중 offscreen command 및 FBO 초기화가 큰 부담임을 보여줬다.

**Engineering interpretation:** 이 타겟/UX에서는 Gaussian tap만 줄이는 접근으로 제거되지 않는 pipeline 비용이 중요하다. V-only와 1-tap의 제한적 효과, H/V bundle을 제거한 Source-only의 큰 변화가 이 판단을 지지한다. 다만 Source-only는 task뿐 아니라 renderer/constraint/binding도 함께 제거한다. **RenderTask 개수가 100% 원인이라는 결론은 아니다.** 실제 BlurEffect와 Reveal의 정상 퇴장은 task 수가 같아도 동작 성능이 달랐다.

glFlush coalescing도 조사했지만 이는 **PC에서 수행한 별도 실험**이다. flush 횟수를 크게 줄였어도 전체 CPU 개선이 명확하지 않아 타겟 해법으로 올리지 않았다. 이 결과를 타겟에서 glFlush의 영향을 완전히 배제한 증거로 쓰지 않는다.

### 안전하게 줄일 수 있는 것과 동작을 깨는 것을 구분했다

Source/Output batching, coverage geometry, A8 처리, page/scratch 최적화는 구조적 낭비를 줄였다. 그러나 추가 stage 제거는 sparse sampling artifact를 만들거나, Source의 의미를 shader에서 반복 재구성하면서 GPU 비용을 늘렸다. 자동 task sleep/wake는 현재 API에서 same-frame reverse/seek를 보장할 수 없었다. clear 제거는 실제 line-boundary contamination을 만들었다. packing 완화는 command 비용을 줄이는 대신 메모리와 reduced sampling phase를 바꿨다.

correctness/lifecycle은 성능과 바꾸지 않았다. None, async stale publication, ImageSpan readiness·교체, owner 파괴, shutdown 및 자원 회수 검증을 수행했다. 마지막 ECONOMY 격리 단계의 기존 기록은 전체 UTC **3613/3613**, focused ASAN/LSAN **7/7**, HIGH/PERFORMANCE 비교 **10/10 identical**이다. 이는 해당 revision과 host 범위의 결과이며, 이번 문서 작업에서 재실행한 검증이나 모든 타겟의 무결함 보증은 아니다.

### 확보한 연구 결과와 남은 한계

HIGH 품질 reference, 두 reduced-resolution 경로, 실제 page/task/memory inventory, 타겟 CPU attribution, filtering과 pipeline 비용을 분리하는 diagnostic, 여러 품질/구조 후보의 중단 근거를 남겼다. 구현 자체나 테스트 기반이 없어서 중단하는 것이 아니다.

현재까지의 작은 최적화로 제품 목표를 만족할 근거는 부족하다. 더 큰 변화는 shared render-target/task scheduling, 안전한 activity/dirty 처리, 다른 Source 표현 등 renderer architecture를 바꾸는 과제다. 그런 설계를 검증하지 않은 채 새로운 cache나 public quality enum을 제품 해결책으로 약속하지 않는다.

## 9. Final Decision

**Text::Reveal Gaussian blur is not integrated as a product feature.**

HIGH는 원하는 품질을 제공하지만 시험 타겟에 적용하기에는 비용 부담이 크다. PERFORMANCE는 filtering과 메모리를 상당히 줄였으나 요구한 안정적인 타겟 성능을 확보하지 못했다. ECONOMY는 이를 더 줄였지만 PERFORMANCE 대비 명확한 타겟 성능 이득이 없었고, 특히 한글 fade/blur→sharp 전환에서 추가 품질 손해가 있었다.

따라서 평가한 어느 경로도 **acceptable quality + required target performance**를 함께 충족하는 제품 선택지가 되지 못했다. 이번 개발은 **적용 불가 / development stopped**로 종료한다. 모든 기기나 모든 blur 구현이 불가능하다는 의미는 아니다.

아직 출시하지 않은 `BlurRadius`, `BlurDurationRatio`, `BlurQuality`와 `HIGH / PERFORMANCE / ECONOMY`를 이 작업의 public product API로 출시하지 않는다. PERFORMANCE라는 이름의 성능 기대와 ECONOMY의 비단조적인 품질 절충을 현재 backend 구조에 고정하지 않기 위한 결정이다. **연구 branch에 선언과 구현이 존재하는 사실과 출시 결정은 별개이며, 이 문서 작업에서 코드를 삭제하지 않았다.** blur 없는 기존 Reveal의 적용 여부와도 구분한다.

후속 검토가 있다면 새로운 renderer architecture와 새 제품 요구사항을 기준으로 별도 판단해야 한다. 같은 Gaussian-kernel tuning과 기존 탈락 PoC를 근거 없이 반복하지 않는다.

---

# Appendix / References

아래는 기존 연구 결과의 인덱스다. **당시 후보 판정 → 후속 검증 → 최종 유지/중단**을 구분한다. 예전 보고서의 READY/PROMISING은 그 단계의 판정이며 위 최종 결론을 대체하지 않는다. 수치는 별도 명시가 없으면 각 링크의 환경·revision·범위에만 적용된다.

## A. Target Profiling

### A.1 Baseline 및 pipeline inventory

- **PERFORMANCE / HIGH / BlurEffect 관측:** 실제 데모 전환의 비용을 비교하는 목적. BlurEffect가 더 나았다는 사용자 관찰과 후속 HIGH 23-sample 통계(§6.2)를 구분하며, PC GPU/CPU와 타겟 FPS를 합치지 않는다. **Verdict: 정성 관찰·별도 실행·정량 측정을 분리.** [방향 재평가](reveal-direction-review.QxzXQ5/REPORT.md), [1-page 감사](reveal-onepage-analysis.RepMKt/REPORT.md).
- **Task/FBO/page inventory:** 1280×720, Sync, entrance R24/exit R48의 실제 데모를 조사. Cards entrance 12 Labels에서 BlurEffect **36 tasks**, Reveal **45**; 정상 WHOLE_TEXT 퇴장은 양쪽 **36**. 전체 정상 퇴장은 양쪽 **57**. **Verdict: task 수 차이만으로 BlurEffect의 이점을 설명할 수 없음.** [실제 inventory](reveal-pipeline-count.a9GqcW/REPORT.md).
- **BlurEffect의 비교 범위:** 이 데모의 Strength animation은 full-resolution H/V를 사용하는 경로다. 별도 static BlurEffect의 축소 정책과 혼동하지 않는다. Label 전체 capture와 Reveal foreground/시퀀스 효과의 품질·halo도 동등하지 않다. [비교 및 제한](reveal-demo-compare.jsbHIi/REPORT.md).

HIGH 추가 로그의 재계산용 `FPS:` 값(사용자 제공 순서, 23개):

```text
59.99, 60.00, 53.98, 53.72, 39.93, 60.00, 60.00, 60.00,
60.00, 34.40, 36.36, 39.38, 54.32, 59.02, 33.08, 48.75,
60.00, 60.00, 50.22, 25.41, 36.40, 49.97, 60.00
```

제공 로그의 첫/마지막 행 접두사는 `33483` / `89737`이다. 이 숫자의 단위·의미는 확인되지 않아 실행 시간으로 환산하지 않았다. 첫/마지막 FPS는 각각 59.99 / 60.00이다. 새 타겟 실행 없이 제공 로그만 집계했다.

### A.2 타겟 coarse attribution과 detailed attribution — 서로 다른 run

대상은 Tizen 10.1 ARMv7l용 계측 패키지다. Core `ecf444c414…`, Adaptor `f451029c14…`, UI 기준 `4d74bbf9…`에 당시 Blur 패치를 적용했다. 현재 host HEAD와 다르다. Source-only는 H/V를 제거한 진단이며 제품 blur가 아니다. [패키지 기준](reveal-trace-tizen101.prtWoU/REPORT.md).

| 별도 측정 / window | Production | Source-only | 확인한 사실 |
|---|---:|---:|---|
| Coarse trace, CardsStart 후 1–3초 × 4회, Update/Render CPU | 23.69 ms/frame | 9.17 ms/frame | setup 이후에도 차이가 지속됨 |
| 같은 coarse run, offscreen command CPU | 16.62 ms/frame | 4.81 ms/frame | offscreen command에 큰 차이 |
| Detailed trace, CardsStart 후 1–3초 × 4회, offscreen command CPU | 22.275 ms/frame | 7.089 ms/frame | 더 세분한 계측의 별도 값 |
| Detailed trace, 정상 퇴장 setup create queue CPU | 46.666 ms | 11.372 ms | FBO/resource 초기화 spike |

Coarse와 detailed 행을 합산하거나 동등한 overhead로 간주하지 않는다. Coarse의 중간 구간은 269/480 frames, detailed는 215/466 iterations로 sample 수도 다르다. detailed의 exclusive DependencySync 차이 **6.002ms**와 RenderPass 차이 **3.285ms**는 command CPU 차이의 약 61%였다. 퇴장 FBO queue 차이 **34.173ms**는 create-queue 차이의 약 97%였다.

이 범주에는 DALi/driver bookkeeping, command 처리와 동기화 관련 CPU가 포함된다. GPU Gaussian 실행시간, 단일 `glGenFramebuffer` 시간 또는 fence 대기만을 의미하지 않는다. GPU 시간은 미측정이며 OFF/ON overhead도 정량 확정하지 못했다. **Verdict: render/driver-side CPU와 resource setup이 중요하다는 attribution; 특정 driver 결함이나 단일 GL 함수가 최종 root cause라는 증명은 아님.** [Coarse](reveal-target-attribution.01tPcM/REPORT.md), [Detailed](reveal-command-results.BftCT1/REPORT.md).

### A.3 PC 비교 및 PC→Target 예측

- **BlurEffect vs PERFORMANCE 정상 퇴장:** PC GTX1650/NVIDIA595.91.07, 1280×720, Sync, R48, WHOLE_TEXT/Fade1/Linear 0.4초, 19-Label Results, warm-up 후 3회. draw GPU **1.335→0.865ms/frame**, 관련 texture payload **6.348→3.840MiB**지만 process 누적 CPU **182.23→334.82ms**였다. adaptive 포함 당시 baseline이며 최종 세 mode benchmark가 아니다. **Verdict: GPU/storage 이득이 전체 CPU 이득을 보장하지 않음.** [방법·원자료](reveal-demo-compare.jsbHIi/REPORT.md).
- **위 exit CPU 추가 분석:** 별도 PC sampling에서 offscreen uniform-buffer `glMapBufferRange` 내부 NVIDIA CPU wait-loop에 차이가 집중됐다. 정상 퇴장은 WHOLE_TEXT여서 PER_LINE raster 증가로 설명되지 않았다. **Verdict: 그 host 실행의 동기화 hotspot; TV와 모든 driver에 일반화 금지.** [CPU attribution](reveal-exit-cpu.VyPki6/REPORT.md).
- **PC→Target predictability:** Update, Draw, FBO 초기화의 PC/TV 비율이 서로 크게 달라 전역 배율 K가 성립하지 않았다. PC는 correctness·구조·방향성 screening에 유용하지만 절대 FPS 외삽에는 부적절하다. **Verdict: 구조적 count와 타겟 trace를 함께 사용.** [분석](reveal-backend-predictability.z457tX/REPORT.md).

## B. Pipeline Reduction Experiments

- **Initial PERFORMANCE / axis-aware Late Smooth:** full Source를 보존하고 H의 Y를 유지하여 early two-axis reduction보다 전환 품질을 개선. **Verdict: 최종 PERFORMANCE의 기준 구조로 유지; half-rate/진단 switch는 최종 기본 경로가 아님.** [품질 API 및 초기 비교](reveal-blur-quality.pbOxDT/report.ko.md).
- **Output batching:** 호환되는 연속 line output을 묶어 renderer/draw 비용 절감. 당시 1,308개 A/B 캡처와 ImageSpan 126개 캡처가 byte-identical. **Verdict: 채택. 세 task/page 자체는 유지.** [측정](reveal-perf-opt.upcgEj/REPORT.md), [정리](reveal-production-cleanup.iVbXZv/REPORT.md).
- **D2 H geometry optimization:** H의 가로 convolution에 필요한 Y coverage band만 raster하여 빈 세로 halo의 Gaussian 작업을 줄임. Source/V, H FBO extent와 clear 범위는 그대로. **Verdict: HIGH/PERFORMANCE에 유지; ECONOMY에는 같은 band를 적용하지 않음.** 독립 초기 D2 report는 이번 보존 자료에서 찾지 못해 [후속 source/cost audit](reveal-cost-audit.0jBjH2/REPORT.md)과 [최종 정책](reveal-economy-production.yd7Ikq/REPORT.md)을 근거로 사용한다. 초기 개선률은 복원하지 않는다.
- **A8 Source batching PoC:** 입력 atlas로 Source draw 감소. 초기 FHD host CPU 증가 때문에 보류했으나 후속 반복에서 동일한 증가가 재현되지 않았고 medium Label 이득을 확인했다. **Verdict: 초기 No-Go를 그대로 일반화하지 않고 후속 구현으로 진행.** [초기](reveal-source-atlas.3igp9v/REPORT.md), [follow-up](reveal-atlas-followup.y6Uy33/REPORT.md).
- **Batch text reveal blur sources / production Source batching:** CPU isolated plane을 atlas 입력으로 대체하고 최대 64개의 호환 quad를 묶음. FHD Source draws 31→6, medium 48→8 / 112→8. 기존 개별 입력 texture를 atlas와 중복 유지하지 않음. **Verdict: 채택; FHD CPU 개선률은 큰 편차로 미확정.** [production·lifecycle·측정](reveal-source-production.hWQHqE/REPORT.md).
- **V-only:** final Source read/Late Smooth 비용을 분리하기 위해 Output에서 V만 읽음. S/H/V task/FBO는 유지. 타겟 소폭 개선이라는 사용자 관찰. **Verdict: final Source read만으로 큰 병목을 설명하지 못함; 제품 후보 아님.** [최초](reveal-vonly-diagnostic.kVClQY/REPORT.md), [local 적용](reveal-vonly-local.WmHrK8/REPORT.md), [타겟 관측](reveal-direction-review.QxzXQ5/REPORT.md).
- **H/V 1-tap:** V-only 위에서 Gaussian을 center sample로 교체하되 stage를 유지. 타겟은 V-only 대비 거의 같은 성능이라는 관찰. **Verdict: 그 조건에서 tap/ALU만 줄이는 해법의 우선순위가 낮음. production 대비 완전 독립 A/B나 GPU 비용 0의 증명은 아님.** [구현](reveal-onetap-local.xAtm4Y/REPORT.md), [해석](reveal-direction-review.QxzXQ5/REPORT.md).
- **Source-only:** H/V Actor/Task/FBO/camera를 실제로 생성하지 않고 Source만 출력. 타겟 대부분 60 FPS, 간헐적 55–57, sync exit 약 52라는 사용자 관찰. **Verdict: H/V pipeline bundle 제거의 영향은 큼; blur 품질이 없으므로 대체 구현 불가.** [진단](reveal-sourceonly-local.l6cJM9/REPORT.md), [관측 범위](reveal-direction-review.QxzXQ5/REPORT.md).

## C. Filter / Blur Quality PoCs

### C.1 Gaussian tuning, adaptive 및 prefilter

- **Reduced tap CAP_12/8/6:** Gaussian 비용 절감을 시도했으나 큰 radius/촘촘한 한글에서 CAP_12도 grid가 보임. **Verdict: 기본 경로로 채택하지 않고 exact Gaussian 유지.** [staged 평가](reveal-perf-opt.upcgEj/REPORT.md), [실험 코드 정리](reveal-production-cleanup.iVbXZv/REPORT.md).
- **WHOLE_TEXT/Fade1 비용 분석:** fade 이동, Output fetch 분기, coverage crop, H-band를 비교. fade의 실수 연산 동등성이 8-bit 저장·겹친 ImageSpan의 동등성을 보장하지 않음. **Verdict: 제한적 후보만 식별; 일괄 Source 제거 승인 아님.** [분석](reveal-whole-analysis.buD2NQ/REPORT.md).
- **Fade-aware Adaptive Gaussian:** 제한된 정상 퇴장에서 PC GPU 평균 A8 13.1%, RGBA 18.1% 감소, 검증 지점 max 1 LSB. p=.2–.6의 높은 비용은 유지. **Verdict: 당시 타겟 PoC로 진행했으나 최종 기본 구조에 남기지 않음.** [연구](reveal-adaptive-study.8A0C8e/REPORT.md), [eligibility audit](reveal-adaptive-integration-audit.BfX6fU/REPORT.md), [적용 PoC](reveal-adaptive-integration.VWDc21/REPORT.md).
- **Adaptive mid-range follow-up:** 고정 kernel 집합의 fine progress 검사에서 1 LSB gate를 초과. **Verdict: 추가 kernel/분기를 늘리지 않고 중단; 새 GPU 성능 측정 없음.** [보고서](reveal-midrange-study.Uxvyif/REPORT.md).
- **Prefilter + low-resolution H/V:** PC draw 비용의 큰 절감 가능성을 확인했지만 별도 pass가 추가됨. B1 strong blur에는 촘촘한 세로 grid가 증가. **Verdict: 비용 잠재력과 품질 불합격을 함께 기록.** [초기 feasibility](reveal-idle-prefilter.o5LPsX/REPORT.md), [품질 gate](reveal-prefilter-quality.eYgUhH/REPORT.md).
- **Y-only prefilter A:** B1의 세로 grid는 CURRENT 수준으로 줄었으나 가로 band가 남음. **Verdict: 원인 분리 성공, 전체 품질 gate 미충족.** [Y-only](reveal-yfirst-quality.K64mLI/REPORT.md).
- **A-R vertical reconstruction / fused Y/2:** A-R은 band를 줄이지만 p=.75 획이 더 soft해짐. Y/2는 중간·후반이 좋아졌지만 p=.20 band가 더 강함. **Verdict: 당시 strict gate에서는 양쪽 중단.** [품질 결과](reveal-yrecon-quality.xHjb5S/REPORT.md).
- **A-R 후속 성능 및 native 구현:** 사용자 시각 검토 후 품질을 바꾸지 않고 비용을 다시 평가. PC native GPU draw 0.716→0.464ms/frame, 약 35.2% 감소; task/page 3→4, FBO payload +4.76%. 타겟에서는 더 무겁다는 후속 관찰. **Verdict: PC 성능 Go가 타겟 Go로 이어지지 않았으며 최종은 pre-A-R PERFORMANCE.** [feasibility](reveal-ar-feasibility.wIhiCC/REPORT.md), [native 후보](reveal-ar-production.FY6dsQ/REPORT.md), [후속 평가](reveal-backend-predictability.z457tX/REPORT.md).

### C.2 Direct 2D / single-pass 계열 — 서로 다른 후보

- **Vogel12:** full Source의 12개 sparse sample로 한 번에 blur. Soft16 한글부터 획 복제 형태가 보임. **Verdict: 품질 No-Go.** [보고서](reveal-vogel12-quality.0TCvfD/REPORT.md).
- **Dense 9×9 / 81-read:** 에너지·분산을 맞췄으나 Soft16/Strong24의 grid·stroke modulation이 남음. **Verdict: native 품질 No-Go; 실제 3→2 runtime/performance 단계 미진행.** [보고서](reveal-dense2d-quality.ECWtnl/REPORT.md).
- **Frequency-optimized 9×9:** 이전 9×9의 일부 개선에도 strong 구간 패턴이 남음. 11×11은 offline 참고만 수행. **Verdict: 품질 gate 미충족.** [보고서](reveal-frequency-quality.HryP4o/REPORT.md).
- **Independent-strength 9×9:** strength별 weights/offsets 독립 최적화로도 제한된 탐색 범위에서 CURRENT의 누설/출력 오차에 근접하지 못함. **Verdict: offline No-Go; GPU 성능 측정 없음.** [보고서](reveal-independent-quality.On1JuQ/REPORT.md).
- **1/3-resolution 7×7:** output 면적과 sample 수 절충을 시험했지만 강한 blur의 alias/leakage 기준 미충족. **Verdict: offline No-Go.** [보고서](reveal-resolution-tradeoff.OHZfwT/REPORT.md).
- **1/2-resolution 5×5:** 더 큰 output에 더 적은 sample을 써도 획 평균화 문제를 해결하지 못함. **Verdict: offline No-Go; 위 7×7과 별도 후보.** [보고서](reveal-resolution-tradeoff.OHZfwT/REPORT.md).
- **single8 reduced blur:** 실제 H Actor/Task/FBO를 제외한 두-stage 외부 PoC. ECONOMY timing/composition을 유지했으나 R16/R24/R48 초기 획 복제·격자 발생. **Verdict: stage 제거는 확인, 품질 No-Go.** [lifecycle/구조 연구](reveal-lifecycle-structure.MYQiQb/REPORT.md).

공통 교훈은 sparse direct sampling의 총 에너지·sigma만 맞춰서는 text의 고주파 획을 안정적으로 평균하지 못한다는 것이다. **시험한 후보들의 No-Go이지 모든 direct convolution의 수학적 불가능성 증명은 아니다.** offline kernel 결과를 실제 GPU animation 결과로 기술하지 않는다.

### C.3 ECONOMY 개발 순서

- **Quarter-quarter Phase 1:** blur-only의 엄격한 Gaussian fidelity 기준에서 한글 줄·획 modulation이 남음. **Verdict: 이 단계의 A 품질 gate 불합격, 당시 B 미구현.** [Phase 1](reveal-economy-quality.fxgYr6/REPORT.md).
- **Phase 2 perceptual Source+Blur:** strict fidelity 대신 mixed effect로 재평가. A의 BALANCED는 더 사용 가능했지만 artifact가 남았고 비교용 B는 이를 완화. **Verdict: A marginal, B는 품질 원인 비교용이지 자동 채택 아님.** [Phase 2](reveal-economy-phase2.2zaqod/REPORT.md).
- **R24/R32 및 gamma:** R32/gamma1.5 후보는 blur 존재감을 늘리고 PC exit GPU 이득을 보였으나 entrance 개선은 없었음. **Verdict: visual pass / PC mixed. 이것을 최종 authored radius/timing으로 오인하지 않음.** [refinement](reveal-economy-refine.eIvjxu/REPORT.md).
- **최종 gamma=1:** 기존 PERFORMANCE의 느낌·시점에 가까운 사용자 선택에 따라 authored radius/native timing으로 구현. generic Gaussian은 기존 base 그대로, 축소 V 계산은 text-private 내부로 격리. **Verdict: host 구현·회귀 완료; 최종 타겟 FPS/품질 결론은 본문 §6–9.** [구현](reveal-economy-production.yd7Ikq/REPORT.md), [격리](reveal-economy-isolation.PJQV3U/REPORT.md).

## D. RenderTask / FBO Experiments

- **Half-rate refresh:** Source/H/V 갱신을 줄이는 이전 실험에서는 그 FBO에 들어간 Reveal 진행도 함께 낮은 cadence가 될 수 있어 “Reveal 60 / blur 30”의 독립 효과가 아니었다. **Verdict: 최종 경로는 REFRESH_ALWAYS.** 독립 초기 측정 보고서는 이번 자료에서 복원하지 못해 정량 개선률을 쓰지 않는다. [정리 이력](reveal-blur-quality.pbOxDT/report.ko.md), 현재 runtime (로컬 자료: `dali/dali-ui/dali-ui-foundation/internal/visuals/text/text-reveal-runtime-blur.cpp`).
- **Idle cache / REFRESH_ONCE:** 실제 pass를 멈추고 texture를 보존하는 것은 native 진단으로 확인. 하지만 PropertyNotification→event setter로 같은 update의 reverse/seek wake를 보장할 수 없고 progress가 고정되어도 gradient/ImageSpan은 바뀔 수 있음. **Verdict: UI-only automatic gating 미적용.** [초기](reveal-idle-prefilter.o5LPsX/REPORT.md), [최종 분석](reveal-task-gating-analysis.T3s39Z/REPORT.md).
- **Visibility/opacity gating:** Actor visibility로 draw를 없애도 clear-only pass가 남을 수 있음. 공유 scratch의 consumer와 wake 순서도 고려해야 함. **Verdict: pass sleep의 안전한 대체가 아님.** [task gating](reveal-task-gating-analysis.T3s39Z/REPORT.md).
- **Bounded glFlush coalescing:** PC private backend에서 flush entrance 54→2, exit 58→2. fence/wait/pass 수는 유지. offscreen command CPU entrance 1.7896→1.7901ms, exit 2.1159→2.0196ms이나 전체 render CPU의 명확한 이득은 없음. **Verdict: PC에서 target escalation 보류; 타겟 실측 결과로 기술하지 않음.** [PoC](reveal-flush-poc.2JyKvS/REPORT.md).
- **Clear removal:** V target을 garbage로 채운 native 비교에서 PERFORMANCE/ECONOMY 줄 경계에 흰색/마젠타 선이 생겼고 p=0에서도 관찰. H도 partial geometry/shared scratch 때문에 full overwrite를 가정할 수 없음. **Verdict: clear 제거 unsafe, production clear 유지.** [실험](reveal-lifecycle-structure.MYQiQb/REPORT.md).
- **기존 자원 절감 감사:** line crop, A8, CPU prepared payload 해제, 같은 extent의 H scratch 공유 및 V의 기존 line-local geometry를 확인. 추가 V halo crop이나 hidden ordinary texture 제거를 검증된 무위험 이득으로 승격하지 않음. **Verdict: 확인된 기존 절감은 유지, ownership/재생성 비용을 무시한 제거는 하지 않음.** [cost audit](reveal-cost-audit.0jBjH2/REPORT.md), [topology audit](reveal-topology-analysis.Thk4x4/REPORT.md).

### 미구현 / future architecture work

다음은 논의된 방향이며 **NOT IMPLEMENTED / FUTURE ARCHITECTURE WORK**다. 현재 제품 해법이나 검증된 성능 개선으로 제시하지 않는다.

- 여러 page/Label을 묶는 shared render-target atlas와 submission 구조. 기존 Source **입력** atlas batching과 다른 과제다.
- animation/constraints와 같은 update에서 판단하는 generic RenderTask activity/dirty scheduling.
- CPU exponential/box blur 또는 blur-response cache. animated Source의 내용·progress 변화, 저장량, 업로드와 무효화 비용을 새로 검증해야 한다.
- glyph-atlas/static-text 기반 Source 표현. ImageSpan, gradient, ownership metadata와의 관계를 포함한 별도 설계가 필요하다.

관련 제한·기존 제안: [topology](reveal-topology-analysis.Thk4x4/REPORT.md), [idle/cache](reveal-idle-prefilter.o5LPsX/REPORT.md), [wake contract](reveal-task-gating-analysis.T3s39Z/REPORT.md). CPU blur 등의 항목에는 이번 보존 자료에서 정량 구현 보고서가 없으므로 측정 결과를 붙이지 않는다.

## E. Page Packing Experiments

**Global threshold sweep:** 실제 Cards의 짧은 마지막 줄 때문에 발생한 split을 줄이는 연구. 1.25는 occupied area 대비 overhead 25%이며, page 자체의 빈 면적 25%라는 뜻은 아니다. 줄별 shader 의미는 유지했지만 reduced texture의 ceil/texel phase는 달라졌다.

| Policy | Cards pages | Offscreen tasks | PERFORMANCE FBO bytes | ECONOMY FBO bytes |
|---|---:|---:|---:|---:|
| BASE 1.25 | 15 | 45 | 603,494 | 517,635 |
| A 1.2623 | 14 | 42 | 623,843 | 535,077 |
| B 1.4656 | 13 | 39 | 645,239 | 553,374 |
| C 1.5557 | 12 | 36 | 669,664 | 574,224 |

이 표는 Ubuntu actual inventory, 같은 12-Label/R24 fixture의 논리 FBO payload다. PC command CPU는 page/task 감소에 따라 낮아지는 방향이 있었지만 전체 thread CPU가 모든 조합에서 같은 비율로 좋아진 것은 아니다. 큰 merge에서는 ECONOMY의 line-local 밝기/profile 차이도 커졌다. **Verdict: global threshold 완화 미채택.** [sweep·품질·CPU](reveal-packing-sweep.7E5egr/REPORT.md).

**Bounded absolute-waste merge:** global ratio만 풀지 않고 추가 allocation에 상한을 둔 후보. 195 fixtures×4 caps, Cards 15→14/45→42, payload 약 +3.37%. PERFORMANCE/ECONOMY 전체 UpdateRender CPU 개선은 확인하지 못했다. **Verdict: BOUNDED MERGE SAVINGS TOO SMALL; production 1.25 유지.** [후속](reveal-source-elision.dw5WOw/REPORT.md).

**Memory admission:** 초기 estimator의 `줄 수 × 전체 면적` 중복을 정리하고 실제 FHD wrap fixture로 정책을 점검했다. 역사적으로 128MiB 검토 뒤 256MiB 정책으로 정리되었으며, admission estimate를 실제 VRAM/전체 프로세스 상한으로 해석하지 않는다. **Verdict: 안전성 정책과 실측 메모리를 구분.** [estimator](reveal-memory-quality.InrGZf/REPORT.md), [128MiB 단계](reveal-cost-audit.0jBjH2/REPORT.md), [FHD 비용](reveal-fhd-native.dHnAFY/REPORT.md), [256MiB 정리](reveal-source-atlas.3igp9v/REPORT.md).

## F. Lifecycle / Reveal::None Experiments

- **Ownership/resource audit:** weak owner, foreground borrow/restore, candidate publication, async stale rejection, ImageSpan occurrence/renderer identity와 shutdown 경계를 점검. 반복 종료 후 tracked private handles/task가 회수되는지와 sanitizer를 확인. **Verdict: 검사 범위에서 추가 lifecycle 수정 불필요; driver 내부 VRAM/RSS까지 무누수 증명한 것은 아님.** [감사](reveal-lifecycle-audit.ZfWQqF/REPORT.md), [최종 ECONOMY 회귀](reveal-economy-isolation.PJQV3U/REPORT.md).
- **Application-lifecycle early cleanup:** shared Animation의 마지막 Label까지 기다리지 않고 완료한 Label부터 None을 적용하는 sample-only Timer/deadline 연구. 일부 타겟 run에서 개선 신호가 있었다는 사용자 기록은 있으나 original mode 통계와 합치지 않는다. **Verdict: 알고리즘 개선이 아니라 활성 runtime 수명 감소 실험.** [구현·제한](reveal-demo-cleanup.a4F90T/REPORT.md).
- **Scheduler 정리:** per-Label deadline/Timer/weak 목록 등 복잡성을 제거하고 shared/single `Animation::FinishedSignal` cleanup으로 복귀. already-None guard와 기존 lifecycle/animation identity guard 유지. **Verdict: 최종 sample은 단순한 완료 cleanup; HIGH/PERFORMANCE/ECONOMY × sync/async host 6/6 기록.** [복귀 검증](reveal-packing-sweep.7E5egr/REPORT.md), [최종 commit 기록](reveal-source-elision.dw5WOw/REPORT.md).
- **None 사용 contract:** 단발 entrance가 progress=1로 끝나고 reverse/seek/loop 계획이 없을 때 해제 가능. exit 후에는 hide/unparent가 먼저다. visible progress=0에서 None을 주면 ordinary foreground가 다시 나타날 수 있다. async publication과 GPU retirement는 setter 안에서 동기 완료되지 않는다. **Verdict: 앱의 효과 종료 의도와 자원 해제를 구분.** [전체 matrix](reveal-lifecycle-structure.MYQiQb/REPORT.md).

Early cleanup의 “demo optimize” FPS는 본문 original 표에 포함하지 않았다. 해당 target raw/window가 이번 자료에서 복원되지 않아 개선률을 만들지 않았다. 최종 단순 cleanup 결과를 이전 Timer 방식의 성능으로 포장하지도 않는다.

## G. Direct Source Experiments

### G.1 A8 Source-pass elimination — 초기 구조/품질 gate

목표는 기존 Source capture/task/FBO를 없애고 A8 atlas coverage+metadata에서 같은 Source 값을 H 및 Output shader 안에서 재구성하는 것이었다. full Source texture를 단순히 원본 glyph texture로 치환한 것이 아니다. **네 texel center에서 Reveal visibility를 평가한 뒤 bilinear 합성**하여 기존 filtering 순서를 근사했다.

실제 Cards의 eligible 8 pages에서 task/FBO **45→37**. PERFORMANCE FBO bytes **603,494→316,445**, ECONOMY **517,635→230,586**. 나머지 7 pages는 기존 fallback이었다. 시험 품질 행렬의 max 차이는 **1–2/255**이고 command/offscreen CPU 절감 신호도 있었다. **Verdict: 구조적 가능성 확인, GPU 비용·production lifecycle 검증 전의 PoC.** [초기 연구](reveal-source-elision.dw5WOw/REPORT.md).

### G.2 GPU gate — 최종 STOP

PC GTX1650/NVIDIA595.91.07/GLES, ECONOMY A8 multiline 4 Labels, PER_LINE/Fade0, p=.20. 독립 process 각 arm 5회, sparse 비동기 64-bit GPU query로 **Source/H/V clear+draw 및 Output draw**를 합산한 값이다. 전체 frame time이나 타겟 FPS가 아니다.

| Radius | BASE GPU | DIRECT GPU | 증가 |
|---|---:|---:|---:|
| 16 | 0.243 ms | 0.322 ms | +32.5% |
| 24 | 0.287 ms | 0.485 ms | +68.8% |
| 48 | 0.331 ms | 0.704 ms | +112.3% |

세 조건 모두 실행별 평균 범위가 겹치지 않았다. R24에서 Source **78.10µs**를 제거했지만 H가 **102.09→343.30µs**, Output이 **28.53→57.93µs**로 증가했다. H의 각 Gaussian lookup마다 coverage/metadata 재구성이 반복되어 절약한 Source 비용을 넘어섰다.

실제 ECONOMY Cards의 eligible pages GPU 중앙값도 **+16.2%**였다. Cards 산술평균은 긴 query 지연의 영향으로 불확실하여 성능 개선 근거로 사용하지 않았다. timer-OFF CPU 감소의 작은 평균 역시 실행 범위가 겹쳤다.

**Verdict: STOP SOURCE-PASS ELIMINATION.** GPU gate 불합격으로 계획했던 추가 lifecycle/binding productionization은 진행하지 않았다. 모든 Source 제거 설계의 불가능성 판정이 아니라 이번 virtual-Source 방식의 No-Go다. [최종 GPU 보고서](reveal-direct-gpu.VMDaVs/REPORT.md), [측정 방법](reveal-direct-gpu.VMDaVs/METHOD.md), [통계](reveal-direct-gpu.VMDaVs/STATISTICS.md).

## H. Detailed Report Index

아래 경로는 이 문서와 같은 연구 디렉터리 루트 기준의 실제 파일이다. 각 보고서 안에 raw logs, 소스/빌드 기준, 캡처, 계산 방법과 당시 미수행 범위가 있다. **이전 권고보다 본 문서의 종료 결정을 우선한다.**

### H.1 초기 구현·배치·안전성·메모리

| 보고서 | 목적 / 결과 / 최종 해석 |
|---|---|
| [Blur quality API 및 비교](reveal-blur-quality.pbOxDT/report.ko.md) | HIGH/PERFORMANCE 정리와 초기 host 비용 비교. 최종 제품 출시 승인 아님 |
| [PERFORMANCE staged optimization](reveal-perf-opt.upcgEj/REPORT.md) | Output batching 유효, reduced tap은 품질 한계 |
| [Production cleanup](reveal-production-cleanup.iVbXZv/REPORT.md) | 실험 경로 제거, batch 경계·description·host parity 확인 |
| [Lifecycle audit](reveal-lifecycle-audit.ZfWQqF/REPORT.md) | async/ImageSpan/reentry/파괴/shutdown ownership 및 회수 감사 |
| [Memory/quality review](reveal-memory-quality.InrGZf/REPORT.md) | 과도한 PER_LINE admission estimate 정리, zero-cost 품질 해법 미확인 |
| [Remaining cost audit](reveal-cost-audit.0jBjH2/REPORT.md) | 당시 128MiB 정책과 D2/V crop/CPU payload의 잔여 비용 조사 |
| [FHD native paragraph](reveal-fhd-native.dHnAFY/REPORT.md) | 실제 wrap 31/23줄 비용 확인, 128MiB estimate의 과도한 거부 확인 |
| [Source atlas PoC](reveal-source-atlas.3igp9v/REPORT.md) | 256MiB 정책 정리, 초기 atlas의 FHD CPU gate 보류 |
| [Atlas focused follow-up](reveal-atlas-followup.y6Uy33/REPORT.md) | CPU 재현성/medium Label ROI 점검, 후속 production 설계 근거 |
| [Production source batching](reveal-source-production.hWQHqE/REPORT.md) | atlas replacement·draw batching·lifecycle 구현, FHD CPU는 미확정 |

### H.2 Adaptive / prefilter / A-R

| 보고서 | 목적 / 결과 / 최종 해석 |
|---|---|
| [WHOLE_TEXT analysis](reveal-whole-analysis.buD2NQ/REPORT.md) | Fade1 퇴장 비용과 exactness 한계, 제한적 Output 후보 |
| [Adaptive study](reveal-adaptive-study.8A0C8e/REPORT.md) | 평균 GPU 감소 가능, 중간 peak는 해결하지 못함 |
| [Mid-range follow-up](reveal-midrange-study.Uxvyif/REPORT.md) | fine progress gate 위반, 추가 kernel 확대 중단 |
| [Adaptive integration audit](reveal-adaptive-integration-audit.BfX6fU/REPORT.md) | 실제 sample eligibility와 적용 범위를 분석 |
| [Adaptive target PoC](reveal-adaptive-integration.VWDc21/REPORT.md) | 조건에 맞는 정상 퇴장만 adaptive, 이후 최종 baseline에서는 제외 |
| [Idle / prefilter feasibility](reveal-idle-prefilter.o5LPsX/REPORT.md) | task suspension의 wake 한계와 prefilter 비용 잠재력 분리 |
| [Prefilter quality](reveal-prefilter-quality.eYgUhH/REPORT.md) | B1의 추가 grid로 중단 |
| [Y-only quality](reveal-yfirst-quality.K64mLI/REPORT.md) | 수직 grid 원인 분리, 가로 band 남음 |
| [Y reconstruction / Y-half](reveal-yrecon-quality.xHjb5S/REPORT.md) | A-R softness 및 Y/2 strong band의 trade-off |
| [A-R performance feasibility](reveal-ar-feasibility.wIhiCC/REPORT.md) | PC draw 비용 유망, 품질/상품 승인과 분리 |
| [A-R native candidate](reveal-ar-production.FY6dsQ/REPORT.md) | 4-stage 후보 구현·host 검증, 최종 구조로 미유지 |

### H.3 Demo, 타겟 attribution 및 backend

| 보고서 | 목적 / 결과 / 최종 해석 |
|---|---|
| [Demo BlurEffect comparison](reveal-demo-compare.jsbHIi/REPORT.md) | PC 정상 exit GPU/storage 이득과 CPU 증가를 각각 측정 |
| [Exit CPU attribution](reveal-exit-cpu.VyPki6/REPORT.md) | 해당 PC의 uniform-buffer mapping hotspot 확인 |
| [Upstream rebase](reveal-upstream-rebase.JeAqys/REPORT.md) | 당시 base/stack/샘플 API 충돌 정리 이력, 성능 근거 아님 |
| [Topology analysis](reveal-topology-analysis.Thk4x4/REPORT.md) | multi-page retained Source, setup/runtime 및 미래 후보 구분 |
| [One-page analysis](reveal-onepage-analysis.RepMKt/REPORT.md) | 같은 task 수와 추가 Output 합성/constraint를 분리 |
| [V-only external diagnostic](reveal-vonly-diagnostic.kVClQY/REPORT.md) | pre-A-R baseline에서 final Source read 제거 비교본 |
| [V-only local diagnostic](reveal-vonly-local.WmHrK8/REPORT.md) | 사용자 타겟용 local V-only 구현, 제품 후보 아님 |
| [H/V one-tap](reveal-onetap-local.xAtm4Y/REPORT.md) | Gaussian fetch/ALU 분리, stage 구조 유지 |
| [Source-only diagnostic](reveal-sourceonly-local.l6cJM9/REPORT.md) | H/V bundle을 실제 제거, blur 없는 비용 대조 |
| [Pipeline inventory](reveal-pipeline-count.a9GqcW/REPORT.md) | Label/page/task/FBO count를 실제 sample에서 확인 |
| [Direction review](reveal-direction-review.QxzXQ5/REPORT.md) | 타겟 관찰의 의미·한계를 정리하고 critical-path 계측으로 전환 |
| [Critical-path instrumentation](reveal-critical-path.CFOTks/REPORT.md) | RYU trace 설계와 host smoke, 당시 타겟 원인 미확정 |
| [Tizen 10.1 delivery](reveal-trace-tizen101.prtWoU/REPORT.md) | ARMv7l 기준 revision/GBS 패키지·측정 절차 |
| [Target coarse attribution](reveal-target-attribution.01tPcM/REPORT.md) | 지속 command CPU 및 setup spike 확인 |
| [Additional command instrumentation](reveal-command-attribution.O3eOCq/REPORT.md) | 세부 범주 분리를 위한 별도 계측/패키지 |
| [Target detailed results](reveal-command-results.BftCT1/REPORT.md) | DependencySync/RenderPass/FBO queue 비중, overhead 미확정 |
| [Backend predictability](reveal-backend-predictability.z457tX/REPORT.md) | same-context wait 기존 fast path 확인, 전역 PC→TV 배율 부정 |
| [glFlush PoC](reveal-flush-poc.2JyKvS/REPORT.md) | flush 수 대폭 감소에도 PC 전체 CPU 이득 불명확 |

### H.4 Direct filtering 및 ECONOMY

| 보고서 | 목적 / 결과 / 최종 해석 |
|---|---|
| [Vogel12 quality](reveal-vogel12-quality.0TCvfD/REPORT.md) | sparse direct blur의 획 복제, No-Go |
| [Dense 2D quality](reveal-dense2d-quality.ECWtnl/REPORT.md) | 9×9 energy/variance 정규화만으로 grid 해결 불가 |
| [Frequency quality](reveal-frequency-quality.HryP4o/REPORT.md) | 주파수 기준 9×9 개선 후에도 strong artifact 잔존 |
| [Independent strength quality](reveal-independent-quality.On1JuQ/REPORT.md) | strength별 독립 kernel 탐색도 제한된 gate 미충족 |
| [Resolution/tap trade-off](reveal-resolution-tradeoff.OHZfwT/REPORT.md) | 1/3 7×7 및 1/2 5×5 offline No-Go |
| [ECONOMY Phase 1](reveal-economy-quality.fxgYr6/REPORT.md) | quarter-quarter blur-only strict 품질 불합격 |
| [ECONOMY Phase 2](reveal-economy-phase2.2zaqod/REPORT.md) | perceptual Source/Blur 구성으로 재평가, 절충 잔존 |
| [ECONOMY refinement](reveal-economy-refine.eIvjxu/REPORT.md) | radius/gamma tuning의 시각 개선과 mixed PC 성능 |
| [ECONOMY production implementation](reveal-economy-production.yd7Ikq/REPORT.md) | authored radius/gamma1 구현과 trace/prototype 정리 |
| [ECONOMY isolation](reveal-economy-isolation.PJQV3U/REPORT.md) | generic Gaussian 보존, private kernel, 회귀·이력 정리 |

### H.5 종료 전 lifecycle / packing / Source 제거

| 보고서 | 목적 / 결과 / 최종 해석 |
|---|---|
| [Task gating analysis](reveal-task-gating-analysis.T3s39Z/REPORT.md) | same-frame wake 보장 불가, UI-only gating 중단 |
| [Demo early cleanup](reveal-demo-cleanup.a4F90T/REPORT.md) | sample-only per-Label deadline cleanup, 이후 단순 구조로 복귀 |
| [Lifecycle / structure](reveal-lifecycle-structure.MYQiQb/REPORT.md) | None 계약, Cards inventory, clear unsafe, single8 품질 실패 |
| [Packing sweep](reveal-packing-sweep.7E5egr/REPORT.md) | simple FinishedSignal 복원, 15→14→13→12 page 절충 조사 |
| [Source-elision feasibility](reveal-source-elision.dw5WOw/REPORT.md) | bounded merge 중단, A8 direct-source 초기 가능성 확인 |
| [Direct-source GPU gate](reveal-direct-gpu.VMDaVs/REPORT.md) | virtual Source 재구성 GPU 손해 확인, 최종 STOP |

### 보존 상태와 검증 범위

이 문서의 메모리 감소율은 보존된 정수 bytes로 다시 계산했다. HIGH FPS는 후속 제공된 23개 로그 값을 직접 집계했고 원시 FPS 목록을 부록 A.1에 남겼다. 그 외 표는 원 보고서 또는 명시한 사용자 요약의 수치이며 새로운 측정으로 표기하지 않았다. PERFORMANCE/ECONOMY original FPS 원본 미복원, HIGH의 별도 실행 조건, 독립 D2/half-rate 초기 보고서 미복원, PC/target 및 revision 차이를 명시했다. 모든 주요 후속 연구의 실제 보고서 경로를 위에 연결했다.

문서 작성 시작 시 UI/Core/Adaptor는 clean이었으며 소스·index·HEAD를 변경하지 않았다. 기존 보고서도 덮어쓰지 않았다. 새 산출물은 이 Markdown 하나다. build, UTC, 앱 실행, 성능/메모리 재측정, shader 수정, commit/amend/rebase/push는 수행하지 않았다.
