# Text Effect Demo — per-Label entrance Reveal cleanup

## A. Executive verdict

**READY FOR TARGET COMPARISON**

샘플만 수정했다. 먼저 끝난 Label은 자기 entrance의 논리적 완료와 실제
progress를 확인한 다음 `Text::Reveal::None()`으로 전환한다.
HIGH / PERFORMANCE / ECONOMY에 같은 정책을 적용한다.

이것은 플랫폼의 자동 task gating이나 shader 최적화가 아니다.
타겟 FPS/CPU/GPU 메모리 개선률은 이번에 측정하지 않았다.

**정확한 제한:** wall-clock Timer를 update/render 완료 신호로 간주하지 않는다.
예약 시점에 아직 완료되지 않은 Label은 다음 authored 완료 시점이나 기존
Animation FinishedSignal에서 정리한다. 따라서 모든 Label을 마지막까지
유지하는 동작은 줄이지만, 모든 Label을 정확히 완료 frame에 해제한다고
보장하지 않는다. 고정 frame 지연이나 매-frame 재검사는 없다.

## B. Files changed

- UI tracked 변경: `samples/text/text-effect-demo.cpp` 하나.
- Foundation / TextVisual / Reveal runtime / shader / public API 변경: **0**.
- Core / Adaptor 변경: **0**.
- 보고서, baseline 사본, native 검사 프로그램과 캡처는 이 외부 디렉터리에만 있다.
- 기존 ECONOMY 및 Gaussian 구현/커밋을 수정하지 않았다.

## C. Before: 실제 sample lifecycle

`mRevealSequenceAnimation` 하나가 Cards의 12개 foreground Reveal track을 구동한다.
각 Card의 시작 간격은 0 / 0.48 / 0.96초다. 마지막 subtitle의 완료는 3.44초이고,
기존에는 이 Animation의 FinishedSignal에서 12개 Label을 모두 None으로 만들었다.

다른 scene Label은 별도의 `mSceneTextAnimation`을 공유한다.
Results scene의 brand/trip/heading/hint는 1초, 긴 subheading은 2초이며
Reveal lead 0.06초가 붙는다. 기존에는 모두 긴 Label의 2.06초 완료를 기다렸다.
장문 분류는 줄 수가 아니라 기존 `IsLongText()`의 문자 수 기준이다.

Card entrance 완료 후 badge/action 두 Label은 별도
`mAffordanceRevealAnimation`의 같은 0.66초 완료 시점을 공유한다.
Generating/detail/completion status는 이미 별도의 단일-Label Animation과
FinishedSignal을 갖고 있다. 이런 경우는 개별 조기 해제 이득이 없다.

정상 exit는 None 상태에서 exit Reveal을 설정하고 역재생한다.
scene은 기존 LayoutTransition의 EXIT ghost로 남았다가 종료 시 제거된다.
exit의 progress=0만 보고 None을 호출하는 로직은 원래도 없으며 추가하지 않았다.

## D. After: per-Label lifecycle

```text
기존 entrance track 등록 + 같은 delay/duration의 cleanup 정보 등록
  → 기존 Animation.Play()
  → 가장 이른 authored 완료 시점에 Timer
  → Animation elapsed >= Label 완료 시각 AND 현재 Label progress == 1
      만족: 해당 Label만 None
      미충족: 유지; 다음 authored deadline 또는 FinishedSignal fallback
  → 다음 미확인 deadline 예약
  → shared FinishedSignal: 아직 None이 아닌 Label만 최종 정리
```

일찍 해제한 Label의 Animation track 자체는 제거하지 않는다.
남은 Label의 track, shared Animation, layout transition은 계속 원래대로 동작한다.

Sync에서는 None에 따라 기존 runtime이 정리된다. Async의 None은 기존
비동기 publication 경로를 사용하므로, **None setter 호출과 모든 offscreen task의
실제 제거가 같은 순간인 것은 아니다.** 기존 결과를 유지하다 ordinary publication이
준비되면 교체한다. 기다리는 loop나 main-thread blocking은 추가하지 않았다.

## E. Scheduling mechanism

- sample-owned vector + Timer 한 개. Label별 Timer/Animation은 만들지 않는다.
- Label과 Animation은 `WeakHandle`로 저장한다.
- `AnimateTextEntrance()`가 이미 사용하는 `delay + 0.06 + duration`을 기록한다.
- `PlayTextEntrance()`는 원래의 Play를 한 번 호출한 뒤 steady-clock deadline을 잡는다.
- 해당 shared Animation의 마지막/단일 track은 기존 FinishedSignal에 맡겨 Timer 후보에서 제외한다.
- Timer는 아직 방문하지 않은 가장 이른 deadline까지만 예약한다.
- 이미 방문한 deadline을 1ms/매-frame 주기로 재시도하지 않는다.
- callback은 현재 Animation state/elapsed와 실제 Label progress를 함께 확인한다.
- 최종 cleanup은 이미 None이면 setter를 다시 호출하지 않는다.

기존 sample에 개별 animator FinishedSignal은 없다. 하나의 Animation을 나누거나
별도 notification animator를 추가하면 workload가 달라지므로 사용하지 않았다.
Core의 단일 progress marker를 반복 재설정하는 방식에도 의존하지 않았다.

타이머 callback 안의 재예약은 기존 DALi Timer의 `SetInterval(interval, true)`를
사용한다. glib/Windows Timer 구현의 Stop→Start 및 callback lifetime guard를
읽어 확인했으며, adaptor는 변경하지 않았다. Windows 실행 검증은 하지 않았다.

## F. Animation workload equivalence

Animation 생성 수/기존 track 등록은 그대로다. 바뀐 Play 호출은 같은 Animation의
Play를 정확히 한 번 호출하는 wrapper로만 대체했다.

다음은 diff에서 변경 없음:

- text, 카드/레이아웃 구성, Unit, entrance/exit order.
- 모든 delay / duration / easing / layout transition 설정.
- entrance: PER_LINE, Fade 0, EASE_OUT_SQUARE.
- 긴 text: Stagger 0.25, BlurDurationRatio 0.5; 짧은 text: 0 / 1.
- Strong radius: entrance **24**, normal exit **48**.
- normal exit: WHOLE_TEXT, Fade 1, Stagger 0, BlurDurationRatio 1, Linear 0.4초.
- quality 버튼 UX, ECONOMY curve, blur shader/kernel.

추가 비용은 bounded deadline callback/작은 vector 순회와 더 이른 None 호출이다.
None에 따른 ordinary publication 비용까지 0이라고 주장하지 않는다.

## G. Cleanup timing table

단위: 초. Cards Animation.Play 기준의 논리 시각이다.
`new earliest`는 해제 자격의 하한이며 실제 Timer/renderer 완료 시각이 아니다.
기존 cleanup의 3.44도 FinishedSignal 전달 지연 전의 논리 시각이다.

| Label | Reveal start | duration | previous cleanup | new earliest |
|---|---:|---:|---:|---:|
| Card1 day | .06 | .42 | 3.44 | .48 |
| Card1 title | .16 | .88 | 3.44 | 1.04 |
| Card1 places | .36 | 1.18 | 3.44 | 1.54 |
| Card1 subtitle | .48 | 2.00 | 3.44 | 2.48 |
| Card2 day | .54 | .42 | 3.44 | .96 |
| Card2 title | .64 | .88 | 3.44 | 1.52 |
| Card2 places | .84 | 1.18 | 3.44 | 2.02 |
| Card2 subtitle | .96 | 2.00 | 3.44 | 2.96 |
| Card3 day | 1.02 | .42 | 3.44 | 1.44 |
| Card3 title | 1.12 | .88 | 3.44 | 2.00 |
| Card3 places | 1.32 | 1.18 | 3.44 | 2.50 |
| Card3 subtitle | 1.44 | 2.00 | 3.44 | 3.44 / Finished fallback |

아래는 각각 자신의 Animation.Play 기준이다.

| 기타 group | start | duration | previous cleanup | new earliest |
|---|---:|---:|---:|---:|
| Results 일반 short labels | .06 | 1.00 | 2.06 | 1.06 |
| Results 긴 subheading | .06 | 2.00 | 2.06 | 2.06 / Finished |
| badge/action | .06 | .60 | .66 | 기존 Finished 유지 |
| Generating status | .06 | .52 | .58 | 기존 Finished 유지 |
| Detail status | .06 | 1.00 | 1.06 | 기존 Finished 유지 |
| Completed status | .06 | .60 | .66 | 기존 Finished 유지 |

실제 native PERFORMANCE/Sync 확인에서는 Card1 day가 Animation 약 .83초,
Card1 title/Card2 day가 약 1.32초에 이미 None이었다. Card2 subtitle은 Timer가
먼저 도착해서 마지막 Finished fallback에서 정리됐다. 관측은 외부 checker의
40ms 간격 snapshot이며 정밀 cleanup latency 측정이 아니다.

같은 실행에서 window 전체 task snapshot은 58→40→28→19→10→7→1로 감소했다.
여기에는 Cards 외 Label, badge/action, 기본 window task도 포함된다.
**Label 수와 task 수가 같다는 의미가 아니며 VRAM 실측도 아니다.**
baseline native 실행은 Cards 12개 모두 shared Finished까지 None이 아니었다.

## H. Cancellation / ownership

- 기존 `StopSceneActivity()`에서 cleanup Timer를 Stop/Reset하고 pending vector를 비운다.
- transition / reset / loading restart / shutdown의 기존 lifecycle token을 그대로 사용한다.
- Timer callback은 captured token 불일치 또는 shutdown이면 no-op이다.
- exit track을 설정하기 **전에** 이전 entrance scheduler를 취소한다.
- pending 기록은 weak이며 completed callback 내부에서만 잠깐 Label handle을 얻는다.
- 제거한 scene의 Label weak handle이 만료되는 것을 native 검사에서 확인했다.
- 기존 shared FinishedSignal의 strong capture 범위는 늘리지 않았다.

## I. Entrance completion

세 quality 모두 먼저 끝난 Card Label이 마지막 subtitle보다 일찍 None이 됐다.
모든 Label 완료 후 None으로 수렴하고 idle window task count는 **1**이었다.
이 숫자는 CPU/GPU allocation 즉시 반환을 증명하지는 않는다. 드라이버의 deferred
destruction과 allocator cache는 이번에 측정하지 않았다.

logical completion 이전에 해제됐다는 native assertion failure는 없었다.
실제 progress가 덜 진행됐을 때는 유지하며 기존 Finished fallback이 남아 있다.

## J. Mid-entrance exit

Cards 시작 약 1.25초에 exit를 시작했다.
이미 None인 Label과 아직 progress<1인 Label이 함께 있는 조건을 검사했다.

- 이미 None: 기존 normal exit 설정을 새로 적용.
- 미완료: 기존 `ConfigureExitReveal()`의 interrupted-entrance 보존 정책 그대로.
- 이전 cleanup queue/Timer 제거 확인.
- exit 중에는 None이 되지 않음 확인.
- scene 제거 후 old Label weak handle 만료 확인.

미완료 entrance를 normal exit와 같은 설정으로 강제로 바꾸지 않았다.
이는 기존 sample 동작을 보존하기 위한 것이다.

## K. Normal exit

완료 후 idle에서 Detail로 전환했다. 이전 Label들이 exit용 WHOLE_TEXT/Fade1
Reveal을 다시 가지는지, 기존 0.4초 exit 동안 None으로 재노출되지 않는지 검사했다.
EXIT 완료 후 새 scene을 설치하는 기존 순서를 유지했다.
hidden preload, async 완료 대기, progress0 cleanup은 추가하지 않았다.

## L. Quality / lifecycle runs

실제 설치된 DALi 라이브러리를 사용하는 native MainLoop로 검사했다.
UI UTC mock renderer 기반 검사가 아니다.
창 1280×900, MSAA4, 기본 Strong(24/48), 기존 폰트/문구/시간을 사용했다.

| quality | Sync lifecycle run | Async lifecycle run |
|---|---|---|
| HIGH | PASS | PASS |
| PERFORMANCE | PASS | PASS (재확인 포함) |
| ECONOMY | PASS | PASS |

각 run은 entrance→idle→normal exit→재생성→mid-entrance exit→재생성→
entrance 중 quality 변경→idle→reset→pending 상태 shutdown을 수행했다.
quality 변경은 cleanup deadline을 재계산하거나 animation을 재시작하지 않는다.
검사용 외부 polling/capture/logger는 **최종 sample에 포함되지 않는다.**

추가 관측을 숨기지 않기 위해 기록한다: 최초 PERFORMANCE/Async 화면 캡처 run의
외부 검사에서 **None 전환 후** `GetTextRevealProgress()==1` exact assertion이
2건 실패했다. 당시 값 자체는 저장하지 않아 원인을 확정할 수 없다.
logical deadline 위반, None 이전 미완료 강제 해제, task leak은 관찰되지 않았다.
수치 저장을 추가한 동일 경로 재확인, 캡처 없는 run, 추가 entrance 캡처 5회에서는
모두 재현되지 않았다. 이 현상을 근거 없이 플랫폼 버그나 해결 완료로 분류하지 않는다.
원래 로그 `performance-async.log`와 재확인 로그를 그대로 보존했다.

## M. Visual regression

외부 native window 캡처로 HIGH/Async, PERFORMANCE/Async, ECONOMY/Sync의
Cards entrance와 cleanup 전후를 확인했다. UI의 색/레이아웃/문구를 수정하지 않았다.

Card1 title의 안정된 RGB 영역 `(60,312,340,46)`을 비교한 결과:

- HIGH: `high-frames/frame-0028.png` ↔ `frame-0032.png`: 동일.
- PERFORMANCE: `performance-frames/frame-0028.png` ↔ `frame-0033.png`: 동일.
- ECONOMY: `economy-frames/frame-0028.png` ↔ `frame-0032.png`: 동일.
- 기존 PERFORMANCE baseline의 `baseline-frames/frame-0032.png`와
  변경 후 PERFORMANCE `frame-0033.png`의 같은 영역도 동일.

RGB PSNR 비교 결과는 모두 inf(해당 ROI 차이 0)였다.
캡처한 전후에서 제목의 blink/disappear/double draw/색 변화는 관찰하지 못했다.
day/본문/gradient 제목도 캡처에서 기존 구성과 함께 정상 표시된다.
별도로 Card2 gradient 제목 영역 `(468,312,330,46)`의 frame39→48을 비교했다.
세 quality 모두 RGB PSNR 약 71.04dB였고, HIGH의 raw RGB 차이는 최대 **1/255**,
45,540개 channel 값 중 233개가 달랐다. Gradient까지 pixel-identical이라고
주장하지 않으며, 확인한 차이는 육안상 색 점프로 보이지 않는 수준이었다.
전체 픽셀/모든 순간/모든 타겟에서 artifact가 없다는 보증은 아니다.
캡처는 약 40ms 간격이므로 단일 refresh artifact를 놓칠 가능성도 있다.

첫 frame과 등장 timing은 해당 deadline 이전의 runtime/animation 코드를 바꾸지
않았음을 확인했다. 별도 실행의 wall-clock screenshot을 frame-identical이라고
주장하지 않는다. idle의 gradient Animation/scene interaction은 계속 유지된다.
현 demo Cards에 ImageSpan은 없으므로 ImageSpan 전후 동일성 검증은 하지 않았다.

추가로 세 quality의 Sync normal/interrupted exit를 native window로 캡처했다
(`transition-0/1/2`, 각 log의 stage1=normal exit, stage3=interrupted exit).
캡처에서도 기존 blur/fade 퇴장 후 scene이 제거됐고 새 scene이 이어서 등장했다.
해당 세 lifecycle run도 모두 PASS였다. 품질별 원래 blur 모습의 차이는 유지되며
quality 사이에 같은 wall-clock screenshot을 동일한 progress로 간주하지 않았다.

## N. Build

```bash
cd /home/bowonryuubuntu/tizen/dali/dali-ui
cmake --build samples/text --target text-effect-demo.example -j4
git diff --check
```

demo 최종 build PASS: build-final.log (로컬 자료: `build-final.log`). `git diff --check` PASS.
플랫폼 rebuild/UTC/GBS/성능 측정은 하지 않았다.
전체 platform 동작이 아니라 이번 sample-only lifecycle 변경 범위를 검사했다.
외부 harness 초기 빌드의 X11 이름 충돌과 BASELINE macro 충돌은 harness 안에서만
수정했다. 실제 sample build에는 이 코드/의존성이 들어가지 않는다.

## O. Remaining risks / interpretation

1. Timer와 Animation은 서로 다른 clock이다. 부하가 크면 cleanup은 늦어지거나
   기존 Finished fallback까지 유지될 수 있다. 개선 효과가 줄어들 수는 있지만
   미완료 Label을 강제로 해제하지 않는다.
2. Async None은 ordinary publication 요청을 앞당긴다. worker/메인 스레드 비용의
   시간 분포도 달라지므로 task 감소만으로 FPS/CPU 개선을 확정할 수 없다.
3. quality에 따른 조건 분기는 없지만 각 기기의 frame/event 진행 속도가 달라서
   관측 wall-clock cleanup 시각이 세 quality에서 완전히 같을 필요는 없다.
4. 새 scheduler는 기존 forward entrance만을 위한 sample-local 구현이다.
   pause/seek/loop/reverse를 지원하는 generic animation scheduler로 확장하지 않는다.
5. 위 Async 캡처에서 나온 일회성 post-None progress 관측과 target의 단일-frame
   visual correctness는 target 비교 때 계속 확인해야 한다.
6. Blur Effect 비교 mode도 같은 entrance helper/ClearTextReveal을 통과하므로
   완료된 effect의 cleanup 역시 앞당겨질 수 있다. Blur Effect와 비교할 때도
   before/after의 cleanup policy를 섞지 않아야 한다.

## P. Exact target comparison

Baseline UI HEAD: `da82f963f7feaea56520e530fa1edc927e9b34d0`.
Candidate: 같은 HEAD + 현재 sample 한 파일의 unstaged diff.
Core/Adaptor/설치된 UI foundation/components 라이브러리는 동일하게 유지한다.

1. 기존 target 빌드/설치 방식으로 baseline demo와 candidate demo를 각각 준비한다.
   이 작업에서 새 RPM을 만들거나 타겟 패키지를 설치하지는 않았다.
2. 같은 해상도/창/Sync 또는 Async/Strong/quality/폰트를 고정한다.
   Blur ON, Blur Effect OFF. Strong은 entrance24/exit48 그대로다.
3. `3` 키 또는 quality 버튼으로 PERFORMANCE를 선택하고, `0`으로 처음부터 시작한다.
   `1`=Sync / `2`=Async 선택도 baseline/candidate에서 맞춘다.
4. 기존 2초 loading 뒤 Enter/Space로 생성 화면에 진입한다. skeleton→Cards entrance의
   **전체 구간**을 기존 target FPS/외부 trace 방식으로 비교한다.
5. early Label completion 이후~마지막 subtitle 완료 전 구간을 별도로 확인한다.
   Cards 완료 뒤 badge/action과 idle도 확인한다. scene 앞뒤를 다르게 잘라서 비교하지 않는다.
6. Cards 중간에 Enter/Space로 detail exit, Esc/Back, `0` reset을 수행해 blink/reappear를 확인한다.
   완전히 완료된 후 정상 exit도 별도로 비교한다.
7. ECONOMY before/after를 같은 방식으로 반복한다. HIGH도 동일하게 lifecycle/visual을 확인한다.
8. 실행 순서를 번갈아 반복하고 기존 target 측정 도구/구간을 그대로 사용한다.
   이 sample에 FPS/RYU/per-frame 로그는 추가하지 않았다.

로컬 직접 실행:

```bash
cd /home/bowonryuubuntu/tizen/dali/dali-ui/samples/text
./bin/text-effect-demo.example
```

최종 성능 판단은 target 결과 이후다. 이번 native task count를 FPS 향상 수치로
대신하지 않는다. 비교용 baseline source 사본은 baseline.cpp (로컬 자료: `baseline.cpp`)에 보존했다.

## Q. Git status

UI branch `devel_blur_text`, HEAD 변경 없음. 최종 tracked 변경은 sample 한 파일뿐이다.
diff: 121 insertions / 8 deletions (대부분 sample-local scheduler 추가).
변경은 unstaged이며 git add/commit/amend/rebase/reset/restore/stash를 실행하지 않았다.

Core HEAD `f43e95be477ad301f84ecc772c753f9357821ecc`, clean.
Adaptor HEAD `dcadcfdc3e0d1abdce20767bee2858cb9e1851a4`, clean.

## R. Push

**NOT PUSHED.**
