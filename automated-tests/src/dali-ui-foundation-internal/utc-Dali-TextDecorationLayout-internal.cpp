/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dali-ui-foundation/internal/text/async-text/text-loading-task.h>
#include <dali-ui-foundation/dali-ui-foundation.h>
#include <dali-ui-test-suite-utils.h>
#include <dali-ui-foundation/integration-api/input-editor-impl.h>
#include <dali-ui-foundation/integration-api/input-field-impl.h>
#include <dali-ui-foundation/integration-api/label-impl.h>
#include <dali-ui-foundation/internal/text/async-text/async-text-loader-impl.h>
#include <dali-ui-foundation/internal/text/controller/text-controller-impl.h>
#include <dali-ui-foundation/internal/text/cursor-helper-functions.h>
#include <dali-ui-foundation/internal/text/decorator/text-decorator.h>
#include <dali-ui-foundation/internal/text/final-elision-result.h>
#include <dali-ui-foundation/internal/text/rendering/text-raster-geometry.h>
#include <dali-ui-foundation/internal/text/rendering/text-typesetter.h>
#include <dali-ui-foundation/internal/text/text-geometry.h>
#include <dali/integration-api/adaptor-framework/input-method-context-integ.h>
#include <dali/integration-api/rendering/visual-renderer.h>
#include <sstream>
#include <string>
#include <utility>

using namespace Dali;
using namespace Dali::Ui;

void utc_dali_text_decoration_layout_internal_startup(void)
{
  test_return_value = TET_UNDEF;
}

void utc_dali_text_decoration_layout_internal_cleanup(void)
{
  test_return_value = TET_PASS;
}

namespace
{
template<typename Tag, auto member>
struct MemberAccess
{
  friend auto GetMember(Tag)
  {
    return member;
  }
};
struct LabelController
{
  friend auto GetMember(LabelController);
};
struct FieldController
{
  friend auto GetMember(FieldController);
};
struct EditorController
{
  friend auto GetMember(EditorController);
};
struct LabelParameters
{
  friend auto GetMember(LabelParameters);
};
struct WorkerModel
{
  friend auto GetMember(WorkerModel);
};
struct WorkerModule
{
  friend auto GetMember(WorkerModule);
};
struct TaskLoad
{
  friend auto GetMember(TaskLoad);
};
template struct MemberAccess<LabelController, &Ui::Integration::LabelImpl::mController>;
template struct MemberAccess<FieldController, &Ui::Integration::InputFieldImpl::mController>;
template struct MemberAccess<EditorController, &Ui::Integration::InputEditorImpl::mController>;
template struct MemberAccess<LabelParameters, &Ui::Integration::LabelImpl::GetAsyncTextParameters>;
template struct MemberAccess<WorkerModel, &Text::Internal::AsyncTextLoader::GetRenderTextModel>;
template struct MemberAccess<WorkerModule, &Text::Internal::AsyncTextLoader::mModule>;
template struct MemberAccess<TaskLoad, &Ui::Internal::TextLoadingTask::Load>;

Text::ControllerPtr ControllerFor(Label label)
{
  return static_cast<Ui::Integration::LabelImpl&>(GetImpl(label)).*GetMember(LabelController{});
}
Text::ControllerPtr ControllerFor(InputField field)
{
  return static_cast<Ui::Integration::InputFieldImpl&>(GetImpl(field)).*GetMember(FieldController{});
}
Text::ControllerPtr ControllerFor(InputEditor editor)
{
  return static_cast<Ui::Integration::InputEditorImpl&>(GetImpl(editor)).*GetMember(EditorController{});
}

void Flush(UiTestApplication& app)
{
  for(unsigned i = 0; i < 3; ++i)
  {
    app.SendNotification();
    app.Render();
  }
}

// Serialize values, not struct padding. Hex floats make equality exact.
std::string ModelSnapshot(const Text::ModelInterface* model)
{
  std::ostringstream out;
  out << std::hexfloat << model->GetLayoutSize().x << ' ' << model->GetLayoutSize().y << ' ';
  out << model->GetNumberOfGlyphs() << ' ' << model->GetNumberOfLines() << ' ';
  for(unsigned i = 0; i < model->GetNumberOfGlyphs(); ++i)
  {
    const auto& glyph = model->GetGlyphs()[i];
    const auto& position = model->GetLayout()[i];
    out << glyph.fontId << ' ' << glyph.index << ' ' << glyph.advance << ' '
        << glyph.width << ' ' << glyph.height << ' ' << glyph.xBearing << ' ' << glyph.yBearing << ' '
        << position.x << ' ' << position.y << ' ';
  }
  for(unsigned i = 0; i < model->GetNumberOfLines(); ++i)
  {
    const auto& line = model->GetLines()[i];
    out << line.glyphRun.glyphIndex << ' ' << line.glyphRun.numberOfGlyphs << ' '
        << line.characterRun.characterIndex << ' ' << line.characterRun.numberOfCharacters << ' '
        << line.width << ' ' << line.ascender << ' ' << line.descender << ' '
        << line.extraLength << ' ' << line.alignmentOffset << ' ' << line.lineSpacing << ' '
        << line.direction << ' ' << line.ellipsis << ' ' << line.isSplitToTwoHalves << ' ';
    if(line.isSplitToTwoHalves)
    {
      out << line.glyphRunSecondHalf.glyphIndex << ' ' << line.glyphRunSecondHalf.numberOfGlyphs << ' '
          << line.characterRunForSecondHalfLine.characterIndex << ' '
          << line.characterRunForSecondHalfLine.numberOfCharacters << ' ';
    }
  }
  return out.str();
}

std::string LayoutSnapshot(Text::ControllerPtr controller)
{
  std::ostringstream out;
  out << std::hexfloat << ModelSnapshot(controller->GetRenderTextModel()) << ' '
      << controller->GetTextFitFontSize(Text::Controller::PIXEL_SIZE) << ' ';
  const auto& impl = Text::Controller::Impl::GetImplementation(*controller);
  for(auto index : impl.mModel->mVisualModel->mGlyphsToCharacters)
  {
    out << index << ' ';
  }
  for(auto count : impl.mModel->mVisualModel->mCharactersPerGlyph)
  {
    out << count << ' ';
  }
  if(const auto* final = controller->GetFinalElisionResult())
  {
    out << final->resolved << ' ' << final->textElided << ' ' << final->startIndex << ' ' << final->endIndex << ' '
        << final->firstMiddleIndex << ' ' << final->secondMiddleIndex << ' ' << final->ellipsisFinalGlyphIndex << ' '
        << final->ellipsisLineIndex << ' ' << final->ellipsisUnitCount << ' ';
    for(auto index : final->sourceToFinalGlyphIndices)
    {
      out << index << ' ';
    }
    for(auto index : final->finalToSourceGlyphIndices)
    {
      out << index << ' ';
    }
    for(auto position : final->viewGlyphPositions)
    {
      out << position.x << ' ' << position.y << ' ';
    }
  }
  return out.str();
}

template<typename Control>
void SetDecoration(Control control, unsigned style)
{
  control.SetTextOutline(Text::Outline::None());
  control.SetTextShadow(Text::Shadow::None());
  control.SetTextUnderline(Text::Underline::None());
  control.SetTextLineThrough(Text::LineThrough::None());
  control.SetTextBackgroundColor(UiColor(Color::TRANSPARENT));
  if((style >= 1 && style <= 3) || style == 8)
  {
    Text::Outline outline;
    outline.SetWidth(style == 1 ? 1 : 12);
    if(style >= 3)
    {
      outline.SetOffset(Vector2(-9, 7));
      outline.SetBlurRadius(5);
    }
    control.SetTextOutline(outline);
  }
  if(style == 4 || style == 8)
  {
    Text::Shadow shadow;
    shadow.SetOffset(Vector2(-20, 15));
    shadow.SetBlurRadius(8);
    control.SetTextShadow(shadow);
  }
  if(style == 5 || style == 8)
  {
    Text::Underline underline;
    underline.SetThickness(15);
    control.SetTextUnderline(underline);
  }
  if(style == 6 || style == 8)
  {
    Text::LineThrough strike;
    strike.SetThickness(10);
    control.SetTextLineThrough(strike);
  }
  if(style == 7)
  {
    control.SetTextBackgroundColor(UiColor(Color::BLUE));
  }
}

template<typename Control>
void Configure(Control control, const char* text)
{
  control.SetText(text);
  control.SetFontFamily("DejaVu Sans");
  control.SetFontSize(20);
  control.SetSystemFontSizeScaleEnabled(false);
  control.SetRequestedWidth(190);
  control.SetRequestedHeight(70);
}

std::string EditableSnapshot(Text::ControllerPtr controller)
{
  auto& impl = Text::Controller::Impl::GetImplementation(*controller);
  std::ostringstream out;
  out << std::hexfloat << LayoutSnapshot(controller) << ' ' << impl.mModel->mScrollPosition.x << ' '
      << impl.mModel->mScrollPosition.y << ' ' << impl.mEventData->mPrimaryCursorPosition << ' '
      << impl.mEventData->mLeftSelectionPosition << ' ' << impl.mEventData->mRightSelectionPosition << ' ';
  const auto count = static_cast<Text::CharacterIndex>(impl.mModel->mLogicalModel->mText.Count());
  for(Text::CharacterIndex i = 0; i <= count; ++i)
  {
    Text::CursorInfo cursor;
    impl.GetCursorPosition(i, cursor);
    out << cursor.primaryPosition.x << ' ' << cursor.primaryPosition.y << ' '
        << cursor.secondaryPosition.x << ' ' << cursor.secondaryPosition.y << ' '
        << cursor.primaryCaretPosition.x << ' ' << cursor.primaryCaretPosition.y << ' '
        << cursor.secondaryCaretPosition.x << ' ' << cursor.secondaryCaretPosition.y << ' '
        << cursor.lineOffset << ' ' << cursor.glyphOffset << ' ' << cursor.lineHeight << ' '
        << cursor.primaryCursorHeight << ' ' << cursor.secondaryCursorHeight << ' '
        << cursor.primaryCaretHeight << ' ' << cursor.secondaryCaretHeight << ' '
        << cursor.isSecondaryCursor << ' ' << cursor.hasPrimaryCaretGeometry << ' ' << cursor.hasSecondaryCaretGeometry << ' ';
  }
  Vector<Vector2> sizes, positions;
  Text::GetTextGeometry(impl.mModel, 1, count - 1, sizes, positions);
  for(auto size : sizes)
  {
    out << size.x << ' ' << size.y << ' ';
  }
  for(auto position : positions)
  {
    out << position.x << ' ' << position.y << ' ';
  }
  for(float y : {5.f, 35.f, 65.f})
  {
    for(float x : {0.f, 10.f, 40.f, 100.f, 185.f})
    {
      bool hit = false;
      out << Text::GetClosestCursorIndex(impl.mModel->mVisualModel, impl.mModel->mLogicalModel,
                                         impl.mMetrics, x, y, Text::CharacterHitTest::TAP, hit) << ' ' << hit << ' ';
    }
  }
  return out.str();
}

template<typename Control>
bool CheckEditable(UiTestApplication& app, const char* text)
{
  auto control = Control::New();
  Configure(control, text);
  control.SetCursorBlinkEnabled(false);
  app.GetScene().Add(control);
  Flush(app);
  auto controller = ControllerFor(control);
  if(controller->GetRenderTextModel()->GetNumberOfGlyphs() == 0u)
  {
    return false;
  }
  controller->KeyboardFocusGainEvent(false);
  const unsigned count = Text::Controller::Impl::GetImplementation(*controller).mModel->mLogicalModel->mText.Count();
  for(unsigned stage = 0; stage < 3; ++stage)
  {
    if(stage == 2)
    {
      // Exercise preedit independently of the selection replacement lifecycle.
      control.Unparent();
      control = Control::New();
      Configure(control, text);
      control.SetCursorBlinkEnabled(false);
      app.GetScene().Add(control);
      Flush(app);
      controller = ControllerFor(control);
      controller->KeyboardFocusGainEvent(false);
    }
    tet_printf("Editable stage %u text %s\n", stage, text);
    SetDecoration(control, 0);
    controller->SetPrimaryCursorPosition(stage == 1 ? 2 : count, true);
    if(stage == 1)
    {
      controller->SelectText(1, count - 1);
    }
    if(stage == 2)
    {
      Flush(app);
      InputMethodContext context = control.GetInputMethodContext();
      Dali::Integration::InputMethodContext::EventData event;
      event.eventName = Dali::Integration::InputMethodContext::PRE_EDIT;
      event.predictiveString = "preedit";
      controller->OnInputMethodContextEvent(context, event);
    }
    Flush(app);
    const auto baseline = EditableSnapshot(controller);
    Actor highlight = control.FindChildByName("HighlightActor");
    const Vector3 highlightPosition = highlight ? highlight.GetCurrentProperty<Vector3>(Actor::Property::POSITION) : Vector3::ZERO;
    const Vector3 highlightSize = highlight ? highlight.GetCurrentProperty<Vector3>(Actor::Property::SIZE) : Vector3::ZERO;
    for(unsigned style = 1; style <= 8; ++style)
    {
      // The existing atlas background/highlight reorder path requires a newly
      // supplied highlight actor. Background layout is covered without selection.
      if(stage == 1 && style == 7)
      {
        continue;
      }
      SetDecoration(control, style);
      Flush(app);
      if(baseline != EditableSnapshot(controller))
      {
        tet_printf("Editable decoration layout mismatch stage %u style %u\n", stage, style);
        return false;
      }
      if(highlight)
      {
        const Actor currentHighlight = control.FindChildByName("HighlightActor");
        if(!currentHighlight || currentHighlight.GetCurrentProperty<Vector3>(Actor::Property::POSITION) != highlightPosition ||
           currentHighlight.GetCurrentProperty<Vector3>(Actor::Property::SIZE) != highlightSize)
        {
          return false;
        }
      }
    }
  }
  control.Unparent();
  return true;
}
} // namespace

int UtcDaliTextDecorationLayoutLabelP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  const char* texts[] = {"A very long line with jAf ffi to wrap and fit", "한글 문장 테스트 여러 글자",
                         "مرحبا بالعالم كيف حالك", "Hello مرحبا 123 world", "jÁf Á ffi fl", "one two\nthree four\nfive six"};
  for(unsigned mode = 0; mode < 12; ++mode)
  {
    for(auto alignment : {Text::Alignment::START, Text::Alignment::CENTER, Text::Alignment::END})
    {
      auto label = Label::New();
      Configure(label, texts[mode % 6]);
      label.SetHorizontalTextAlignment(alignment);
      label.SetVerticalTextAlignment(alignment);
      label.SetTextOverflowMode(Text::OverflowMode::CLIP);
      auto controller = ControllerFor(label);
      if(mode == 0)
      {
        label.SetRequestedWidth(WRAP_CONTENT);
        label.SetRequestedHeight(WRAP_CONTENT);
      }
      if(mode == 1 || mode == 8)
      {
        label.SetTextFit(Text::Fit::Range(8, 24, 1));
      }
      if(mode == 2 || mode == 3 || mode == 9)
      {
        label.SetTextOverflowMode(Text::OverflowMode::ELLIPSIS);
      }
      if(mode == 5 || mode == 9)
      {
        label.SetMaximumLines(2);
      }
      if(mode == 6 || mode == 7)
      {
        controller->SetRemoveFrontInset(mode == 6);
      }
      if(mode == 6 || mode == 7)
      {
        controller->SetRemoveBackInset(mode == 7);
      }
      app.GetScene().Add(label);
      std::string baseline;
      Vector3 natural, actorSize;
      for(unsigned style = 0; style <= 8; ++style)
      {
        SetDecoration(label, style);
        Flush(app);
        const auto currentNatural = label.GetNaturalSize();
        const auto currentSize = label.GetCurrentProperty<Vector3>(Actor::Property::SIZE);
        // Natural-size queries temporarily lay out the model. Restore the actual box before comparison.
        controller->Relayout(Vector2(currentSize.x, currentSize.y));
        DALI_TEST_CHECK(controller->GetRenderTextModel()->GetNumberOfGlyphs() > 0u);
        const auto snapshot = LayoutSnapshot(controller);
        if(style == 0)
        {
          baseline  = snapshot;
          natural   = currentNatural;
          actorSize = currentSize;
        }
        tet_printf("Label layout mode %u alignment %u style %u\n", mode, static_cast<unsigned>(alignment), style);
        DALI_TEST_CHECK(snapshot == baseline);
        DALI_TEST_EQUALS(currentNatural, natural, TEST_LOCATION);
        DALI_TEST_EQUALS(currentSize, actorSize, TEST_LOCATION);
      }
      label.Unparent();
    }
  }
  END_TEST;
}

int UtcDaliTextDecorationLayoutInputFieldP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  DALI_TEST_CHECK(CheckEditable<InputField>(app, "one two three four five six seven eight"));
  DALI_TEST_CHECK(CheckEditable<InputField>(app, "Hello مرحبا 123 world"));
  END_TEST;
}

int UtcDaliTextDecorationLayoutInputEditorP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  DALI_TEST_CHECK(CheckEditable<InputEditor>(app, "one two\nthree four\nfive six\nseven eight\nnine ten"));
  DALI_TEST_CHECK(CheckEditable<InputEditor>(app, "مرحبا بالعالم\nHello world 123\n한글 테스트"));
  END_TEST;
}

int UtcDaliTextDecorationLayoutPaintP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto label = Label::New();
  Configure(label, "jÁf ffi Hello مرحبا");
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  app.GetScene().Add(label);
  auto controller = ControllerFor(label);
  for(auto alignment : {Text::Alignment::START, Text::Alignment::CENTER, Text::Alignment::END})
  {
    label.SetHorizontalTextAlignment(alignment);
    Text::Raster::Bounds fill;
    for(unsigned style = 0; style <= 3; ++style)
    {
      SetDecoration(label, style);
      Flush(app);
      auto typesetter = Text::Typesetter::New(controller->GetRenderTextModel());
      typesetter->SetFinalElisionResult(controller->GetFinalElisionResult());
      Text::TextRasterResult result;
      DALI_TEST_CHECK(typesetter->RenderRasterPlanes(Vector2(190, 70), controller->GetTextDirection(), Pixel::L8, 4094, result));
      if(style == 0)
      {
        fill = result.geometry.fill;
      }
      DALI_TEST_CHECK(result.geometry.fill.left == fill.left && result.geometry.fill.top == fill.top &&
                      result.geometry.fill.right == fill.right && result.geometry.fill.bottom == fill.bottom);
      if(style)
      {
        DALI_TEST_CHECK(result.underlay);
        DALI_TEST_CHECK(result.geometry.composition.left < fill.left || result.geometry.composition.top < fill.top);
      }
    }
  }
  END_TEST;
}

int UtcDaliTextDecorationLayoutAsyncScaleP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  struct ScaleGuard
  {
    float scale{UiScaleManager::Get().GetScale()};
    bool scalable{UiScaleManager::Get().IsScalable()};
    ~ScaleGuard()
    {
      UiScaleManager::Get().SetScale(scale);
      UiScaleManager::Get().SetScalable(scalable);
    }
  } guard;
  UiScaleManager::Get().SetScalable(true);
  auto label = Label::New();
  Configure(label, "Hello مرحبا jÁf ffi");
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  app.GetScene().Add(label);
  auto controller = ControllerFor(label);
  auto loader = Text::AsyncTextLoader::New();
  (Text::GetImplementation(loader).*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  std::string firstLayout;
  for(float uiScale : {1.f, 1.5f, 2.f, 1.f})
  {
    UiScaleManager::Get().SetScale(uiScale);
    for(float renderScale : {1.f, 1.5f, 2.f})
    {
      std::string workerBaseline, sourceBaseline;
      Text::Raster::Bounds fill;
      for(unsigned style = 0; style <= 3; ++style)
      {
        SetDecoration(label, style);
        Flush(app);
        const auto source = LayoutSnapshot(controller);
        auto& labelImpl = static_cast<Ui::Integration::LabelImpl&>(GetImpl(label));
        const auto size = label.GetCurrentProperty<Vector3>(Actor::Property::SIZE);
        auto params = (labelImpl.*GetMember(LabelParameters{}))(Ui::Integration::Text::Async::RENDER_FIXED_SIZE,
                                                               Vector2(size.x, size.y), Insets(), LayoutDirection::LEFT_TO_RIGHT);
        params.isEffectOverflowRequested = true;
        params.maxTextureSize = 4096;
        params.renderScale = renderScale;
        Ui::Internal::TextLoadingTask task(1u, std::move(params), MakeCallback(+[]() {}));
        task.mLoader = loader;
        (task.*GetMember(TaskLoad{}))();
        task.mLoader.Reset();
        DALI_TEST_CHECK(task.mRenderInfo.effectOverflow);
        const auto model = (Text::GetImplementation(loader).*GetMember(WorkerModel{}))();
        DALI_TEST_CHECK(model->GetNumberOfGlyphs() > 0u);
        const auto snapshot = ModelSnapshot(model);
        const auto& bounds = task.mRenderInfo.effectOverflow->geometry.fill;
        if(style == 0)
        {
          workerBaseline = snapshot;
          sourceBaseline = source;
          fill = bounds;
          if(uiScale == 1.f)
          {
            if(firstLayout.empty())
            {
              firstLayout = source;
            }
            DALI_TEST_CHECK(firstLayout == source);
          }
        }
        tet_printf("Decoration worker UI %g renderScale %g style %u\n", uiScale, renderScale, style);
        DALI_TEST_CHECK(snapshot == workerBaseline);
        DALI_TEST_CHECK(source == sourceBaseline);
        DALI_TEST_CHECK(bounds.left == fill.left && bounds.top == fill.top && bounds.right == fill.right && bounds.bottom == fill.bottom);
      }
    }
  }
  END_TEST;
}

int UtcDaliTextDecorationLayoutPaintUpdateP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto label = Label::New();
  Configure(label, "Hello");
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  static_cast<Ui::Integration::LabelImpl&>(GetImpl(label)).SetEffectOverflowEnabled(true);
  app.GetScene().Add(label);
  Flush(app);
  const auto baseline = LayoutSnapshot(ControllerFor(label));
  const auto fillSize = label.GetRendererAt(0).GetProperty<Vector2>(VisualRenderer::Property::TRANSFORM_SIZE);
  Vector2 previousSize, previousOffset;
  for(unsigned stage = 0; stage < 5; ++stage)
  {
    Text::Outline outline;
    outline.SetWidth(12);
    if(stage >= 1)
    {
      outline.SetOffset(Vector2(-25, -15));
    }
    if(stage >= 2)
    {
      outline.SetBlurRadius(10);
    }
    if(stage >= 3)
    {
      outline.SetColor(UiColor(Color::RED));
    }
    label.SetTextOutline(stage == 4 ? Text::Outline::None() : outline);
    Flush(app);
    DALI_TEST_CHECK(LayoutSnapshot(ControllerFor(label)) == baseline);
    const auto renderer = label.GetRendererAt(0);
    const auto size = renderer.GetProperty<Vector2>(VisualRenderer::Property::TRANSFORM_SIZE);
    const auto offset = renderer.GetProperty<Vector2>(VisualRenderer::Property::TRANSFORM_OFFSET);
    if(stage < 4)
    {
      DALI_TEST_EQUALS(renderer.GetTextures().GetTextureCount(), 2u, TEST_LOCATION);
    }
    if(stage == 1)
    {
      DALI_TEST_CHECK(offset != previousOffset);
    }
    if(stage == 2)
    {
      DALI_TEST_CHECK(size.width > previousSize.width && size.height > previousSize.height);
    }
    if(stage == 4)
    {
      DALI_TEST_EQUALS(renderer.GetTextures().GetTextureCount(), 1u, TEST_LOCATION);
      DALI_TEST_EQUALS(size, fillSize, TEST_LOCATION);
    }
    previousSize = size;
    previousOffset = offset;
  }
  END_TEST;
}
