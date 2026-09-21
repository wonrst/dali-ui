# Target command/create attribution — Production vs Source-only

## 결론

**이번 계측 실행의 지속 비용 차이는 DependencySync와 RenderPass 범주에 가장 많이 모인다.**
**퇴장 직후 resource 생성 차이는 Framebuffer queue가 대부분을 차지한다.**

- 지속 offscreen command CPU: **22.275 vs 7.089 ms/frame**, 차이 **15.186ms**.
- DependencySync 차이 **6.002ms**, RenderPass 차이 **3.285ms**.
  두 범주가 command CPU 차이의 약 **61%**다. exclusive 시간으로 중복 제거했다.
- 퇴장 spike의 create queue CPU: **46.666 vs 11.372ms**.
  Framebuffer queue 차이 **34.173ms**로 parent 차이의 약 **97%**다.
- Animation/Update, uniform/buffer update만이 주 병목이라는 근거는 없다.
- **개별 GL 함수, GPU 실행 시간, 특정 드라이버 버그까지 확정한 결과는 아니다.**
- **OFF/ON 오버헤드는 검증하지 못했다.** OFF app.log에는 FPS/시간 자료가 없다.
  사용자는 동일 Production에서 **ON이 살짝 느리게 느껴졌다**고 확인했다(정성 관찰).
  이전 coarse 계측보다 이번 CPU 시간이 크게 증가했으므로, 아래 수치를 무계측 비용으로 사용하거나
  제거 가능한 최적화 이득이라고 해석하면 안 된다.

## 1. 입력과 건강 상태

사용자가 전달한 파일을 오프라인 분석했다. 타겟 실행·설치, 새 벤치마크, 소스 수정은 없다.

| 입력 | PID / Render TID | Event / Render events | Render categories | iterations |
|---|---|---:|---:|---:|
| OFF | app.log 및 packages.txt만 있음 | 없음(정상) | 없음(정상) | 알 수 없음 |
| Production | 3175 / 3181 | 2,673 / 155,318 | 52,055 | 4,540 |
| Source-only | 3108 / 3114 | 2,529 / 165,432 | 70,387 | 5,882 |

모든 trace의 dropped/unmatched/open/allocation_failed/clock_failed는 0이다.
새 categories는 enabled=1이며 별도 누락/불균형도 없다.
Event의 category records=0은 상세 계측이 Render 경로에 있으므로 정상이다.

packages.txt에서 확인한 버전:

- Core / Adaptor / TV profile: 양쪽 `2.ryucmd`.
- Production 및 OFF UI: `2.ryucmd.production`.
- Source-only UI: `2.ryucmd.sourceonly`.
- sample: 전부 `1.ryutrace`.

패키지 설치 기록은 의도한 비교와 일치한다. 실행한 ELF hash나 라이브러리 mapping을 수집한 것은 아니다.
세 app.log 모두 GLES이며, FPS 수치는 모두 없다. 버튼 설정/주파수/열/다른 프로세스 상태도 기록되지 않았다.
PERFORMANCE / Strong / Sync 등은 전달 가이드와 같은 설정이라는 전제로 비교한다.

## 2. 구간 / 계산

- 진입: CardsStart 후 1–3초, 양쪽 4회. Production 215 / Source-only 466 iterations.
- 정상 퇴장: 각 새 CardsReady 뒤 첫 ExitStart부터 ExitFinished까지.
- 퇴장 후반: ExitStart +300ms → ExitFinished, 4회씩, 24 / 42 iterations.
- 퇴장 spike: 각 정상 퇴장 중 가장 긴 wall iteration 1개, 4개씩.
  선택된 모든 spike에 Framebuffer 생성 57 / 19회가 실제 기록되어 있어 setup 위치와 일치한다.
- 평균은 선택한 iterations를 합친 frame-weighted mean. 시작 시각이 구간 안인 iteration의 전체 duration을 사용한다.
  경계 iteration은 구간 끝을 조금 넘을 수 있다.
- CPU는 Update/Render thread의 CLOCK_THREAD_CPUTIME_ID다. 전체 프로세스 CPU가 아니다.
- command parent는 offscreen 안의 ProcessCommandBuffer CPU interval union.
- category는 **bucket 3(offscreen + command 내부)의 exclusive CPU**.
  부모/자식 중복을 뺐으며 category + Other는 additive다.
- Create queue 표는 queue끼리만 additive. 다른 parent/command 표와 합산하지 않는다.
- GPU는 unknown. wall−CPU는 선점/대기 등을 포함하며 GPU 시간이 아니다.

두 steady 구간에는 양쪽 모두 Reveal.Prepare/Publish/Construct 및 Shader.Create가 없다.
각 정상 퇴장 전체에 Prepare/Publish/Construct 19회가 있고 Shader.Create는 없다.
따라서 이번 steady 차이와 setup spike는 별도로 다뤄야 한다.

## 3. 지속 command 처리

CardsStart 후 1–3초, 단위 ms/frame. count는 handler/block 호출 수이며 GL 호출 수가 아니다.

| Category | Production count | CPU | Source-only count | CPU | CPU 차이 |
|---|---:|---:|---:|---:|---:|
| DependencySync | 171.70 | **8.738** | 69.06 | **2.736** | **+6.002** |
| RenderPass | 102.42 | **4.884** | 34.03 | **1.599** | **+3.285** |
| Draw | 51.21 | 3.022 | 17.02 | 0.826 | +2.196 |
| BufferUniform | 102.42 | 1.007 | 34.03 | 0.313 | +0.694 |
| TextureSampler | 51.21 | 0.889 | 17.02 | 0.387 | +0.502 |
| PipelineState | 102.42 | 0.678 | 34.03 | 0.219 | +0.459 |
| Other (parent residual) | — | 3.056 | — | 1.010 | +2.046 |
| **command 합계** | — | **22.275** | — | **7.089** | **+15.186** |

Command 밖 BufferUniform은 0.178 / 0.150ms/frame으로, 위 표에 더하지 않았다.
이번 범위에서는 uniform mapping만을 최우선 원인으로 볼 근거가 약하다.

RenderPass와 Draw 등 여러 count가 약 3배다. 기존 Source→H→V와 Source-only의
stage 수 차이와 일관되지만, 모든 추가 CPU가 task 개수 하나로 결정된다는 뜻은 아니다.
Source-only는 Gaussian만 끈 동일 topology가 아니므로 Gaussian GPU 비용을 직접 분리한 비교도 아니다.

퇴장 후반에도 같은 방향이다:

| ms/frame | Production | Source-only | 차이 |
|---|---:|---:|---:|
| DependencySync | 9.613 | 3.255 | +6.358 |
| RenderPass | 5.233 | 1.769 | +3.464 |
| command 합계 | 23.709 | 8.090 | +15.619 |

퇴장 후반은 표본이 작으므로 진입 중간이 주 근거다.

### 반복별 확인

| 진입 회차 | Production 전체 CPU / command / dependency | Source-only 전체 CPU / command / dependency |
|---|---:|---:|
| 1 | 29.294 / 19.792 / 7.650 | 12.928 / 6.531 / 2.445 |
| 2 | 32.463 / 22.819 / 8.875 | 14.259 / 7.341 / 2.888 |
| 3 | 33.572 / 23.672 / 9.391 | 13.984 / 7.196 / 2.786 |
| 4 | 32.970 / 23.283 / 9.242 | 14.161 / 7.313 / 2.838 |

차이는 첫 회뿐 아니라 이후 반복에서도 유지된다. 반복 중 증가의 원인은 주파수/열 자료가 없어 확정하지 않는다.

## 4. 퇴장 생성 spike

4회 평균. items는 InitializeResource 시도 횟수로, 성공한 GL allocation 수와 동일하다고 가정하지 않는다.

| Create queue | Production items | CPU ms | Source-only items | CPU ms | 차이 |
|---|---:|---:|---:|---:|---:|
| Texture | 97 | 2.213 | 59 | 1.098 | +1.115 |
| Buffer | 0.25 | 0.006 | 0 | 0.000 | +0.006 |
| **Framebuffer** | **57** | **44.355** | **19** | **10.182** | **+34.173** |
| parent ProcessCreateQueues | — | 46.666 | — | 11.372 | +35.295 |

parent에는 위 queue 외 호출/계측 비용도 포함되므로 세 행의 합과 완전히 같지는 않다.

| 퇴장 회차 | Production Framebuffer CPU | Source-only Framebuffer CPU |
|---|---:|---:|
| 1 | 39.320 | 9.810 |
| 2 | 41.990 | 10.186 |
| 3 | 47.091 | 10.314 |
| 4 | 49.019 | 10.418 |

4회 모두 57 / 19 items다. 첫 실행만의 shader compile 문제가 아니다.
개별 resource timing은 없으므로 44ms가 57개에 균등 분포한다고 말할 수 없다.

spike iteration 자체는 평균 wall **145.322 / 60.060ms**, thread CPU **128.265 / 51.926ms**다.
그 안의 offscreen command CPU는 **46.799 / 12.098ms**이며 create queue와 별도 비용도 크다.
Texture update CPU는 **16.977 / 16.772ms**로 유사하다.
부모/자식 항목을 모두 합쳐 전체 시간을 계산하지 않는다.

## 5. 실제 코드로 좁힐 수 있는 범위

측정된 Tizen10.1 소스는 `../reveal-command-attribution.O3eOCq/adaptor/` 기준이다.

**DependencySync는 단순히 "GPU가 끝나기를 CPU가 기다린 시간"이 아니다.**

- `gles-texture-dependency-checker.cpp:172` AddTextures: attachment 추적, container 처리,
  SyncPool::AllocateSyncObject 및 fence 생성 경로를 포함한다.
- 같은 파일의 CheckNeedsSync: 다른 context에 한해 forward sync 처리.
  현재 Context::Flush의 호출 인자는 `cpu=false`다.
- CheckFramebufferNeedsSync: 이전/current frame의 backward dependency 확인 및 필요한 sync wait/free.
- 같은 context이면 일부 wait가 생략되지만 AddTextures의 sync 생성은 별도로 수행된다.
- 위 함수들이 하나의 tag를 공유하므로 **생성/검색/등록/wait/free 중 어느 하나가 8.7ms인지 아직 모른다.**
  EGL/GL sync backend의 실제 선택도 이번 trace에는 없다.
- 기존 `Context::EndRenderPass`의 `glFlush`는 DependencySync가 아니라
  부모 RenderPass의 exclusive 비용에 남는다.

따라서 현재 결과만 보고 fence를 생략하거나 sync를 제거하면 안 된다.
다른 context/다음 frame의 read-write 수명 보장이 필요하다.

**Framebuffer queue는 단순 텍스처 메모리 할당 시간과 다르다.**

`gles-graphics-framebuffer.cpp:118` InitializeResource는
GenFramebuffers → BindFramebuffer → AttachTexture → DrawBuffers → unbind/cache 갱신을 수행한다.
AttachTexture는 FramebufferTexture2D 등의 driver API로 연결된다.
현재 결과는 이 전체 queue 경로에 비용이 집중됨을 뜻한다.
glGenFramebuffers 한 함수 또는 GPU 메모리 할당만의 44.355ms라는 뜻은 아니다.
driver 내부 validation/deferred 작업의 비중도 알 수 없다.

## 6. 계측 영향 — 반드시 주의

동일 시나리오의 이전 coarse 계측과 비교하면:

| 진입 steady CPU ms/frame | 이전 Production | 이번 Production | 이전 Source-only | 이번 Source-only |
|---|---:|---:|---:|---:|
| 전체 Update/Render | 23.69 | 31.95 | 9.17 | 13.82 |
| offscreen command | 16.62 | 22.27 | 4.81 | 7.09 |
| Update | 1.20 | 1.19 | 0.87 | 0.89 |
| Window Render | 2.52 | 5.06 | 1.97 | 4.28 |

전체 CPU는 각각 약 **35% / 51%** 증가했다. 이것은 서로 다른 실행 간 비교이므로
증가량 전부를 계측 오버헤드라고 단정할 수 없다. 다만 상세 scope가 추가된 Render 쪽에
증가가 집중되어 **계측 영향이 무시할 만큼 작다고 볼 근거도 없다.**
동일 계측을 양쪽에 적용해도 Production은 block 호출이 더 많아 오버헤드가 상쇄되지 않는다.
이전 수치에 이번 category 비율을 곱해 "보정한 진짜 비용"을 만들지 않았다.

OFF 폴더는 올바른 Production 패키지 기록과 정상 종료 흔적을 포함하지만,
FPS/시간 측정이 없어 OFF/ON 정량 비교는 불가능하다.
사용자는 **"ON이 살짝 느린 느낌"**이라고 답했다. 이는 계측 영향에 대한 정성 근거이며,
위의 실행 간 CPU 증가율을 OFF/ON 오버헤드 비율로 확정해 주는 자료는 아니다.
정량 오버헤드 검증에는 별도의 같은 조건 OFF/ON FPS 자료가 필요하다.
보유 자료만으로 37ms iteration을 실제 scanout FPS로 환산하지 않았다.

## 7. 다음 판단

1. **먼저 계측 오버헤드 확인.** 이미 수집한 OFF/ON FPS가 있다면 그것을 사용한다.
   이번 단계에서 새로 빌드하거나 target 실행하지 않았다.
2. **지속 비용은 DependencySync 내부가 다음 좁힐 지점.**
   기존 category를 생성·등록 / forward read / backward write 검사로 나누거나 CPU sampling으로
   확인하는 편이 Gaussian tap/품질을 다시 바꾸는 것보다 근거가 있다.
   기존 범주를 대체해 측정 밀도를 늘리지 않는 방향이 적절하다. 아직 구현하지 않았다.
3. **setup은 Framebuffer 초기화 경로를 별도로 검토.**
   57→19의 차이가 확실하지만, 실제 생성 최소화/안전한 재사용은 publication/lifecycle/메모리
   검증이 필요한 별도 변경이다. 이번 결과만으로 cache를 바로 넣지 않는다.

현재 확정한 것은 **비용이 기록된 범주**다. 무계측 비용, 최종 원인, 예상 개선율 또는
안전한 최적화 구현까지 확정하지 않았다. GPU 시간은 여전히 unknown이다.

## 8. 산출물 / 보존

- [자동 category 비교](analysis/COMPARISON.md)
- 반복별/상위 scope 집계 (로컬 자료: `SUMMARY.json`)
- Production 상세 (로컬 자료: `analysis/production/categories.json`), Source-only 상세 (로컬 자료: `analysis/source-only/categories.json`)
- `analysis/*/baseline/frames.csv` 및 `timeline.json`: 기존 parser의 결과.
- 입력 파일 경로·크기·SHA256 (로컬 자료: `INPUT_MANIFEST.json`)
- 보조 집계 스크립트 (로컬 자료: `summarize.py`)

재현:

```sh
python3 /home/bowonryuubuntu/tizen/reveal-command-attribution.O3eOCq/analyze.py \
  /home/bowonryuubuntu/Downloads/trace/ryu-cmd-production \
  --source-only /home/bowonryuubuntu/Downloads/trace/ryu-cmd-sourceonly \
  --out /path/to/new-analysis-directory
```

전달 raw trace/app.log/packages.txt는 수정하지 않았다. 이전 보고서와 계측 소스도 보존했다.
이번 산출물은 새 외부 진단 폴더에만 기록했다. 소스/commit/build/target 상태 변경 없음.
