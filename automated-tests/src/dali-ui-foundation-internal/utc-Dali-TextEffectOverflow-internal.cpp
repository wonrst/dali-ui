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

#include <dali-ui-foundation/dali-ui-foundation.h>
#include <dali-ui-foundation/integration-api/label-impl.h>
#include <dali-ui-foundation/integration-api/text/reveal-integ.h>
#include <dali-ui-foundation/integration-api/visual-factory/visual-factory.h>
#include <dali-ui-foundation/integration-api/visuals/text-visual-properties-integ.h>
#include <dali-ui-foundation/internal/views/view/view-data-impl.h>
#include <dali-ui-foundation/integration-api/visuals/visual-properties-integ.h>
#include <dali-ui-foundation/internal/text/rendering/text-raster-geometry.h>
#include <dali-ui-foundation/internal/text/multi-language-support.h>
#include <dali-ui-foundation/internal/text/rendering/text-typesetter.h>
#include <dali-ui-foundation/internal/text/text-model.h>
#include <dali-ui-foundation/internal/visuals/text/text-visual.h>
#include <dali-ui-test-suite-utils.h>
#include <test-font-glyph-constants.h>
#include <dali-ui/ui-event-thread-callback.h>
#include <dali/integration-api/pixel-data-integ.h>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace Dali;
using namespace Dali::Ui;

void utc_dali_text_effect_overflow_internal_startup(void)
{
  test_return_value = TET_UNDEF;
}
void utc_dali_text_effect_overflow_internal_cleanup(void)
{
  test_return_value = TET_PASS;
}

namespace
{
template<class Tag, auto member>
struct MemberAccess
{
  friend auto GetMember(Tag)
  {
    return member;
  }
};
struct LabelVisual
{
  friend auto GetMember(LabelVisual);
};
template struct MemberAccess<LabelVisual, &Ui::Integration::LabelImpl::mVisual>;
Ui::Integration::Visual::Base VisualFor(Label label)
{
  return static_cast<Ui::Integration::LabelImpl&>(GetImpl(label)).*GetMember(LabelVisual{});
}
auto ControllerFor(Label label)
{
  return Ui::Internal::TextVisual::GetController(VisualFor(label));
}
void Flush(UiTestApplication& app)
{
  for(unsigned i = 0; i < 3; ++i)
  {
    app.SendNotification();
    app.Render();
  }
}
// Probe the generic culling behavior through rendering, without Core-private
// headers or a production getter. Preserve the publication under test.
bool UsesVisualGeometryCulling(UiTestApplication& app, Label label, Renderer renderer)
{
  const Property::Index indices[] = {VisualRenderer::Property::TRANSFORM_SIZE,
    VisualRenderer::Property::TRANSFORM_OFFSET, VisualRenderer::Property::TRANSFORM_OFFSET_SIZE_MODE,
    VisualRenderer::Property::TRANSFORM_ORIGIN, VisualRenderer::Property::TRANSFORM_PIVOT,
    VisualRenderer::Property::EXTRA_SIZE};
  Property::Map saved;
  for(auto index : indices) saved.Insert(index, renderer.GetProperty(index));
  auto shader = renderer.GetShader();
  bool attached = false;
  for(unsigned i = 0; i < label.GetRendererCount(); ++i) attached |= label.GetRendererAt(i) == renderer;
  if(attached) label.RemoveRenderer(renderer);
  Actor probe = Actor::New();
  probe.SetProperty(Actor::Property::PARENT_ORIGIN, ParentOrigin::TOP_LEFT);
  probe.SetProperty(Actor::Property::PIVOT, Pivot::CENTER);
  probe.SetProperty(Actor::Property::SIZE, Vector2(100, 100));
  probe.SetProperty(Actor::Property::POSITION, Vector3(-100, 200, 0));
  auto probeShader = Shader::New("vertexSrc", "fragmentSrc");
  renderer.SetShader(probeShader);
  renderer.SetProperty(VisualRenderer::Property::TRANSFORM_SIZE, Vector2(100, 100));
  renderer.SetProperty(VisualRenderer::Property::TRANSFORM_OFFSET, Vector2(100, 0));
  renderer.SetProperty(VisualRenderer::Property::TRANSFORM_OFFSET_SIZE_MODE, Vector4::ONE);
  renderer.SetProperty(VisualRenderer::Property::TRANSFORM_ORIGIN, Vector2::ZERO);
  renderer.SetProperty(VisualRenderer::Property::TRANSFORM_PIVOT, Vector2::ZERO);
  renderer.SetProperty(VisualRenderer::Property::EXTRA_SIZE, Vector2::ZERO);
  probe.AddRenderer(renderer);
  app.GetScene().Add(probe);
  Flush(app);
  auto& trace = app.GetGlAbstraction().GetDrawTrace();
  trace.Enable(true);
  trace.Reset();
  app.Render();
  const bool drawn = trace.CountMethod("DrawElements") + trace.CountMethod("DrawArrays") > 0;
  trace.Enable(false);
  probe.RemoveRenderer(renderer);
  probe.Unparent();
  renderer.SetShader(shader);
  for(auto index : indices) renderer.SetProperty(index, *saved.Find(index));
  if(attached) label.AddRenderer(renderer);
  Flush(app);
  return drawn;
}
void Enable(Label label, bool enabled)
{
  static_cast<Ui::Integration::LabelImpl&>(GetImpl(label)).SetEffectOverflowEnabled(enabled);
}
Label MakeLabel(UiTestApplication& app)
{
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  Label label = Label::New("Hello");
  label.SetFontFamily("DejaVu Sans");
  label.SetFontSize(24.f);
  label.SetSystemFontSizeScaleEnabled(false);
  label.SetAsyncRendering(false);
  label.SetRequestedWidth(240.75f);
  label.SetRequestedHeight(100.5f);
  app.GetScene().Add(label);
  return label;
}
bool Advanced(Label label)
{
  if(!label.GetRendererCount()) return false;
  auto         program = label.GetRendererAt(0).GetShader().GetProperty(Shader::Property::PROGRAM);
  Dali::String vertex;
  program.GetMap()->Find("vertex")->Get(vertex);
  return std::string(vertex.CStr()).find("vFillUv") != std::string::npos;
}
void SetShadowStyle(Label label, float radius = 30.f)
{
  Text::Shadow shadow;
  shadow.SetOffset(Vector2(-30, 25));
  shadow.SetBlurRadius(radius);
  label.SetTextShadow(shadow);
}
void Underline(Label label, float thickness = 25.f)
{
  Text::Underline underline;
  underline.SetThickness(thickness);
  underline.SetType(Text::Underline::Type::DOUBLE);
  label.SetTextUnderline(underline);
}
Vector2 VisualSize(Renderer renderer)
{
  return renderer.GetProperty<Vector2>(VisualRenderer::Property::TRANSFORM_SIZE);
}
Vector2 Offset(Renderer renderer)
{
  return renderer.GetProperty<Vector2>(VisualRenderer::Property::TRANSFORM_OFFSET);
}

void CheckPlanes(Label label, const Vector2& rasterSize, Pixel::Format format = Pixel::L8)
{
  auto controller = ControllerFor(label);
  auto typesetter = Text::Typesetter::New(controller->GetRenderTextModel());
  typesetter->SetFinalElisionResult(controller->GetFinalElisionResult());
  Text::TextRasterResult result;
  DALI_TEST_CHECK(typesetter->RenderRasterPlanes(rasterSize, controller->GetTextDirection(), format, 4094, result));
  auto                                      renderer    = label.GetRendererAt(0);
  auto                                      textures    = renderer.GetTextures();
  const auto&                               composition = result.geometry.composition;
  const Vector2                             extent(composition.right - composition.left + 2, composition.bottom - composition.top + 2);
  const std::array<PixelData, 3>            planes{result.fill, result.underlay, result.overlay};
  const std::array<Text::Raster::Bounds, 3> bounds{result.geometry.fill, result.geometry.underlay, result.geometry.overlay};
  const char*                               uniforms[] = {"uFillUvTransform", "uUnderlayUvTransform", "uOverlayUvTransform"};
  unsigned                                  index      = 0;
  for(unsigned i = 0; i < planes.size(); ++i)
  {
    if(!planes[i]) continue;
    auto texture = textures.GetTexture(index++);
    DALI_TEST_EQUALS(texture.GetWidth(), planes[i].GetWidth() + 2, TEST_LOCATION);
    DALI_TEST_EQUALS(texture.GetHeight(), planes[i].GetHeight() + 2, TEST_LOCATION);
    const Vector4 expected(extent.x / texture.GetWidth(), extent.y / texture.GetHeight(),
                           float(composition.left - bounds[i].left) / texture.GetWidth(),
                           float(composition.top - bounds[i].top) / texture.GetHeight());
    DALI_TEST_EQUALS(renderer.GetProperty<Vector4>(renderer.GetPropertyIndex(uniforms[i])), expected, .0001f, TEST_LOCATION);
  }
  DALI_TEST_EQUALS(textures.GetTextureCount(), index, TEST_LOCATION);
  DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
}
} // namespace

int UtcDaliTextEffectOverflowDefaultAndToggleP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  SetShadowStyle(label);
  Flush(app);
  auto       renderer = label.GetRendererAt(0);
  const auto shader   = renderer.GetShader();
  const auto size     = VisualSize(renderer);
  const auto offset   = Offset(renderer);
  const auto natural  = label.GetNaturalSize();
  const auto layout   = ControllerFor(label)->GetRenderTextModel()->GetLayoutSize();
  const auto width    = renderer.GetTextures().GetTexture(0).GetWidth();
  DALI_TEST_CHECK(!Advanced(label));
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
  DALI_TEST_EQUALS(renderer.GetPropertyIndex("uFillUvTransform"), Property::INVALID_INDEX, TEST_LOCATION);
  Enable(label, true);
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, renderer));
  CheckPlanes(label, size);
  Property::Map properties;
  VisualFor(label).CreatePropertyMap(properties);
  auto* transform = properties.Find(Ui::Integration::Visual::Property::TRANSFORM)->GetMap();
  DALI_TEST_EQUALS(transform->Find(Ui::Integration::Visual::Transform::Property::SIZE)->Get<Vector2>(), size, TEST_LOCATION);
  DALI_TEST_EQUALS(transform->Find(Ui::Integration::Visual::Transform::Property::OFFSET)->Get<Vector2>(), offset, TEST_LOCATION);
  app.GetScene().Remove(label);
  Flush(app);
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
  VisualFor(label).CreatePropertyMap(properties);
  transform = properties.Find(Ui::Integration::Visual::Property::TRANSFORM)->GetMap();
  DALI_TEST_EQUALS(transform->Find(Ui::Integration::Visual::Transform::Property::SIZE)->Get<Vector2>(), size, TEST_LOCATION);
  app.GetScene().Add(label);
  Enable(label, false);
  Flush(app);
  DALI_TEST_CHECK(!Advanced(label));
  DALI_TEST_EQUALS(renderer.GetShader(), shader, TEST_LOCATION);
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
  DALI_TEST_EQUALS(VisualSize(renderer), size, TEST_LOCATION);
  DALI_TEST_EQUALS(Offset(renderer), offset, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetTextures().GetTexture(0).GetWidth(), width, TEST_LOCATION);
  DALI_TEST_EQUALS(label.GetNaturalSize(), natural, TEST_LOCATION);
  DALI_TEST_EQUALS(ControllerFor(label)->GetRenderTextModel()->GetLayoutSize(), layout, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextEffectOverflowCullingLifecycleP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  label.SetPadding(12.f);
  Enable(label, true);
  Flush(app);
  auto renderer = label.GetRendererAt(0);
  DALI_TEST_CHECK(Advanced(label));
  // The padded fill quad is inside the node, even with advanced publication.
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
  SetShadowStyle(label);
  Flush(app);
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, renderer));
  app.GetScene().Remove(label);
  Flush(app);
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
  app.GetScene().Add(label);
  Flush(app);
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
  label.SetTextShadow(Text::Shadow());
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
  // Resource removal and later publication must not retain an old opt-in.
  label.SetText("");
  Flush(app);
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
  label.SetText("Hello");
  SetShadowStyle(label);
  Flush(app);
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
  Enable(label, false);
  Flush(app);
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
  END_TEST;
}

int UtcDaliTextEffectOverflowAlignmentP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  SetShadowStyle(label);
  for(float scale : {1.f, 1.5f, 2.f})
  {
    UiScaleManager::Get().SetScale(scale);
    for(unsigned alignment = 0; alignment < 9; ++alignment)
    {
      Enable(label, false);
      label.SetHorizontalTextAlignment(static_cast<Text::Alignment>(alignment % 3));
      label.SetVerticalTextAlignment(static_cast<Text::Alignment>(alignment / 3));
      label.SetRenderScale(alignment % 2 ? 2.f : 1.f);
      Flush(app);
      const auto    size    = VisualSize(label.GetRendererAt(0));
      const auto    offset  = Offset(label.GetRendererAt(0));
      const auto    natural = label.GetNaturalSize();
      const auto    layout  = ControllerFor(label)->GetRenderTextModel()->GetLayoutSize();
      const Vector2 pixelScale(size.x / std::trunc(size.x), size.y / std::trunc(size.y));
      auto          controller = ControllerFor(label);
      auto          typesetter = Text::Typesetter::New(controller->GetRenderTextModel());
      typesetter->SetFinalElisionResult(controller->GetFinalElisionResult());
      Text::TextRasterGeometry geometry;
      DALI_TEST_CHECK(typesetter->ResolveRasterGeometry(size, controller->GetTextDirection(), geometry));
      Enable(label, true);
      Flush(app);
      DALI_TEST_CHECK(Advanced(label));
      CheckPlanes(label, size);
      const auto&   c = geometry.composition;
      const Vector2 extent(c.right - c.left + 2, c.bottom - c.top + 2);
      DALI_TEST_EQUALS(VisualSize(label.GetRendererAt(0)), extent * pixelScale, .0001f, TEST_LOCATION);
      // Any raster pixel has the same local position after expanding the quad.
      const Vector2 probe(17.5f, 11.5f);
      const Vector2 before = offset + probe * pixelScale;
      const Vector2 after  = Offset(label.GetRendererAt(0)) + (probe - Vector2(c.left - 1, c.top - 1)) * pixelScale;
      DALI_TEST_EQUALS(before, after, .0001f, TEST_LOCATION);
      DALI_TEST_EQUALS(label.GetNaturalSize(), natural, TEST_LOCATION);
      DALI_TEST_EQUALS(controller->GetRenderTextModel()->GetLayoutSize(), layout, TEST_LOCATION);
    }
  }
  UiScaleManager::Get().SetScale(1.f);
  END_TEST;
}

int UtcDaliTextEffectOverflowPlaneIsolationP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  Enable(label, true);
  Flush(app);
  const auto fill = label.GetRendererAt(0).GetTextures().GetTexture(0);
  SetShadowStyle(label);
  Flush(app);
  auto textures = label.GetRendererAt(0).GetTextures();
  DALI_TEST_EQUALS(textures.GetTextureCount(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(0).GetWidth(), fill.GetWidth(), TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(0).GetHeight(), fill.GetHeight(), TEST_LOCATION);
  auto underlay = textures.GetTexture(1);
  Underline(label, 100);
  Flush(app);
  textures = label.GetRendererAt(0).GetTextures();
  DALI_TEST_EQUALS(textures.GetTextureCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(1).GetWidth(), underlay.GetWidth(), TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(1).GetHeight(), underlay.GetHeight(), TEST_LOCATION);
  label.SetTextShadow(Text::Shadow::None());
  Flush(app);
  DALI_TEST_EQUALS(label.GetRendererAt(0).GetTextures().GetTextureCount(), 2u, TEST_LOCATION);
  label.SetTextUnderline(Text::Underline::None());
  Flush(app);
  DALI_TEST_EQUALS(label.GetRendererAt(0).GetTextures().GetTextureCount(), 1u, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextEffectOverflowReuseP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  Enable(label, true);
  SetShadowStyle(label);
  Underline(label);
  Flush(app);
  auto       renderer = label.GetRendererAt(0);
  const auto count    = renderer.GetPropertyCount();
  for(unsigned i = 0; i < 8; ++i)
  {
    SetShadowStyle(label, i % 2 ? 1 : 30);
    Underline(label, i % 2 ? 2 : 25);
    Flush(app);
    DALI_TEST_CHECK(Advanced(label));
    DALI_TEST_EQUALS(renderer.GetPropertyCount(), count, TEST_LOCATION);
    Enable(label, false);
    Flush(app);
    DALI_TEST_CHECK(!Advanced(label));
    Enable(label, true);
    Flush(app);
    DALI_TEST_EQUALS(renderer.GetPropertyCount(), count, TEST_LOCATION);
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowStyledTextP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  label.SetStyledText(Text::StyledText::FromMarkup("<u height='25'>He<color value='red'><s height='8'>llo</s></color></u>"));
  SetShadowStyle(label);
  Flush(app);
  const auto size = VisualSize(label.GetRendererAt(0));
  Enable(label, true);
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  CheckPlanes(label, size, Pixel::RGBA8888);
  END_TEST;
}

int UtcDaliTextEffectOverflowFallbackP(void)
{
  UiTestApplication app;
  for(unsigned mode = 0; mode < 4; ++mode)
  {
    tet_printf("Effect overflow fallback mode %u\n", mode);
    auto label = MakeLabel(app);
    Enable(label, true);
    SetShadowStyle(label);
    Flush(app);
    DALI_TEST_CHECK(Advanced(label));
    DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
    if(mode == 0) label.SetTextCutoutEnabled(true);
    if(mode == 1)
    {
      label.SetText("Hello marquee text extending beyond the control width");
      label.StartMarquee();
    }
    if(mode == 2)
    {
      Text::Reveal reveal;
      reveal.SetUnit(Text::Reveal::Unit::WORD);
      label.SetTextReveal(reveal);
    }
    if(mode == 3)
    {
      Text::Shadow shadow;
      shadow.SetOffset(Vector2(-100000, 0));
      label.SetTextShadow(shadow); // Checked composition limit, before allocation.
    }
    Flush(app);
    DALI_TEST_CHECK(!Advanced(label));
    if(mode != 1)
    {
      DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
    }
    label.SetAsyncRendering(true);
    Flush(app);
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
    DALI_TEST_CHECK(!Advanced(label));
    if(mode != 1)
    {
      DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
    }
    app.GetScene().Remove(label);
    Flush(app);
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowAsyncPublicationP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  SetShadowStyle(label);
  Enable(label, true);
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
  auto       renderer = label.GetRendererAt(0);
  const auto size     = VisualSize(renderer);
  const auto offset   = Offset(renderer);
  const auto textures = renderer.GetTextures();
  const auto shader   = renderer.GetShader();
  label.SetAsyncRendering(true);
  Flush(app);
  DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_EQUALS(VisualSize(renderer), size, .001f, TEST_LOCATION);
  DALI_TEST_EQUALS(Offset(renderer), offset, .001f, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetShader(), shader, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetTextures().GetTextureCount(), textures.GetTextureCount(), TEST_LOCATION);
  DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
  label.SetAsyncRendering(false);
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
  END_TEST;
}

int UtcDaliTextEffectOverflowShaderReuseP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  Enable(label, true);
  std::array<Shader, 16> shaders;
  for(unsigned pass = 0; pass < 2; ++pass)
  {
    for(unsigned variant = 0; variant < shaders.size(); ++variant)
    {
      if(variant & 4u)
      {
        label.SetStyledText(Text::StyledText::FromMarkup("He<color value='red'>llo</color>"));
      }
      else
      {
        label.SetText("Hello");
      }
      label.SetTextShadow(Text::Shadow::None());
      label.SetTextUnderline(Text::Underline::None());
      label.SetTextBevel(variant & 8u ? Text::Bevel() : Text::Bevel::None());
      if(variant & 2u) SetShadowStyle(label);
      if(variant & 1u) Underline(label);
      Flush(app);
      DALI_TEST_CHECK(Advanced(label));
      auto renderer = label.GetRendererAt(0);
      DALI_TEST_EQUALS(renderer.GetTextures().GetTextureCount(), 1u + !!(variant & 2u) + !!(variant & 1u), TEST_LOCATION);
      if(pass)
        DALI_TEST_EQUALS(renderer.GetShader(), shaders[variant], TEST_LOCATION);
      else
        shaders[variant] = renderer.GetShader();
    }
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowLayoutP(void)
{
  UiTestApplication app;
  for(unsigned mode = 0; mode < 3; ++mode)
  {
    auto label = MakeLabel(app);
    label.SetText("Hello world with several long lines of text for wrapping and ellipsis");
    label.SetMultiLine(mode != 0);
    if(mode == 2) label.SetTextFit(Text::Fit::Range(10, 36, 1));
    SetShadowStyle(label);
    Flush(app);
    const auto natural = label.GetNaturalSize();
    // Natural-size measurement can temporarily lay out unconstrained text.
    // Republish the ordinary control layout before comparing the final models.
    Enable(label, false);
    Flush(app);
    auto                 controller = ControllerFor(label);
    const auto*          model      = controller->GetRenderTextModel();
    const auto           layout     = model->GetLayoutSize();
    const auto           lines      = model->GetNumberOfLines();
    std::vector<Vector2> positions(model->GetLayout(), model->GetLayout() + model->GetNumberOfGlyphs());
    const auto           size = VisualSize(label.GetRendererAt(0));
    Enable(label, true);
    Flush(app);
    DALI_TEST_CHECK(Advanced(label));
    CheckPlanes(label, size);
    DALI_TEST_EQUALS(model->GetLayoutSize(), layout, TEST_LOCATION);
    DALI_TEST_EQUALS(model->GetNumberOfLines(), lines, TEST_LOCATION);
    DALI_TEST_EQUALS(model->GetNumberOfGlyphs(), static_cast<unsigned>(positions.size()), TEST_LOCATION);
    for(unsigned i = 0; i < positions.size(); ++i) DALI_TEST_EQUALS(model->GetLayout()[i], positions[i], TEST_LOCATION);
    DALI_TEST_EQUALS(label.GetNaturalSize(), natural, TEST_LOCATION);
    app.GetScene().Remove(label);
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowAsyncGpuParityP(void)
{
  UiTestApplication app;
  for(unsigned variant = 0; variant < 9; ++variant)
  {
    auto label = MakeLabel(app);
    if(variant == 1 || variant == 6) SetShadowStyle(label, 3);
    if(variant == 2) SetShadowStyle(label);
    if(variant == 3)
    {
      Text::Outline outline;
      outline.SetWidth(6);
      outline.SetOffset(Vector2(10, -10));
      label.SetTextOutline(outline);
    }
    if(variant == 4 || variant == 6) Underline(label);
    if(variant == 5) label.SetStyledText(Text::StyledText::FromMarkup("<color value='red'><u height='12'>Hello</u></color> <s height='8'>world</s>"));
    if(variant == 7) label.SetText("مرحبا Hello ffi Á");
    if(variant == 8) label.SetTextFit(Text::Fit::Range(8, 24, 1));
    Enable(label, true);
    Flush(app);
    auto          renderer = label.GetRendererAt(0);
    const auto    textures = renderer.GetTextures();
    const auto    shader   = renderer.GetShader();
    const auto    size     = VisualSize(renderer);
    const auto    offset   = Offset(renderer);
    const bool    culling  = UsesVisualGeometryCulling(app, label, renderer);
    const char*   names[]  = {"uFillUvTransform", "uUnderlayUvTransform", "uOverlayUvTransform"};
    Property::Map uvs;
    for(auto name : names)
    {
      auto index = renderer.GetPropertyIndex(name);
      if(index != Property::INVALID_INDEX) uvs.Insert(name, renderer.GetProperty(index));
    }
    label.SetAsyncRendering(true);
    Flush(app);
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
    DALI_TEST_CHECK(Advanced(label));
    DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
    DALI_TEST_EQUALS(label.GetRendererAt(0), renderer, TEST_LOCATION);
    DALI_TEST_EQUALS(renderer.GetShader(), shader, TEST_LOCATION);
    DALI_TEST_EQUALS(VisualSize(renderer), size, .001f, TEST_LOCATION);
    DALI_TEST_EQUALS(Offset(renderer), offset, .001f, TEST_LOCATION);
    DALI_TEST_EQUALS(UsesVisualGeometryCulling(app, label, renderer), culling, TEST_LOCATION);
    const auto asyncTextures = renderer.GetTextures();
    DALI_TEST_EQUALS(asyncTextures.GetTextureCount(), textures.GetTextureCount(), TEST_LOCATION);
    for(unsigned i = 0; i < textures.GetTextureCount(); ++i)
    {
      DALI_TEST_EQUALS(asyncTextures.GetTexture(i).GetWidth(), textures.GetTexture(i).GetWidth(), TEST_LOCATION);
      DALI_TEST_EQUALS(asyncTextures.GetTexture(i).GetHeight(), textures.GetTexture(i).GetHeight(), TEST_LOCATION);
      DALI_TEST_EQUALS(asyncTextures.GetTexture(i).GetPixelFormat(), textures.GetTexture(i).GetPixelFormat(), TEST_LOCATION);
    }
    for(auto name : names)
    {
      if(auto value = uvs.Find(name)) DALI_TEST_EQUALS(renderer.GetProperty<Vector4>(renderer.GetPropertyIndex(name)), value->Get<Vector4>(), .0001f, TEST_LOCATION);
    }
    label.Unparent();
    Flush(app);
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowAsyncLifecycleP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  SetShadowStyle(label);
  Underline(label);
  Enable(label, true);
  Flush(app);
  auto       renderer       = label.GetRendererAt(0);
  const auto advancedShader = renderer.GetShader();
  const auto advancedSize   = VisualSize(renderer);
  label.SetAsyncRendering(true);
  Flush(app);
  DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
  Flush(app);
  const auto propertyCount = renderer.GetPropertyCount();
  for(unsigned i = 0; i < 32; ++i)
  {
    auto           oldTextures = renderer.GetTextures();
    WeakHandleBase previous(oldTextures);
    oldTextures.Reset();
    Enable(label, false);
    Flush(app);
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
    DALI_TEST_CHECK(!Advanced(label));
    DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
    DALI_TEST_CHECK(!previous.GetBaseHandle());
    Enable(label, true);
    Flush(app);
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
    DALI_TEST_CHECK(Advanced(label));
    DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
    DALI_TEST_EQUALS(label.GetRendererAt(0), renderer, TEST_LOCATION);
    DALI_TEST_EQUALS(renderer.GetPropertyCount(), propertyCount, TEST_LOCATION);
    DALI_TEST_EQUALS(renderer.GetShader(), advancedShader, TEST_LOCATION);
    DALI_TEST_EQUALS(VisualSize(renderer), advancedSize, TEST_LOCATION);
    DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, renderer));
  }
  label.SetText("");
  Flush(app);
  DALI_TEST_EQUALS(label.GetRendererCount(), 0u, TEST_LOCATION);
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
  END_TEST;
}

int UtcDaliTextEffectOverflowAsyncCancellationP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  SetShadowStyle(label, 3);
  Enable(label, true);
  Flush(app);
  auto              renderer = label.GetRendererAt(0);
  ConnectionTracker tracker;
  unsigned          completed = 0;
  label.AsyncRenderFinishedSignal().Connect(&tracker, [&](View, float, float)
  { ++completed; });
  Gradient::Linear gradient(Vector2(-.5f, 0), Vector2(.5f, 0));
  gradient.SetStopNodes({Gradient::StopNode(0, UiColor(Color::RED)), Gradient::StopNode(1, UiColor(Color::BLUE))});
  for(unsigned mutation = 0; mutation < 13; ++mutation)
  {
    label.SetAsyncRendering(true);
    Enable(label, true);
    Flush(app); // Submit A, but do not dispatch its completion yet.
    const auto oldTextures = renderer.GetTextures();
    const auto before      = completed;
    if(mutation == 0) Enable(label, false);
    if(mutation == 1) SetShadowStyle(label, 30);
    if(mutation == 2) label.SetProperty(Actor::Property::SIZE_WIDTH, 300.f);
    if(mutation == 3) label.SetRenderScale(1.5f);
    if(mutation == 4) label.SetRequestedWidth(320.f);
    if(mutation == 5) UiScaleManager::Get().SetScale(1.5f);
    if(mutation == 6) label.SetTextGradient(gradient);
    if(mutation == 7) label.SetTextGradient(Gradient::Base::None());
    if(mutation == 8) label.SetTextGradientOverlay(gradient);
    if(mutation == 9) label.SetTextGradientOverlay(Gradient::Base::None());
    if(mutation == 10) label.SetTextBevel(Text::Bevel());
    if(mutation == 11)
    {
      Text::Bevel bevel;
      bevel.SetDirection(Vector2(1, 0));
      bevel.SetIntensity(5);
      label.SetTextBevel(bevel);
    }
    if(mutation == 12) label.SetTextBevel(Text::Bevel::None());
    Test::WaitForEventThreadTrigger(1, 5);
    DALI_TEST_EQUALS(completed, before, TEST_LOCATION);
    DALI_TEST_EQUALS(renderer.GetTextures(), oldTextures, TEST_LOCATION);
    Flush(app); // Submit the replacement request B.
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
    DALI_TEST_EQUALS(completed, before + 1, TEST_LOCATION);
    DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
    DALI_TEST_EQUALS(Advanced(label), mutation != 0, TEST_LOCATION);
    label.SetAsyncRendering(false);
    Flush(app);
  }
  UiScaleManager::Get().SetScale(1.f);
  label.SetAsyncRendering(true);
  Enable(label, true);
  SetShadowStyle(label, 3);
  Flush(app); // A is pending.
  const auto before = completed;
  Enable(label, false);
  SetShadowStyle(label, 30);
  Flush(app); // B replaces A before either completion is dispatched.
  for(unsigned i = 0; i < 3 && completed == before; ++i)
  {
    Test::WaitForEventThreadTrigger(1, 5);
  }
  while(Test::WaitForEventThreadTrigger(1, 0))
  {
  }
  Flush(app);
  DALI_TEST_EQUALS(completed, before + 1, TEST_LOCATION);
  DALI_TEST_CHECK(!Advanced(label));
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
  END_TEST;
}

int UtcDaliTextEffectOverflowAsyncCompletionReentryP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  SetShadowStyle(label, 3);
  Enable(label, true);
  Flush(app);
  auto              renderer            = label.GetRendererAt(0);
  const auto        oldSize             = VisualSize(renderer);
  unsigned          completed           = 0;
  bool              completePublication = false;
  ConnectionTracker tracker;
  label.AsyncRenderFinishedSignal().Connect(&tracker, [&](View, float, float)
  {
    if(++completed == 1)
    {
      completePublication = Advanced(label) && VisualSize(renderer) == oldSize && label.GetRendererCount() == 1u;
      SetShadowStyle(label, 30);
      Enable(label, false);
    }
  });
  label.SetAsyncRendering(true);
  Flush(app);
  DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
  DALI_TEST_CHECK(completePublication);
  DALI_TEST_EQUALS(completed, 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(VisualSize(renderer), oldSize, TEST_LOCATION);
  Flush(app);
  DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
  Flush(app);
  DALI_TEST_EQUALS(completed, 2u, TEST_LOCATION);
  DALI_TEST_CHECK(!Advanced(label));
  DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
  DALI_TEST_CHECK(!UsesVisualGeometryCulling(app, label, renderer));
  END_TEST;
}

int UtcDaliTextEffectOverflowAsyncManualCancellationP(void)
{
  UiTestApplication app;
  for(unsigned mutation = 0; mutation < 4; ++mutation)
  {
    auto label = MakeLabel(app);
    Enable(label, true);
    SetShadowStyle(label, 3);
    Flush(app);
    label.SetAsyncRendering(true);
    Flush(app);
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
    unsigned          completed = 0;
    ConnectionTracker tracker;
    label.AsyncRenderFinishedSignal().Connect(&tracker, [&](View, float, float)
    { ++completed; });
    auto& impl = static_cast<Ui::Integration::LabelImpl&>(GetImpl(label));
    impl.RequestAsyncRenderWithFixedSize(240, 100);
    if(mutation == 0) SetShadowStyle(label, 30);
    if(mutation == 1) label.SetRequestedWidth(300);
    if(mutation == 2) label.SetProperty(Actor::Property::SIZE_WIDTH, 300.f);
    if(mutation == 3) Enable(label, false);
    Flush(app);
    for(unsigned i = 0; i < 3 && !completed; ++i) Test::WaitForEventThreadTrigger(1, 5);
    Flush(app);
    DALI_TEST_EQUALS(completed, 1u, TEST_LOCATION);
    DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
    DALI_TEST_EQUALS(Advanced(label), mutation != 3, TEST_LOCATION);
    label.Unparent();
    Flush(app);
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowAsyncManualResizeCompletionP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  Enable(label, true);
  SetShadowStyle(label, 3);
  Flush(app);
  label.SetAsyncRendering(true);
  Flush(app);
  DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
  Flush(app);
  unsigned          completed = 0;
  ConnectionTracker tracker;
  label.AsyncRenderFinishedSignal().Connect(&tracker, [&](View, float width, float height)
  {
    ++completed;
    label.SetRequestedWidth(width);
    label.SetRequestedHeight(height);
  });
  static_cast<Ui::Integration::LabelImpl&>(GetImpl(label)).RequestAsyncRenderWithFixedSize(220, 90);
  DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
  Flush(app);
  DALI_TEST_EQUALS(completed, 1u, TEST_LOCATION);
  DALI_TEST_CHECK(!Test::WaitForEventThreadTrigger(1, 0));
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextEffectOverflowAsyncSupersampledSceneGeometryP(void)
{
  UiTestApplication app;
  for(unsigned style = 0; style < 5; ++style)
  {
    auto label = MakeLabel(app);
    label.SetRequestedWidth(260);
    label.SetRequestedHeight(76);
    label.SetFontSize(44);
    label.SetTextOverflowMode(Text::OverflowMode::CLIP);
    if(style < 2)
    {
      Text::Shadow shadow;
      shadow.SetOffset(style == 1 ? Vector2(3, 3) : Vector2(-30, 25));
      shadow.SetBlurRadius(style == 1 ? 3 : 30);
      label.SetTextShadow(shadow);
      if(style == 1)
      {
        label.SetHorizontalTextAlignment(Text::Alignment::END);
        label.SetVerticalTextAlignment(Text::Alignment::END);
      }
    }
    else if(style == 2)
    {
      Text::Outline outline;
      outline.SetWidth(12);
      outline.SetOffset(Vector2(15, -12));
      outline.SetBlurRadius(12);
      label.SetTextOutline(outline);
    }
    else if(style == 3)
    {
      Text::Underline underline;
      underline.SetThickness(25);
      label.SetTextUnderline(underline);
    }
    else
    {
      Text::Bevel bevel;
      bevel.SetDirection(Vector2(-1, 1));
      bevel.SetIntensity(12);
      label.SetTextBevel(bevel);
    }
    Enable(label, true);
    Flush(app);
    DALI_TEST_CHECK(Advanced(label));
    const auto paintExtent = [style](Renderer renderer)
    {
      const auto size = VisualSize(renderer);
      if(style == 3)
      {
        const auto    overlayUv = renderer.GetProperty<Vector4>(renderer.GetPropertyIndex("uOverlayUvTransform"));
        const Vector2 overlaySize(size.x / overlayUv.x, size.y / overlayUv.y);
        const auto    topLeft = Offset(renderer) - Vector2(overlayUv.z * overlaySize.x, overlayUv.w * overlaySize.y);
        return Vector4(overlaySize.x, overlaySize.y, topLeft.x, topLeft.y);
      }
      const auto uv   = renderer.GetProperty<Vector4>(renderer.GetPropertyIndex("uFillUvTransform"));
      // Recover the fill rectangle in the published quad's local coordinates.
      // Compare the effect extent relative to ink, independent of font hinting.
      const Vector2 fillSize(size.x / uv.x, size.y / uv.y);
      const Vector2 leftTop(-uv.z * fillSize.x, -uv.w * fillSize.y);
      const auto    rightBottom = size - leftTop - fillSize;
      return Vector4(leftTop.x, leftTop.y, rightBottom.x, rightBottom.y);
    };
    const auto syncExtent = paintExtent(label.GetRendererAt(0));
    for(float scale : {1.f, 1.5f, 2.f})
    {
      tet_printf("Scene geometry: renderScale=%g style=%u\n", scale, style);
      label.SetRenderScale(scale);
      label.SetAsyncRendering(true);
      Flush(app);
      DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
      Flush(app);
      DALI_TEST_CHECK(Advanced(label));
      // Gutter/offset quantization costs up to one scene pixel. Underline also
      // quantizes its terminal edge and glyph advance, allowing two pixels.
      DALI_TEST_EQUALS(paintExtent(label.GetRendererAt(0)), syncExtent, style == 3 ? 2.f : 1.f, TEST_LOCATION);
    }
    label.Unparent();
    Flush(app);
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowColorGlyphPublicationP(void)
{
  UiTestApplication app;
  auto label = MakeLabel(app);
  label.SetText("ABC");
  label.SetTextColor(UiColor(Color::RED));
  SetShadowStyle(label);
  Underline(label);
  Flush(app);
  auto* model = static_cast<const Text::Model*>(ControllerFor(label)->GetRenderTextModel());
  model->mVisualModel->mGlyphs[1u].index = TextAbstraction::Test::COLOR_GLYPH_INDEX;
  Enable(label, true);
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  auto renderer = label.GetRendererAt(0);
  auto textures = renderer.GetTextures();
  DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTextureCount(), 4u, TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(0).GetPixelFormat(), Pixel::RGBA8888, TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(3).GetPixelFormat(), Pixel::L8, TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(0).GetWidth(), textures.GetTexture(3).GetWidth(), TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(0).GetHeight(), textures.GetTexture(3).GetHeight(), TEST_LOCATION);
  auto multipleIndex = renderer.GetPropertyIndex("uHasMultipleTextColors");
  DALI_TEST_EQUALS(renderer.GetProperty<float>(multipleIndex), 0.f, TEST_LOCATION);
  const auto bakedIndex = renderer.GetPropertyIndex("uTextColorBaked");
  DALI_TEST_EQUALS(renderer.GetProperty<Vector4>(bakedIndex), Color::RED, TEST_LOCATION);
  const auto shader = renderer.GetShader();
  const auto propertyCount = renderer.GetPropertyCount();
  Animation animation = Animation::New(1.f);
  animation.AnimateTo(Property(label, Label::Property::TEXT_COLOR), Color::GREEN);
  animation.Play();
  app.SendNotification();
  app.Render(500);
  app.SendNotification();
  auto colorIndex = renderer.GetPropertyIndex("uTextColorAnimatable");
  DALI_TEST_EQUALS(renderer.GetCurrentProperty<Vector4>(colorIndex), Vector4(.5f, .5f, 0, 1), .01f, TEST_LOCATION);
  app.Render(501);
  app.SendNotification();
  DALI_TEST_EQUALS(renderer.GetCurrentProperty<Vector4>(colorIndex), Color::GREEN, .01f, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetProperty<Vector4>(bakedIndex), Color::RED, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetTextures(), textures, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetShader(), shader, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetPropertyCount(), propertyCount, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetProperty<float>(multipleIndex), 0.f, TEST_LOCATION);
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, renderer));
  // Returning to ordinary text drops the mask and uses the existing L8 path.
  label.SetText("Plain");
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_EQUALS(renderer.GetTextures().GetTextureCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetTextures().GetTexture(0).GetPixelFormat(), Pixel::L8, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextEffectOverflowColorGlyphLifecycleP(void)
{
  UiTestApplication app;
  auto label = MakeLabel(app);
  SetShadowStyle(label, 3);
  Underline(label);
  Enable(label, true);
  Shader mixedShader;
  unsigned propertyCount = 0;
  for(unsigned cycle = 0; cycle < 25; ++cycle)
  {
    label.SetText(cycle % 2 ? "ABC" : "ABCD");
    Flush(app);
    auto* model = static_cast<const Text::Model*>(ControllerFor(label)->GetRenderTextModel());
    model->mVisualModel->mGlyphs[1u].index = TextAbstraction::Test::COLOR_GLYPH_INDEX;
    Enable(label, true);
    Flush(app);
    auto renderer = label.GetRendererAt(0);
    DALI_TEST_CHECK(Advanced(label));
    DALI_TEST_EQUALS(renderer.GetTextures().GetTextureCount(), 4u, TEST_LOCATION);
    DALI_TEST_EQUALS(renderer.GetProperty<float>(renderer.GetPropertyIndex("uHasMultipleTextColors")), 0.f, TEST_LOCATION);
    if(!mixedShader)
    {
      mixedShader = renderer.GetShader();
      propertyCount = renderer.GetPropertyCount();
    }
    DALI_TEST_EQUALS(renderer.GetShader(), mixedShader, TEST_LOCATION);
    DALI_TEST_EQUALS(renderer.GetPropertyCount(), propertyCount, TEST_LOCATION);
    auto& trace = app.GetGlAbstraction().GetDrawTrace();
    trace.Enable(true);
    trace.Reset();
    app.Render();
    DALI_TEST_EQUALS(trace.CountMethod("DrawElements") + trace.CountMethod("DrawArrays"), 1u, TEST_LOCATION);
    trace.Enable(false);
    Enable(label, false);
    Flush(app);
    DALI_TEST_CHECK(!Advanced(label));
    Enable(label, true);
    Flush(app);
    DALI_TEST_EQUALS(renderer.GetShader(), mixedShader, TEST_LOCATION);
    label.SetStyledText(Text::StyledText::FromMarkup("<color value='red'>A</color>BC"));
    Flush(app);
    model = static_cast<const Text::Model*>(ControllerFor(label)->GetRenderTextModel());
    model->mVisualModel->mGlyphs[1u].index = TextAbstraction::Test::COLOR_GLYPH_INDEX;
    Enable(label, true);
    Flush(app);
    DALI_TEST_CHECK(Advanced(label));
    DALI_TEST_EQUALS(renderer.GetProperty<float>(renderer.GetPropertyIndex("uHasMultipleTextColors")), 1.f, TEST_LOCATION);
    DALI_TEST_EQUALS(renderer.GetTextures().GetTextureCount(), 3u, TEST_LOCATION);
    label.Unparent();
    Flush(app);
    app.GetScene().Add(label);
    Flush(app);
    label.SetText("");
    Flush(app);
    DALI_TEST_EQUALS(label.GetRendererCount(), 0u, TEST_LOCATION);
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowGradientSpanP(void)
{
  UiTestApplication app;
  UiScaleManager::Get().SetScalable(true);
  UiScaleManager::Get().SetScale(1.f);
  auto label = MakeLabel(app);
  Enable(label, true);
  SetShadowStyle(label);
  Gradient::Linear gradient(Vector2::ZERO, Vector2::ONE);
  gradient.SetStopNodes({Gradient::StopNode(0, UiColor(Color::RED)), Gradient::StopNode(1, UiColor(Color::BLUE))});
  auto builder = Text::StyledTextBuilder::New("Hello gradient");
  builder.SetSpan(Text::GradientSpan::New(gradient), 0, 5);
  const auto styledText = builder.Build();
  label.SetStyledText(styledText);
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
  label.SetAsyncRendering(true);
  Flush(app);
  DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
  Flush(app);
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, label.GetRendererAt(0)));
  const auto initialSize = VisualSize(label.GetRendererAt(0));
  for(float scale : {1.5f, 2.f, 1.f})
  {
    UiScaleManager::Get().SetScale(scale);
    Flush(app);
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
    DALI_TEST_CHECK(label.GetStyledText() == styledText);
    DALI_TEST_CHECK(Advanced(label));
    DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
  }
  DALI_TEST_EQUALS(VisualSize(label.GetRendererAt(0)), initialSize, TEST_LOCATION);
  END_TEST;
}

namespace
{
int CheckColdColorGlyphPublication(bool async)
{
  UiTestApplication app;
  Text::MultilanguageSupport::Get().ClearCache();
  auto label = MakeLabel(app);
  label.SetAsyncRendering(async);
  label.SetFontFamily(TextAbstraction::Test::FALLBACK_SOURCE_FAMILY);
  label.SetFontSize(48.f);
  label.SetRequestedWidth(760.f);
  label.SetRequestedHeight(700.f);
  label.SetText("        Hello 😀 Emoji");
  label.SetMultiLine(false);
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  label.SetVerticalTextAlignment(Text::Alignment::CENTER);
  Enable(label, true);
  Flush(app);
  if(async)
  {
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
  }
  DALI_TEST_CHECK(Advanced(label));
  DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
  if(!async)
  {
    const auto* model = ControllerFor(label)->GetRenderTextModel();
    const auto* glyphs = model->GetGlyphs();
    // Assert the first publication before any style toggle or second validation.
    DALI_TEST_EQUALS(glyphs[0].fontId, glyphs[8].fontId, TEST_LOCATION);
    DALI_TEST_EQUALS(glyphs[0].advance, glyphs[8].advance, .001f, TEST_LOCATION);
  }
  auto renderer = label.GetRendererAt(0);
  const auto textures = renderer.GetTextures();
  DALI_TEST_EQUALS(textures.GetTextureCount(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(0).GetPixelFormat(), Pixel::RGBA8888, TEST_LOCATION);
  DALI_TEST_EQUALS(textures.GetTexture(1).GetPixelFormat(), Pixel::L8, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetProperty<float>(renderer.GetPropertyIndex("uHasMultipleTextColors")), 0.f, TEST_LOCATION);
  const auto size = VisualSize(renderer);
  const auto offset = Offset(renderer);
  // The mock rasterizer emits 2x4 glyph bitmaps at the shaped positions.
  DALI_TEST_EQUALS(size, Vector2(454.f, 6.f), .001f, TEST_LOCATION);
  DALI_TEST_EQUALS(offset, Vector2(-1.f, 331.f), .001f, TEST_LOCATION);
  DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, renderer));
  SetShadowStyle(label, 0.f);
  Flush(app);
  if(async)
  {
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
  }
  label.SetTextShadow(Text::Shadow::None());
  Flush(app);
  if(async)
  {
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
  }
  DALI_TEST_EQUALS(VisualSize(label.GetRendererAt(0)), size, .001f, TEST_LOCATION);
  DALI_TEST_EQUALS(Offset(label.GetRendererAt(0)), offset, .001f, TEST_LOCATION);
  // Same authored content after another validation must keep the cold geometry.
  label.SetText("        Hello 😀 Emoji");
  Flush(app);
  if(async)
  {
    DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
    Flush(app);
  }
  DALI_TEST_EQUALS(VisualSize(label.GetRendererAt(0)), size, .001f, TEST_LOCATION);
  DALI_TEST_EQUALS(Offset(label.GetRendererAt(0)), offset, .001f, TEST_LOCATION);
  END_TEST;
}
} // namespace

int UtcDaliTextEffectOverflowColdColorGlyphPublicationP(void)
{
  return CheckColdColorGlyphPublication(false);
}

int UtcDaliTextEffectOverflowColdColorGlyphAsyncPublicationP(void)
{
  return CheckColdColorGlyphPublication(true);
}

namespace
{
int CheckRecolorPayload(int order)
{
  UiTestApplication app;
  auto label = MakeLabel(app);
  label.SetText(order == 0 ? "H" : "HH");
  Flush(app);
  auto controller = ControllerFor(label);
  auto* model = static_cast<const Text::Model*>(controller->GetRenderTextModel());
  auto& visual = *model->mVisualModel;
  visual.mGlyphs[0].index = TextAbstraction::Test::HALF_COVERAGE_GLYPH_INDEX;
  if(order != 0)
  {
    visual.mGlyphs[1] = visual.mGlyphs[0];
    visual.mGlyphPositions[1] = visual.mGlyphPositions[0];
    visual.mGlyphs[order == 1 ? 1 : 0].index = TextAbstraction::Test::COLOR_GLYPH_INDEX;
  }
  auto typesetter = Text::Typesetter::New(model);
  const auto multiply = [](uint32_t x, uint32_t y)
  {
    const auto product = x * y;
    return ((product << 15) + (product << 7) + product) >> 23;
  };
  for(float alpha : {1.f, .75f, .5f, .25f, 0.f})
  {
    visual.SetTextColor(Vector4(1.f, 1.f, 1.f, alpha));
    Text::TextRasterResult result;
    DALI_TEST_CHECK(typesetter->RenderRasterPlanes(model->GetControlSize(), controller->GetTextDirection(),
                                                    Pixel::RGBA8888, 4094, result, true));
    DALI_TEST_CHECK(result.fill && result.colorGlyphMask);
    const auto fill = Dali::Integration::GetPixelDataBuffer(result.fill);
    const auto mask = Dali::Integration::GetPixelDataBuffer(result.colorGlyphMask);
    DALI_TEST_EQUALS(result.fill.GetWidth(), 2u, TEST_LOCATION);
    DALI_TEST_EQUALS(result.fill.GetHeight(), 4u, TEST_LOCATION);
    unsigned nonzero = 0u;
    for(unsigned y = 0u; y < result.fill.GetHeight(); ++y)
    {
      for(unsigned x = 0u; x < result.fill.GetWidth(); ++x)
      {
        const auto* pixel = fill.buffer + y * (fill.strideBytes ? fill.strideBytes : result.fill.GetWidth() * 4u) + x * 4u;
        const auto coefficient = mask.buffer[y * (mask.strideBytes ? mask.strideBytes : result.colorGlyphMask.GetWidth()) + x];
        if(order == 1)
        {
          // Native overwrite removes all monochrome ownership, even with authored alpha zero.
          DALI_TEST_EQUALS(coefficient, 0u, TEST_LOCATION);
        }
        else if(coefficient)
        {
          ++nonzero;
          const auto bakedAlpha = static_cast<uint8_t>(alpha * 255.f);
          const auto nativeAlpha = multiply(bakedAlpha, (x == 0u && y == 0u) ? 128u : 255u);
          DALI_TEST_EQUALS(coefficient, order == 0 ? 128u : std::max(128u, nativeAlpha), TEST_LOCATION);
          for(unsigned channel = 0; channel < 4; ++channel)
          {
            DALI_TEST_EQUALS(pixel[channel], multiply(bakedAlpha, coefficient), TEST_LOCATION);
          }
          // Recolor to red: preserve premultiplication, including a zero-alpha source.
          const float expected = coefficient / 255.f;
          const float red = pixel[0] / 255.f + (1.f - bakedAlpha / 255.f) * expected;
          DALI_TEST_EQUALS(red, expected, 1.01f / 255.f, TEST_LOCATION);
        }
      }
    }
    if(order != 1)
    {
      DALI_TEST_EQUALS(nonzero, 8u, TEST_LOCATION);
    }

    PixelData legacyMask;
    auto legacyFill = typesetter->Render(model->GetControlSize(), controller->GetTextDirection(),
                                          Text::Typesetter::RENDER_NO_STYLES, false, Pixel::RGBA8888, Size::ZERO, &legacyMask);
    DALI_TEST_CHECK(legacyFill && legacyMask);
    const auto legacy = Dali::Integration::GetPixelDataBuffer(legacyMask);
    for(unsigned y = 0u; y < result.colorGlyphMask.GetHeight(); ++y)
    {
      for(unsigned x = 0u; x < result.colorGlyphMask.GetWidth(); ++x)
      {
        const auto expected = mask.buffer[y * (mask.strideBytes ? mask.strideBytes : result.colorGlyphMask.GetWidth()) + x];
        const auto actual = legacy.buffer[(y + result.geometry.fill.top) * (legacy.strideBytes ? legacy.strideBytes : legacyMask.GetWidth()) + x + result.geometry.fill.left];
        DALI_TEST_EQUALS(actual, expected, TEST_LOCATION);
      }
    }
  }
  END_TEST;
}
} // namespace

int UtcDaliTextRecolorIsolatedCoverageP(void)
{
  return CheckRecolorPayload(0);
}

int UtcDaliTextRecolorNativeOverMonochromeP(void)
{
  return CheckRecolorPayload(1);
}

int UtcDaliTextRecolorMonochromeOverNativeP(void)
{
  return CheckRecolorPayload(2);
}

int UtcDaliTextRecolorUnconstrainedColorP(void)
{
  UiTestApplication app;
  View view = View::New();
  view.SetProperty(Actor::Property::SIZE, Vector3(200.f, 80.f, 0.f));
  Property::Map properties;
  properties.Add(Ui::Integration::Visual::Property::TYPE, Ui::Integration::InternalVisualType::TEXT);
  properties.Add(Ui::Integration::TextVisual::Property::TEXT, "HH");
  properties.Add(Ui::Integration::TextVisual::Property::FONT_SIZE, 20.f);
  properties.Add(Ui::Integration::TextVisual::Property::TEXT_COLOR, Vector4(1.f, .5f, .25f, .5f));
  auto visual = Ui::Integration::VisualFactory::Get().CreateVisual(properties);
  Ui::Internal::ViewDataImpl::Get(GetImpl(view)).RegisterVisual(Text::LabelPropertyIndex::TEXT, visual, 0);
  app.GetScene().Add(view);
  Flush(app);
  auto controller = Ui::Internal::TextVisual::GetController(visual);
  auto* model = static_cast<const Text::Model*>(controller->GetRenderTextModel());
  DALI_TEST_EQUALS(model->mVisualModel->mGlyphs.Count(), 2u, TEST_LOCATION);
  model->mVisualModel->mGlyphs[1].index = TextAbstraction::Test::COLOR_GLYPH_INDEX;
  Ui::Internal::TextVisual::EnableRendererUpdate(visual);
  Ui::Internal::TextVisual::UpdateRenderer(visual);
  Flush(app);
  DALI_TEST_EQUALS(view.GetRendererCount(), 1u, TEST_LOCATION);
  const auto renderer = view.GetRendererAt(0);
  const auto index = renderer.GetPropertyIndex("uTextColorAnimatable");
  DALI_TEST_EQUALS(renderer.GetCurrentProperty<Vector4>(index), Vector4(.5f, .25f, .125f, .5f), .0001f, TEST_LOCATION);
  DALI_TEST_EQUALS(renderer.GetProperty<Vector4>(renderer.GetPropertyIndex("uTextColorBaked")),
                   Vector4(127.f, 63.f, 31.f, 127.f) / 255.f, .0001f, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRecolorAsyncBlurShaderPublicationP(void)
{
  UiTestApplication app;
  auto label = MakeLabel(app);
  label.SetFontFamily(TextAbstraction::Test::FALLBACK_SOURCE_FAMILY);
  label.SetText("H😀");
  Text::Reveal reveal;
  Ui::Integration::Text::Reveal::SetBlurRadius(reveal, 8.f);
  label.SetTextReveal(reveal);
  label.SetTextRevealProgress(1.f);
  Flush(app);
  auto foreground = label.FindChildByName("RevealGaussianForeground");
  DALI_TEST_CHECK(foreground && foreground.GetRendererCount() == 1u);
  const auto shader = foreground.GetRendererAt(0).GetShader();
  label.SetAsyncRendering(true);
  SetShadowStyle(label, 8.f);
  Flush(app);
  DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
  Flush(app);
  label.SetTextShadow(Text::Shadow::None());
  Flush(app);
  DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
  Flush(app);
  foreground = label.FindChildByName("RevealGaussianForeground");
  DALI_TEST_CHECK(foreground && foreground.GetRendererCount() == 1u);
  // The retired capture must not restore its old styled shader over the new mask bindings.
  DALI_TEST_EQUALS(foreground.GetRendererAt(0).GetShader(), shader, TEST_LOCATION);
  DALI_TEST_EQUALS(foreground.GetRendererAt(0).GetTextures().GetTextureCount(), 3u, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextEffectOverflowGradientPublicationP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  label.SetRequestedWidth(240);
  label.SetRequestedHeight(100);
  Enable(label, true);
  SetShadowStyle(label, 4);
  Gradient::Linear gradient(Vector2(-.5f, -.5f), Vector2(.5f, .5f));
  gradient.SetStopNodes({Gradient::StopNode(0, UiColor(Color::RED)), Gradient::StopNode(1, UiColor(Color::BLUE))});
  for(bool mixed : {false, true})
    for(bool async : {false, true})
      for(unsigned mode = 0; mode < 4; ++mode)
      {
        label.SetAsyncRendering(async);
        if(mixed)
          label.SetStyledText(Text::StyledText::FromMarkup("<color value='red'>Hi</color> Text"));
        else
          label.SetText("Hello");
        label.SetTextGradient(mode & 1 ? gradient : Gradient::Base::None());
        label.SetTextGradientOverlay(mode & 2 ? gradient : Gradient::Base::None());
        Flush(app);
        if(async)
        {
          DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
          Flush(app);
        }
        DALI_TEST_CHECK(Advanced(label));
        DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
        const auto renderer = label.GetRendererAt(0);
        const auto textures = renderer.GetTextures();
        // Fill + Shadow, plus gradient LUT and mixed target mask when requested.
        const unsigned expected = 2u + (mixed && (mode & 1) ? 1u : 0u) + (mode & 1 ? 1u : 0u) + (mode & 2 ? 1u : 0u);
        DALI_TEST_EQUALS(textures.GetTextureCount(), expected, TEST_LOCATION);
        if(mode)
        {
          auto index = renderer.GetPropertyIndex("uTextCoordTransform");
          DALI_TEST_CHECK(index != Property::INVALID_INDEX);
          auto uv = renderer.GetProperty<Vector4>(index);
          DALI_TEST_CHECK(uv.x > 0.f && uv.y > 0.f && std::isfinite(uv.z) && std::isfinite(uv.w));
          DALI_TEST_CHECK(UsesVisualGeometryCulling(app, label, renderer));
        }
      }
  END_TEST;
}

int UtcDaliTextEffectOverflowGradientAnimationP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  Enable(label, true);
  label.SetFontFamily(TextAbstraction::Test::FALLBACK_SOURCE_FAMILY);
  label.SetFontSize(48);
  label.SetRequestedWidth(760);
  label.SetText("Hello 😀");
  SetShadowStyle(label, 3);
  Gradient::Linear gradient(Vector2(-.5f, 0), Vector2(.5f, 0));
  gradient.SetStopNodes({Gradient::StopNode(0, UiColor(Color::RED)), Gradient::StopNode(1, UiColor(Color::BLUE))});
  for(bool async : {false, true})
  {
    label.SetAsyncRendering(async);
    label.SetTextGradient(gradient);
    label.SetTextGradientOverlay(gradient);
    Flush(app);
    if(async)
    {
      DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
      Flush(app);
    }
    DALI_TEST_CHECK(Advanced(label));
    auto      renderer  = label.GetRendererAt(0);
    auto      textures  = renderer.GetTextures();
    Animation animation = Animation::New(1.f);
    label.Animate(animation).TextGradientStartOffset(.8f, Duration(1.f)).TextGradientOverlayStartOffset(.6f, Duration(1.f));
    animation.Play();
    for(unsigned frame = 0; frame < 5; ++frame)
    {
      app.SendNotification();
      app.Render(frame ? 250 : 0);
      app.SendNotification();
      app.Render(0);
      DALI_TEST_EQUALS(label.GetRendererAt(0), renderer, TEST_LOCATION);
      DALI_TEST_EQUALS(renderer.GetTextures(), textures, TEST_LOCATION);
      for(const char* name : {"uTextGradientStartOffset", "uTextGradientOverlayStartOffset"})
      {
        auto source = label.GetPropertyIndex(name), target = renderer.GetPropertyIndex(name);
        DALI_TEST_CHECK(source != Property::INVALID_INDEX && target != Property::INVALID_INDEX);
        DALI_TEST_EQUALS(renderer.GetCurrentProperty<float>(target), label.GetCurrentProperty<float>(source), .001f, TEST_LOCATION);
      }
    }
    animation.Stop();
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowInvalidGradientP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  Enable(label, true);
  label.SetStyledText(Text::StyledText::FromMarkup("<color value='red'>Red</color> Text"));
  Gradient::Linear gradient(Vector2::ZERO, Vector2::ZERO);
  gradient.SetStopNodes({Gradient::StopNode(0, UiColor(Color::RED)), Gradient::StopNode(1, UiColor(Color::BLUE))});
  for(bool async : {false, true})
  {
    label.SetAsyncRendering(async);
    label.SetTextGradient(gradient);
    label.SetTextGradientOverlay(gradient);
    Flush(app);
    if(async)
    {
      DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
      Flush(app);
    }
    DALI_TEST_CHECK(Advanced(label));
    DALI_TEST_EQUALS(label.GetRendererAt(0).GetTextures().GetTextureCount(), 1u, TEST_LOCATION);
  }
  END_TEST;
}

int UtcDaliTextEffectOverflowEmbossPublicationP(void)
{
  UiTestApplication app;
  auto label = MakeLabel(app);
  Enable(label, true);
  label.SetFontFamily(TextAbstraction::Test::FALLBACK_SOURCE_FAMILY);
  label.SetFontSize(40);
  label.SetRequestedWidth(400);
  label.SetRequestedHeight(120);
  Text::Bevel bevel;
  bevel.SetDirection(Vector2(-1, 1));
  bevel.SetIntensity(6);
  Gradient::Linear gradient(Vector2(-.5f, 0), Vector2(.5f, 0));
  gradient.SetStopNodes({Gradient::StopNode(0, UiColor(Color::RED)), Gradient::StopNode(1, UiColor(Color::BLUE))});
  for(bool async : {false, true})
    for(unsigned kind = 0; kind < 3; ++kind)
      for(unsigned mode = 0; mode < 4; ++mode)
      {
        label.SetAsyncRendering(async);
        if(kind == 0) label.SetText("Hello");
        if(kind == 1) label.SetText("Hello 😀");
        if(kind == 2) label.SetStyledText(Text::StyledText::FromMarkup("<color value='red'>Red</color> <color value='blue'>Blue</color>"));
        label.SetTextBevel(bevel);
        label.SetTextGradient(mode & 1 ? gradient : Gradient::Base::None());
        label.SetTextGradientOverlay(mode & 2 ? gradient : Gradient::Base::None());
        Flush(app);
        if(async)
        {
          DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
          Flush(app);
        }
        DALI_TEST_CHECK(Advanced(label));
        DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
        auto renderer = label.GetRendererAt(0);
        auto textures = renderer.GetTextures();
        DALI_TEST_EQUALS(textures.GetTextureCount(), 1u + (kind == 1 ? 1u : 0u) + (mode & 2 ? 1u : 0u), TEST_LOCATION);
        auto size = renderer.GetProperty<Vector2>(renderer.GetPropertyIndex("uEmbossSize"));
        DALI_TEST_EQUALS(size.x, 1.f / textures.GetTexture(0).GetWidth(), .00001f, TEST_LOCATION);
        DALI_TEST_EQUALS(size.y, 1.f / textures.GetTexture(0).GetHeight(), .00001f, TEST_LOCATION);
        DALI_TEST_EQUALS(renderer.GetProperty<float>(renderer.GetPropertyIndex("uEmbossStrength")), 6.f, TEST_LOCATION);
      }
  END_TEST;
}

int UtcDaliTextEffectOverflowEmbossInvalidGeometryP(void)
{
  UiTestApplication app;
  for(bool async : {false, true})
  {
    auto label = MakeLabel(app);
    Enable(label, true);
    label.SetAsyncRendering(async);
    for(unsigned invalid = 0; invalid < 3; ++invalid)
    {
      Text::Bevel bevel;
      if(invalid == 0) bevel.SetDirection(Vector2::ZERO);
      if(invalid == 1) bevel.SetDirection(Vector2(std::numeric_limits<float>::infinity(), 1));
      if(invalid == 2) bevel.SetIntensity(std::numeric_limits<float>::infinity());
      label.SetTextBevel(bevel);
      Flush(app);
      if(async)
      {
        DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
        Flush(app);
      }
      // Sync retains its legacy fallback. Async has no fallback pixels in a
      // completed plane payload, so failed preparation removes the publication.
      DALI_TEST_CHECK(!Advanced(label));
      DALI_TEST_EQUALS(label.GetRendererCount(), async ? 0u : 1u, TEST_LOCATION);
      label.SetTextBevel(Text::Bevel());
      Flush(app);
      if(async)
      {
        DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
        Flush(app);
      }
      DALI_TEST_CHECK(Advanced(label));
      DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
    }
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRevealSharedPublicationLifecycleP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app);
  Enable(label, true);
  const auto taskCount = app.GetScene().GetRenderTaskList().GetTaskCount();
  for(bool async : {false, true})
  {
    label.SetAsyncRendering(async);
    for(unsigned cycle = 0; cycle < 24; ++cycle)
    {
      label.SetText(cycle % 2 ? "First\nSecond" : "Replaced\ntext");
      label.SetRequestedWidth(cycle % 2 ? 220.f : 180.f);
      label.SetMultiLine(true);
      label.SetRenderScale(std::array<float, 4>{1.f, 1.5f, 2.f, 1.f}[cycle % 4]);
      Text::Reveal reveal;
      reveal.SetUnit(static_cast<Text::Reveal::Unit>(cycle % 4));
      reveal.SetSequence(cycle % 2 ? Text::Reveal::Sequence::PER_LINE : Text::Reveal::Sequence::WHOLE_TEXT);
      const bool blur = cycle % 3 == 0;
      Ui::Integration::Text::Reveal::SetBlurRadius(reveal, blur ? 3.f : 0.f);
      Ui::Integration::Text::Reveal::SetBlurDurationRatio(reveal, .3f);
      label.SetTextReveal(reveal);
      label.SetTextRevealProgress(0.f);
      SetShadowStyle(label, 3.f);
      Underline(label, 3.f);
      Flush(app);
      if(async)
      {
        DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
        Flush(app);
      }
      auto renderer = Ui::GetImplementation(VisualFor(label)).GetRenderer();
      DALI_TEST_CHECK(renderer);
      const auto progress = renderer.GetPropertyIndex("uTextRevealProgress");
      DALI_TEST_CHECK(progress != Property::INVALID_INDEX);
      auto       textures = renderer.GetTextures();
      const auto fill     = textures.GetTexture(0);
      const auto metadata = textures.GetTexture(textures.GetTextureCount() - 1);
      DALI_TEST_CHECK(fill && metadata);
      for(float value : {0.f, .5f, 1.f, .5f, 0.f})
      {
        label.SetTextRevealProgress(value);
        Flush(app);
        DALI_TEST_CHECK(Ui::GetImplementation(VisualFor(label)).GetRenderer() == renderer);
        DALI_TEST_CHECK(renderer.GetTextures().GetTexture(0) == fill);
        DALI_TEST_EQUALS(renderer.GetCurrentProperty<float>(progress), value, .001f, TEST_LOCATION);
      }
      label.SetText("");
      Flush(app);
      DALI_TEST_EQUALS(app.GetScene().GetRenderTaskList().GetTaskCount(), taskCount, TEST_LOCATION);
      label.Unparent();
      Flush(app);
      label.SetText("Reconnect final text");
      app.GetScene().Add(label);
      Flush(app);
      if(async)
      {
        DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
        Flush(app);
      }
      label.SetTextReveal(Text::Reveal::None());
      Flush(app);
      if(async)
      {
        DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
        Flush(app);
      }
      DALI_TEST_EQUALS(app.GetScene().GetRenderTaskList().GetTaskCount(), taskCount, TEST_LOCATION);
    }
  }
  label.Unparent();
  Flush(app);
  DALI_TEST_EQUALS(app.GetScene().GetRenderTaskList().GetTaskCount(), taskCount, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextMarqueeAxisSelectiveGeometryP(void)
{
  UiTestApplication app;
  for(bool horizontal : {true, false})
    for(float displacement : {-50.f, 50.f})
    {
      auto label = MakeLabel(app);
      label.SetText(horizontal ? "MMMMMMMMMMMM" : "MMMM\nMMMM\nMMMM");
      label.SetMultiLine(!horizontal);
      label.SetRequestedWidth(horizontal ? 80.f : 40.f);
      label.SetRequestedHeight(40.f);
      label.SetMarqueeOrientation(horizontal ? Text::MarqueeOrientation::HORIZONTAL : Text::MarqueeOrientation::VERTICAL);
      label.SetMarqueeTriggerPolicy(Text::MarqueeTriggerPolicy::MANUAL);
      label.SetMarqueeLoopCount(0);
      Text::Shadow shadow;
      shadow.SetOffset(horizontal ? Vector2(0, displacement) : Vector2(displacement, 0));
      shadow.SetBlurRadius(5.f);
      label.SetTextShadow(shadow);
      Enable(label, true);
      Flush(app);
      label.StartMarquee();
      Flush(app);
      DALI_TEST_CHECK(label.IsMarqueeRunning());
      DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
      auto renderer = label.GetRendererAt(0);
      const auto viewport = renderer.GetProperty<Vector4>(renderer.GetPropertyIndex("uMarqueeViewport"));
      const auto size = VisualSize(renderer);
      const unsigned cross = horizontal ? 1u : 0u;
      const unsigned scroll = 1u - cross;
      DALI_TEST_EQUALS(size[scroll], viewport[scroll], .001f, TEST_LOCATION);
      DALI_TEST_CHECK(size[cross] > viewport[cross]);
      DALI_TEST_EQUALS(viewport[scroll + 2u], 0.f, .001f, TEST_LOCATION);
      DALI_TEST_CHECK(viewport[cross + 2u] <= 0.f);
      const auto textures = renderer.GetTextures();
      const auto shader = renderer.GetShader();
      app.Render(100);
      app.SendNotification();
      DALI_TEST_CHECK(renderer.GetShader() == shader);
      DALI_TEST_CHECK(renderer.GetTextures() == textures);
      DALI_TEST_EQUALS(VisualSize(renderer), size, .001f, TEST_LOCATION);
      label.Unparent();
      Flush(app);
    }
  END_TEST;
}

int UtcDaliTextMarqueeStopRestartPresentationP(void)
{
  UiTestApplication app;
  for(bool async : {false, true})
    for(bool horizontal : {true, false})
      for(auto mode : {Text::MarqueeStopMode::IMMEDIATE, Text::MarqueeStopMode::FINISH_LOOP})
      {
        tet_printf("Stop fixture async=%d horizontal=%d mode=%d\n", async, horizontal, int(mode));
        auto label = MakeLabel(app);
        label.SetText(horizontal ? "MMMMMMMMMMMM" : "MMMM\nMMMM\nMMMM");
        label.SetMultiLine(!horizontal);
        label.SetRequestedWidth(80.f);
        label.SetRequestedHeight(40.f);
        label.SetMarqueeOrientation(horizontal ? Text::MarqueeOrientation::HORIZONTAL : Text::MarqueeOrientation::VERTICAL);
        label.SetMarqueeTriggerPolicy(Text::MarqueeTriggerPolicy::MANUAL);
        label.SetMarqueeLoopCount(0);
        label.SetMarqueeSpeed(100.f);
        label.SetMarqueeStopMode(mode);
        SetShadowStyle(label, 5.f);
        Enable(label, true);
        label.SetAsyncRendering(async);
        auto publish = [&]()
        {
          Flush(app);
          if(async)
          {
            DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
            Flush(app);
          }
        };
        publish();
        auto renderer = label.GetRendererAt(0);
        const auto staticSize = VisualSize(renderer);
        const auto staticOffset = Offset(renderer);
        for(unsigned cycle = 0; cycle < 2; ++cycle)
        {
          label.StartMarquee();
          publish();
          DALI_TEST_CHECK(label.IsMarqueeRunning());
          const auto movingShader = renderer.GetShader();
          const auto movingSize = VisualSize(renderer);
          label.StopMarquee();
          if(mode == Text::MarqueeStopMode::FINISH_LOOP)
          {
            DALI_TEST_CHECK(label.IsMarqueeRunning());
            DALI_TEST_CHECK(renderer.GetShader() == movingShader);
            DALI_TEST_EQUALS(VisualSize(renderer), movingSize, .001f, TEST_LOCATION);
            Flush(app);
            app.Render(60000);
            app.SendNotification();
          }
          publish();
          DALI_TEST_CHECK(!label.IsMarqueeRunning());
          DALI_TEST_CHECK(renderer.GetShader() != movingShader);
          DALI_TEST_EQUALS(VisualSize(renderer), staticSize, .001f, TEST_LOCATION);
          DALI_TEST_EQUALS(Offset(renderer), staticOffset, .001f, TEST_LOCATION);
          DALI_TEST_EQUALS(label.GetRendererCount(), 1u, TEST_LOCATION);
        }
        label.Unparent();
        Flush(app);
      }
  END_TEST;
}

int UtcDaliTextMarqueeStoppedAsyncCompletionP(void)
{
  UiTestApplication app;
  for(bool horizontal : {true, false})
    for(auto mode : {Text::MarqueeStopMode::IMMEDIATE, Text::MarqueeStopMode::FINISH_LOOP})
    {
      auto label = MakeLabel(app);
      label.SetText("First long line\nSecond long line\nThird line");
      label.SetMultiLine(!horizontal);
      label.SetRequestedWidth(80.f);
      label.SetRequestedHeight(40.f);
      label.SetMarqueeOrientation(horizontal ? Text::MarqueeOrientation::HORIZONTAL : Text::MarqueeOrientation::VERTICAL);
      label.SetMarqueeTriggerPolicy(Text::MarqueeTriggerPolicy::MANUAL);
      label.SetMarqueeStopMode(mode);
      label.SetMarqueeLoopCount(0);
      SetShadowStyle(label, 5.f);
      Enable(label, true);
      label.SetAsyncRendering(true);
      label.StartMarquee();
      Flush(app);
      DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
      Flush(app);
      DALI_TEST_CHECK(label.IsMarqueeRunning());
      const auto movingShader = label.GetRendererAt(0).GetShader();
      label.SetText("Changed source\nSecond changed line\nLast line");
      app.SendNotification();
      app.Render(); // Dispatch the request without delivering its completion.
      label.StopMarquee();
      DALI_TEST_CHECK(Test::WaitForEventThreadTrigger(1, 5));
      Flush(app);
      if(mode == Text::MarqueeStopMode::FINISH_LOOP)
      {
        DALI_TEST_CHECK(label.IsMarqueeRunning());
        DALI_TEST_CHECK(label.GetRendererAt(0).GetShader() == movingShader);
        app.Render(60000);
        app.SendNotification();
      }
      for(unsigned pending = 0; pending < 3 && !Advanced(label); ++pending)
      {
        Flush(app);
        Test::WaitForEventThreadTrigger(1, 5);
        Flush(app);
      }
      DALI_TEST_CHECK(!label.IsMarqueeRunning());
      DALI_TEST_CHECK(Advanced(label));
      DALI_TEST_CHECK(label.GetRendererAt(0).GetShader() != movingShader);
      const auto staticShader = label.GetRendererAt(0).GetShader();
      for(unsigned frame = 0; frame < 3; ++frame)
      {
        Test::WaitForEventThreadTrigger(1, 0);
        Flush(app);
        DALI_TEST_CHECK(label.GetRendererAt(0).GetShader() == staticShader);
      }
      label.Unparent();
      Flush(app);
    }
  END_TEST;
}
