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
 *
 */

#include <dali-ui-foundation/dali-ui-foundation.h>
#include <dali-ui-test-suite-utils.h>
#include <dali-ui-foundation/integration-api/input-field-impl.h>
#include <dali-ui-foundation/integration-api/input-editor-impl.h>
#include <dali-ui-foundation/internal/text/controller/text-controller.h>
#include <dali-ui-foundation/internal/text/rendering/editable-text-group.h>
#include <dali-ui-foundation/internal/text/rendering/atlas/atlas-glyph-manager.h>
#include <dali/integration-api/adaptor-framework/input-method-context-integ.h>
#include <dali/public-api/adaptor-framework/pixel-buffer.h>
#include <dali/internal/event/rendering/geometry-impl.h>
#include <dali/public-api/object/weak-handle.h>
#include <memory>

using namespace Dali;
using namespace Dali::Ui;

void utc_dali_editable_text_paint_internal_startup(void)
{
  test_return_value = TET_UNDEF;
}
void utc_dali_editable_text_paint_internal_cleanup(void)
{
  test_return_value = TET_PASS;
}

namespace
{
template<typename Tag, auto member> struct Access
{
  friend auto Get(Tag) { return member; }
};
struct FieldGroup { friend auto Get(FieldGroup); };
struct EditorGroup { friend auto Get(EditorGroup); };
struct FieldController { friend auto Get(FieldController); };
struct EditorController { friend auto Get(EditorController); };
struct GroupTask { friend auto Get(GroupTask); };
struct GroupOutput { friend auto Get(GroupOutput); };
struct FieldText { friend auto Get(FieldText); };
struct EditorText { friend auto Get(EditorText); };
struct GeometryBuffers { friend auto Get(GeometryBuffers); };
template struct Access<FieldGroup, &Ui::Integration::InputFieldImpl::mTextGroup>;
template struct Access<EditorGroup, &Ui::Integration::InputEditorImpl::mTextGroup>;
template struct Access<FieldController, &Ui::Integration::InputFieldImpl::mController>;
template struct Access<EditorController, &Ui::Integration::InputEditorImpl::mController>;
template struct Access<GroupTask, &Text::EditableTextGroup::mTask>;
template struct Access<GroupOutput, &Text::EditableTextGroup::mOutput>;
template struct Access<FieldText, &Ui::Integration::InputFieldImpl::mRenderableActor>;
template struct Access<EditorText, &Ui::Integration::InputEditorImpl::mRenderableActor>;
template struct Access<GeometryBuffers, &Dali::Internal::Geometry::mVertexBuffers>;

Text::EditableTextGroup* Group(InputField c) { return (static_cast<Ui::Integration::InputFieldImpl&>(GetImpl(c)).*Get(FieldGroup{})).get(); }
Text::EditableTextGroup* Group(InputEditor c) { return (static_cast<Ui::Integration::InputEditorImpl&>(GetImpl(c)).*Get(EditorGroup{})).get(); }
Text::ControllerPtr Controller(InputField c) { return static_cast<Ui::Integration::InputFieldImpl&>(GetImpl(c)).*Get(FieldController{}); }
Text::ControllerPtr Controller(InputEditor c) { return static_cast<Ui::Integration::InputEditorImpl&>(GetImpl(c)).*Get(EditorController{}); }
Actor TextActor(InputField c) { return static_cast<Ui::Integration::InputFieldImpl&>(GetImpl(c)).*Get(FieldText{}); }
Actor TextActor(InputEditor c) { return static_cast<Ui::Integration::InputEditorImpl&>(GetImpl(c)).*Get(EditorText{}); }
void Flush(UiTestApplication& app)
{
  for(unsigned i = 0; i < 3u; ++i) { app.SendNotification(); app.Render(); }
}
template<class Control> int Composition()
{
  UiTestApplication app;
  auto control = Control::New();
  control.SetText("Group opacity");
  control.SetRequestedWidth(240.0f);
  control.SetRequestedHeight(80.0f);
  control.SetFontFamily("DejaVu Sans");
  control.SetFontSize(20.0f);
  control.SetSystemFontSizeScaleEnabled(false);
  control.SetCursorBlinkEnabled(false);
  app.GetScene().Add(control);
  Flush(app);
  auto tasks = app.GetScene().GetRenderTaskList();
  const auto count = tasks.GetTaskCount();
  DALI_TEST_CHECK(!Group(control));
  auto controller = Controller(control);
  const Vector2 logicalSize = controller->GetRenderTextModel()->GetLayoutSize();
  const Vector2 firstGlyph = controller->GetRenderTextModel()->GetLayout()[0u];
  Text::Outline outline;
  outline.SetWidth(12.0f);
  outline.SetOffset(Vector2(-7.0f, 5.0f));
  outline.SetBlurRadius(3.0f);
  control.SetTextOutline(outline);
  control.SetProperty(Actor::Property::OPACITY, 0.5f);
  Flush(app);
  DALI_TEST_CHECK(Group(control));
  DALI_TEST_EQUALS(tasks.GetTaskCount(), count + 1u, TEST_LOCATION);
  RenderTask task = Group(control)->*Get(GroupTask{});
  Actor output = Group(control)->*Get(GroupOutput{});
  DALI_TEST_CHECK(task.IsExclusive());
  DALI_TEST_CHECK(!task.GetInputEnabled());
  DALI_TEST_CHECK(task.GetSourceActor() != control);
  DALI_TEST_EQUALS(task.GetSourceActor().GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR), Vector4::ONE, 0.001f, TEST_LOCATION);
  DALI_TEST_EQUALS(output.GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, 0.5f, 0.001f, TEST_LOCATION);
  DALI_TEST_CHECK(output.GetRendererAt(0u).GetProperty<bool>(Renderer::Property::BLEND_PRE_MULTIPLIED_ALPHA));
  DALI_TEST_EQUALS(controller->GetRenderTextModel()->GetLayoutSize(), logicalSize, TEST_LOCATION);
  DALI_TEST_EQUALS(controller->GetRenderTextModel()->GetLayout()[0u], firstGlyph, TEST_LOCATION);
  Texture target = output.GetRendererAt(0u).GetTextures().GetTexture(0u);

  controller->SetMaximumNumberOfCharacters(1000u);
  controller->KeyboardFocusGainEvent(false);
  controller->SetPrimaryCursorPosition(5u, true);
  auto context = control.GetInputMethodContext();
  Dali::Integration::InputMethodContext::EventData commit;
  commit.eventName = Dali::Integration::InputMethodContext::COMMIT;
  commit.predictiveString = "x";
  controller->OnInputMethodContextEvent(context, commit);
  Flush(app);
  DALI_TEST_EQUALS(control.GetText(), Dali::String("Groupx opacity"), TEST_LOCATION);
  DALI_TEST_CHECK(Group(control)->*Get(GroupTask{}) == task);
  DALI_TEST_CHECK((Group(control)->*Get(GroupOutput{})).GetRendererAt(0u).GetTextures().GetTexture(0u) == target);
  controller->SelectText(0u, 5u);
  controller->ScrollBy(Vector2(-5.0f, -5.0f));
  Flush(app);
  DALI_TEST_CHECK(Group(control)->*Get(GroupTask{}) == task);
  Actor highlight = task.GetSourceActor().FindChildByName("HighlightActor");
  DALI_TEST_CHECK(highlight && highlight.GetParent() == task.GetSourceActor());
  DALI_TEST_EQUALS(control.GetSelectedText(), Dali::String("Group"), TEST_LOCATION);
  END_TEST;
}

template<class Control> int BackgroundComposition()
{
  UiTestApplication app;
  auto control = Control::New();
  control.SetText("Background");
  control.SetRequestedWidth(240.0f);
  control.SetRequestedHeight(80.0f);
  control.SetTextBackgroundColor(UiColor(Color::BLUE));
  control.SetProperty(Actor::Property::OPACITY, 0.5f);
  app.GetScene().Add(control);
  Flush(app);
  DALI_TEST_CHECK(Group(control));
  RenderTask task = Group(control)->*Get(GroupTask{});
  Actor background = task.GetSourceActor().FindChildByName("TextBackgroundColorActor");
  DALI_TEST_CHECK(background && background.GetParent() == task.GetSourceActor());
  DALI_TEST_EQUALS(background.GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, 1.0f, 0.001f, TEST_LOCATION);
  END_TEST;
}

template<class Control> int Lifecycle()
{
  UiTestApplication app;
  auto control = Control::New();
  control.SetText("Lifecycle");
  control.SetRequestedWidth(200.0f);
  control.SetRequestedHeight(70.0f);
  app.GetScene().Add(control);
  Flush(app);
  auto tasks = app.GetScene().GetRenderTaskList();
  const auto count = tasks.GetTaskCount();
  for(unsigned i = 0; i < 30u; ++i)
  {
    Text::Outline outline;
    outline.SetWidth(2.0f);
    control.SetTextOutline(outline);
    Flush(app);
    DALI_TEST_EQUALS(tasks.GetTaskCount(), count + 1u, TEST_LOCATION);
    control.Unparent();
    Flush(app);
    DALI_TEST_EQUALS(tasks.GetTaskCount(), count, TEST_LOCATION);
    app.GetScene().Add(control);
    Flush(app);
    DALI_TEST_EQUALS(tasks.GetTaskCount(), count + 1u, TEST_LOCATION);
    control.SetRequestedWidth(200.0f + i);
    Flush(app);
    Actor output = Group(control)->*Get(GroupOutput{});
    DALI_TEST_EQUALS(output.GetRendererAt(0u).GetTextures().GetTexture(0u).GetWidth(), 200u + i, TEST_LOCATION);
    control.SetTextOutline(Text::Outline::None());
    Flush(app);
    DALI_TEST_CHECK(!Group(control));
    DALI_TEST_EQUALS(tasks.GetTaskCount(), count, TEST_LOCATION);
  }
  END_TEST;
}
} // namespace

int UtcDaliEditableTextPaintInputFieldCompositionP(void)
{
  return Composition<InputField>();
}
int UtcDaliEditableTextPaintInputEditorCompositionP(void)
{
  return Composition<InputEditor>();
}
int UtcDaliEditableTextPaintInputFieldLifecycleP(void)
{
  return Lifecycle<InputField>();
}
int UtcDaliEditableTextPaintInputEditorLifecycleP(void)
{
  return Lifecycle<InputEditor>();
}

int UtcDaliEditableTextPaintGlyphCacheOriginP(void)
{
  UiTestApplication app;
  AtlasGlyphManager cache = AtlasGlyphManager::Get();
  Text::GlyphInfo glyph{};
  glyph.fontId = 123u;
  glyph.index = 42u;
  AtlasGlyphManager::GlyphStyle style;
  style.outline = 2u;
  style.blurRadius = 3.0f;
  auto pixels = PixelBuffer::New(16u, 16u, Pixel::L8);
  PixelData bitmap = PixelBuffer::Convert(pixels);
  cache.SetNewAtlasSize(64u, 64u, 16u, 16u);
  AtlasManager::AtlasSlot slot{};
  cache.Add(glyph, style, bitmap, slot, Vector2(-6.0f, -5.0f));
  DALI_TEST_CHECK(slot.mImageId != 0u);
  AtlasManager::AtlasSlot found{};
  Vector2 origin;
  DALI_TEST_CHECK(cache.IsCached(glyph.fontId, glyph.index, style, found, &origin));
  DALI_TEST_EQUALS(origin, Vector2(-6.0f, -5.0f), TEST_LOCATION);
  DALI_TEST_EQUALS(found.mImageId, slot.mImageId, TEST_LOCATION);
  style.blurRadius = 4.0f;
  DALI_TEST_CHECK(!cache.IsCached(glyph.fontId, glyph.index, style, found, &origin));
  style.blurRadius = 3.0f;
  style.mask = true;
  DALI_TEST_CHECK(!cache.IsCached(glyph.fontId, glyph.index, style, found, &origin));
  cache.AdjustReferenceCount(glyph.fontId, slot.mImageId, -1);
  END_TEST;
}

int UtcDaliEditableTextPaintInputFieldBackgroundP(void)
{
  return BackgroundComposition<InputField>();
}

int UtcDaliEditableTextPaintInputEditorBackgroundP(void)
{
  return BackgroundComposition<InputEditor>();
}

namespace
{
template<class Control> int SelectionBackgroundMatrix()
{
  UiTestApplication app;
  for(unsigned style = 0u; style < 6u; ++style)
  {
    for(unsigned opacityMode = 0u; opacityMode < 3u; ++opacityMode)
    {
      const float opacity = opacityMode == 0u ? 1.0f : 0.5f;
      auto parent = Actor::New();
      app.GetScene().Add(parent);
      auto c = Control::New();
      c.SetText("one two three");
      c.SetRequestedWidth(300.0f);
      c.SetRequestedHeight(100.0f);
      c.SetCursorBlinkEnabled(false);
      parent.Add(c);
      Flush(app);
      const bool selection = style != 0u;
      if(selection) { c.SelectText(0u, 7u); Flush(app); }
      if(style != 1u) { c.SetTextBackgroundColor(UiColor(Color::BLUE)); }
      if(style == 3u || style == 5u)
      {
        Text::Outline outline;
        outline.SetWidth(5.0f);
        c.SetTextOutline(outline);
      }
      if(style == 4u || style == 5u)
      {
        Text::Shadow shadow;
        shadow.SetOffset(Vector2(-7.0f, 9.0f));
        shadow.SetBlurRadius(3.0f);
        c.SetTextShadow(shadow);
      }
      Actor opacityActor = opacityMode == 2u ? parent : Actor(c);
      opacityActor.SetProperty(Actor::Property::OPACITY, opacity);
      Flush(app);
      DALI_TEST_CHECK(Group(c));
      RenderTask task = Group(c)->*Get(GroupTask{});
      Actor source = task.GetSourceActor();
      DALI_TEST_EQUALS(source.GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, 1.0f, 0.001f, TEST_LOCATION);
      Actor output = Group(c)->*Get(GroupOutput{});
      DALI_TEST_EQUALS(output.GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, opacity, 0.001f, TEST_LOCATION);
      if(selection)
      {
        DALI_TEST_EQUALS(c.GetSelectedText(), Dali::String("one two"), TEST_LOCATION);
        DALI_TEST_CHECK(source.FindChildByName("HighlightActor").GetParent() == source);
      }
      if(style != 1u)
      {
        DALI_TEST_CHECK(source.FindChildByName("TextBackgroundColorActor").GetParent() == source);
        c.SetTextBackgroundColor(UiColor(Color::RED));
        Flush(app);
        DALI_TEST_CHECK((Group(c)->*Get(GroupTask{})) == task);
      }
      c.ClearSelection();
      c.SetTextBackgroundColor(UiColor(Color::TRANSPARENT));
      c.SetTextOutline(Text::Outline::None());
      c.SetTextShadow(Text::Shadow::None());
      Flush(app);
      DALI_TEST_CHECK(!Group(c));
      parent.Unparent();
      Flush(app);
    }
  }
  END_TEST;
}


template<class Control> int OverlapComposition()
{
  UiTestApplication app;
  auto c = Control::New();
  c.SetText("MMMM");
  c.SetRequestedWidth(300.0f);
  c.SetRequestedHeight(100.0f);
  app.GetScene().Add(c);
  Flush(app);
  DALI_TEST_CHECK(!Group(c));
  auto controller = Controller(c);
  const auto* positions = controller->GetRenderTextModel()->GetLayout();
  controller->SetCharacterSpacing(positions[0u].x - positions[1u].x);
  Flush(app);
  DALI_TEST_CHECK(Group(c));
  const RenderTask task = Group(c)->*Get(GroupTask{});
  for(float alpha : {1.0f, 0.75f, 0.5f, 0.25f, 0.0f, 1.0f})
  {
    c.SetProperty(Actor::Property::OPACITY, alpha);
    Flush(app);
    DALI_TEST_CHECK((Group(c)->*Get(GroupTask{})) == task);
  }
  controller->SetCharacterSpacing(0.0f);
  Flush(app);
  DALI_TEST_CHECK(!Group(c));
  END_TEST;
}

} // namespace

int UtcDaliEditableTextPaintFieldSelectionBackgroundP(void)
{
  return SelectionBackgroundMatrix<InputField>();
}
int UtcDaliEditableTextPaintEditorSelectionBackgroundP(void)
{
  return SelectionBackgroundMatrix<InputEditor>();
}
int UtcDaliEditableTextPaintPreeditMatrixP(void)
{
  UiTestApplication app;
  const std::string base("one two\nthree four\nfive six");
  for(unsigned style = 0u; style < 5u; ++style)
  {
    for(unsigned position : {2u, 12u, 23u})
    {
      for(bool commit : {false, true})
      {
        auto c = InputEditor::New();
        c.SetText(base.c_str());
        c.SetRequestedWidth(300.0f);
        c.SetRequestedHeight(110.0f);
        c.SetCursorBlinkEnabled(false);
        if(style == 1u) { Text::Outline outline; outline.SetWidth(8.0f); c.SetTextOutline(outline); }
        if(style == 2u) { c.SetTextBackgroundColor(UiColor(Color::BLUE)); }
        if(style == 4u)
        {
          Text::Shadow shadow;
          shadow.SetOffset(Vector2(-7.0f, 9.0f));
          shadow.SetBlurRadius(3.0f);
          c.SetTextShadow(shadow);
        }
        app.GetScene().Add(c);
        Flush(app);
        auto controller = Controller(c);
        controller->SetMaximumNumberOfCharacters(1000u);
        controller->KeyboardFocusGainEvent(false);
        controller->SetPrimaryCursorPosition(position, true);
        if(style == 3u) { c.SelectText(position, position + 2u); }
        Flush(app);
        auto context = c.GetInputMethodContext();
        auto send = [&](Dali::Integration::InputMethodContext::EventType type, const char* text)
        {
          Dali::Integration::InputMethodContext::EventData event;
          event.eventName = type;
          event.predictiveString = text;
          event.cursorOffset = static_cast<int32_t>(std::string(text).size());
          controller->OnInputMethodContextEvent(context, event);
          Flush(app);
        };
        std::string remainder = base;
        if(style == 3u) { remainder.erase(position, 2u); }
        send(Dali::Integration::InputMethodContext::PRE_EDIT, "ime");
        auto expected = remainder;
        expected.insert(position, "ime");
        DALI_TEST_EQUALS(c.GetText(), Dali::String(expected.c_str()), TEST_LOCATION);
        DALI_TEST_EQUALS(c.GetCursorPosition(), position + 3u, TEST_LOCATION);
        send(Dali::Integration::InputMethodContext::PRE_EDIT, "updated");
        expected = remainder;
        expected.insert(position, "updated");
        DALI_TEST_EQUALS(c.GetText(), Dali::String(expected.c_str()), TEST_LOCATION);
        DALI_TEST_EQUALS(c.GetCursorPosition(), position + 7u, TEST_LOCATION);
        send(commit ? Dali::Integration::InputMethodContext::COMMIT : Dali::Integration::InputMethodContext::PRE_EDIT,
             commit ? "done" : "");
        expected = remainder;
        if(commit) { expected.insert(position, "done"); }
        DALI_TEST_EQUALS(c.GetText(), Dali::String(expected.c_str()), TEST_LOCATION);
        DALI_TEST_EQUALS(c.GetCursorPosition(), position + (commit ? 4u : 0u), TEST_LOCATION);
        DALI_TEST_EQUALS(c.GetSelectedText(), Dali::String(), TEST_LOCATION);
        if(style == 1u || style == 2u || style == 4u)
        {
          DALI_TEST_CHECK(Group(c));
        }
        c.Unparent();
        Flush(app);
      }
    }
  }
  END_TEST;
}
int UtcDaliEditableTextPaintFieldOverlapP(void)
{
  return OverlapComposition<InputField>();
}
int UtcDaliEditableTextPaintEditorOverlapP(void)
{
  return OverlapComposition<InputEditor>();
}
int UtcDaliEditableTextPaintTargetFallbackP(void)
{
  UiTestApplication app;
  Actor viewport = Actor::New();
  viewport.SetProperty(Actor::Property::SIZE, Vector2(200.1f, 100.1f));
  viewport.SetProperty(Actor::Property::OPACITY, 0.5f);
  app.GetScene().Add(viewport);
  Actor text = Actor::New();
  viewport.Add(text);
  Text::EditableTextGroup group(viewport);
  group.Update(text, Actor(), Actor(), Actor());
  Flush(app);
  const auto task = group.*Get(GroupTask{});
  DALI_TEST_CHECK(task);
  viewport.SetProperty(Actor::Property::SIZE, Vector2(200.2f, 100.2f));
  group.Update(text, Actor(), Actor(), Actor());
  Flush(app);
  DALI_TEST_CHECK((group.*Get(GroupTask{})) == task);
  viewport.SetProperty(Actor::Property::SIZE, Vector2(100000.0f, 100.0f));
  group.Update(text, Actor(), Actor(), Actor());
  Flush(app);
  DALI_TEST_CHECK(!(group.*Get(GroupTask{})));
  DALI_TEST_EQUALS(text.GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, 0.5f, 0.001f, TEST_LOCATION);
  DALI_TEST_CHECK(!(group.*Get(GroupOutput{})).GetProperty<bool>(Actor::Property::VISIBLE));
  viewport.SetProperty(Actor::Property::SIZE, Vector2(200.0f, 100.0f));
  group.Update(text, Actor(), Actor(), Actor());
  Flush(app);
  DALI_TEST_CHECK(group.*Get(GroupTask{}));
  viewport.SetProperty(Actor::Property::SIZE, Vector2::ZERO);
  group.Update(text, Actor(), Actor(), Actor());
  Flush(app);
  DALI_TEST_CHECK(!(group.*Get(GroupTask{})));
  END_TEST;
}

namespace
{
uint32_t VertexCount(Actor actor)
{
  uint32_t count = 0u;
  for(uint32_t i = 0u; i < actor.GetRendererCount(); ++i)
  {
    auto geometry = actor.GetRendererAt(i).GetGeometry();
    for(const auto& buffer : Dali::GetImplementation(geometry).*Get(GeometryBuffers{}))
    {
      count += buffer->GetSize();
    }
  }
  for(uint32_t i = 0u; i < actor.GetChildCount(); ++i)
  {
    count += VertexCount(actor.GetChildAt(i));
  }
  return count;
}

template<class Control> int GlyphShadowPolicy()
{
  UiTestApplication app;
  auto control = Control::New();
  control.SetText("one two");
  control.SetRequestedWidth(300.0f);
  control.SetRequestedHeight(120.0f);
  control.SetCursorBlinkEnabled(false);
  Text::Shadow shadow;
  shadow.SetOffset(Vector2(-12.0f, 20.0f));
  shadow.SetBlurRadius(3.0f);
  control.SetTextShadow(shadow);
  app.GetScene().Add(control);
  Flush(app);
  DALI_TEST_CHECK(Group(control));
  auto text = TextActor(control);
  DALI_TEST_EQUALS(text.GetChildCount(), 2u, TEST_LOCATION);
  const auto shadowVertices = VertexCount(text.GetChildAt(0u));
  const auto fillVertices = VertexCount(text.GetChildAt(1u));
  DALI_TEST_CHECK(shadowVertices > 0u);
  const auto task = Group(control)->*Get(GroupTask{});
  const auto target = task.GetFrameBuffer();

  // Decorative lines belong to Fill; TextShadow covers glyph ink only.
  for(uint32_t style = 0u; style < 5u; ++style)
  {
    Text::Underline underline;
    underline.SetThickness(4.0f);
    if(style == 1u)
    {
      underline.SetType(Text::Underline::Type::DOUBLE);
    }
    else if(style == 2u)
    {
      underline.SetType(Text::Underline::Type::DASHED);
      underline.SetDashLength(8.0f);
      underline.SetDashGap(4.0f);
    }
    Text::LineThrough strike;
    strike.SetThickness(3.0f);
    control.SetTextUnderline(style == 3u ? Text::Underline::None() : underline);
    control.SetTextLineThrough(style >= 3u ? strike : Text::LineThrough::None());
    Flush(app);
    text = TextActor(control);
    DALI_TEST_EQUALS(text.GetChildCount(), 2u, TEST_LOCATION);
    DALI_TEST_EQUALS(VertexCount(text.GetChildAt(0u)), shadowVertices, TEST_LOCATION);
    DALI_TEST_CHECK(VertexCount(text.GetChildAt(1u)) > fillVertices);
    DALI_TEST_CHECK((Group(control)->*Get(GroupTask{})) == task);
    DALI_TEST_CHECK(task.GetFrameBuffer() == target);
  }
  END_TEST;
}

template<class Control> int AnimatedGroupOpacity()
{
  UiTestApplication app;
  for(uint32_t style = 0u; style < 6u; ++style)
  {
    for(bool animateParent : {false, true})
    {
      auto parent = View::New();
      parent.SetRequestedWidth(320.0f);
      parent.SetRequestedHeight(140.0f);
      auto control = Control::New();
      control.SetText("MMMM");
      control.SetRequestedWidth(300.0f);
      control.SetRequestedHeight(120.0f);
      control.SetCursorBlinkEnabled(false);
      parent.Add(control);
      app.GetScene().Add(parent);
      Flush(app);
      if(style == 0u || style == 5u)
      {
        Text::Outline outline;
        outline.SetWidth(3.0f);
        control.SetTextOutline(outline);
      }
      if(style == 1u || style == 5u)
      {
        Text::Shadow shadow;
        shadow.SetOffset(Vector2(4.0f, 6.0f));
        shadow.SetBlurRadius(3.0f);
        control.SetTextShadow(shadow);
      }
      if(style == 2u)
      {
        const auto* positions = Controller(control)->GetRenderTextModel()->GetLayout();
        Controller(control)->SetCharacterSpacing(positions[0u].x - positions[1u].x);
      }
      if(style == 3u || style == 4u)
      {
        control.SelectText(0u, 3u);
      }
      if(style == 4u)
      {
        control.SetTextBackgroundColor(UiColor(Color::BLUE));
      }
      Flush(app);
      DALI_TEST_CHECK(Group(control));
      const Text::EditableTextGroup* group = Group(control);
      const RenderTask task = group->*Get(GroupTask{});
      const auto target = task.GetFrameBuffer();
      const Actor output = group->*Get(GroupOutput{});
      auto frames = KeyFrames::New();
      frames.Add(0.0f, 1.0f);
      frames.Add(0.2f, 0.75f);
      frames.Add(0.4f, 0.5f);
      frames.Add(0.6f, 0.25f);
      frames.Add(0.8f, 0.0f);
      frames.Add(1.0f, 1.0f);
      auto animation = Animation::New(1.2f);
      Actor animated = animateParent ? Actor(parent) : Actor(control);
      animation.AnimateBetween(Property(animated, Actor::Property::OPACITY), frames);
      animation.Play();
      for(uint32_t frame = 0u; frame < 90u; ++frame)
      {
        app.SendNotification();
        app.Render(16u);
        DALI_TEST_CHECK(Group(control) == group);
        DALI_TEST_CHECK((group->*Get(GroupTask{})) == task);
        DALI_TEST_CHECK(task.GetFrameBuffer() == target);
        const float alpha = Actor(control).GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a;
        DALI_TEST_EQUALS(output.GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, alpha, 0.001f, TEST_LOCATION);
        DALI_TEST_EQUALS(task.GetSourceActor().GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, 1.0f, 0.001f, TEST_LOCATION);
      }
      parent.Unparent();
      Flush(app);
    }
  }
  END_TEST;
}
} // namespace

int UtcDaliEditableTextPaintFieldGlyphShadowPolicyP(void)
{
  return GlyphShadowPolicy<InputField>();
}

int UtcDaliEditableTextPaintEditorGlyphShadowPolicyP(void)
{
  return GlyphShadowPolicy<InputEditor>();
}

int UtcDaliEditableTextPaintFieldOpacityAnimationP(void)
{
  return AnimatedGroupOpacity<InputField>();
}

int UtcDaliEditableTextPaintEditorOpacityAnimationP(void)
{
  return AnimatedGroupOpacity<InputEditor>();
}

namespace
{
template<class Control> int GroupTransitionCycles()
{
  UiTestApplication app;
  auto control = Control::New();
  control.SetText("MMMM");
  control.SetRequestedWidth(300.0f);
  control.SetRequestedHeight(120.0f);
  control.SetCursorBlinkEnabled(false);
  app.GetScene().Add(control);
  Flush(app);
  auto controller = Controller(control);
  control.SelectText(0u, 2u);
  Flush(app);
  control.ClearSelection();
  Flush(app);
  const auto tasks = app.GetScene().GetRenderTaskList();
  const uint32_t baselineTasks = tasks.GetTaskCount();
  const auto* positions = controller->GetRenderTextModel()->GetLayout();
  const float overlapSpacing = positions[0u].x - positions[1u].x;
  const uint32_t baselineGlyphs = AtlasGlyphManager::Get().GetMetrics().mGlyphCount;
  for(uint32_t cycle = 0u; cycle < 50u; ++cycle)
  {
    for(uint32_t reason = 0u; reason < 3u; ++reason)
    {
      if(reason == 0u)
      {
        controller->SetCharacterSpacing(overlapSpacing);
      }
      else if(reason == 1u)
      {
        control.SelectText(0u, 2u);
      }
      else
      {
        Text::Shadow shadow;
        shadow.SetOffset(Vector2(4.0f, -6.0f));
        shadow.SetBlurRadius(3.0f);
        control.SetTextShadow(shadow);
        control.SetTextBackgroundColor(UiColor(Color::BLUE));
        Text::Outline outline;
        outline.SetWidth(3.0f);
        control.SetTextOutline(outline);
      }
      Flush(app);
      DALI_TEST_CHECK(Group(control));
      DALI_TEST_EQUALS(tasks.GetTaskCount(), baselineTasks + 1u, TEST_LOCATION);
      WeakHandleBase task;
      WeakHandleBase camera;
      WeakHandleBase target;
      WeakHandleBase texture;
      WeakHandleBase output;
      {
        auto handle = Group(control)->*Get(GroupTask{});
        task = WeakHandleBase(handle);
        auto cameraHandle = handle.GetCameraActor();
        auto targetHandle = handle.GetFrameBuffer();
        auto textureHandle = targetHandle.GetColorTexture();
        camera = WeakHandleBase(cameraHandle);
        target = WeakHandleBase(targetHandle);
        texture = WeakHandleBase(textureHandle);
        output = WeakHandleBase(Group(control)->*Get(GroupOutput{}));
      }
      controller->SetCharacterSpacing(0.0f);
      control.ClearSelection();
      control.SetTextShadow(Text::Shadow::None());
      control.SetTextOutline(Text::Outline::None());
      control.SetTextBackgroundColor(UiColor(Color::TRANSPARENT));
      Flush(app);
      DALI_TEST_CHECK(!Group(control));
      DALI_TEST_EQUALS(tasks.GetTaskCount(), baselineTasks, TEST_LOCATION);
      DALI_TEST_CHECK(!task.GetBaseHandle());
      DALI_TEST_CHECK(!camera.GetBaseHandle());
      DALI_TEST_CHECK(!target.GetBaseHandle());
      DALI_TEST_CHECK(!texture.GetBaseHandle());
      DALI_TEST_CHECK(!output.GetBaseHandle());
      DALI_TEST_EQUALS(AtlasGlyphManager::Get().GetMetrics().mGlyphCount, baselineGlyphs, TEST_LOCATION);
      const Actor textActor = TextActor(control);
      DALI_TEST_CHECK(textActor.GetProperty<bool>(Actor::Property::VISIBLE));
      DALI_TEST_EQUALS(textActor.GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, 1.0f, 0.001f, TEST_LOCATION);
    }
    control.Unparent();
    Flush(app);
    app.GetScene().Add(control);
    Flush(app);
    DALI_TEST_EQUALS(tasks.GetTaskCount(), baselineTasks, TEST_LOCATION);
  }
  END_TEST;
}
} // namespace

int UtcDaliEditableTextPaintFieldTransitionCyclesP(void)
{
  return GroupTransitionCycles<InputField>();
}

int UtcDaliEditableTextPaintEditorTransitionCyclesP(void)
{
  return GroupTransitionCycles<InputEditor>();
}

namespace
{
template<class Control> int GroupGradientAnimation()
{
  UiTestApplication app;
  auto control = Control::New();
  control.SetText("Grouped gradient");
  control.SetRequestedWidth(300.0f);
  control.SetRequestedHeight(100.0f);
  Text::Outline outline;
  outline.SetWidth(3.0f);
  control.SetTextOutline(outline);
  Gradient::Linear gradient(Vector2::ZERO, Vector2(1.0f, 0.0f));
  gradient.SetStopNodes({{0.0f, UiColor(Color::RED)}, {1.0f, UiColor(Color::BLUE)}});
  control.SetTextGradient(gradient);
  control.SetProperty(Actor::Property::OPACITY, 0.5f);
  app.GetScene().Add(control);
  Flush(app);
  DALI_TEST_CHECK(Group(control));
  auto* group = Group(control);
  const RenderTask task = group->*Get(GroupTask{});
  const FrameBuffer target = task.GetFrameBuffer();
  Animation animation = Animation::New(1.0f);
  control.Animate(animation).TextGradientStartOffset(0.75f, Duration(1.0f));
  animation.Play();
  for(unsigned frame = 0u; frame < 70u; ++frame)
  {
    app.SendNotification();
    app.Render(16u);
    DALI_TEST_CHECK(Group(control) == group);
    DALI_TEST_CHECK((group->*Get(GroupTask{})) == task);
    DALI_TEST_CHECK(task.GetFrameBuffer() == target);
  }
  const Property::Index offset = control.GetPropertyIndex("uTextGradientStartOffset");
  DALI_TEST_CHECK(offset != Property::INVALID_INDEX);
  DALI_TEST_EQUALS(Actor(control).GetCurrentProperty<float>(offset), 0.75f, 0.001f, TEST_LOCATION);
  const Actor source = task.GetSourceActor();
  const Actor output = group->*Get(GroupOutput{});
  DALI_TEST_EQUALS(source.GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, 1.0f, 0.001f, TEST_LOCATION);
  DALI_TEST_EQUALS(output.GetCurrentProperty<Vector4>(Actor::Property::WORLD_COLOR).a, 0.5f, 0.001f, TEST_LOCATION);
  control.SetTextGradient(Gradient::Base::None());
  control.SetTextColor(UiColor(Color::GREEN));
  Flush(app);
  DALI_TEST_CHECK(Group(control) == group);
  DALI_TEST_CHECK(task.GetFrameBuffer() == target);
  END_TEST;
}
} // namespace

int UtcDaliEditableTextPaintFieldGradientAnimationP(void)
{
  return GroupGradientAnimation<InputField>();
}

int UtcDaliEditableTextPaintEditorGradientAnimationP(void)
{
  return GroupGradientAnimation<InputEditor>();
}
