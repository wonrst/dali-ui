# TextVisualizer 구현 계획

## 구현 목표

1차 목표는 "동적 레이아웃 변화에 강한 읽기 전용 text View"를 작동시키는 것이다.
즉, 이번 단계에서는 기능을 넓히기보다 구조를 올바르게 만드는 것이 우선이다.

## 구현 범위

### 1차 포함

- `Dali::Ui::TextVisualizer` public handle
- internal impl
- display-only controller facade
- measure / arrange / relayout lifecycle
- atlas 기반 렌더링 연결
- 기본 text/style property
- natural size / height-for-width

### 1차 제외

- editable
- IME
- cursor
- selection
- placeholder interaction
- clipboard
- async render

## 단계별 계획

### Phase 1. 최소 골격 생성

목표:

- `TextVisualizer` public/internal 클래스 추가
- View 생성 및 기본 배치 확인

작업:

- public-api 헤더 / cpp 추가
- integration/internal impl 추가
- `ViewImpl` 기반 최소 lifecycle 연결

완료 기준:

- 빈 `TextVisualizer`를 scene에 올릴 수 있다.
- relayout 경로가 호출된다.

### Phase 2. display-only text host 연결

목표:

- 내부에 `Text::Controller`를 안전하게 붙인다.

작업:

- 표시용 facade 클래스 추가
- editable 관련 진입 차단
- `GetNaturalSize`, `GetHeightForWidth`, `Relayout` 연결

완료 기준:

- 텍스트 문자열과 기본 폰트 설정이 가능하다.
- natural size가 정상 계산된다.

### Phase 3. renderer host 연결

목표:

- relayout 후 실제 텍스트가 보이게 한다.

작업:

- `Text::Backend::Get().NewRenderer()` 연결
- renderable actor attach/detach
- alignment offset 반영
- background actor 처리

완료 기준:

- multiline text가 scene에 표시된다.
- width/height 변경 시 텍스트가 다시 layout된다.

### Phase 4. property 확장

목표:

- 표시용 View로서 필요한 스타일 속성 확장

작업:

- color
- alignment
- markup
- underline / outline / shadow / line-through
- background color
- padding

완료 기준:

- 주요 text style property가 정상 반영된다.

### Phase 5. dirty 정책 정리

목표:

- 불필요한 renderer reset과 relayout을 줄인다.

작업:

- dirty flag 도입
- property별 invalidation 테이블 정리
- measure invalidation 조건 분리

완료 기준:

- 폭 변경, 글자색 변경, 텍스트 변경이 서로 다른 갱신 경로를 탄다.

### Phase 6. 검증 및 샘플

목표:

- 실제 동적 레이아웃 환경에서 동작 확인

작업:

- sample 추가 또는 기존 sample 확장
- resize / wrap / repeated relayout 테스트
- color glyph / markup / style 조합 확인

완료 기준:

- 빈번한 relayout에서도 텍스트 표시가 안정적이다.

## 파일 구조 제안

정확한 경로는 팀 규칙에 맞게 조정 가능하지만, 추천 구조는 아래와 같다.

```text
dali-ui-foundation/public-api/text-visualizer.h
dali-ui-foundation/public-api/text-visualizer.cpp
dali-ui-foundation/integration-api/text-visualizer-impl.h
dali-ui-foundation/integration-api/text-visualizer-impl.cpp
dali-ui-foundation/internal/text/display/display-text-controller.h
dali-ui-foundation/internal/text/display/display-text-controller.cpp
```

필요 시 `common-text-utils` 쪽에 보조 함수 일부를 추가한다.

## 위험 요소와 대응

### 위험 1. controller 재사용 시 숨은 editable 의존성

대응:

- facade에서 입력 관련 API를 전부 숨긴다.
- `EnableTextInput()`을 호출하지 않는다.
- `mEventData`가 생성되지 않는 경로를 유지한다.

### 위험 2. renderer reset 과다

대응:

- 1차 구현부터 dirty flag를 둔다.
- renderer recreate가 정말 필요한 property만 분리한다.

### 위험 3. atlas actor 수 증가

대응:

- 1차는 기본 atlas path로 구현
- 이후 profile 결과에 따라 actor 재사용/pooling 검토

### 위험 4. `InputField` 코드 복붙 유혹

대응:

- 필요한 lifecycle 패턴만 가져오고,
  이벤트/IME/decorator 코드는 의도적으로 분리한다.

## 우선 구현 순서

실제 작업 순서는 아래를 권장한다.

1. public/internal skeleton 생성
2. controller facade 연결
3. natural size / height-for-width 연결
4. relayout + renderer host 연결
5. 기본 text/style property 연결
6. sample 및 검증

## 테스트 관점 체크리스트

- 텍스트 설정 직후 표시되는가
- parent width 변경 시 줄바꿈이 즉시 갱신되는가
- height-for-width가 일관적인가
- padding / RTL에서 위치가 맞는가
- markup/style이 보존되는가
- empty text / whitespace only / emoji text가 깨지지 않는가
- repeated relayout 시 crash/leak 없이 안정적인가

## 구현 완료 정의

이번 기준 문서에서의 "구현 완료"는 아래를 의미한다.

- `TextVisualizer`가 읽기 전용 text View로 사용할 수 있다.
- dynamic relayout에 안정적으로 반응한다.
- 기존 `InputField`나 `TextVisual`를 우회하지 않고도 독립적으로 사용할 수 있다.
- 향후 selection/async/fallback renderer 같은 확장을 붙일 수 있는 구조다.

## 최종 제안

가장 안전한 시작점은 아래 조합이다.

> 1차 구현은 `InputField`의 View lifecycle 패턴을 참고해 `TextVisualizer`를 만들고, 내부 엔진은 `Text::Controller`, 렌더링은 기존 backend/atlas 경로를 재사용한다.
