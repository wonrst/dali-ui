# Sample-only pipeline budget sweep — 구현하지 않은 준비안

이번 inventory에서 BlurEffect의 task 수는 예상한 9개가 아니라 Cards 36개다.
Reveal의 45개와 차이는 1.25배이며, 정상 퇴장에서는 둘 다 36개다.
따라서 "BlurEffect가 훨씬 적은 pipeline을 쓴다"는 조건은 성립하지 않았다.
이번에는 조건부 sweep 코드를 추가하지 않는다. 아래는 후속 작업용 exact plan이다.

## 보호 / baseline

- 현재 UI HEAD `05087317`의 Source-only를 유지한다.
- 독립 소스 트리/빌드에서 `b54bb666` production으로 준비한다.
- `text-effect-demo.cpp`만 아래처럼 수정한다. production library 수정 없음.
- 현재 HEAD 라이브러리로 sweep하면 Source-only 실험이므로 무효다.
- Windows/TV 빌드는 해당 타겟의 정상 toolchain으로 수행한다. 이 폴더의 `.so`는 Ubuntu inventory 전용이다.

## sample 변경 지점

1. sample-private enum `PipelineBudget { LOW, MID, HIGH, FULL }`와 **고정 constant 하나**를 둔다.
   animation 중 키 변경은 하지 않는다. 각 variant를 같은 compiler/options로 빌드한다.
2. `mCards[0..2]`의 `day/title/places/subtitle` handle로 고정 membership을 판정한다.
   선택 함수는 bool만 반환하며 Label/pipeline 생성, 전역 카운터, cache는 만들지 않는다.
3. `NewItineraryCard`가 로컬 card를 구성할 때는 card index + field kind로 판정한다.
   아직 `mCards`에 들어가지 않은 handle을 membership 함수로 검사하면 안 된다.
4. `ConfigureEntranceReveal` 호출 지점 **모두**에서 해당 Label의 radius만 선택한다.
   특히 초기 card 구성과 `StartSceneTextEntrance`의 재설정을 모두 처리한다.
   선택되지 않은 Label에는 radius `0.0f`. Unit/Fade/Stagger/Sequence/BlurDuration/Quality는 유지한다.
5. `AnimateTextExit` → `ConfigureExitReveal`에도 동일 membership으로 radius를 전달한다.
   partial entrance를 역재생할 때 기존 Reveal을 유지하는 분기는 변경하지 않는다.
6. badge/action/status/scene header 등 **카드 12개 이외 Label의 blur는 모든 level에서 OFF**.
   이들의 ordinary Reveal/animation/layout은 유지한다. BlurEffect mode는 비교 reference로만 별도 실행한다.
7. `StartResultReveal`의 duration/delay/alpha/root layout transition, 완료/None 정리 코드는 변경하지 않는다.
8. sample-private 선택 수준과 선택된 Label 이름은 초기화 때 한 번 출력한다.
   count 확인은 external inventory로 하고, production 내부에는 카운터를 추가하지 않는다.

## 고정 선택 (nested sets)

`C1/C2/C3`는 카드 번호. LOW만 2-page places를 건너뛰어 정확히 3-page가 되도록 한다.

| level | blur-enabled Label | 등장 pages / tasks / FBO | 정상 퇴장 pages / tasks / FBO |
|---|---|---:|---:|
| LOW | C1 day,title + C2 day | 3 / 9 / 9 | 3 / 9 / 9 |
| MID | C1 day,title,places,subtitle + C2 day | 6 / 18 / 18 | 5 / 15 / 15 |
| HIGH | C1+C2의 day,title,places,subtitle | 9 / 27 / 27 | 8 / 24 / 24 |
| FULL | C1+C2+C3의 12 Label 전체 | 15 / 45 / 45 | 12 / 36 / 36 |

위 값은 **이번 native per-Label inventory의 합**이며 variant를 빌드/실행해 확인한 값은 아니다.
1280×720, 동일 corpus/font/UI scale/Strong/SYNC 기준이다. TV font/layout/window 크기가 다르면
pages도 달라질 수 있으므로 **타겟에서도 resulting count를 확인**해야 한다.
offscreen만 세며 window task 1개는 별도다.

원본 앱 전체 FULL은 등장 60, 퇴장 57 tasks였다. 위 controlled FULL은 Cards만 blur를 켜므로
원본 전체 FULL과 같은 것이 아니다. 나머지 Label의 blur를 원본대로 유지하면 각각
등장 `24/33/42/60`, 이번 퇴장 `30/36/45/57`가 된다(현재 layout에서의 산술 합).
두 방식의 결과를 혼합하지 않는다. 권장 controlled sweep은 첫 번째 표다.

## public API로 보존할 수 있는 것 / 없는 것

radius0은 지원되는 blur 비활성화다. Reveal None으로 바꾸지 않는다.
Animation duration, progress, alpha, Fade/Stagger/Sequence authored 값은 그대로 유지할 수 있다.
다만 blur-enabled plan은 `text-reveal-blur-preparation.cpp`에서 blur 끝을 포함해 unit timing을
정규화한다. radius0이면 이 경로가 없으므로 **모든 unit의 실제 화면 출현 시각까지 같다고
보장할 수 없다**. 이를 강제로 같게 하려고 internal timing API/metadata를 수정하지 않는다.
따라서 이 실험은 pure task-count microbenchmark가 아닌 practical UX budget sweep이다.

## 다음 구현 단계의 host 확인 범위

각 4개 level당 build/한 번 launch/actual pages·S/H/V tasks·FBO count/no-crash만 확인한다.
production Gaussian + Late Smooth, H/V 양쪽 REFRESH_ALWAYS가 유지되어야 한다.
호스트 FPS/CPU/GPU latency는 측정하지 않는다.

## target procedure / 판단

1. 동일 TV build, 해상도/font, SYNC, Strong(entrance24/exit48), PERFORMANCE로 고정한다.
2. app restart → 동일 Skeleton→Results→Cards. 완료 후 정상 퇴장도 별도 관찰한다.
3. level별 같은 횟수(예: 3회)의 raw 1초 FPS sequence, 최소 FPS, transition 체감,
   Card root animation이 처음부터 보이는지, 반복 일관성을 기록한다.
4. BlurEffect reference도 같은 workload/logger로 실행한다. 기존 사용자 관찰
   `60 44 38 40 44 55 43 57 60`을 새 측정처럼 표기하지 않는다.
5. 충분히 부드러운 **가장 큰 level의 실제 task 수**를 practical budget 후보로 삼는다.
   예: 18 등장 tasks는 충분하고 27부터 부족하면 해당 workload에서 ~18을 후보로 삼는다.
6. LOW에서도 부족하거나 강도/면적에 따른 결과가 다르면 count-only 가설을 재검토한다.
   task당 ms 환산, 임의 threshold 확정, 바로 atlas/pooling 구현은 하지 않는다.

level 간 pixels/A8·RGBA mix/constraints/Label 수/blur timing도 변한다.
이 한 workload의 budget을 전 제품/글꼴/해상도의 공통 limit로 만들지 않는다.
