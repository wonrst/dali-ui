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
#include <dali-ui-foundation/internal/text/async-text/text-loading-task.h>
#include <dali-ui-test-suite-utils.h>
#include <test-font-glyph-constants.h>
#include <dali/integration-api/pixel-data-integ.h>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

#include <dali-ui-foundation/integration-api/label-impl.h>
#include <dali-ui-foundation/internal/text/async-text/async-text-loader-impl.h>
#include <dali-ui-foundation/internal/text/controller/text-controller.h>
#include <dali-ui-foundation/internal/text/marquee/marquee-paint.h>
#include <dali-ui-foundation/internal/text/rendering/text-raster-coordinate.h>
#include <dali-ui-foundation/internal/text/rendering/text-raster-geometry.h>
#include <dali-ui-foundation/internal/text/rendering/text-typesetter-impl.h>
#include <dali-ui-foundation/internal/text/rendering/text-typesetter.h>
#include <dali-ui-foundation/internal/text/rendering/view-model.h>
#include <dali-ui-foundation/internal/text/text-model.h>

using namespace Dali;
using namespace Dali::Ui;

void utc_dali_text_raster_geometry_internal_startup(void)
{
  test_return_value = TET_UNDEF;
}
void utc_dali_text_raster_geometry_internal_cleanup(void)
{
  test_return_value = TET_PASS;
}

namespace
{
// Follow the existing internal UTC private-member access convention without
// changing production access levels or introducing an inspection API.
template<class Tag, auto member>
struct MemberAccess
{
  friend auto GetMember(Tag)
  {
    return member;
  }
};
struct TypesetterImpl
{
  friend auto GetMember(TypesetterImpl);
};
template struct MemberAccess<TypesetterImpl, &Text::Typesetter::mImpl>;
struct LabelController
{
  friend auto GetMember(LabelController);
};
struct LabelParameters
{
  friend auto GetMember(LabelParameters);
};
struct WorkerTypesetter
{
  friend auto GetMember(WorkerTypesetter);
};
struct WorkerModule
{
  friend auto GetMember(WorkerModule);
};
template struct MemberAccess<LabelController, &Ui::Integration::LabelImpl::mController>;
template struct MemberAccess<LabelParameters, &Ui::Integration::LabelImpl::GetAsyncTextParameters>;
template struct MemberAccess<WorkerTypesetter, &Text::Internal::AsyncTextLoader::mTypesetter>;
template struct MemberAccess<WorkerModule, &Text::Internal::AsyncTextLoader::mModule>;

struct TaskLoad
{
  friend auto GetMember(TaskLoad);
};
template struct MemberAccess<TaskLoad, &Ui::Internal::TextLoadingTask::Load>;

Text::AsyncTextRenderInfo LoadRequest(Text::AsyncTextLoader loader, Text::AsyncTextParameters params, Text::AsyncTextParameters* completedParameters = nullptr)
{
  Ui::Internal::TextLoadingTask task(1u, std::move(params), MakeCallback(+[]() {}));
  task.mLoader = loader;
  (task.*GetMember(TaskLoad{}))();
  if(completedParameters) *completedParameters = task.mParameters;
  task.mLoader.Reset();
  return task.mRenderInfo;
}

Text::ControllerPtr ControllerFor(Label label)
{
  return static_cast<Ui::Integration::LabelImpl&>(GetImpl(label)).*GetMember(LabelController{});
}

void Flush(UiTestApplication& app)
{
  for(unsigned i = 0; i < 3; ++i)
  {
    app.SendNotification();
    app.Render();
  }
}

bool Equal(const Text::Raster::Bounds& a, const Text::Raster::Bounds& b)
{
  return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

bool Equal(const Text::TextRasterGeometry& a, const Text::TextRasterGeometry& b)
{
  return Equal(a.fill, b.fill) && Equal(a.underlay, b.underlay) && Equal(a.overlay, b.overlay) && Equal(a.composition, b.composition);
}

std::vector<uint8_t> Bytes(PixelData data)
{
  if(!data) return {};
  const auto           buffer = Dali::Integration::GetPixelDataBuffer(data);
  const auto           row    = buffer.width * Pixel::GetBytesPerPixel(data.GetPixelFormat());
  const auto           stride = buffer.strideBytes ? buffer.strideBytes : row;
  std::vector<uint8_t> bytes;
  for(uint32_t y = 0; y < buffer.height; ++y)
  {
    bytes.insert(bytes.end(), buffer.buffer + y * stride, buffer.buffer + y * stride + row);
  }
  return bytes;
}

Text::TypesetterPtr TypesetterFor(Label label)
{
  const auto controller = ControllerFor(label);
  auto       typesetter = Text::Typesetter::New(controller->GetRenderTextModel());
  typesetter->SetFinalElisionResult(controller->GetFinalElisionResult());
  return typesetter;
}

Label MakeLabel(UiTestApplication& app, const char* text)
{
  Label label = Label::New();
  label.SetText(text);
  label.SetFontFamily("DejaVu Sans");
  label.SetFontSize(24.f);
  label.SetSystemFontSizeScaleEnabled(false);
  label.SetAsyncRendering(false);
  label.SetMultiLine(true);
  label.SetRequestedWidth(320.f);
  label.SetRequestedHeight(160.f);
  app.GetScene().Add(label);
  return label;
}

void CheckUnchanged(Label label, const Vector2& size)
{
  auto                                                   controller = ControllerFor(label);
  auto                                                   typesetter = TypesetterFor(label);
  const auto                                             natural    = label.GetNaturalSize();
  const auto                                             layout     = controller->GetRenderTextModel()->GetLayoutSize();
  const auto                                             renderers  = label.GetRendererCount();
  const auto*                                            model      = controller->GetRenderTextModel();
  std::vector<Vector2>                                   positions(model->GetLayout(), model->GetLayout() + model->GetNumberOfGlyphs());
  const std::array<Text::Typesetter::RenderBehaviour, 3> modes{Text::Typesetter::RENDER_NO_STYLES,
                                                               Text::Typesetter::RENDER_NO_TEXT, Text::Typesetter::RENDER_OVERLAY_STYLE};
  std::array<PixelData, 3>                               before;
  for(unsigned i = 0; i < modes.size(); ++i)
  {
    before[i] = typesetter->Render(size, controller->GetTextDirection(), modes[i]);
  }
  Text::TextRasterGeometry geometry;
  DALI_TEST_CHECK(typesetter->ResolveRasterGeometry(size, controller->GetTextDirection(), geometry));
  Text::Raster::BoundsAccumulator composition;
  composition.Include(geometry.fill);
  composition.Include(geometry.underlay);
  composition.Include(geometry.overlay);
  DALI_TEST_CHECK(Equal(composition.bounds, geometry.composition));
  const std::array<Text::Raster::Bounds, 3> bounds{geometry.fill, geometry.underlay, geometry.overlay};
  for(unsigned i = 0; i < modes.size(); ++i)
  {
    auto after = typesetter->Render(size, controller->GetTextDirection(), modes[i]);
    DALI_TEST_EQUALS(before[i].GetWidth(), after.GetWidth(), TEST_LOCATION);
    DALI_TEST_EQUALS(before[i].GetHeight(), after.GetHeight(), TEST_LOCATION);
    DALI_TEST_CHECK(Bytes(before[i]) == Bytes(after));
    const auto pixels        = Dali::Integration::GetPixelDataBuffer(before[i]);
    const auto stride        = pixels.strideBytes ? pixels.strideBytes : pixels.width * 4;
    bool       containsPaint = true;
    for(uint32_t y = 0; y < pixels.height; ++y)
    {
      for(uint32_t x = 0; x < pixels.width; ++x)
      {
        if(pixels.buffer[y * stride + x * 4 + 3])
        {
          containsPaint = containsPaint && static_cast<int64_t>(x) >= bounds[i].left &&
                          static_cast<int64_t>(x) < bounds[i].right && static_cast<int64_t>(y) >= bounds[i].top &&
                          static_cast<int64_t>(y) < bounds[i].bottom;
        }
      }
    }
    DALI_TEST_CHECK(containsPaint);
  }
  DALI_TEST_EQUALS(label.GetNaturalSize(), natural, TEST_LOCATION);
  DALI_TEST_EQUALS(model->GetLayoutSize(), layout, TEST_LOCATION);
  DALI_TEST_EQUALS(label.GetRendererCount(), renderers, TEST_LOCATION);
  for(unsigned i = 0; i < positions.size(); ++i)
  {
    DALI_TEST_EQUALS(model->GetLayout()[i], positions[i], TEST_LOCATION);
  }
}
} // namespace

int UtcDaliTextRasterGeometryBoundsP(void)
{
  Text::Raster::BoundsAccumulator bounds;
  DALI_TEST_CHECK(bounds.bounds.Empty());
  bounds.Include(0, 0, 100, 40);
  bounds.Include(-30, -20, 140, 90);
  bounds.Include(0, 30, 100, 70);
  DALI_TEST_CHECK(Equal(bounds.bounds, {-30, -20, 140, 90}));
  bounds.Include(100, 100, 100, 200); // Empty rectangles do not activate a plane.
  DALI_TEST_CHECK(Equal(bounds.bounds, {-30, -20, 140, 90}));
  bounds.Include(std::numeric_limits<int32_t>::min(), 0, std::numeric_limits<int32_t>::max(), 1);
  DALI_TEST_CHECK(bounds.valid);
  DALI_TEST_EQUALS(static_cast<int64_t>(bounds.bounds.right) - bounds.bounds.left, 4294967295LL, TEST_LOCATION);
  bounds.Include(static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - 1, 0, 1, 1);
  DALI_TEST_CHECK(!bounds.valid);
  END_TEST;
}

int UtcDaliTextRasterGeometryQuantizationP(void)
{
  int32_t x, y;
  DALI_TEST_CHECK(Text::Raster::ResolveGlyphPosition(-0.5f, -1.75f, 10, 10, 4, 8, 1, x, y));
  DALI_TEST_EQUALS(x, 10, TEST_LOCATION);
  DALI_TEST_EQUALS(y, 9, TEST_LOCATION);
  DALI_TEST_CHECK(!Text::Raster::ResolveGlyphPosition(std::numeric_limits<float>::infinity(), 0, 0, 0, 1, 1, 1, x, y));
  DALI_TEST_CHECK(!Text::Raster::ResolveGlyphPosition(2147483648.f, 0, 0, 0, 1, 1, 1, x, y));
  Text::Raster::GlyphClip clip;
  DALI_TEST_CHECK(Text::Raster::ClipGlyph(-0.5f, -1.75f, 10, 10, 4, 8, 1, 100, 100, clip));
  DALI_TEST_EQUALS(clip.x, 10, TEST_LOCATION);
  DALI_TEST_EQUALS(clip.y, 9, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRasterGeometryLegacyEffectsP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  const Vector2 size(320, 160);
  for(const char* text : {"Hello", "Hello\nworld", "مرحبا بالعالم", "Hello مرحبا ffi Á", "jÁf Áģ ffi fl", "😀 Hello"})
  {
    auto label = MakeLabel(app, text);
    for(float sign : {-1.f, 1.f})
    {
      for(float radius : {0.f, 0.25f, 4.f, 30.f})
      {
        Text::Shadow shadow;
        shadow.SetOffset(Vector2(sign * 25, -sign * 20));
        shadow.SetBlurRadius(radius);
        shadow.SetColor(UiColor(Color::BLACK));
        label.SetTextShadow(shadow);
        Text::Outline outline;
        outline.SetWidth(12);
        outline.SetOffset(Vector2(-sign * 20, sign * 10));
        outline.SetBlurRadius(radius);
        outline.SetColor(UiColor(Color::BLACK));
        label.SetTextOutline(outline);
        Flush(app);
        CheckUnchanged(label, size);
      }
    }
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRasterGeometryDecorationsP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  for(auto type : {Text::Underline::Type::SOLID, Text::Underline::Type::DASHED, Text::Underline::Type::DOUBLE})
  {
    auto label = MakeLabel(app, "Hello\nworld");
    for(float thickness : {2.f, 25.f, 80.f})
    {
      Text::Underline underline;
      underline.SetType(type);
      underline.SetThickness(thickness);
      underline.SetDashLength(4.25f);
      underline.SetDashGap(2.5f);
      label.SetTextUnderline(underline);
      Text::LineThrough strike;
      strike.SetThickness(thickness);
      label.SetTextLineThrough(strike);
      Flush(app);
      CheckUnchanged(label, Vector2(320, 160));
      auto                     typesetter = TypesetterFor(label);
      Text::TextRasterGeometry geometry;
      DALI_TEST_CHECK(typesetter->ResolveRasterGeometry(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), geometry));
      DALI_TEST_CHECK(!geometry.overlay.Empty());
      DALI_TEST_CHECK(geometry.underlay.Empty());
    }
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRasterGeometryFinalModelP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  // Markup includes range background and overlapping decorations. ImageSpan's
  // synthetic glyph is layout-only; use the final replacement processing model.
  for(unsigned variant = 0; variant < 4; ++variant)
  {
    auto label = MakeLabel(app, "Hello ffi مرحبا long text that must fit or elide");
    if(variant == 0)
    {
      label.SetTextFit(Text::Fit::Range(8, 24, 1));
    }
    if(variant == 1)
    {
      ControllerFor(label)->SetTextElideEnabled(true);
    }
    if(variant == 2)
    {
      label.SetStyledText(Text::StyledText::FromMarkup("<background color='red'>AB</background><u height='25'><s height='8'>CD</s></u>"));
    }
    if(variant == 3)
    {
      auto            builder = Text::StyledTextBuilder::New("plain decorated image plain");
      Text::Underline underline;
      underline.SetThickness(25);
      builder.SetSpan(Text::UnderlineSpan::New(underline), 6u, 21u);
      Text::LineThrough strike;
      strike.SetThickness(8);
      builder.SetSpan(Text::LineThroughSpan::New(strike), 10u, 21u);
      Text::ImageAttributes image("missing-raster-geometry.png", Vector2(40, 30));
      builder.SetSpan(Text::ImageSpan::New(image), 16u, 21u);
      label.SetStyledText(builder.Build());
    }
    label.SetRequestedWidth(140.f);
    label.SetRequestedHeight(60.f);
    Flush(app);
    CheckUnchanged(label, Vector2(140, 60));
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRasterGeometryWorkerP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto  loader = Text::AsyncTextLoader::New();
  auto& worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  for(bool ellipsis : {false, true})
  {
    auto label = MakeLabel(app, "Hello مرحبا ffi Á a long line for final ellipsis");
    ControllerFor(label)->SetTextElideEnabled(ellipsis);
    Text::Shadow shadow;
    shadow.SetOffset(Vector2(-30, 25));
    shadow.SetBlurRadius(30);
    label.SetTextShadow(shadow);
    Flush(app);
    const auto controller     = ControllerFor(label);
    auto&      impl           = static_cast<Ui::Integration::LabelImpl&>(GetImpl(label));
    auto       parameters     = (impl.*GetMember(LabelParameters{}))(Ui::Integration::Text::Async::RENDER_FIXED_SIZE,
                                                           Vector2(320, 160), Insets{}, Dali::LayoutDirection::LEFT_TO_RIGHT);
    parameters.maxTextureSize = 4096;
    parameters.ellipsis       = ellipsis;
    loader.RenderText(parameters, false, Size::ZERO);
    auto                     sync  = TypesetterFor(label);
    auto                     async = worker.*GetMember(WorkerTypesetter{});
    Text::TextRasterGeometry a, b;
    DALI_TEST_CHECK(sync->ResolveRasterGeometry(Vector2(320, 160), controller->GetTextDirection(), a));
    DALI_TEST_CHECK(async->ResolveRasterGeometry(Vector2(320, 160), controller->GetTextDirection(), b));
    DALI_TEST_CHECK(Equal(a, b));
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRasterGeometryBackgroundAndReuseP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto label = MakeLabel(app, "Hello\nworld");
  label.SetTextBackgroundColor(Color::RED);
  Flush(app);
  const Vector2 size(320, 160);
  const auto    resolve = [&](Text::TextRasterGeometry& geometry)
  {
    auto typesetter = TypesetterFor(label);
    return typesetter->ResolveRasterGeometry(size, ControllerFor(label)->GetTextDirection(), geometry);
  };
  Text::TextRasterGeometry background, both, shadowOnly, empty;
  DALI_TEST_CHECK(resolve(background));
  DALI_TEST_CHECK(!background.underlay.Empty());
  Text::Shadow shadow;
  shadow.SetOffset(Vector2(-100, -80));
  shadow.SetBlurRadius(30);
  label.SetTextShadow(shadow);
  Flush(app);
  DALI_TEST_CHECK(resolve(both));
  label.ClearTextBackgroundColor();
  Flush(app);
  DALI_TEST_CHECK(resolve(shadowOnly));
  Text::Raster::BoundsAccumulator expected;
  expected.Include(background.underlay);
  expected.Include(shadowOnly.underlay);
  DALI_TEST_CHECK(Equal(both.underlay, expected.bounds));
  DALI_TEST_CHECK(Equal(background.fill, both.fill));
  DALI_TEST_CHECK(both.underlay.left < background.underlay.left);
  label.SetTextShadow(Text::Shadow::None());
  label.SetText("");
  Flush(app);
  DALI_TEST_CHECK(resolve(empty));
  DALI_TEST_CHECK(empty.fill.Empty() && empty.underlay.Empty() && empty.overlay.Empty() && empty.composition.Empty());
  END_TEST;
}

int UtcDaliTextRasterGeometrySyntheticFontP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto label = MakeLabel(app, "jÁf ffi Á");
  ControllerFor(label)->SetTextElideEnabled(false);
  Flush(app);
  const auto* model = static_cast<const Text::Model*>(ControllerFor(label)->GetRenderTextModel());
  // Exercise the FontClient synthesis flags on the regular face, independently
  // of whether this machine has a matching bold/italic face installed.
  auto                     typesetter = Text::Typesetter::New(model);
  Text::TextRasterGeometry regular, synthetic;
  DALI_TEST_CHECK(typesetter->ResolveRasterGeometry(Vector2(320, 160), Text::Direction::LEFT_TO_RIGHT, regular));
  for(auto& glyph : model->mVisualModel->mGlyphs)
  {
    glyph.isItalicRequired = true;
    glyph.isBoldRequired   = true;
  }
  typesetter->SetModel(model);
  const auto before = typesetter->Render(Vector2(320, 160), Text::Direction::LEFT_TO_RIGHT, Text::Typesetter::RENDER_NO_STYLES);
  DALI_TEST_CHECK(typesetter->ResolveRasterGeometry(Vector2(320, 160), Text::Direction::LEFT_TO_RIGHT, synthetic));
  const auto after = typesetter->Render(Vector2(320, 160), Text::Direction::LEFT_TO_RIGHT, Text::Typesetter::RENDER_NO_STYLES);
  DALI_TEST_CHECK(Bytes(before) == Bytes(after));
  DALI_TEST_CHECK(!synthetic.fill.Empty());
  DALI_TEST_CHECK(synthetic.fill.right >= regular.fill.right);
  // Fractional positions and synthesis must survive an integer-only origin shift.
  for(auto& position : model->mVisualModel->mGlyphPositions)
  {
    position.x -= 0.5f;
  }
  typesetter->SetModel(model);
  Text::TextRasterResult planes;
  DALI_TEST_CHECK(typesetter->RenderRasterPlanes(Vector2(320, 160), Text::Direction::LEFT_TO_RIGHT, Pixel::RGBA8888, 4096, planes));
  const auto  legacy   = typesetter->Render(Vector2(320, 160), Text::Direction::LEFT_TO_RIGHT, Text::Typesetter::RENDER_NO_STYLES);
  const auto  expected = Bytes(legacy), actual = Bytes(planes.fill);
  const auto& bounds = planes.geometry.fill;
  bool        equal  = true;
  for(uint32_t y = 0; y < legacy.GetHeight(); ++y)
  {
    for(uint32_t x = 0; x < legacy.GetWidth(); ++x)
    {
      for(unsigned channel = 0; channel < 4; ++channel)
      {
        const int64_t localX = static_cast<int64_t>(x) - bounds.left, localY = static_cast<int64_t>(y) - bounds.top;
        const uint8_t pixel = localX >= 0 && localY >= 0 && localX < planes.fill.GetWidth() && localY < planes.fill.GetHeight() ? actual[(localY * planes.fill.GetWidth() + localX) * 4 + channel] : 0;
        equal               = equal && pixel == expected[(y * legacy.GetWidth() + x) * 4 + channel];
      }
    }
  }
  DALI_TEST_CHECK(equal);
  const float savedX                        = model->mVisualModel->mGlyphPositions[0].x;
  model->mVisualModel->mGlyphPositions[0].x = -1.0e30f;
  typesetter->SetModel(model);
  DALI_TEST_CHECK(!typesetter->RenderRasterPlanes(Vector2(320, 160), Text::Direction::LEFT_TO_RIGHT, Pixel::RGBA8888, 4096, planes));
  DALI_TEST_CHECK(!planes.fill && !planes.underlay && !planes.overlay);
  model->mVisualModel->mGlyphPositions[0].x = savedX;
  END_TEST;
}

namespace
{
uint32_t Over(uint32_t top, uint32_t bottom)
{
  const uint8_t alpha    = reinterpret_cast<uint8_t*>(&top)[3];
  auto*         channels = reinterpret_cast<uint8_t*>(&bottom);
  for(unsigned i = 0; i < 4; ++i)
  {
    const uint32_t xy = static_cast<uint32_t>(channels[i]) * (255u - alpha);
    channels[i]       = ((xy << 15) + (xy << 7) + xy) >> 23;
  }
  return top + bottom;
}

PixelBuffer Compose(const Text::TextRasterResult& result)
{
  const auto& bounds = result.geometry.composition;
  if(bounds.Empty())
  {
    return {};
  }
  auto  canvas = PixelBuffer::New(bounds.right - bounds.left, bounds.bottom - bounds.top, Pixel::RGBA8888);
  auto* target = reinterpret_cast<uint32_t*>(canvas.GetBuffer());
  std::fill(target, target + canvas.GetWidth() * canvas.GetHeight(), 0u);
  const std::array<PixelData, 3>            pixels{result.underlay, result.fill, result.overlay};
  const std::array<Text::Raster::Bounds, 3> planes{result.geometry.underlay, result.geometry.fill, result.geometry.overlay};
  for(unsigned p = 0; p < 3; ++p)
  {
    if(!pixels[p])
    {
      continue;
    }
    const auto     src    = Dali::Integration::GetPixelDataBuffer(pixels[p]);
    const uint32_t stride = src.strideBytes ? src.strideBytes : src.width * 4;
    for(uint32_t y = 0; y < src.height; ++y)
    {
      for(uint32_t x = 0; x < src.width; ++x)
      {
        uint32_t pixel;
        memcpy(&pixel, src.buffer + y * stride + x * 4, 4);
        const size_t index = (y + static_cast<int64_t>(planes[p].top) - bounds.top) * canvas.GetWidth() + x + static_cast<int64_t>(planes[p].left) - bounds.left;
        target[index]      = Over(pixel, target[index]);
      }
    }
  }
  return canvas;
}

void CheckPlane(PixelData data, const Text::Raster::Bounds& bounds)
{
  DALI_TEST_EQUALS(static_cast<bool>(data), !bounds.Empty(), TEST_LOCATION);
  if(data)
  {
    DALI_TEST_EQUALS(data.GetWidth(), static_cast<uint32_t>(static_cast<int64_t>(bounds.right) - bounds.left), TEST_LOCATION);
    DALI_TEST_EQUALS(data.GetHeight(), static_cast<uint32_t>(static_cast<int64_t>(bounds.bottom) - bounds.top), TEST_LOCATION);
  }
}

bool SamePlanes(const Text::TextRasterResult& a, const Text::TextRasterResult& b)
{
  if(!Equal(a.geometry, b.geometry))
  {
    return false;
  }
  const std::array<PixelData, 3> ap{a.fill, a.underlay, a.overlay}, bp{b.fill, b.underlay, b.overlay};
  for(unsigned i = 0; i < 3; ++i)
  {
    if(bool(ap[i]) != bool(bp[i]) || (ap[i] && Bytes(ap[i]) != Bytes(bp[i])))
    {
      return false;
    }
  }
  return true;
}
} // namespace

int UtcDaliTextRasterPlanesCommonReferenceP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  for(const char* text : {"Hello", "Hello\nworld\nHello", "مرحبا بالعالم", "Hello مرحبا ffi Á", "jÁf", "😀 Hello"})
  {
    auto         label = MakeLabel(app, text);
    Text::Shadow shadow;
    shadow.SetOffset(Vector2(-30, 25));
    shadow.SetBlurRadius(30);
    shadow.SetColor(Color::BLUE);
    label.SetTextShadow(shadow);
    Text::Outline outline;
    outline.SetWidth(12);
    outline.SetOffset(Vector2(20, -25));
    outline.SetBlurRadius(20);
    outline.SetColor(Color::RED);
    label.SetTextOutline(outline);
    Text::Underline underline;
    underline.SetThickness(25);
    underline.SetType(Text::Underline::Type::DOUBLE);
    underline.SetColor(Color::GREEN);
    label.SetTextUnderline(underline);
    Flush(app);
    auto                   controller = ControllerFor(label);
    auto                   ts         = TypesetterFor(label);
    Text::TextRasterResult result;
    DALI_TEST_CHECK(ts->RenderRasterPlanes(Vector2(320, 160), controller->GetTextDirection(), Pixel::RGBA8888, 4096, result));
    CheckPlane(result.fill, result.geometry.fill);
    CheckPlane(result.underlay, result.geometry.underlay);
    CheckPlane(result.overlay, result.geometry.overlay);
    auto& impl = *((*ts).*GetMember(TypesetterImpl{}));
    auto& view = *impl.GetViewModel();
    // Independent reference uses the existing legacy draw entry, with ample
    // common transparent padding. Integer model positions avoid requantization.
    const int      pad = 256;
    const uint32_t w = 832, h = 672;
    auto           fill = impl.CreateImageBuffer(w, h, Text::Typesetter::STYLE_NONE, false, Pixel::RGBA8888, pad, pad, view.GetStartIndexOfElidedGlyphs(), view.GetEndIndexOfElidedGlyphs());
    auto           out  = impl.CreateImageBuffer(w, h, Text::Typesetter::STYLE_OUTLINE, false, Pixel::RGBA8888, pad, pad, view.GetStartIndexOfElidedGlyphs(), view.GetEndIndexOfElidedGlyphs());
    auto           sh   = impl.CreateImageBuffer(w, h, Text::Typesetter::STYLE_SHADOW, false, Pixel::RGBA8888, pad, pad, view.GetStartIndexOfElidedGlyphs(), view.GetEndIndexOfElidedGlyphs());
    auto           ul   = impl.CreateImageBuffer(w, h, Text::Typesetter::STYLE_UNDERLINE, false, Pixel::RGBA8888, pad, pad, view.GetStartIndexOfElidedGlyphs(), view.GetEndIndexOfElidedGlyphs());
    DALI_TEST_CHECK(out.ApplyGaussianBlur(20));
    DALI_TEST_CHECK(sh.ApplyGaussianBlur(30));
    auto        actual = Compose(result);
    const auto& b      = result.geometry.composition;
    bool        equal  = true;
    const auto* f      = reinterpret_cast<const uint32_t*>(fill.GetBuffer());
    const auto* o      = reinterpret_cast<const uint32_t*>(out.GetBuffer());
    const auto* u      = reinterpret_cast<const uint32_t*>(ul.GetBuffer());
    const auto* ss     = reinterpret_cast<const uint32_t*>(sh.GetBuffer());
    const auto* a      = reinterpret_cast<const uint32_t*>(actual.GetBuffer());
    for(uint32_t y = 0; y < actual.GetHeight(); ++y)
    {
      for(uint32_t x = 0; x < actual.GetWidth(); ++x)
      {
        const size_t index = (y + b.top + pad) * w + x + b.left + pad;
        equal              = equal && a[y * actual.GetWidth() + x] == Over(u[index], Over(f[index], Over(o[index], ss[index])));
      }
    }
    DALI_TEST_CHECK(equal);
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesIndependentP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto label = MakeLabel(app, "Hello");
  Flush(app);
  auto                   controller = ControllerFor(label);
  Text::TextRasterResult plain, shadowed, underlined;
  DALI_TEST_CHECK(TypesetterFor(label)->RenderRasterPlanes(Vector2(320, 160), controller->GetTextDirection(), Pixel::L8, 4096, plain));
  DALI_TEST_CHECK(plain.fill && !plain.underlay && !plain.overlay);
  DALI_TEST_EQUALS(plain.fill.GetPixelFormat(), Pixel::L8, TEST_LOCATION);
  Text::Shadow shadow;
  shadow.SetOffset(Vector2(-30, 25));
  shadow.SetBlurRadius(128);
  label.SetTextShadow(shadow);
  Flush(app);
  DALI_TEST_CHECK(TypesetterFor(label)->RenderRasterPlanes(Vector2(320, 160), controller->GetTextDirection(), Pixel::L8, 4096, shadowed));
  DALI_TEST_CHECK(Equal(plain.geometry.fill, shadowed.geometry.fill));
  DALI_TEST_CHECK(Bytes(plain.fill) == Bytes(shadowed.fill));
  DALI_TEST_CHECK(!shadowed.overlay);
  Text::Underline underline;
  underline.SetThickness(80);
  label.SetTextUnderline(underline);
  Flush(app);
  DALI_TEST_CHECK(TypesetterFor(label)->RenderRasterPlanes(Vector2(320, 160), controller->GetTextDirection(), Pixel::L8, 4096, underlined));
  DALI_TEST_CHECK(Bytes(plain.fill) == Bytes(underlined.fill));
  DALI_TEST_CHECK(Bytes(shadowed.underlay) == Bytes(underlined.underlay));
  CheckPlane(underlined.overlay, underlined.geometry.overlay);
  END_TEST;
}

int UtcDaliTextRasterPlanesFinalModelP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto  loader = Text::AsyncTextLoader::New();
  auto& worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  for(unsigned variant = 0; variant < 5; ++variant)
  {
    auto label = MakeLabel(app, "Hello مرحبا ffi Á a long line for final ellipsis");
    label.SetRequestedWidth(140);
    label.SetRequestedHeight(60);
    if(variant == 0)
    {
      label.SetTextFit(Text::Fit::Range(8, 24, 1));
    }
    if(variant == 1)
    {
      ControllerFor(label)->SetTextElideEnabled(true);
    }
    if(variant == 2)
    {
      label.SetStyledText(Text::StyledText::FromMarkup("<background color='red'>AB</background><u type='dashed' height='25' dash-width='4.25' dash-gap='2.5'><s height='8'>CD</s></u>"));
    }
    if(variant == 3)
    {
      auto            builder = Text::StyledTextBuilder::New("plain decorated image plain");
      Text::Underline u;
      u.SetThickness(25);
      builder.SetSpan(Text::UnderlineSpan::New(u), 6, 21);
      Text::LineThrough l;
      l.SetThickness(8);
      builder.SetSpan(Text::LineThroughSpan::New(l), 10, 21);
      Text::ImageAttributes image("missing-raster-plane.png", Vector2(40, 30));
      builder.SetSpan(Text::ImageSpan::New(image), 16, 21);
      label.SetStyledText(builder.Build());
    }
    if(variant == 4)
    {
      label.SetStyledText(Text::StyledText::FromMarkup("<color value='red'>AB</color><color value='#00ff0080'>CD</color>😀"));
    }
    Flush(app);
    auto                   controller = ControllerFor(label);
    auto                   ts         = TypesetterFor(label);
    Text::TextRasterResult sync, async;
    DALI_TEST_CHECK(ts->RenderRasterPlanes(Vector2(140, 60), controller->GetTextDirection(), Pixel::RGBA8888, 4096, sync));
    auto& impl            = static_cast<Ui::Integration::LabelImpl&>(GetImpl(label));
    auto  params          = (impl.*GetMember(LabelParameters{}))(Ui::Integration::Text::Async::RENDER_FIXED_SIZE, Vector2(140, 60), Insets{}, Dali::LayoutDirection::LEFT_TO_RIGHT);
    params.maxTextureSize = 4096;
    params.ellipsis       = controller->IsTextElideEnabled();
    if(variant == 0)
    {
      loader.RenderTextFit(params, false, Size::ZERO);
    }
    else
    {
      loader.RenderText(params, false, Size::ZERO);
    }
    auto wt = worker.*GetMember(WorkerTypesetter{});
    DALI_TEST_CHECK(wt->RenderRasterPlanes(Vector2(140, 60), controller->GetTextDirection(), Pixel::RGBA8888, 4096, async));
    DALI_TEST_CHECK(SamePlanes(sync, async));
    CheckUnchanged(label, Vector2(140, 60));
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesFailureP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto label = MakeLabel(app, "Hello");
  Flush(app);
  auto                   ts  = TypesetterFor(label);
  auto                   dir = ControllerFor(label)->GetTextDirection();
  Text::TextRasterResult result;
  DALI_TEST_CHECK(ts->RenderRasterPlanes(Vector2(320, 160), dir, Pixel::RGBA8888, 4096, result));
  DALI_TEST_CHECK(!ts->RenderRasterPlanes(Vector2(320, 160), dir, Pixel::RGBA8888, 8, result));
  DALI_TEST_CHECK(!result.fill && !result.underlay && !result.overlay && result.geometry.composition.Empty());
  DALI_TEST_CHECK(!ts->RenderRasterPlanes(Vector2(std::numeric_limits<float>::infinity(), 160), dir, Pixel::RGBA8888, 4096, result));
  DALI_TEST_CHECK(!ts->RenderRasterPlanes(Vector2(65536, 65536), dir, Pixel::RGBA8888, 4096, result));
  DALI_TEST_CHECK(!ts->RenderRasterPlanes(Vector2(320, 160), dir, Pixel::RGB888, 4096, result));
  Text::Shadow shadow;
  shadow.SetOffset(Vector2(-1000000, 0));
  label.SetTextShadow(shadow);
  // Relayout is sufficient; never send the pathological style to legacy raster.
  ControllerFor(label)->Relayout(Vector2(320, 160), Dali::LayoutDirection::LEFT_TO_RIGHT);
  ts = TypesetterFor(label);
  DALI_TEST_CHECK(!ts->RenderRasterPlanes(Vector2(320, 160), dir, Pixel::RGBA8888, 4096, result));
  shadow.SetOffset(Vector2(1, 1));
  shadow.SetBlurRadius(50000);
  label.SetTextShadow(shadow);
  ControllerFor(label)->Relayout(Vector2(320, 160), Dali::LayoutDirection::LEFT_TO_RIGHT);
  ts = TypesetterFor(label);
  DALI_TEST_CHECK(!ts->RenderRasterPlanes(Vector2(320, 160), dir, Pixel::RGBA8888, 4096, result));
  label.SetTextShadow(Text::Shadow::None());
  Text::Outline outline;
  outline.SetWidth(std::numeric_limits<uint16_t>::max());
  label.SetTextOutline(outline);
  ControllerFor(label)->Relayout(Vector2(320, 160), Dali::LayoutDirection::LEFT_TO_RIGHT);
  ts = TypesetterFor(label);
  DALI_TEST_CHECK(!ts->RenderRasterPlanes(Vector2(320, 160), dir, Pixel::RGBA8888, 4096, result));
  label.SetTextOutline(Text::Outline::None());
  label.SetText("");
  label.SetTextShadow(Text::Shadow::None());
  Flush(app);
  ts = TypesetterFor(label);
  DALI_TEST_CHECK(ts->RenderRasterPlanes(Vector2(320, 160), dir, Pixel::RGBA8888, 4096, result));
  DALI_TEST_CHECK(!result.fill && !result.underlay && !result.overlay);
  END_TEST;
}

int UtcDaliTextRasterPlanesRangesP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  for(bool background : {false, true})
  {
    auto label = MakeLabel(app, "unused");
    label.SetStyledText(Text::StyledText::FromMarkup("<background color='#ff000080'>AB</background><u type='dashed' height='25' dash-width='4.25' dash-gap='2.5' color='#00ff0080'><s height='8' color='#0000ff80'>CD</s></u>EF"));
    if(background)
    {
      label.SetTextBackgroundColor(Color::YELLOW);
    }
    Text::Shadow shadow;
    shadow.SetOffset(Vector2(-30, 25));
    shadow.SetBlurRadius(30);
    label.SetTextShadow(shadow);
    ControllerFor(label)->SetTextElideEnabled(false);
    Flush(app);
    const auto* model = static_cast<const Text::Model*>(ControllerFor(label)->GetRenderTextModel());
    // This padded legacy oracle needs integral final metrics: adding padding
    // before truncating a negative fraction would change the reference itself.
    // The synthetic-font case separately tests negative fractional positions.
    for(auto& line : model->mVisualModel->mLines)
    {
      line.ascender  = std::round(line.ascender);
      line.descender = std::round(line.descender);
    }
    for(auto& glyph : model->mVisualModel->mGlyphs)
    {
      glyph.xBearing = std::round(glyph.xBearing);
      glyph.yBearing = std::round(glyph.yBearing);
      glyph.advance  = std::round(glyph.advance);
    }
    for(auto& position : model->mVisualModel->mGlyphPositions)
    {
      position.x = std::round(position.x);
      position.y = std::round(position.y);
    }
    auto                   ts = Text::Typesetter::New(model);
    Text::TextRasterResult result;
    DALI_TEST_CHECK(ts->RenderRasterPlanes(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, result));
    auto&          impl  = *((*ts).*GetMember(TypesetterImpl{}));
    auto&          view  = *impl.GetViewModel();
    const int      pad   = 128;
    const uint32_t width = 576, height = 416;
    const auto     draw = [&](Text::Typesetter::Style style, Text::GlyphIndex first, Text::GlyphIndex last)
    {
      return impl.CreateImageBuffer(width, height, style, false, Pixel::RGBA8888, pad, pad, first, last);
    };
    const auto first = view.GetStartIndexOfElidedGlyphs(), last = view.GetEndIndexOfElidedGlyphs();
    auto       fill     = draw(Text::Typesetter::STYLE_NONE, first, last);
    auto       underlay = draw(Text::Typesetter::STYLE_SHADOW, first, last);
    DALI_TEST_CHECK(underlay.ApplyGaussianBlur(30));
    auto bg = background ? draw(Text::Typesetter::STYLE_BACKGROUND, first, last) : impl.CreateTransparentImageBuffer(width, height, Pixel::RGBA8888);
    impl.DrawGlyphsBackground(bg, width, height, false, pad, pad);
    auto       overlay = impl.CreateTransparentImageBuffer(width, height, Pixel::RGBA8888);
    const auto behind  = [&](PixelBuffer& top, PixelBuffer bottom)
    {
      auto*       a = reinterpret_cast<uint32_t*>(top.GetBuffer());
      const auto* b = reinterpret_cast<const uint32_t*>(bottom.GetBuffer());
      for(uint32_t i = 0; i < width * height; ++i)
      {
        a[i] = Over(a[i], b[i]);
      }
    };
    behind(underlay, bg);
    Vector<Text::UnderlinedGlyphRun> underlines;
    underlines.Resize(view.GetNumberOfUnderlineRuns());
    view.GetUnderlineRuns(underlines.Begin(), 0u, underlines.Count());
    for(const auto& run : underlines)
    {
      behind(overlay, draw(Text::Typesetter::STYLE_UNDERLINE, run.glyphRun.glyphIndex, run.glyphRun.glyphIndex + run.glyphRun.numberOfGlyphs - 1u));
    }
    Vector<Text::StrikethroughGlyphRun> strikes;
    strikes.Resize(view.GetNumberOfStrikethroughRuns());
    view.GetStrikethroughRuns(strikes.Begin(), 0u, strikes.Count());
    for(const auto& run : strikes)
    {
      behind(overlay, draw(Text::Typesetter::STYLE_STRIKETHROUGH, run.glyphRun.glyphIndex, run.glyphRun.glyphIndex + run.glyphRun.numberOfGlyphs - 1u));
    }
    behind(fill, underlay);
    behind(overlay, fill);
    auto        actual   = Compose(result);
    const auto* expected = reinterpret_cast<const uint32_t*>(overlay.GetBuffer());
    const auto* pixels   = reinterpret_cast<const uint32_t*>(actual.GetBuffer());
    bool        equal    = true;
    for(uint32_t y = 0; y < actual.GetHeight(); ++y)
    {
      for(uint32_t x = 0; x < actual.GetWidth(); ++x)
      {
        const auto&  b     = result.geometry.composition;
        const size_t index = (static_cast<int64_t>(y) + b.top + pad) * width + x + b.left + pad;
        equal              = equal && pixels[y * actual.GetWidth() + x] == expected[index];
      }
    }
    DALI_TEST_CHECK(equal);
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesCompositionSafetyP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app, "Hello");
  Flush(app);
  auto               ts   = TypesetterFor(label);
  auto&              impl = *((*ts).*GetMember(TypesetterImpl{}));
  Text::RasterTarget destination{PixelBuffer::New(4, 3, Pixel::RGBA8888), {-2, -1, 2, 2}, true};
  Text::RasterTarget source{PixelBuffer::New(2, 1, Pixel::RGBA8888), {-1, 0, 1, 1}, true};
  memset(destination.buffer.GetBuffer(), 0x33, destination.buffer.GetBufferSize());
  memset(source.buffer.GetBuffer(), 0x77, source.buffer.GetBufferSize());
  const std::vector<uint8_t> before(destination.buffer.GetBuffer(), destination.buffer.GetBuffer() + destination.buffer.GetBufferSize());
  auto                       invalid = source;
  invalid.bounds                     = {-3, 0, -1, 1};
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));
  invalid.bounds = {1, 0, 3, 1};
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));
  invalid.bounds = {-1, -2, 1, -1};
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));
  invalid.bounds = {-1, 2, 1, 3};
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));
  invalid = source;
  invalid.bounds.right++;
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));
  invalid        = source;
  invalid.bounds = {std::numeric_limits<int32_t>::min(), 0, std::numeric_limits<int32_t>::max(), 1};
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));
  invalid        = source;
  invalid.buffer = PixelBuffer::New(2, 1, Pixel::L8);
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));
  invalid        = source;
  invalid.buffer = {};
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));
  invalid       = source;
  invalid.valid = false;
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));
  DALI_TEST_CHECK(std::equal(before.begin(), before.end(), destination.buffer.GetBuffer()));
  auto invalidDestination = destination;
  invalidDestination.bounds.right++;
  DALI_TEST_CHECK(!impl.CompositeBehind(invalidDestination, source));
  invalidDestination        = destination;
  invalidDestination.buffer = {};
  DALI_TEST_CHECK(!impl.CompositeBehind(invalidDestination, source));
  Text::RasterTarget empty;
  DALI_TEST_CHECK(impl.CompositeBehind(destination, empty));
  // Retain a valid handle after Convert(), but with no backing allocation.
  invalid        = source;
  invalid.buffer = PixelBuffer::New(2, 1, Pixel::RGBA8888);
  auto handle    = invalid.buffer;
  auto consumed  = PixelBuffer::Convert(handle);
  DALI_TEST_CHECK(invalid.buffer && !invalid.buffer.GetBuffer());
  DALI_TEST_CHECK(!impl.CompositeBehind(destination, invalid));

  // For premultiplied input, top.rgb <= top.a and scaled bottom.rgb <= 255-top.a.
  // Therefore every channel sum is <= 255: packed addition cannot carry.
  bool equal = true;
  for(unsigned topAlpha = 0; topAlpha < 256; ++topAlpha)
    for(unsigned bottomAlpha = 0; bottomAlpha < 256; ++bottomAlpha)
    {
      const std::array<uint8_t, 4> top{static_cast<uint8_t>(topAlpha), static_cast<uint8_t>(topAlpha / 2), static_cast<uint8_t>(topAlpha ? topAlpha - 1 : 0), static_cast<uint8_t>(topAlpha)};
      const std::array<uint8_t, 4> bottom{static_cast<uint8_t>(bottomAlpha), static_cast<uint8_t>(bottomAlpha ? bottomAlpha - 1 : 0), static_cast<uint8_t>(bottomAlpha / 2), static_cast<uint8_t>(bottomAlpha)};
      for(unsigned i = 0; i < 12; ++i) memcpy(destination.buffer.GetBuffer() + i * 4, top.data(), 4);
      for(unsigned i = 0; i < 2; ++i) memcpy(source.buffer.GetBuffer() + i * 4, bottom.data(), 4);
      equal = equal && impl.CompositeBehind(destination, source);
      for(unsigned pixel = 0; pixel < 12; ++pixel)
        for(unsigned channel = 0; channel < 4; ++channel)
        {
          const uint8_t expected = top[channel] + ((pixel == 5 || pixel == 6) ? static_cast<unsigned>(bottom[channel]) * (255 - topAlpha) / 255 : 0);
          equal                  = equal && destination.buffer.GetBuffer()[pixel * 4 + channel] == expected;
        }
    }
  DALI_TEST_CHECK(equal);
  END_TEST;
}

int UtcDaliTextRasterGaussianSupportContractP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto label = MakeLabel(app, "Hello");
  for(float radius : {0.5f, 1.25f, 4.5f, 12.0f})
  {
    const int    halo = static_cast<int>(std::ceil(radius));
    Text::Shadow shadow;
    shadow.SetOffset(Vector2(1, -1));
    shadow.SetBlurRadius(radius);
    label.SetTextShadow(shadow);
    Flush(app);
    Text::TextRasterGeometry geometry;
    DALI_TEST_CHECK(TypesetterFor(label)->ResolveRasterGeometry(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), geometry));
    DALI_TEST_EQUALS(geometry.underlay.left, geometry.fill.left + 1 - halo, TEST_LOCATION);
    DALI_TEST_EQUALS(geometry.underlay.top, geometry.fill.top - 1 - halo, TEST_LOCATION);
    DALI_TEST_EQUALS(geometry.underlay.right, geometry.fill.right + 1 + halo, TEST_LOCATION);
    DALI_TEST_EQUALS(geometry.underlay.bottom, geometry.fill.bottom - 1 + halo, TEST_LOCATION);
    // Compare the Adaptor kernel on precisely this support against an independently
    // padded source, including every channel outside the proposed Text ROI.
    const unsigned roiSize = 3 + 2 * halo, extra = 8, fullSize = roiSize + 2 * extra;
    auto           roi  = PixelBuffer::New(roiSize, roiSize, Pixel::RGBA8888);
    auto           full = PixelBuffer::New(fullSize, fullSize, Pixel::RGBA8888);
    memset(roi.GetBuffer(), 0, roi.GetBufferSize());
    memset(full.GetBuffer(), 0, full.GetBufferSize());
    for(unsigned y = 0; y < 3; ++y)
      for(unsigned x = 0; x < 3; ++x)
      {
        const std::array<uint8_t, 4> color{255, 127, 63, 255};
        memcpy(roi.GetBuffer() + ((y + halo) * roiSize + x + halo) * 4, color.data(), 4);
        memcpy(full.GetBuffer() + ((y + halo + extra) * fullSize + x + halo + extra) * 4, color.data(), 4);
      }
    DALI_TEST_CHECK(roi.ApplyGaussianBlur(radius));
    DALI_TEST_CHECK(full.ApplyGaussianBlur(radius));
    bool equal = true;
    for(unsigned y = 0; y < fullSize; ++y)
      for(unsigned x = 0; x < fullSize; ++x)
        for(unsigned c = 0; c < 4; ++c)
        {
          const uint8_t expected = x >= extra && y >= extra && x < extra + roiSize && y < extra + roiSize ? roi.GetBuffer()[((y - extra) * roiSize + x - extra) * 4 + c] : 0;
          equal                  = equal && full.GetBuffer()[(y * fullSize + x) * 4 + c] == expected;
        }
    DALI_TEST_CHECK(equal);
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesSmallRangesP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto label = MakeLabel(app, "unused");
  ControllerFor(label)->SetTextElideEnabled(false);
  const std::string text(96, 'W');
  auto              builder = Text::StyledTextBuilder::New(text.c_str());
  for(unsigned i = 0; i + 4 < text.size(); i += 6)
  {
    Text::Underline underline;
    underline.SetThickness(3);
    underline.SetType(Text::Underline::Type::DASHED);
    underline.SetDashLength(4.25f);
    underline.SetDashGap(2.5f);
    underline.SetColor(Vector4(.8f, .5f, .2f, .6f));
    builder.SetSpan(Text::UnderlineSpan::New(underline), i, i + 4);
    Text::LineThrough strike;
    strike.SetThickness(2);
    strike.SetColor(Vector4(.3f, .7f, .9f, .7f));
    builder.SetSpan(Text::LineThroughSpan::New(strike), i + 2, i + 5);
  }
  label.SetStyledText(builder.Build());
  Flush(app);
  auto                   ts = TypesetterFor(label);
  Text::TextRasterResult result;
  DALI_TEST_CHECK(ts->RenderRasterPlanes(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, result));
  CheckPlane(result.fill, result.geometry.fill);
  CheckPlane(result.underlay, result.geometry.underlay);
  CheckPlane(result.overlay, result.geometry.overlay);
  auto&                                              impl = *((*ts).*GetMember(TypesetterImpl{}));
  auto&                                              view = *impl.GetViewModel();
  std::remove_reference_t<decltype(impl)>::StyleRuns runs;
  runs.underline.Resize(view.GetNumberOfUnderlineRuns());
  view.GetUnderlineRuns(runs.underline.Begin(), 0, runs.underline.Count());
  runs.strikethrough.Resize(view.GetNumberOfStrikethroughRuns());
  view.GetStrikethroughRuns(runs.strikethrough.Begin(), 0, runs.strikethrough.Count());
  const auto&        bounds = result.geometry.overlay;
  Text::RasterTarget reference{PixelBuffer::New(bounds.right - bounds.left, bounds.bottom - bounds.top, Pixel::RGBA8888), bounds, true};
  memset(reference.buffer.GetBuffer(), 0, reference.buffer.GetBufferSize());
  // Reproduce the previous union-sized temporary path independently of RasterPlan.
  const auto draw = [&](Text::Typesetter::Style style, const Text::GlyphRun& run)
  {
    Text::RasterTarget temporary{PixelBuffer::New(reference.buffer.GetWidth(), reference.buffer.GetHeight(), Pixel::RGBA8888), bounds, true};
    memset(temporary.buffer.GetBuffer(), 0, temporary.buffer.GetBufferSize());
    return impl.RasterizeStyle(320, 160, style, false, 0, 0, run.glyphIndex, run.glyphIndex + run.numberOfGlyphs - 1, runs, temporary) && impl.CompositeBehind(reference, temporary);
  };
  for(const auto& run : runs.underline) DALI_TEST_CHECK(draw(Text::Typesetter::STYLE_UNDERLINE, run.glyphRun));
  for(const auto& run : runs.strikethrough) DALI_TEST_CHECK(draw(Text::Typesetter::STYLE_STRIKETHROUGH, run.glyphRun));
  DALI_TEST_CHECK(Bytes(PixelBuffer::Convert(reference.buffer)) == Bytes(result.overlay));
  END_TEST;
}

int UtcDaliTextRasterPlanesAsyncResultParityP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto  loader = Text::AsyncTextLoader::New();
  auto& worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  for(unsigned variant = 0; variant < 17; ++variant)
  {
    tet_printf("Async raster parity case %u\n", variant);
    auto label = MakeLabel(app, variant == 8 ? "مرحبا بالعالم" : variant == 9  ? "Hello مرحبا world"
                                                               : variant == 10 ? "ffi Á office ä"
                                                                               : "Hello effect overflow");
    label.SetRequestedWidth(220);
    label.SetRequestedHeight(80);
    if((variant >= 1 && variant <= 3) || variant == 6)
    {
      Text::Shadow shadow;
      shadow.SetOffset(variant == 1 ? Vector2(3, 3) : variant == 2 ? Vector2(-30, -25)
                                                                   : Vector2(30, 25));
      shadow.SetBlurRadius(variant == 1 ? 3 : 30);
      label.SetTextShadow(shadow);
    }
    if(variant == 4 || variant == 6)
    {
      Text::Outline outline;
      outline.SetWidth(6);
      outline.SetOffset(Vector2(-8, 5));
      label.SetTextOutline(outline);
    }
    if(variant == 5 || variant == 6)
    {
      Text::Underline underline;
      underline.SetThickness(25);
      label.SetTextUnderline(underline);
    }
    if(variant == 7)
    {
      label.SetStyledText(Text::StyledText::FromMarkup("<color value='red'><u height='12'>Hello</u></color> <s height='8'>world</s>"));
    }
    if(variant == 11) label.SetTextFit(Text::Fit::Range(8, 24, 1));
    if(variant == 12)
    {
      label.SetText("A very long line of text that requires final ellipsis");
      ControllerFor(label)->SetTextElideEnabled(true);
    }
    if(variant == 13)
    {
      auto            builder = Text::StyledTextBuilder::New("plain decorated image plain");
      Text::Underline underline;
      underline.SetThickness(12);
      builder.SetSpan(Text::UnderlineSpan::New(underline), 6, 21);
      Text::LineThrough strike;
      strike.SetThickness(8);
      builder.SetSpan(Text::LineThroughSpan::New(strike), 10, 21);
      builder.SetSpan(Text::ImageSpan::New(Text::ImageAttributes("missing-raster-plane.png", Vector2(40, 30))), 16, 21);
      label.SetStyledText(builder.Build());
    }
    if(variant >= 14)
    {
      Gradient::Linear gradient(Vector2::ZERO, Vector2::ONE);
      gradient.SetStopNodes({Gradient::StopNode(0, UiColor(Color::RED)), Gradient::StopNode(1, UiColor(Color::BLUE))});
      auto builder = Text::StyledTextBuilder::New("plain gradient image plain");
      builder.SetSpan(Text::GradientSpan::New(gradient, variant == 14 ? Text::GradientSpan::BoundsMode::SPAN_BOUND
                                                                    : variant == 15 ? Text::GradientSpan::BoundsMode::CONTENT_BOUND
                                                                                    : Text::GradientSpan::BoundsMode::VIEW_BOUND), 6, 20);
      builder.SetSpan(Text::ImageSpan::New(Text::ImageAttributes("missing-raster-plane.png", Vector2(30, 25))), 15, 20);
      Text::Underline underline;
      underline.SetThickness(12);
      builder.SetSpan(Text::UnderlineSpan::New(underline), 6, 20);
      label.SetStyledText(builder.Build());
    }
    Flush(app);
    auto    controller = ControllerFor(label);
    auto    model      = controller->GetRenderTextModel();
    Vector2 rasterSize(220, model->GetLayoutSize().y);
    if(model->IsShadowEnabled()) rasterSize.y += std::max(0.f, model->GetShadowOffset().y);
    if(model->IsOutlineEnabled()) rasterSize.y += 2 * model->GetOutlineWidth();
    rasterSize.y = std::min(rasterSize.y, 80.f);
    Text::TextRasterResult sync;
    const auto             format = (variant == 7 || variant >= 14) ? Pixel::RGBA8888 : Pixel::L8;
    DALI_TEST_CHECK(TypesetterFor(label)->RenderRasterPlanes(rasterSize, controller->GetTextDirection(), format, 4094, sync));
    auto& impl                       = static_cast<Ui::Integration::LabelImpl&>(GetImpl(label));
    auto  params                     = (impl.*GetMember(LabelParameters{}))(Ui::Integration::Text::Async::RENDER_FIXED_SIZE,
                                                       Vector2(220, 80), Insets{}, Dali::LayoutDirection::LEFT_TO_RIGHT);
    params.maxTextureSize            = 4096;
    params.isEffectOverflowRequested = true;
    const auto result                = variant == 11 ? loader.RenderTextFit(params, false, Size::ZERO)
                                                     : loader.RenderText(params, false, Size::ZERO);
    DALI_TEST_CHECK(result.effectOverflow);
    DALI_TEST_CHECK(!result.textPixelData && !result.stylePixelData && !result.overlayStylePixelData && !result.maskPixelData);
    if(result.effectOverflow)
    {
      DALI_TEST_EQUALS(result.effectOverflow->legacyRasterSize, sync.legacyRasterSize, .001f, TEST_LOCATION);
      DALI_TEST_EQUALS(result.effectOverflow->layoutSize, sync.layoutSize, .001f, TEST_LOCATION);
      DALI_TEST_CHECK(SamePlanes(sync, *result.effectOverflow));
      std::array<PixelData, 3> a{sync.fill, sync.underlay, sync.overlay};
      std::array<PixelData, 3> b{result.effectOverflow->fill, result.effectOverflow->underlay, result.effectOverflow->overlay};
      for(unsigned i = 0; i < a.size(); ++i)
      {
        if(a[i] && b[i])
        {
          DALI_TEST_EQUALS(a[i].GetWidth(), b[i].GetWidth(), TEST_LOCATION);
          DALI_TEST_EQUALS(a[i].GetHeight(), b[i].GetHeight(), TEST_LOCATION);
          DALI_TEST_EQUALS(a[i].GetPixelFormat(), b[i].GetPixelFormat(), TEST_LOCATION);
        }
      }
    }
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesAsyncFallbackP(void)
{
  UiTestApplication app;
  auto              loader = Text::AsyncTextLoader::New();
  auto&             worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  for(unsigned variant = 0; variant < 8; ++variant)
  {
    tet_printf("Async fallback case %u\n", variant);
    Text::AsyncTextParameters params;
    params.text                          = "Hello";
    params.isBackgroundWithCutoutEnabled = variant == 6;
    params.fontFamily                    = "DejaVu Sans";
    params.fontSize                      = 18;
    params.textWidth                     = 220;
    params.textHeight                    = 80;
    params.maxTextureSize                = variant == 7 ? 2 : 4096;
    params.isEffectOverflowRequested     = variant != 0;
    params.isMarqueeEnabled              = variant == 1;
    params.isTextGradientRequested       = variant == 2;
    params.isTextRevealEnabled           = variant == 3;
    params.isEmbossEnabled               = variant == 4;
    if(variant == 5)
    {
      params.isShadowEnabled  = true;
      params.shadowOffset     = Vector2(-100000, 0);
      params.shadowBlurRadius = 3;
    }
    const auto result = loader.RenderText(params, false, Size::ZERO);
    if(variant >= 1 && variant <= 4)
    {
      DALI_TEST_CHECK(result.effectOverflow && !result.textPixelData);
    }
    else
    {
      DALI_TEST_CHECK(!result.effectOverflow);
      DALI_TEST_CHECK(result.textPixelData);
    }
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesAsyncSupersampledShadowP(void)
{
  UiTestApplication app;
  auto              loader = Text::AsyncTextLoader::New();
  auto&             worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  for(float renderScale : {1.f, 1.5f, 2.f})
  {
    for(float uiScale : {1.f, 1.5f, 2.f})
    {
      for(bool realistic : {false, true})
      {
        tet_printf("Shadow raster domain: renderScale=%g UI=%g realistic=%d\n", renderScale, uiScale, realistic);
        Text::AsyncTextParameters params;
        params.requestType               = Ui::Integration::Text::Async::RENDER_FIXED_SIZE;
        params.text                      = "Hello";
        params.fontFamily                = "DejaVu Sans";
        params.fontSize                  = 33; // 44 px at 96 dpi, as in the sample.
        params.textWidth                 = 260;
        params.textHeight                = 76;
        params.maxTextureSize            = 4096;
        params.renderScale               = renderScale;
        params.decorationUiScale         = uiScale;
        params.effectiveTextScale        = uiScale;
        params.isEffectOverflowRequested = true;
        params.isShadowEnabled           = true;
        params.shadowOffset              = realistic ? Vector2(3, 3) : Vector2(-30, 25);
        params.shadowBlurRadius          = realistic ? 3 : 30;
        if(realistic)
        {
          params.horizontalAlignment = Text::Alignment::END;
          params.verticalAlignment   = Text::Alignment::END;
        }
        bool cached = false;
        Size naturalSize;
        if(renderScale > 1.f) naturalSize = loader.SetupRenderScale(params, cached);
        const auto result = loader.RenderText(params, cached, naturalSize);
        DALI_TEST_CHECK(result.effectOverflow);
        DALI_TEST_CHECK(!result.textPixelData && !result.stylePixelData);
        if(result.effectOverflow)
        {
          const auto& geometry = result.effectOverflow->geometry;
          // Compare halo relative to glyph ink, independent of font hinting.
          // UI scales offsets, while supersampling scales both offsets and blur.
          const int dx   = static_cast<int>(params.shadowOffset.x * uiScale * renderScale);
          const int dy   = static_cast<int>(params.shadowOffset.y * uiScale * renderScale);
          const int halo = static_cast<int>(std::ceil(params.shadowBlurRadius * renderScale));
          DALI_TEST_EQUALS(geometry.underlay.left - geometry.fill.left, dx - halo, TEST_LOCATION);
          DALI_TEST_EQUALS(geometry.underlay.top - geometry.fill.top, dy - halo, TEST_LOCATION);
          DALI_TEST_EQUALS(geometry.underlay.right - geometry.fill.right, dx + halo, TEST_LOCATION);
          DALI_TEST_EQUALS(geometry.underlay.bottom - geometry.fill.bottom, dy + halo, TEST_LOCATION);
          DALI_TEST_EQUALS(result.effectOverflow->legacyRasterSize / renderScale, result.size, .001f, TEST_LOCATION);
        }
      }
    }
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesSupersampledFallbackP(void)
{
  UiTestApplication app;
  auto              loader = Text::AsyncTextLoader::New();
  auto&             worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  for(float scale : {1.5f, 2.f})
  {
    for(unsigned variant = 0; variant < 8; ++variant)
    {
      tet_printf("Supersampled fallback: scale=%g case=%u\n", scale, variant);
      Text::AsyncTextParameters params;
      params.text                           = "Hello";
      params.fontFamily                     = "DejaVu Sans";
      params.fontSize                       = 33;
      params.textWidth                      = 260;
      params.textHeight                     = 76;
      params.renderScale                    = scale;
      params.decorationUiScale              = 1.5f;
      params.effectiveTextScale             = 1.5f;
      params.maxTextureSize                 = variant == 7 ? 2 : 4096;
      params.isShadowEnabled                = true;
      params.shadowOffset                   = variant == 5 ? Vector2(-100000, 0) : Vector2(-30, 25);
      params.shadowBlurRadius               = 3;
      params.isTextGradientRequested        = variant == 0;
      params.isTextGradientOverlayRequested = variant == 1;
      params.isTextRevealEnabled            = variant == 2;
      params.isEmbossEnabled                = variant == 3;
      params.isMarqueeEnabled               = variant == 4;
      params.isBackgroundWithCutoutEnabled  = variant == 6;
      params.isEffectOverflowRequested      = false;
      Text::AsyncTextParameters legacyParameters, fallbackParameters;
      const auto                legacy = LoadRequest(loader, params, &legacyParameters);
      params.isEffectOverflowRequested = true;
      const auto fallback              = LoadRequest(loader, params, &fallbackParameters);
      // Completion retains request policy and authored styles, with the same
      // worker size bookkeeping as a request that never enabled overflow.
      DALI_TEST_CHECK(fallbackParameters.isEffectOverflowRequested);
      if(variant <= 4)
      {
        DALI_TEST_CHECK(fallback.effectOverflow && !fallback.textPixelData);
        if(variant != 4) DALI_TEST_EQUALS(fallback.effectOverflow->legacyRasterSize / scale, fallback.size, .001f, TEST_LOCATION);
        continue;
      }
      DALI_TEST_EQUALS(fallbackParameters.requestType, legacyParameters.requestType, TEST_LOCATION);
      DALI_TEST_EQUALS(Vector4(fallbackParameters.textWidth, fallbackParameters.textHeight,
                               fallbackParameters.renderScaleWidth, fallbackParameters.renderScaleHeight),
                       Vector4(legacyParameters.textWidth, legacyParameters.textHeight,
                               legacyParameters.renderScaleWidth, legacyParameters.renderScaleHeight),
                       TEST_LOCATION);
      DALI_TEST_EQUALS(fallbackParameters.fontSize, legacyParameters.fontSize, TEST_LOCATION);
      DALI_TEST_EQUALS(fallbackParameters.minLineSize, legacyParameters.minLineSize, TEST_LOCATION);
      DALI_TEST_EQUALS(fallbackParameters.renderScale, scale, TEST_LOCATION);
      DALI_TEST_EQUALS(fallbackParameters.decorationUiScale, params.decorationUiScale, TEST_LOCATION);
      DALI_TEST_EQUALS(fallbackParameters.shadowOffset, params.shadowOffset, TEST_LOCATION);
      DALI_TEST_EQUALS(fallbackParameters.shadowBlurRadius, params.shadowBlurRadius, TEST_LOCATION);
      DALI_TEST_CHECK(!fallback.effectOverflow);
      DALI_TEST_CHECK(fallback.textPixelData);
      DALI_TEST_EQUALS(fallback.size, legacy.size, TEST_LOCATION);
      DALI_TEST_EQUALS(fallback.renderedSize, legacy.renderedSize, TEST_LOCATION);
      DALI_TEST_EQUALS(fallback.lineCount, legacy.lineCount, TEST_LOCATION);
      DALI_TEST_EQUALS(fallback.textLogicalBounds, legacy.textLogicalBounds, TEST_LOCATION);
      DALI_TEST_EQUALS(fallback.isTextDirectionRTL, legacy.isTextDirectionRTL, TEST_LOCATION);
      DALI_TEST_EQUALS(fallback.isCutoutEnabled, legacy.isCutoutEnabled, TEST_LOCATION);
      DALI_TEST_EQUALS(fallback.styleEnabled, legacy.styleEnabled, TEST_LOCATION);
      DALI_TEST_CHECK(Bytes(fallback.textPixelData) == Bytes(legacy.textPixelData));
      DALI_TEST_CHECK(Bytes(fallback.stylePixelData) == Bytes(legacy.stylePixelData));
      DALI_TEST_CHECK(Bytes(fallback.overlayStylePixelData) == Bytes(legacy.overlayStylePixelData));
      DALI_TEST_CHECK(Bytes(fallback.maskPixelData) == Bytes(legacy.maskPixelData));
      DALI_TEST_CHECK(Bytes(fallback.marqueePixelData) == Bytes(legacy.marqueePixelData));
    }
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesSupersampledStyleSnapshotP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  auto  loader = Text::AsyncTextLoader::New();
  auto& worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  auto            label   = MakeLabel(app, "unused");
  auto            builder = Text::StyledTextBuilder::New("Hello image world");
  Text::Underline underline;
  underline.SetThickness(4);
  underline.SetType(Text::Underline::Type::DASHED);
  underline.SetDashLength(6);
  underline.SetDashGap(2);
  builder.SetSpan(Text::UnderlineSpan::New(underline), 0, 16);
  Text::LineThrough strike;
  strike.SetThickness(3);
  builder.SetSpan(Text::LineThroughSpan::New(strike), 3, 16);
  builder.SetSpan(Text::ImageSpan::New(Text::ImageAttributes("missing-raster-plane.png", Vector2(20, 20))), 6, 11);
  label.SetStyledText(builder.Build());
  label.SetTextUnderline(underline);
  label.SetTextLineThrough(strike);
  Text::Shadow shadow;
  shadow.SetOffset(Vector2(-3, 2));
  shadow.SetBlurRadius(3);
  label.SetTextShadow(shadow);
  Text::Outline outline;
  outline.SetWidth(1);
  outline.SetBlurRadius(2);
  outline.SetOffset(Vector2(-2, 3));
  label.SetTextOutline(outline);
  Flush(app);
  auto&      labelImpl = static_cast<Ui::Integration::LabelImpl&>(GetImpl(label));
  const auto original  = (labelImpl.*GetMember(LabelParameters{}))(Ui::Integration::Text::Async::RENDER_FIXED_SIZE,
                                                                  Vector2(320, 160), Insets{}, Dali::LayoutDirection::LEFT_TO_RIGHT);
  const float uiScales[] = {1.f, 1.5f, 2.f, 1.f, 1.5f};
  unsigned    request    = 0;
  for(float scale : {1.f, 1.5f, 2.f, 1.f, 2.f})
  {
    const float uiScale              = uiScales[request++];
    auto        params               = original;
    params.maxTextureSize            = 4096;
    params.isEffectOverflowRequested = true;
    params.renderScale               = scale;
    params.decorationUiScale         = uiScale;
    const auto result                = LoadRequest(loader, params);
    DALI_TEST_CHECK(result.effectOverflow);
    DALI_TEST_CHECK(!result.replacementPlacements.Empty());
    auto  typesetter = worker.*GetMember(WorkerTypesetter{});
    auto& impl       = *((*typesetter).*GetMember(TypesetterImpl{}));
    auto& view       = *impl.GetViewModel();
    DALI_TEST_EQUALS(view.GetOutlineWidth(), static_cast<uint16_t>(std::round(std::round(uiScale) * scale)), TEST_LOCATION);
    DALI_TEST_EQUALS(view.GetOutlineOffset(), Vector2(-2, 3) * uiScale * scale, .001f, TEST_LOCATION);
    DALI_TEST_EQUALS(view.GetOutlineBlurRadius(), 2.f * scale, .001f, TEST_LOCATION);
    DALI_TEST_EQUALS(view.GetShadowOffset(), Vector2(-3, 2) * uiScale * scale, .001f, TEST_LOCATION);
    DALI_TEST_EQUALS(view.GetShadowBlurRadius(), 3.f * scale, .001f, TEST_LOCATION);
    DALI_TEST_EQUALS(view.GetUnderlineHeight(), 4.f * uiScale * scale, .001f, TEST_LOCATION);
    DALI_TEST_EQUALS(view.GetDashedUnderlineWidth(), 6.f * uiScale * scale, .001f, TEST_LOCATION);
    DALI_TEST_EQUALS(view.GetDashedUnderlineGap(), 2.f * uiScale * scale, .001f, TEST_LOCATION);
    DALI_TEST_EQUALS(view.GetStrikethroughHeight(), 3.f * uiScale * scale, .001f, TEST_LOCATION);
    Vector<Text::UnderlinedGlyphRun> underlines;
    underlines.Resize(view.GetNumberOfUnderlineRuns());
    view.GetUnderlineRuns(underlines.Begin(), 0, underlines.Count());
    DALI_TEST_CHECK(!underlines.Empty());
    for(const auto& run : underlines)
    {
      DALI_TEST_EQUALS(run.properties.height, view.GetUnderlineHeight(), .001f, TEST_LOCATION);
      DALI_TEST_EQUALS(run.properties.dashWidth, view.GetDashedUnderlineWidth(), .001f, TEST_LOCATION);
      DALI_TEST_EQUALS(run.properties.dashGap, view.GetDashedUnderlineGap(), .001f, TEST_LOCATION);
    }
    Vector<Text::StrikethroughGlyphRun> strikes;
    strikes.Resize(view.GetNumberOfStrikethroughRuns());
    view.GetStrikethroughRuns(strikes.Begin(), 0, strikes.Count());
    DALI_TEST_CHECK(!strikes.Empty());
    for(const auto& run : strikes) DALI_TEST_EQUALS(run.properties.height, view.GetStrikethroughHeight(), .001f, TEST_LOCATION);
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesColorGlyphMaskP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  for(bool emojiOnly : {false, true})
  {
    auto label = MakeLabel(app, emojiOnly ? "A" : "ABC");
    Flush(app);
    auto* model = static_cast<const Text::Model*>(ControllerFor(label)->GetRenderTextModel());
    model->mVisualModel->mGlyphs[emojiOnly ? 0u : 1u].index = TextAbstraction::Test::COLOR_GLYPH_INDEX;
    auto ts = TypesetterFor(label);
    Text::TextRasterResult result;
    DALI_TEST_CHECK(ts->RenderRasterPlanes(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4094, result, true));
    DALI_TEST_CHECK(result.fill && result.colorGlyphMask);
    DALI_TEST_EQUALS(result.colorGlyphMask.GetPixelFormat(), Pixel::L8, TEST_LOCATION);
    DALI_TEST_EQUALS(result.colorGlyphMask.GetWidth(), result.fill.GetWidth(), TEST_LOCATION);
    DALI_TEST_EQUALS(result.colorGlyphMask.GetHeight(), result.fill.GetHeight(), TEST_LOCATION);
    const auto mask = Bytes(result.colorGlyphMask);
    const auto fill = Bytes(result.fill);
    const auto legacyMask = Bytes(ts->Render(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), Text::Typesetter::RENDER_MASK, false, Pixel::L8));
    const auto legacyFill = Bytes(ts->Render(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), Text::Typesetter::RENDER_NO_STYLES, false, Pixel::RGBA8888));
    bool hasMonochrome = false, hasNativeColor = false;
    for(uint32_t y = 0; y < result.fill.GetHeight(); ++y)
    {
      for(uint32_t x = 0; x < result.fill.GetWidth(); ++x)
      {
        const auto source = (y + result.geometry.fill.top) * 320u + x + result.geometry.fill.left;
        const auto pixel = y * result.fill.GetWidth() + x;
        DALI_TEST_EQUALS(mask[pixel], legacyMask[source], TEST_LOCATION);
        for(unsigned channel = 0; channel < 4; ++channel)
          DALI_TEST_EQUALS(fill[4u * pixel + channel], legacyFill[4u * source + channel], TEST_LOCATION);
        hasMonochrome |= mask[pixel] != 0u;
        hasNativeColor |= mask[pixel] == 0u && fill[4u * pixel + 3u] != 0u && fill[4u * pixel] != fill[4u * pixel + 2u];
      }
    }
    DALI_TEST_EQUALS(hasMonochrome, !emojiOnly, TEST_LOCATION);
    DALI_TEST_CHECK(hasNativeColor);
    // The ordinary and multi-color callers do not request or allocate a mask.
    DALI_TEST_CHECK(ts->RenderRasterPlanes(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4094, result));
    DALI_TEST_CHECK(!result.colorGlyphMask);
    DALI_TEST_CHECK(!ts->RenderRasterPlanes(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), Pixel::L8, 4094, result, true));
    DALI_TEST_CHECK(!result.fill && !result.colorGlyphMask);
    DALI_TEST_CHECK(!ts->RenderRasterPlanes(Vector2(320, 160), ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 1, result, true));
    DALI_TEST_CHECK(!result.fill && !result.colorGlyphMask && !result.underlay && !result.overlay);
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesGradientSpanDomainP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  std::array<Gradient::Base, 3> gradients{Gradient::Linear(Vector2::ZERO, Vector2::ONE),
                                          Gradient::Radial(Vector2(.5f, .5f), .4f),
                                          Gradient::Conic(Vector2(.5f, .5f), Radian(.3f))};
  for(auto& gradient : gradients)
  {
    gradient.SetStopNodes({Gradient::StopNode(0, UiColor(Vector4(1, 0, 0, .5f))),
                           Gradient::StopNode(1, UiColor(Vector4(0, 0, 1, .8f)))});
    gradient.SetSpreadMethod(Gradient::SpreadMethod::REFLECT);
    for(auto mode : {Text::GradientSpan::BoundsMode::SPAN_BOUND,
                     Text::GradientSpan::BoundsMode::CONTENT_BOUND,
                     Text::GradientSpan::BoundsMode::VIEW_BOUND})
    {
      auto label = MakeLabel(app, "");
      auto builder = Text::StyledTextBuilder::New("jÁf ffi Á Hello\nworld");
      builder.SetSpan(Text::GradientSpan::New(gradient, mode), 0u, 18u);
      Text::Underline underline;
      underline.SetThickness(20);
      builder.SetSpan(Text::UnderlineSpan::New(underline), 4u, 12u);
      label.SetStyledText(builder.Build());
      label.SetHorizontalTextAlignment(Text::Alignment::CENTER);
      Flush(app);
      Text::TextRasterResult previous;
      for(unsigned effect = 0; effect < 2; ++effect)
      {
        if(effect)
        {
          Text::Shadow shadow;
          shadow.SetOffset(Vector2(-40, -30));
          shadow.SetBlurRadius(12);
          label.SetTextShadow(shadow);
          Flush(app);
        }
        auto controller = ControllerFor(label);
        auto* model = static_cast<const Text::Model*>(controller->GetRenderTextModel());
        auto savedPositions = model->mVisualModel->mGlyphPositions;
        // Keep a signed fill origin and fractional positions in every brush domain.
        for(auto& position : model->mVisualModel->mGlyphPositions)
        {
          position -= Vector2(80.5f, 12.25f);
        }
        auto typesetter = TypesetterFor(label);
        Text::TextRasterResult result;
        DALI_TEST_CHECK(typesetter->RenderRasterPlanes(Vector2(320, 160), controller->GetTextDirection(), Pixel::RGBA8888, 4094, result));
        auto legacy = typesetter->Render(Vector2(320, 160), controller->GetTextDirection(), Text::Typesetter::RENDER_NO_STYLES, false, Pixel::RGBA8888);
        const auto oldPixels = Dali::Integration::GetPixelDataBuffer(legacy);
        const auto newPixels = Dali::Integration::GetPixelDataBuffer(result.fill);
        const auto& bounds = result.geometry.fill;
        bool equal = true;
        unsigned compared = 0;
        for(int32_t y = std::max(0, bounds.top); y < std::min(160, bounds.bottom); ++y)
        {
          for(int32_t x = std::max(0, bounds.left); x < std::min(320, bounds.right); ++x)
          {
            equal = equal && std::memcmp(oldPixels.buffer + (y * 320 + x) * 4,
                                        newPixels.buffer + ((y - bounds.top) * result.fill.GetWidth() + x - bounds.left) * 4, 4) == 0;
            ++compared;
          }
        }
        DALI_TEST_CHECK(compared > 0 && equal);
        if(effect)
        {
          DALI_TEST_CHECK(Equal(previous.geometry.fill, result.geometry.fill));
          DALI_TEST_CHECK(Bytes(previous.fill) == Bytes(result.fill));
          DALI_TEST_CHECK(result.geometry.composition.left < previous.geometry.composition.left);
        }
        model->mVisualModel->mGlyphPositions = savedPositions;
        previous = std::move(result);
      }
      label.Unparent();
    }
  }
  END_TEST;
}

int UtcDaliTextSourcePlanesActiveEdgesP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  for(unsigned alignment = 0; alignment < 3; ++alignment)
  {
    auto label = MakeLabel(app, "MMMMMMMMMMMM");
    label.SetMultiLine(false);
    label.SetRequestedWidth(70);
    label.SetRequestedHeight(80);
    label.SetHorizontalTextAlignment(static_cast<Text::Alignment>(alignment));
    label.SetTextOverflowMode(Text::OverflowMode::CLIP);
    Flush(app);
    auto                     ts = TypesetterFor(label);
    Text::Raster::SourceClip clip;
    DALI_TEST_CHECK(ts->ResolveSourceClip(Vector2(70, 80), Vector4(0, 0, 70, 80), clip));
    const unsigned expected = alignment == 0 ? Text::Raster::SourceClip::RIGHT : alignment == 1 ? Text::Raster::SourceClip::LEFT | Text::Raster::SourceClip::RIGHT
                                                                                                : Text::Raster::SourceClip::LEFT;
    DALI_TEST_EQUALS(unsigned(clip.planes), expected, TEST_LOCATION);
    Text::TextRasterResult result;
    const Vector4          viewport(0, 0, 70, 80);
    DALI_TEST_CHECK(ts->RenderRasterPlanes(Vector2(70, 80), ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, result, false, &viewport));
    DALI_TEST_CHECK(result.geometry.fill.left >= 0 && result.geometry.fill.right <= 70);
    label.Unparent();
  }
  for(unsigned alignment = 0; alignment < 3; ++alignment)
  {
    auto label = MakeLabel(app, "MMM\nMMM\nMMM\nMMM");
    label.SetRequestedHeight(35);
    label.SetVerticalTextAlignment(static_cast<Text::Alignment>(alignment));
    label.SetTextOverflowMode(Text::OverflowMode::CLIP);
    Flush(app);
    Text::Raster::SourceClip clip;
    DALI_TEST_CHECK(TypesetterFor(label)->ResolveSourceClip(Vector2(320, 35), Vector4(0, 0, 320, 35), clip));
    const unsigned expected = alignment == 0 ? Text::Raster::SourceClip::BOTTOM : alignment == 1 ? Text::Raster::SourceClip::TOP | Text::Raster::SourceClip::BOTTOM
                                                                                                 : Text::Raster::SourceClip::TOP;
    DALI_TEST_EQUALS(unsigned(clip.planes), expected, TEST_LOCATION);
    label.Unparent();
  }
  END_TEST;
}

int UtcDaliTextSourcePlanesFittingAndEllipsisP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app, "short");
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  Text::Outline outline;
  outline.SetWidth(8);
  outline.SetOffset(Vector2(-12, 9));
  outline.SetBlurRadius(4);
  label.SetTextOutline(outline);
  Text::Shadow shadow;
  shadow.SetOffset(Vector2(20, -12));
  shadow.SetBlurRadius(5);
  label.SetTextShadow(shadow);
  for(unsigned mode = 0; mode < 2; ++mode)
  {
    if(mode)
    {
      label.SetMultiLine(false);
      label.SetText("MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM");
      label.SetTextOverflowMode(Text::OverflowMode::ELLIPSIS);
    }
    Flush(app);
    auto                     ts = TypesetterFor(label);
    const Vector2            size(320, 160);
    const Vector4            viewport(0, 0, 320, 160);
    Text::Raster::SourceClip clip;
    DALI_TEST_CHECK(ts->ResolveSourceClip(size, viewport, clip));
    DALI_TEST_EQUALS(unsigned(clip.planes), 0u, TEST_LOCATION);
    Text::TextRasterResult original, clipped;
    DALI_TEST_CHECK(ts->RenderRasterPlanes(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, original));
    DALI_TEST_CHECK(ts->RenderRasterPlanes(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, clipped, false, &viewport));
    DALI_TEST_CHECK(SamePlanes(original, clipped));
  }
  END_TEST;
}

int UtcDaliTextSourcePlanesHiddenEffectsP(void)
{
  UiTestApplication app;
  auto              hidden = MakeLabel(app, "MM"), visible = MakeLabel(app, "M");
  for(auto label : {hidden, visible})
  {
    label.SetMultiLine(false);
    label.SetTextOverflowMode(Text::OverflowMode::CLIP);
    Text::Shadow shadow;
    shadow.SetOffset(Vector2(-120, -8));
    shadow.SetBlurRadius(5);
    shadow.SetColor(UiColor(Color::RED));
    label.SetTextShadow(shadow);
    Text::Outline outline;
    outline.SetWidth(6);
    outline.SetOffset(Vector2(-100, 3));
    label.SetTextOutline(outline);
  }
  Flush(app);
  // The mock FontClient paints even spaces; place the second real test glyph
  // beyond the viewport explicitly. Native coverage uses "M         M".
  auto* hiddenModel                               = static_cast<const Text::Model*>(ControllerFor(hidden)->GetRenderTextModel());
  hiddenModel->mVisualModel->mGlyphPositions[1].x = 100.f;
  Text::TextRasterResult reference;
  std::vector<uint8_t>   legacyReference;
  const Vector2          size(20, 80);
  const Vector4          viewport(0, 0, 20, 80);
  for(auto label : {visible, hidden})
  {
    auto                     ts = TypesetterFor(label);
    Text::Raster::SourceClip clip;
    DALI_TEST_CHECK(ts->ResolveSourceClip(size, viewport, clip));
    const auto*            active = clip.planes ? &clip : nullptr;
    Text::TextRasterResult result;
    DALI_TEST_CHECK(ts->RenderRasterPlanes(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, result, false, &viewport));
    auto legacy = ts->Render(size, ControllerFor(label)->GetTextDirection(), Text::Typesetter::RENDER_NO_TEXT, false, Pixel::RGBA8888, Size::ZERO, nullptr, active);
    if(label == visible)
    {
      reference       = std::move(result);
      legacyReference = Bytes(legacy);
    }
    else
    {
      DALI_TEST_CHECK(SamePlanes(reference, result));
      DALI_TEST_CHECK(legacyReference == Bytes(legacy));
    }
  }
  // A viewport lying wholly in the gap cannot retain union-bounds of two hidden glyphs.
  auto                   ts = TypesetterFor(hidden);
  const Vector4          gap(10, 0, 5, 80);
  Text::TextRasterResult empty;
  DALI_TEST_CHECK(ts->RenderRasterPlanes(size, ControllerFor(hidden)->GetTextDirection(), Pixel::RGBA8888, 4096, empty, false, &gap));
  DALI_TEST_CHECK(empty.geometry.fill.Empty());
  DALI_TEST_CHECK(empty.geometry.underlay.Empty());
  END_TEST;
}

int UtcDaliTextSourcePlanesDecorationsAndReuseP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app, "");
  label.SetMultiLine(false);
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  label.SetStyledText(Text::StyledText::FromMarkup("<u type='dashed' height='3' dash-width='4' dash-gap='2'><s height='2'><background color='blue'>MMMMMMMMMMMM</background></s></u>"));
  label.SetHorizontalTextAlignment(Text::Alignment::CENTER);
  Flush(app);
  auto                   ts = TypesetterFor(label);
  const Vector2          size(320, 160);
  Text::TextRasterResult full;
  DALI_TEST_CHECK(ts->RenderRasterPlanes(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, full));
  for(const Vector4 viewport : {Vector4(60, 0, 71, 160), Vector4(80, 0, 41, 160), Vector4(0, 0, 320, 160)})
  {
    Text::TextRasterResult result;
    DALI_TEST_CHECK(ts->RenderRasterPlanes(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, result, false, &viewport));
    for(const auto& pair : {std::make_pair(full.overlay, result.overlay), std::make_pair(full.underlay, result.underlay)})
    {
      if(!pair.first || !pair.second) continue;
      const auto& originalBounds = pair.first == full.overlay ? full.geometry.overlay : full.geometry.underlay;
      const auto& clippedBounds  = pair.first == full.overlay ? result.geometry.overlay : result.geometry.underlay;
      auto        a = Bytes(pair.first), b = Bytes(pair.second);
      for(int y = clippedBounds.top; y < clippedBounds.bottom; ++y)
        for(int x = clippedBounds.left; x < clippedBounds.right; ++x)
          for(unsigned channel = 0; channel < 4; ++channel)
            DALI_TEST_EQUALS(a[((y - originalBounds.top) * pair.first.GetWidth() + x - originalBounds.left) * 4 + channel],
                             b[((y - clippedBounds.top) * pair.second.GetWidth() + x - clippedBounds.left) * 4 + channel], TEST_LOCATION);
    }
  }
  END_TEST;
}

int UtcDaliTextSourcePlanesPairedColorP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app, "AB");
  label.SetMultiLine(false);
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  Flush(app);
  auto* model               = static_cast<const Text::Model*>(ControllerFor(label)->GetRenderTextModel());
  auto& visual              = *model->mVisualModel;
  visual.mGlyphPositions[1] = visual.mGlyphPositions[0];
  const Vector2 size(320, 160);
  for(bool nativeOnTop : {false, true})
  {
    visual.mGlyphs[nativeOnTop ? 0 : 1].index = TextAbstraction::Test::HALF_COVERAGE_GLYPH_INDEX;
    visual.mGlyphs[nativeOnTop ? 1 : 0].index = TextAbstraction::Test::COLOR_GLYPH_INDEX;
    auto                   ts                 = TypesetterFor(label);
    Text::TextRasterResult full;
    DALI_TEST_CHECK(ts->RenderRasterPlanes(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, full, true));
    const auto  bounds = full.geometry.fill;
    const float x = bounds.left, y = bounds.top;
    // Each side, exact boundaries, empty intersection, then fitting again.
    for(const Vector4 viewport : {Vector4(x + 1, y, 320, 160), Vector4(x, y, 1, 160),
                                  Vector4(x, y + 1, 320, 160), Vector4(x, y, 320, 1),
                                  Vector4(x + 1, y, 0, 160), Vector4(x + 30, y, 5, 160), Vector4(0, 0, 320, 160)})
    {
      Text::TextRasterResult result;
      DALI_TEST_CHECK(ts->RenderRasterPlanes(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4096, result, true, &viewport));
      if(result.geometry.fill.Empty())
      {
        DALI_TEST_CHECK(!result.fill && !result.colorGlyphMask);
        continue;
      }
      DALI_TEST_EQUALS(result.fill.GetWidth(), result.colorGlyphMask.GetWidth(), TEST_LOCATION);
      DALI_TEST_EQUALS(result.fill.GetHeight(), result.colorGlyphMask.GetHeight(), TEST_LOCATION);
      const auto  originalFill = Bytes(full.fill), originalMask = Bytes(full.colorGlyphMask);
      const auto  fill = Bytes(result.fill), mask = Bytes(result.colorGlyphMask);
      const auto& clipped = result.geometry.fill;
      for(int py = clipped.top; py < clipped.bottom; ++py)
        for(int px = clipped.left; px < clipped.right; ++px)
        {
          const size_t before = (py - bounds.top) * full.fill.GetWidth() + px - bounds.left;
          const size_t after  = (py - clipped.top) * result.fill.GetWidth() + px - clipped.left;
          DALI_TEST_EQUALS(originalMask[before], mask[after], TEST_LOCATION);
          for(unsigned channel = 0; channel < 4; ++channel) DALI_TEST_EQUALS(originalFill[before * 4 + channel], fill[after * 4 + channel], TEST_LOCATION);
        }
    }
  }
  END_TEST;
}

int UtcDaliTextSourcePlanesConstrainedRasterP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app, "MMMMMMMMMMMMMMMMMMMMMMMMMM");
  label.SetMultiLine(false);
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  Flush(app);
  auto                     ts = TypesetterFor(label);
  const Vector2            size(20, 80);
  const Vector4            viewport(0, 0, 20, 80);
  Text::Raster::SourceClip direct, constrained;
  DALI_TEST_CHECK(ts->ResolveSourceClip(size, viewport, direct));
  DALI_TEST_CHECK(direct.planes & Text::Raster::SourceClip::RIGHT);
  const auto storageClip = ts->RenderFill(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, &viewport, constrained);
  DALI_TEST_EQUALS(unsigned(constrained.planes), unsigned(direct.planes), TEST_LOCATION);
  const auto explicitClip = ts->Render(size, ControllerFor(label)->GetTextDirection(), Text::Typesetter::RENDER_NO_STYLES, false, Pixel::RGBA8888, Size::ZERO, nullptr, &direct);
  DALI_TEST_CHECK(Bytes(explicitClip) == Bytes(storageClip));
  const Vector4 inset(1, 0, 19, 80);
  DALI_TEST_CHECK(ts->RenderFill(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, &inset, constrained));
  DALI_TEST_CHECK(constrained.planes & Text::Raster::SourceClip::LEFT);
  const Vector4 invalid(0, 0, std::numeric_limits<float>::infinity(), 80);
  DALI_TEST_CHECK(!ts->RenderFill(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, &invalid, constrained));
  Text::Shadow shadow;
  shadow.SetOffset(Vector2(-15, 3));
  label.SetTextShadow(shadow);
  Flush(app);
  DALI_TEST_CHECK(TypesetterFor(label)->RenderFill(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, &viewport, constrained));
  DALI_TEST_CHECK(constrained.planes & Text::Raster::SourceClip::RIGHT);
  END_TEST;
}

int UtcDaliTextSourcePlanesAsyncViewportP(void)
{
  UiTestApplication app;
  auto              loader = Text::AsyncTextLoader::New();
  auto&             worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  for(float scale : {1.f, 1.5f, 2.f})
    for(float width : {30.25f, 31.25f, 32.25f, 33.25f, 34.25f})
    {
      Text::AsyncTextParameters params;
      params.requestType               = Ui::Integration::Text::Async::RENDER_FIXED_SIZE;
      params.text                      = "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM";
      params.fontFamily                = "DejaVu Sans";
      params.fontSize                  = 24;
      params.textWidth                 = width;
      params.textHeight                = 40.25f;
      params.maxTextureSize            = 4096;
      params.renderScale               = scale;
      params.ellipsis                  = false;
      params.isEffectOverflowRequested = true;
      params.isShadowEnabled           = true;
      params.shadowOffset              = Vector2(20, 20);
      params.shadowBlurRadius          = 5;
      params.isOutlineEnabled          = true;
      params.outlineWidth              = 6;
      params.outlineOffset             = Vector2(10, 10);
      const auto result                = LoadRequest(loader, params);
      DALI_TEST_CHECK(result.effectOverflow);
      const auto& geometry = result.effectOverflow->geometry;
      // Source is the fractional logical width, not ceil/even supersampled storage.
      DALI_TEST_CHECK(geometry.fill.right <= static_cast<int>(width * scale));
      DALI_TEST_CHECK(geometry.underlay.right > static_cast<int>(width * scale));
      auto                     ts = worker.*GetMember(WorkerTypesetter{});
      const Vector4            viewport(0, 0, width * scale, 40.25f * scale);
      Text::Raster::SourceClip clip;
      DALI_TEST_CHECK(ts->ResolveSourceClip(result.effectOverflow->legacyRasterSize, viewport, clip));
      DALI_TEST_CHECK(clip.planes & Text::Raster::SourceClip::RIGHT);
      params.ellipsis   = true;
      const auto elided = LoadRequest(loader, params);
      DALI_TEST_CHECK(elided.effectOverflow);
      ts = worker.*GetMember(WorkerTypesetter{});
      DALI_TEST_CHECK(ts->ResolveSourceClip(elided.effectOverflow->legacyRasterSize, viewport, clip));
      DALI_TEST_EQUALS(unsigned(clip.planes), 0u, TEST_LOCATION);
      Text::TextRasterResult full;
      DALI_TEST_CHECK(ts->RenderRasterPlanes(elided.effectOverflow->legacyRasterSize, Text::Direction::LEFT_TO_RIGHT,
                                             Pixel::L8, 4094, full));
      DALI_TEST_CHECK(SamePlanes(full, *elided.effectOverflow));
    }
  END_TEST;
}

int UtcDaliTextRasterPlanesGlobalGradientSourceP(void)
{
  UiTestApplication app;
  TextAbstraction::FontClient::Get().SetDpi(96, 96);
  Gradient::Linear gradient(Vector2(-.5f, -.5f), Vector2(.5f, .5f));
  gradient.SetStopNodes({Gradient::StopNode(0, UiColor(Color::RED)), Gradient::StopNode(1, UiColor(Color::BLUE))});
  for(auto mode : {Text::GradientSpan::BoundsMode::SPAN_BOUND, Text::GradientSpan::BoundsMode::CONTENT_BOUND, Text::GradientSpan::BoundsMode::VIEW_BOUND})
    for(unsigned alignment = 0; alignment < 3; ++alignment)
    {
      auto label = MakeLabel(app, "");
      label.SetMultiLine(false);
      label.SetTextOverflowMode(Text::OverflowMode::CLIP);
      label.SetHorizontalTextAlignment(static_cast<Text::Alignment>(alignment));
      auto builder = Text::StyledTextBuilder::New("AA BB CC DD EE");
      builder.SetSpan(Text::GradientSpan::New(gradient, mode), 3, 5);
      builder.SetSpan(Text::GradientSpan::New(gradient, mode), 9, 11);
      label.SetStyledText(builder.Build());
      Flush(app);
      auto* model                           = static_cast<const Text::Model*>(ControllerFor(label)->GetRenderTextModel());
      model->mVisualModel->mGlyphs[6].index = TextAbstraction::Test::COLOR_GLYPH_INDEX;
      for(auto& position : model->mVisualModel->mGlyphPositions) position.x -= 20.f;
      auto ts = TypesetterFor(label);
      const Vector2 size(320, 160);
      for(const Vector4 viewport : {Vector4(-100, -100, 700, 360), Vector4(0, 0, 320, 160), Vector4(10, 0, 30, 160), Vector4(0, 5, 60, 20)})
      {
        Text::Raster::SourceClip clip;
        DALI_TEST_CHECK(ts->ResolveSourceClip(size, viewport, clip));
        const auto*            active = clip.planes ? &clip : nullptr;
        Text::TextRasterResult result;
        DALI_TEST_CHECK(ts->RenderRasterPlanes(size, ControllerFor(label)->GetTextDirection(), Pixel::RGBA8888, 4094, result, false, &viewport, true));
        if(alignment == 0 && viewport.x < 0.f)
        {
          DALI_TEST_CHECK(result.geometry.fill.left < 0);
        }
        if(result.geometry.fill.Empty()) continue;
        DALI_TEST_CHECK(result.fill && result.textGradientMask && !result.colorGlyphMask);
        auto        preserved = ts->RenderTextGradientPreserved(size, Text::Direction::LEFT_TO_RIGHT, false, Pixel::RGBA8888, Size::ZERO, active);
        auto        mask      = ts->RenderTextGradientMask(size, Text::Direction::LEFT_TO_RIGHT, false, Pixel::L8, Size::ZERO, active);
        auto        p = Bytes(preserved), m = Bytes(mask), tightP = Bytes(result.fill), tightM = Bytes(result.textGradientMask);
        const auto& bounds = result.geometry.fill;
        for(int y = std::max(0, bounds.top); y < std::min(160, bounds.bottom); ++y)
          for(int x = std::max(0, bounds.left); x < std::min(320, bounds.right); ++x)
          {
            const size_t src = y * 320 + x, dst = (y - bounds.top) * result.fill.GetWidth() + x - bounds.left;
            DALI_TEST_EQUALS(tightM[dst], m[src], TEST_LOCATION);
            for(unsigned channel = 0; channel < 4; ++channel)
              DALI_TEST_EQUALS(tightP[dst * 4 + channel], p[src * 4 + channel], TEST_LOCATION);
          }
        DALI_TEST_CHECK(!ts->RenderRasterPlanes(size, Text::Direction::LEFT_TO_RIGHT, Pixel::L8, 4094, result, false, &viewport, true));
        DALI_TEST_CHECK(!result.fill && !result.textGradientMask);
      }
      label.Unparent();
    }
  END_TEST;
}

int UtcDaliTextRasterPlanesEmbossRawMaskP(void)
{
  UiTestApplication app;
  auto label = MakeLabel(app, "AB");
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  Flush(app);
  auto* model = static_cast<const Text::Model*>(ControllerFor(label)->GetRenderTextModel());
  auto& visual = *model->mVisualModel;
  visual.mGlyphPositions[1] = visual.mGlyphPositions[0];
  const Vector2 size(320, 160);
  for(bool nativeOnTop : {false, true})
  {
    visual.mGlyphs[nativeOnTop ? 0 : 1].index = TextAbstraction::Test::HALF_COVERAGE_GLYPH_INDEX;
    visual.mGlyphs[nativeOnTop ? 1 : 0].index = TextAbstraction::Test::COLOR_GLYPH_INDEX;
    auto ts = TypesetterFor(label);
    for(const Vector4 viewport : {Vector4(0, 0, 320, 160), Vector4(1, 0, 2, 160), Vector4(0, 1, 320, 1)})
    {
      Text::Raster::SourceClip clip;
      DALI_TEST_CHECK(ts->ResolveSourceClip(size, viewport, clip));
      Text::TextRasterResult result;
      DALI_TEST_CHECK(ts->RenderRasterPlanes(size, Text::Direction::LEFT_TO_RIGHT, Pixel::RGBA8888, 4094, result, false, &viewport, false, true));
      DALI_TEST_CHECK(!result.colorGlyphMask && !result.textGradientMask);
      if(result.geometry.fill.Empty()) continue;
      DALI_TEST_CHECK(result.embossMask);
      auto raw = ts->Render(size, Text::Direction::LEFT_TO_RIGHT, Text::Typesetter::RENDER_MASK, false, Pixel::L8, Size::ZERO, nullptr, clip.planes ? &clip : nullptr);
      auto a = Bytes(raw), b = Bytes(result.embossMask);
      const auto& bounds = result.geometry.fill;
      for(int y = std::max(0, bounds.top); y < std::min(160, bounds.bottom); ++y)
        for(int x = std::max(0, bounds.left); x < std::min(320, bounds.right); ++x)
          DALI_TEST_EQUALS(a[y * 320 + x], b[(y - bounds.top) * result.embossMask.GetWidth() + x - bounds.left], TEST_LOCATION);
      DALI_TEST_CHECK(!ts->RenderRasterPlanes(size, Text::Direction::LEFT_TO_RIGHT, Pixel::RGBA8888, 4094, result, true, &viewport, false, true));
      DALI_TEST_CHECK(!result.fill && !result.embossMask);
    }
  }
  END_TEST;
}

int UtcDaliTextRasterPlanesEmbossEllipsisP(void)
{
  UiTestApplication app;
  auto              loader = Text::AsyncTextLoader::New();
  auto&             worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  for(auto position : {Text::EllipsisPosition::START, Text::EllipsisPosition::MIDDLE, Text::EllipsisPosition::END})
    for(float scale : {1.f, 1.5f, 2.f})
      for(float width : {50.f, 500.f})
      {
        Text::AsyncTextParameters params;
        params.requestType               = Ui::Integration::Text::Async::RENDER_FIXED_SIZE;
        params.text                      = "Hello long text";
        params.fontFamily                = "DejaVu Sans";
        params.fontSize                  = 24;
        params.textWidth                 = width;
        params.textHeight                = 80;
        params.maxTextureSize            = 4096;
        params.renderScale               = scale;
        params.ellipsis                  = true;
        params.ellipsisPosition          = position;
        params.isEffectOverflowRequested = true;
        params.isEmbossEnabled           = true;
        params.embossDirection           = Vector2(-1, 1);
        params.embossStrength            = 8;
        params.isShadowEnabled           = true;
        params.shadowOffset              = Vector2(-12, 8);
        const auto result                = LoadRequest(loader, params);
        DALI_TEST_CHECK(result.effectOverflow);
        auto                     typesetter = worker.*GetMember(WorkerTypesetter{});
        Text::Raster::SourceClip clip;
        DALI_TEST_CHECK(typesetter->ResolveSourceClip(result.effectOverflow->legacyRasterSize, Vector4(0, 0, width * scale, 80 * scale), clip));
        DALI_TEST_EQUALS(unsigned(clip.planes), 0u, TEST_LOCATION);
        Text::TextRasterResult unclipped;
        DALI_TEST_CHECK(typesetter->RenderRasterPlanes(result.effectOverflow->legacyRasterSize, Text::Direction::LEFT_TO_RIGHT, Pixel::L8, 4094, unclipped));
        DALI_TEST_CHECK(SamePlanes(unclipped, *result.effectOverflow));
      }
  END_TEST;
}

int UtcDaliMarqueeSharedPaintFullSourceP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app, "Complete scrolling source remains available");
  static_cast<Ui::Integration::LabelImpl&>(GetImpl(label)).SetEffectOverflowEnabled(true);
  label.SetMultiLine(false);
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  label.SetRequestedWidth(40);
  Text::Shadow shadow;
  shadow.SetOffset(Vector2(-20, 5));
  shadow.SetBlurRadius(4);
  label.SetTextShadow(shadow);
  Text::Outline outline;
  outline.SetWidth(3);
  label.SetTextOutline(outline);
  Flush(app);
  label.StartMarquee();
  Flush(app);
  auto                   controller = ControllerFor(label);
  auto                   ts         = TypesetterFor(label);
  const Vector2          size(controller->GetNaturalSize().x + 12, 100);
  Text::TextRasterResult result;
  DALI_TEST_CHECK(ts->RenderRasterPlanes(size, controller->GetTextDirection(), Pixel::RGBA8888, 4094, result,
                                         false, nullptr, false, false, true));
  DALI_TEST_CHECK(result.geometry.fill.right > 40);
  DALI_TEST_CHECK(result.geometry.underlay.left < result.geometry.fill.left);
  const auto reference = ts->Render(size, controller->GetTextDirection(), Text::Typesetter::RENDER_NO_STYLES, true);
  const auto a = Bytes(reference), b = Bytes(result.fill);
  for(int y = result.geometry.fill.top; y < result.geometry.fill.bottom; ++y)
  {
    for(int x = result.geometry.fill.left; x < result.geometry.fill.right; ++x)
    {
      if(x < 0 || y < 0 || x >= static_cast<int>(reference.GetWidth()) || y >= static_cast<int>(reference.GetHeight())) continue;
      for(unsigned c = 0; c < 4; ++c)
      {
        DALI_TEST_EQUALS(a[(y * reference.GetWidth() + x) * 4 + c],
                         b[((y - result.geometry.fill.top) * result.fill.GetWidth() + x - result.geometry.fill.left) * 4 + c], TEST_LOCATION);
      }
    }
  }
  label.SetMarqueeStopMode(Text::MarqueeStopMode::IMMEDIATE);
  label.StopMarquee();
  Flush(app);
  label.Unparent();
  ts.Reset();
  controller.Reset();
  label.Reset();
  Flush(app);
  END_TEST;
}

int UtcDaliMarqueeSharedPaintAsyncPeriodP(void)
{
  UiTestApplication app;
  auto              loader = Text::AsyncTextLoader::New();
  for(auto orientation : {Text::MarqueeOrientation::HORIZONTAL, Text::MarqueeOrientation::VERTICAL})
  {
    for(int gap : {0, 12, 400})
    {
      Text::AsyncTextParameters params;
      params.requestType               = Ui::Integration::Text::Async::RENDER_FIXED_SIZE;
      params.text                      = "First line of source\nSecond line of source\nThird line";
      params.fontFamily                = "DejaVu Sans";
      params.fontSize                  = 24;
      params.textWidth                 = 80;
      params.textHeight                = 40;
      params.maxTextureSize            = 4096;
      params.isMarqueeEnabled          = true;
      params.marqueeOrientation        = orientation;
      params.marqueeGap                = gap;
      const auto legacy                = LoadRequest(loader, params);
      params.isEffectOverflowRequested = true;
      const auto modern                = LoadRequest(loader, params);
      DALI_TEST_CHECK(modern.effectOverflow);
      DALI_TEST_CHECK(!modern.marqueePixelData && !modern.textPixelData && !modern.stylePixelData);
      DALI_TEST_EQUALS(modern.size, legacy.size, TEST_LOCATION);
      DALI_TEST_EQUALS(modern.controlSize, legacy.controlSize, TEST_LOCATION);
      DALI_TEST_EQUALS(modern.marqueeWrapGap, legacy.marqueeWrapGap, TEST_LOCATION);
      DALI_TEST_EQUALS(modern.marqueeTextureAnchor.valid, legacy.marqueeTextureAnchor.valid, TEST_LOCATION);
      if(legacy.marqueeTextureAnchor.valid)
      {
        DALI_TEST_EQUALS(modern.marqueeTextureAnchor.textureX, legacy.marqueeTextureAnchor.textureX, TEST_LOCATION);
      }
    }
  }
  END_TEST;
}

int UtcDaliMarqueeSharedPaintShaderIsolationP(void)
{
  UiTestApplication app;
  auto              first = MakeLabel(app, "First scrolling source"), second = MakeLabel(app, "Second scrolling source");
  for(auto label : {first, second})
  {
    label.SetMultiLine(false);
    label.SetRequestedWidth(70);
    static_cast<Ui::Integration::LabelImpl&>(GetImpl(label)).SetEffectOverflowEnabled(true);
  }
  Flush(app);
  first.StartMarquee();
  second.StartMarquee();
  Flush(app);
  DALI_TEST_EQUALS(first.GetRendererCount(), 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(second.GetRendererCount(), 1u, TEST_LOCATION);
  const auto a = first.GetRendererAt(0), b = second.GetRendererAt(0);
  DALI_TEST_CHECK(a.GetPropertyIndex("uMarqueeSupport") != Property::INVALID_INDEX);
  DALI_TEST_CHECK(b.GetPropertyIndex("uMarqueeSupport") != Property::INVALID_INDEX);
  DALI_TEST_CHECK(a.GetShader() != b.GetShader());
  DALI_TEST_CHECK(a.GetTextures().GetTextureCount() == 1u);
  const auto texture = a.GetTextures().GetTexture(0);
  for(unsigned i = 0; i < 10; ++i)
  {
    app.SendNotification();
    app.Render(16);
  }
  DALI_TEST_CHECK(first.GetRendererAt(0) == a);
  DALI_TEST_CHECK(a.GetTextures().GetTexture(0) == texture);
  END_TEST;
}

int UtcDaliMarqueeSharedPaintAsyncMasksP(void)
{
  UiTestApplication         app;
  auto                      loader = Text::AsyncTextLoader::New();
  Text::AsyncTextParameters params;
  params.requestType               = Ui::Integration::Text::Async::RENDER_FIXED_SIZE;
  params.text                      = "Text source with effects";
  params.fontSize                  = 24;
  params.textWidth                 = 80;
  params.textHeight                = 80;
  params.maxTextureSize            = 4096;
  params.isMarqueeEnabled          = true;
  params.isEffectOverflowRequested = true;
  params.isShadowEnabled           = true;
  params.shadowOffset              = Vector2(-40, 8);
  params.shadowBlurRadius          = 5;
  params.isUnderlineEnabled        = true;
  params.underlineHeight           = 3;
  for(bool gradient : {false, true})
  {
    params.isTextGradientRequested    = gradient;
    params.hasStyledTextStyleSnapshot = true;
    params.styledTextStyleSnapshot.foregroundColorRuns.push_back({0u, 4u, Color::RED, 0u});
    auto result = LoadRequest(loader, params);
    DALI_TEST_CHECK(result.effectOverflow);
    DALI_TEST_CHECK(result.effectOverflow->fill && result.effectOverflow->underlay && result.effectOverflow->overlay);
    DALI_TEST_EQUALS(static_cast<bool>(result.effectOverflow->textGradientMask), gradient, TEST_LOCATION);
    DALI_TEST_CHECK(!result.effectOverflow->colorGlyphMask && !result.effectOverflow->embossMask);
  }
  END_TEST;
}

int UtcDaliMarqueeSharedPaintContributorBoundsP(void)
{
  const Text::Raster::Bounds fill{0, 0, 10, 10};
  const Text::Raster::Bounds expanded{-25, -25, 35, 35};
  Vector2                    support;
  for(bool horizontal : {false, true})
  {
    DALI_TEST_CHECK(Text::ResolveMarqueePaintSupport(expanded, fill, Vector2(10, 10), horizontal, false, Vector2::ZERO, 0, support));
    unsigned maximum = 0;
    for(unsigned i = 0; i < 100; ++i)
    {
      const double coordinate = i / 100.;
      unsigned     count      = 0;
      for(int copy = -20; copy <= 20; ++copy)
      {
        const double local = coordinate - copy;
        if(local >= support.x && local <= support.y) ++count;
      }
      maximum = std::max(maximum, count);
      DALI_TEST_CHECK(count <= Text::MARQUEE_PAINT_COPIES);
    }
    DALI_TEST_CHECK(maximum > 2u);
  }
  DALI_TEST_CHECK(!Text::ResolveMarqueePaintSupport(expanded, fill, Vector2(1, 1), true, false, Vector2::ZERO, 0, support));
  DALI_TEST_CHECK(!Text::ResolveMarqueePaintSupport(expanded, fill, Vector2::ZERO, true, false, Vector2::ZERO, 0, support));
  DALI_TEST_CHECK(!Text::ResolveMarqueePaintSupport(fill, fill, Vector2(10, 10), true, true, Vector2::ZERO, 1, support));
  DALI_TEST_CHECK(Text::ResolveMarqueePaintSupport(fill, fill, Vector2(10, 10), true, true, Vector2(1, 0), 20, support));
  DALI_TEST_CHECK(support.x < -2.f && support.y > 3.f);
  END_TEST;
}

int UtcDaliMarqueeSharedPaintLogicalScaleP(void)
{
  UiTestApplication app;
  auto              loader = Text::AsyncTextLoader::New();
  for(auto orientation : {Text::MarqueeOrientation::HORIZONTAL, Text::MarqueeOrientation::VERTICAL})
  {
    Text::AsyncTextParameters params;
    params.requestType               = Ui::Integration::Text::Async::RENDER_FIXED_SIZE;
    params.text                      = "Original source period\nSecond line";
    params.fontFamily                = "DejaVu Sans";
    params.fontSize                  = 24;
    params.textWidth                 = 82;
    params.textHeight                = 80;
    params.maxTextureSize            = 4096;
    params.isMarqueeEnabled          = true;
    params.marqueeOrientation        = orientation;
    params.marqueeGap                = 13;
    params.isEffectOverflowRequested = true;
    const auto reference             = LoadRequest(loader, params);
    DALI_TEST_CHECK(reference.effectOverflow);
    for(float scale : {1.5f, 2.f, 1.f})
    {
      params.renderScale = scale;
      auto result        = LoadRequest(loader, params);
      DALI_TEST_CHECK(result.effectOverflow);
      DALI_TEST_EQUALS(orientation == Text::MarqueeOrientation::HORIZONTAL ? result.size.x : result.size.y,
                       orientation == Text::MarqueeOrientation::HORIZONTAL ? reference.size.x : reference.size.y, .001f, TEST_LOCATION);
      DALI_TEST_EQUALS(result.marqueeWrapGap, reference.marqueeWrapGap, .001f, TEST_LOCATION);
      DALI_TEST_EQUALS(result.controlSize, reference.controlSize, TEST_LOCATION);
    }
  }
  END_TEST;
}

int UtcDaliTextRevealSharedWorkerParityP(void)
{
  UiTestApplication app;
  auto              loader = Text::AsyncTextLoader::New();
  auto&             worker = Text::GetImplementation(loader);
  (worker.*GetMember(WorkerModule{})).GetFontClient().SetDpi(96, 96);
  for(float scale : {1.f, 1.5f, 2.f, 1.f})
  {
    for(unsigned unit = 1; unit <= 4; ++unit)
    {
      for(unsigned sequence = 0; sequence < 2; ++sequence)
      {
        Text::AsyncTextParameters params;
        params.requestType                    = Ui::Integration::Text::Async::RENDER_FIXED_SIZE;
        params.text                           = "First final line\nSecond final line and clipped tail";
        params.fontFamily                     = "DejaVu Sans";
        params.fontSize                       = 24;
        params.textWidth                      = 150.25f;
        params.textHeight                     = 85.25f;
        params.maxTextureSize                 = 4096;
        params.renderScale                    = scale;
        params.ellipsis                       = sequence != 0;
        params.isMultiLine                    = true;
        params.isTextRevealEnabled            = true;
        params.textRevealUnit                 = static_cast<Text::Internal::Reveal::Unit>(unit);
        params.textRevealSequence             = static_cast<Text::Internal::Reveal::Sequence>(sequence);
        params.textRevealSequenceStaggerRatio = .25f;
        params.textRevealFadeDurationRatio    = .2f;
        params.isShadowEnabled                = true;
        params.shadowOffset                   = Vector2(-12, 8);
        params.shadowBlurRadius               = 3;
        params.isOutlineEnabled               = true;
        params.outlineWidth                   = 3;
        params.outlineOffset                  = Vector2(5, -2);
        params.isUnderlineEnabled             = true;
        params.underlineHeight                = 3;
        const auto ordinary                   = LoadRequest(loader, params);
        params.isEffectOverflowRequested      = true;
        const auto shared                     = LoadRequest(loader, params);
        DALI_TEST_CHECK(shared.effectOverflow);
        DALI_TEST_CHECK(!shared.textPixelData && !shared.stylePixelData && !shared.overlayStylePixelData);
        DALI_TEST_CHECK(Bytes(ordinary.textPixelData) == Bytes(shared.effectOverflow->fill));
        DALI_TEST_CHECK(Bytes(ordinary.stylePixelData) == Bytes(shared.effectOverflow->underlay));
        DALI_TEST_CHECK(Bytes(ordinary.overlayStylePixelData) == Bytes(shared.effectOverflow->overlay));
        DALI_TEST_EQUALS(shared.revealMetadataTiles.size(), ordinary.revealMetadataTiles.size(), TEST_LOCATION);
        DALI_TEST_CHECK(!shared.revealMetadataTiles.empty());
        DALI_TEST_CHECK(Bytes(shared.revealMetadataTiles.front()) == Bytes(ordinary.revealMetadataTiles.front()));
        DALI_TEST_EQUALS(shared.textRevealFadeDuration, ordinary.textRevealFadeDuration, TEST_LOCATION);
        DALI_TEST_EQUALS(shared.effectOverflow->geometry.fill.left, 0, TEST_LOCATION);
        DALI_TEST_EQUALS(shared.effectOverflow->geometry.fill.right, static_cast<int>(ordinary.textPixelData.GetWidth()), TEST_LOCATION);
      }
    }
  }
  END_TEST;
}

int UtcDaliTextRevealBoundedPayloadP(void)
{
  UiTestApplication app;
  auto              label = MakeLabel(app, "");
  label.SetTextOverflowMode(Text::OverflowMode::CLIP);
  label.SetStyledText(Text::StyledText::FromMarkup("<background color='blue'><u type='dashed' height='3' dash-width='4' dash-gap='2'><s height='2'>Long styled text past the source edge</s></u></background>"));
  Flush(app);
  auto                   ts = TypesetterFor(label);
  const Vector2          size(80, 60);
  Text::TextRasterResult result;
  for(unsigned mask = 0; mask < 4; ++mask)
  {
    const auto direction = ControllerFor(label)->GetTextDirection();
    DALI_TEST_CHECK(ts->RenderRevealPlanes(size, direction, Pixel::RGBA8888, 4096, result, mask == 1, mask == 2, mask == 3));
    PixelData  coefficient;
    const auto fill = mask == 2 ? ts->RenderTextGradientPreserved(size, direction)
                                : ts->Render(size, direction, Text::Typesetter::RENDER_NO_STYLES, false, Pixel::RGBA8888, Size::ZERO, mask == 1 ? &coefficient : nullptr);
    DALI_TEST_CHECK(Bytes(result.fill) == Bytes(fill));
    DALI_TEST_CHECK(Bytes(result.colorGlyphMask) == Bytes(coefficient));
    if(mask == 2)
    {
      DALI_TEST_CHECK(Bytes(result.textGradientMask) == Bytes(ts->RenderTextGradientMask(size, direction)));
    }
    if(mask == 3)
    {
      DALI_TEST_CHECK(Bytes(result.embossMask) == Bytes(ts->Render(size, direction, Text::Typesetter::RENDER_MASK, false, Pixel::L8)));
    }
    DALI_TEST_CHECK(Bytes(result.underlay) == Bytes(ts->Render(size, direction, Text::Typesetter::RENDER_NO_TEXT)));
    DALI_TEST_CHECK(Bytes(result.overlay) == Bytes(ts->Render(size, direction, Text::Typesetter::RENDER_OVERLAY_STYLE)));
  }
  DALI_TEST_CHECK(!ts->RenderRevealPlanes(size, Text::Direction::LEFT_TO_RIGHT, Pixel::RGBA8888, 80, result, false, false, false));
  DALI_TEST_CHECK(!result.fill && !result.underlay && !result.overlay);
  DALI_TEST_CHECK(!ts->RenderRevealPlanes(Vector2(-1, 60), Text::Direction::LEFT_TO_RIGHT, Pixel::RGBA8888, 4096, result, false, false, false));
  DALI_TEST_CHECK(ts->RenderRevealPlanes(size, Text::Direction::LEFT_TO_RIGHT, Pixel::L8, 4096, result, false, false, false));
  END_TEST;
}
