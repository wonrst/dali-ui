# TextVisualizer 구현 계획

## 구현 목표

1차 목표는 "동적 레이아웃 변화에 강한 읽기 전용 text View"를 작동시키는 것이다.
즉, 이번 단계에서는 기능을 넓히기보다 구조를 올바르게 만드는 것이 우선이다.

## 구현 범위

### 1차 포함

- `Dali::Ui::TextVisualizer` public handle
- internal impl
- prepare model
- layout model
- measure / arrange / relayout lifecycle
- atlas 기반 렌더링 연결
- text / font / fontSize / textColor
- natural size / height-for-width
- multiple bounds / exclusion region 대응

### 1차 제외

- editable
- IME
- cursor
- selection
- placeholder interaction
- clipboard
- async render
- style
- ellipsis
- underline / outline / shadow / background

## 단계별 계획

### Phase 1. View 골격 생성

목표:

- `InputField`를 참고한 `TextVisualizer` public/internal 클래스 추가
- View 생성 및 기본 배치 확인

작업:

- public-api 헤더 / cpp 추가
- integration/internal impl 추가
- `ViewImpl` 기반 최소 lifecycle 연결
- InputField에서 editing/decorator 없는 최소 host 구조만 반영

완료 기준:

- 빈 `TextVisualizer`를 scene에 올릴 수 있다.
- relayout 경로가 호출된다.

### Phase 2. Prepare 모델 연결

목표:

- text / font / fontSize 기반 expensive prepare를 분리한다.

작업:

- prepared text model 구조 추가
- font fallback
- shaping
- glyph metrics
- cluster / glyph mapping 정보 정리

완료 기준:

- `Prepare()` 호출로 prepared 결과가 생성된다.
- text/font/fontSize 변경 시 prepare dirty가 올바르게 갱신된다.

### Phase 3. Layout 엔진 연결

목표:

- prepared 결과를 기반으로 shaping 없이 layout만 반복 수행한다.

작업:

- layout bounds 구조 추가
- exclusion region 구조 추가
- line interval 계산
- multi-line placement
- laid out glyph result 생성

완료 기준:

- width/height 변경 시 prepare 없이 layout만 다시 수행된다.
- 한 줄 안에 여러 interval이 생겨도 glyph 배치가 가능하다.

### Phase 4. AtlasRenderer adapter 연결

목표:

- layout 결과를 실제 atlas renderer에 전달한다.

작업:

- `AtlasRenderer`가 요구하는 최소 glyph view adapter 정의
- `Text::Backend::Get().NewRenderer()` 또는 atlas renderer 직접 연결 검토
- renderable actor attach/detach
- plain glyph rendering 동작 확인

완료 기준:

- multiline text가 scene에 표시된다.
- exclusion region을 반영한 결과가 렌더링된다.

### Phase 5. Public API 정리

목표:

- Dali UI 스타일과 일관된 naming을 확정한다.

작업:

- `SetText`
- `SetFontFamily`
- `SetFontSize`
- `SetTextColor`
- `Prepare`
- `SetLayoutBounds`
- `SetExclusionRegions`
- explicit `Layout()` API 노출 필요성 검토

완료 기준:

- API naming이 문서와 코드에서 일관된다.
- prepare와 relayout 역할이 분리된다.

### Phase 6. Dirty 정책 및 검증

목표:

- prepare/layout/render 경계를 안정화하고 실제 동적 레이아웃 환경에서 검증한다.

작업:

- sample 추가 또는 기존 sample 확장
- resize / wrap / repeated relayout 테스트
- exclusion region 이동/추가/삭제 테스트
- prepare 재실행 조건 검증

완료 기준:

- 빈번한 relayout에서도 텍스트 표시가 안정적이다.
- bounds 변화 시 shaping 재실행이 발생하지 않는다.

## 파일 구조 제안

정확한 경로는 팀 규칙에 맞게 조정 가능하지만, 추천 구조는 아래와 같다.

```text
dali-ui-foundation/public-api/text-visualizer.h
dali-ui-foundation/public-api/text-visualizer.cpp
dali-ui-foundation/integration-api/text-visualizer-impl.h
dali-ui-foundation/integration-api/text-visualizer-impl.cpp
dali-ui-foundation/internal/text/text-visualizer/text-prepare-model.h
dali-ui-foundation/internal/text/text-visualizer/text-prepare-model.cpp
dali-ui-foundation/internal/text/text-visualizer/text-layout-model.h
dali-ui-foundation/internal/text/text-visualizer/text-layout-model.cpp
dali-ui-foundation/internal/text/text-visualizer/text-atlas-view-adapter.h
dali-ui-foundation/internal/text/text-visualizer/text-atlas-view-adapter.cpp
```

필요 시 `common-text-utils` 쪽에 보조 함수 일부를 추가한다.

## 위험 요소와 대응

### 위험 1. prepare 단계가 기존 controller와 과도하게 얽힐 수 있음

대응:

- 1차는 필요한 prepare 데이터 계약을 먼저 문서화한다.
- shaping/fallback/glyph metrics 관련 로직만 선별적으로 사용한다.
- 편집 관련 상태가 들어오는 즉시 차단한다.

### 위험 2. exclusion region layout 복잡도 증가

대응:

- 1차는 직사각형 region만 지원한다.
- line interval 계산을 독립 유틸로 분리한다.
- cluster 단위 줄바꿈 정책을 명확히 정의한다.

### 위험 3. atlas renderer adapter 범위가 예상보다 넓을 수 있음

대응:

- `Text::ViewInterface` 전체를 구현하지 말고 1차 최소 subset을 명확히 잡는다.
- underline/shadow/ellipsis 관련 함수는 dummy 반환으로 시작한다.

### 위험 4. `InputField` 코드 복붙 유혹

대응:

- 필요한 lifecycle 패턴만 가져오고,
  이벤트/IME/decorator 코드는 의도적으로 분리한다.

## 우선 구현 순서

실제 작업 순서는 아래를 권장한다.

1. public/internal skeleton 생성
2. prepare model 연결
3. layout model 연결
4. atlas view adapter + renderer host 연결
5. public API naming 정리
6. sample 및 검증

## 테스트 관점 체크리스트

- 텍스트 설정 직후 표시되는가
- parent width 변경 시 shaping 없이 줄바꿈이 즉시 갱신되는가
- height-for-width가 일관적인가
- padding / RTL에서 위치가 맞는가
- empty text / whitespace only / emoji text가 깨지지 않는가
- repeated relayout 시 crash/leak 없이 안정적인가
- exclusion region이 한 줄을 여러 interval로 나눌 때 정상 배치되는가
- bounds만 바뀔 때 prepare가 다시 수행되지 않는가

## 구현 완료 정의

이번 기준 문서에서의 "구현 완료"는 아래를 의미한다.

- `TextVisualizer`가 읽기 전용 text View로 사용할 수 있다.
- dynamic relayout에 안정적으로 반응한다.
- 기존 `InputField`나 `TextVisual`를 우회하지 않고도 독립적으로 사용할 수 있다.
- 향후 selection/async/fallback renderer 같은 확장을 붙일 수 있는 구조다.
- prepare와 layout이 명확히 분리되어 있다.

## 최종 제안

가장 안전한 시작점은 아래 조합이다.

> 1차 구현은 `InputField`의 View 구조를 참고해 `TextVisualizer`를 만들고, 내부는 `Prepare -> Layout -> AtlasRenderer Adapter` 3단계로 분리한다.
