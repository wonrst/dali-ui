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

// Install the test callback before the task includes the adaptor declaration.
#include <dali-ui/ui-event-thread-callback.h>

#include <dali-ui-foundation/integration-api/label-impl.h>
#include <dali-ui-foundation/internal/text/async-text/text-loading-task.h>
#include <dali-ui-foundation/internal/text/styled-text/gradient-span-data.h>
#include <dali-ui-foundation/public-api/text/label-properties.h>
#include <dali-ui-foundation/public-api/views/text-controls/label.h>
#include <dali-ui-test-suite-utils.h>
#include <dali/devel-api/adaptor-framework/image-loading-devel.h>
#include <dali/devel-api/text-abstraction/font-client.h>
#include <dali/integration-api/pixel-data-integ.h>
#include <dali/integration-api/rendering/visual-renderer.h>
#include <cstring>
#include <limits>

using namespace Dali;
namespace
{
namespace Text = Ui::Text;
Ui::Internal::TextLoadingTaskPtr Run(Text::AsyncTextParameters parameters)
{
  Ui::Internal::TextLoadingTaskPtr task = new Ui::Internal::TextLoadingTask(1u, std::move(parameters), nullptr);
  task->mLoader                         = Text::AsyncTextLoader::New();
  task->Process();
  return task;
}
Text::AsyncTextParameters Parameters(float width, float height, float scale)
{
  Text::AsyncTextParameters p;
  p.text           = "Render scale boundary";
  p.fontSize       = 12.0f;
  p.textWidth      = width;
  p.textHeight     = height;
  p.renderScale    = scale;
  p.maxTextureSize = 128;
  p.requestType    = Ui::Integration::Text::Async::RENDER_FIXED_SIZE;
  return p;
}
void SamePixels(PixelData a, PixelData b)
{
  DALI_TEST_CHECK(a && b);
  if(!a || !b)
  {
    return;
  }
  DALI_TEST_EQUALS(a.GetWidth(), b.GetWidth(), TEST_LOCATION);
  DALI_TEST_EQUALS(a.GetHeight(), b.GetHeight(), TEST_LOCATION);
  DALI_TEST_EQUALS(a.GetPixelFormat(), b.GetPixelFormat(), TEST_LOCATION);
  const auto   x     = Integration::GetPixelDataBuffer(a);
  const auto   y     = Integration::GetPixelDataBuffer(b);
  const size_t bytes = a.GetWidth() * a.GetHeight() * Pixel::GetBytesPerPixel(a.GetPixelFormat());
  if(a.GetWidth() == b.GetWidth() && a.GetHeight() == b.GetHeight() && a.GetPixelFormat() == b.GetPixelFormat())
  {
    DALI_TEST_EQUALS(std::memcmp(x.buffer, y.buffer, bytes), 0, TEST_LOCATION);
  }
}
bool finished = false;
void Finished(Ui::View, float, float)
{
  finished = true;
}
bool Wait(UiTestApplication& app)
{
  for(int i = 0; i < 10 && !finished; ++i)
  {
    Test::WaitForEventThreadTrigger(1, 5);
    app.SendNotification();
    app.Render();
  }
  return finished;
}
void CheckTiles(Ui::Label label, uint32_t maximum)
{
  DALI_TEST_CHECK(label.GetRendererCount() > 0u);
  float previousEnd = 0.0f;
  for(uint32_t i = 0; i < label.GetRendererCount(); ++i)
  {
    auto renderer = label.GetRendererAt(i);
    auto textures = renderer.GetTextures();
    for(uint32_t t = 0; t < textures.GetTextureCount(); ++t)
    {
      auto texture = textures.GetTexture(t);
      if(texture)
      {
        DALI_TEST_CHECK(texture.GetWidth() <= maximum);
        DALI_TEST_CHECK(texture.GetHeight() <= maximum);
      }
    }
    if(label.GetRendererCount() > 1u)
    {
      auto size   = renderer.GetProperty<Vector2>(VisualRenderer::Property::TRANSFORM_SIZE);
      auto offset = renderer.GetProperty<Vector2>(VisualRenderer::Property::TRANSFORM_OFFSET);
      if(i)
      {
        DALI_TEST_EQUALS(offset.y, previousEnd, 0.001f, TEST_LOCATION);
      }
      DALI_TEST_EQUALS(size.y, float(textures.GetTexture(0u).GetHeight()), 0.001f, TEST_LOCATION);
      previousEnd = offset.y + size.y;
    }
  }
}
} //namespace

int UtcDaliTextRenderScaleFallbackWidthP(void)
{
  UiTestApplication application;
  for(float width : {63.0f, 64.0f, 64.5f, 65.0f, 128.0f, 129.0f, 300.0f})
  {
    auto p      = Parameters(width, 48.0f, 2.0f);
    auto result = Run(p);
    DALI_TEST_CHECK(result->mRenderInfo.textPixelData);
    DALI_TEST_CHECK(result->mRenderInfo.textPixelData.GetWidth() <= 128u);
    DALI_TEST_EQUALS(result->mParameters.renderScale, width <= 64.0f ? 2.0f : 1.0f, TEST_LOCATION);
    if(width > 64.0f)
    {
      p.renderScale  = 1.0f;
      auto reference = Run(p);
      SamePixels(result->mRenderInfo.textPixelData, reference->mRenderInfo.textPixelData);
      DALI_TEST_EQUALS(result->mRenderInfo.size, reference->mRenderInfo.size, TEST_LOCATION);
    }
  }
  END_TEST;
}

int UtcDaliTextRenderScaleFallbackHeightAndStylesP(void)
{
  UiTestApplication application;
  for(float height : {63.0f, 64.0f, 64.5f, 65.0f, 128.0f, 129.0f, 400.0f})
  {
    for(int style = 0; style < 4; ++style)
    {
      auto p = Parameters(48.0f, height, 2.0f);
      p.text.clear();
      for(int i = 0; i < 100; ++i)
      {
        p.text += "A אב 😀\n";
      }
      p.isMultiLine        = true;
      p.ellipsis           = false;
      p.isShadowEnabled    = style == 1;
      p.shadowOffset       = Vector2(2.0f, 3.0f);
      p.isOutlineEnabled   = style == 2;
      p.outlineWidth       = 2;
      p.isUnderlineEnabled = style == 3;
      auto result          = Run(p);
      DALI_TEST_CHECK(result->mRenderInfo.textPixelData);
      DALI_TEST_EQUALS(result->mParameters.renderScale, height <= 64.0f ? 2.0f : 1.0f, TEST_LOCATION);
      if(height > 64.0f)
      {
        p.renderScale  = 1.0f;
        auto reference = Run(p);
        SamePixels(result->mRenderInfo.textPixelData, reference->mRenderInfo.textPixelData);
        if(style == 1 || style == 2)
        {
          SamePixels(result->mRenderInfo.stylePixelData, reference->mRenderInfo.stylePixelData);
        }
        if(style == 3)
        {
          SamePixels(result->mRenderInfo.overlayStylePixelData, reference->mRenderInfo.overlayStylePixelData);
        }
      }
    }
  }
  END_TEST;
}

int UtcDaliTextRenderScaleFallbackFitAndMarqueeP(void)
{
  UiTestApplication application;
  auto              p = Parameters(100.0f, 48.0f, 2.0f);
  p.isTextFitEnabled  = true;
  auto result         = Run(p);
  p.renderScale       = 1.0f;
  auto reference      = Run(p);
  DALI_TEST_EQUALS(result->mParameters.renderScale, 1.0f, TEST_LOCATION);
  SamePixels(result->mRenderInfo.textPixelData, reference->mRenderInfo.textPixelData);
  p                  = Parameters(48.0f, 48.0f, 2.0f);
  p.isMarqueeEnabled = true;
  result             = Run(p);
  DALI_TEST_EQUALS(result->mParameters.renderScale, 2.0f, TEST_LOCATION);
  DALI_TEST_CHECK(result->mRenderInfo.textPixelData.GetWidth() <= 128u);
  END_TEST;
}

int UtcDaliLabelRenderScaleFallbackAutoAndTilingP(void)
{
  UiTestApplication application;
  (void)TextAbstraction::FontClient::Get();
  const uint32_t maximum = Dali::GetMaxTextureSize();
  Ui::Label      label   = Ui::Label::New("Auto async fallback");
  label.SetRequestedWidth(float(maximum / 2u + 1u));
  label.SetRequestedHeight(80.0f);
  label.SetAsyncRendering(true);
  label.SetRenderScale(2.0f);
  label.AsyncRenderFinishedSignal().Connect(&Finished);
  finished = false;
  application.GetScene().Add(label);
  application.SendNotification();
  application.Render();
  DALI_TEST_CHECK(Wait(application));
  CheckTiles(label, maximum);
  DALI_TEST_EQUALS(label.GetRenderScale(), 2.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(label.GetRendererAt(0u).GetTextures().GetTexture(0u).GetWidth(), maximum / 2u + 1u, TEST_LOCATION);
  finished = false;
  label.SetText("Updated automatic request");
  label.SetRequestedWidth(120.0f);
  application.SendNotification();
  application.Render();
  DALI_TEST_CHECK(Wait(application));
  DALI_TEST_EQUALS(label.GetRendererAt(0u).GetTextures().GetTexture(0u).GetWidth(), 240u, TEST_LOCATION);
  std::string lines;
  for(uint32_t i = 0; i < maximum / 8u + 100u; ++i)
  {
    lines += "Tile\n";
  }
  finished = false;
  label.SetText(String(lines.c_str()));
  label.SetMultiLine(true);
  label.SetRequestedHeight(float(maximum + 100u));
  application.SendNotification();
  application.Render();
  DALI_TEST_CHECK(Wait(application));
  DALI_TEST_CHECK(label.GetRendererCount() > 1u);
  CheckTiles(label, maximum);
  label.SetAsyncRendering(false);
  application.SendNotification();
  application.Render();
  DALI_TEST_CHECK(label.GetRendererCount() > 1u);
  CheckTiles(label, maximum);
  END_TEST;
}

int UtcDaliLabelRenderScaleInvalidInputP(void)
{
  UiTestApplication application;
  auto              label = Ui::Label::New("finite");
  for(float value : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
  {
    label.SetRenderScale(2.f);
    label.SetRenderScale(value);
    DALI_TEST_EQUALS(label.GetRenderScale(), 1.f, TEST_LOCATION);
  }
  auto p         = Parameters(64.f, 48.f, std::numeric_limits<float>::max());
  auto result    = Run(p);
  p.renderScale  = 1.f;
  auto reference = Run(p);
  SamePixels(result->mRenderInfo.textPixelData, reference->mRenderInfo.textPixelData);
  END_TEST;
}

int UtcDaliTextRenderScaleFallbackRequestKindsAndSpanP(void)
{
  UiTestApplication application;
  for(auto request : {Ui::Integration::Text::Async::RENDER_FIXED_SIZE,
                      Ui::Integration::Text::Async::RENDER_FIXED_WIDTH,
                      Ui::Integration::Text::Async::RENDER_FIXED_HEIGHT,
                      Ui::Integration::Text::Async::RENDER_CONSTRAINT})
  {
    auto p                                 = Parameters(100.f, 200.f, 2.f);
    p.requestType                          = request;
    p.isMultiLine                          = true;
    p.text                                 = "Gradient span אב 😀 with several wrapping lines";
    p.hasStyledTextStyleSnapshot           = true;
    auto data                              = std::make_shared<Text::Internal::StyledTextGradientSnapshotData>();
    p.styledTextStyleSnapshot.gradientData = data;
    Text::Internal::StyledTextGradientRunSnapshot run;
    run.numberOfCharacters = 10;
    run.style.enabled      = true;
    run.style.type         = Ui::Gradient::Type::LINEAR;
    run.style.units        = Ui::Gradient::Units::USER_SPACE;
    run.style.linearStart  = Vector2::ZERO;
    run.style.linearEnd    = Vector2(80, 40);
    run.style.stops.PushBack({0.f, Color::RED});
    run.style.stops.PushBack({1.f, Color::BLUE});
    data->runs.push_back(run);
    auto result    = Run(p);
    p.renderScale  = 1.f;
    auto reference = Run(p);
    DALI_TEST_EQUALS(result->mParameters.renderScale, 1.f, TEST_LOCATION);
    SamePixels(result->mRenderInfo.textPixelData, reference->mRenderInfo.textPixelData);
  }
  END_TEST;
}

int UtcDaliLabelRenderScaleFallbackParentAndManualP(void)
{
  UiTestApplication application;
  (void)TextAbstraction::FontClient::Get();
  const uint32_t maximum = Dali::GetMaxTextureSize();
  auto           parent  = Ui::View::New();
  parent.SetRequestedWidth(float(maximum / 2 + 10));
  parent.SetRequestedHeight(100.f);
  auto label = Ui::Label::New("Parent resize issues an automatic async request with wrapping text");
  label.SetRequestedWidth(Ui::MATCH_PARENT);
  label.SetRequestedHeight(100.f);
  label.SetAsyncRendering(true);
  label.SetRenderScale(2.f);
  label.SetMultiLine(true);
  label.AsyncRenderFinishedSignal().Connect(&Finished);
  parent.Add(label);
  finished = false;
  application.GetScene().Add(parent);
  application.SendNotification();
  application.Render();
  DALI_TEST_CHECK(Wait(application));
  CheckTiles(label, maximum);
  DALI_TEST_EQUALS(label.GetRendererAt(0).GetTextures().GetTexture(0).GetWidth(), maximum / 2 + 10, TEST_LOCATION);
  finished = false;
  parent.SetRequestedWidth(120.f);
  label.SetLineWrapMode(Text::LineWrapMode::CHARACTER);
  application.SendNotification();
  application.Render();
  DALI_TEST_CHECK(Wait(application));
  DALI_TEST_EQUALS(label.GetRendererAt(0).GetTextures().GetTexture(0).GetWidth(), 240u, TEST_LOCATION);
  auto& impl = static_cast<Ui::Integration::LabelImpl&>(label.GetImplementation());
  for(int i = 0; i < 4; ++i)
  {
    finished = false;
    switch(i)
    {
      case 0:
      {
        impl.RequestAsyncRenderWithFixedSize(float(maximum / 2 + 10), 100.f);
        break;
      }

      case 1:
      {
        impl.RequestAsyncRenderWithFixedWidth(float(maximum / 2 + 10), 100.f);
        break;
      }

      case 2:
      {
        impl.RequestAsyncRenderWithFixedHeight(float(maximum), 100.f);
        break;
      }

      case 3:
      {
        impl.RequestAsyncRenderWithConstraints(float(maximum), 100.f);
        break;
      }
    }
    DALI_TEST_CHECK(Wait(application));
    CheckTiles(label, maximum);
  }
  finished = false;
  for(int i = 0; i < 4; ++i)
  {
    application.SendNotification();
    application.Render();
    Test::WaitForEventThreadTrigger(1, 0);
  }
  DALI_TEST_CHECK(!finished);
  END_TEST;
}

int UtcDaliTextRenderScaleEarlyFallbackP(void)
{
  UiTestApplication application;
  for(auto request : {Ui::Integration::Text::Async::RENDER_FIXED_SIZE,
                      Ui::Integration::Text::Async::RENDER_FIXED_WIDTH,
                      Ui::Integration::Text::Async::RENDER_FIXED_HEIGHT,
                      Ui::Integration::Text::Async::RENDER_CONSTRAINT})
  {
    for(bool longText : {false, true})
    {
      auto parameters        = Parameters(48.0f, 300.0f, 2.0f);
      parameters.requestType = request;
      parameters.isMultiLine = true;
      parameters.ellipsis    = false;
      parameters.text        = "Hi";
      if(longText)
      {
        for(int i = 0; i < 60; ++i)
        {
          parameters.text += "\nTile";
        }
      }
      auto actual            = Run(parameters);
      parameters.renderScale = 1.0f;
      auto expected          = Run(parameters);
      DALI_TEST_EQUALS(actual->mParameters.renderScale, 1.0f, TEST_LOCATION);
      SamePixels(actual->mRenderInfo.textPixelData, expected->mRenderInfo.textPixelData);
      DALI_TEST_EQUALS(actual->mRenderInfo.size, expected->mRenderInfo.size, TEST_LOCATION);

      parameters             = Parameters(48.0f, 24.0f, 2.0f);
      parameters.requestType = request;
      parameters.text        = "Hi";
      actual                 = Run(parameters);
      DALI_TEST_CHECK(actual->mRenderInfo.textPixelData);
      DALI_TEST_EQUALS(actual->mParameters.renderScale, 2.0f, TEST_LOCATION);
    }
  }
  END_TEST;
}

int UtcDaliTextRenderScaleFinalFallbackAfterEarlyCheckP(void)
{
  UiTestApplication application;
  for(bool cutout : {false, true})
  {
    // Both requested axes fit at RS2, but the actual raster grows beyond 128.
    auto parameters = Parameters(cutout ? 60.0f : 48.0f, cutout ? 40.0f : 24.0f, 2.0f);
    parameters.text = "Tall scrolling";
    if(cutout)
    {
      parameters.padding                       = Insets(10, 10, 10, 10);
      parameters.isCutoutEnabled               = true;
      parameters.isBackgroundWithCutoutEnabled = true;
      parameters.backgroundColorWithCutout     = Color::RED;
    }
    else
    {
      parameters.fontSize         = 100.0f;
      parameters.isMarqueeEnabled = true;
    }
    auto actual            = Run(parameters);
    parameters.renderScale = 1.0f;
    auto expected          = Run(parameters);
    DALI_TEST_EQUALS(actual->mParameters.renderScale, 1.0f, TEST_LOCATION);
    SamePixels(actual->mRenderInfo.textPixelData, expected->mRenderInfo.textPixelData);
    DALI_TEST_EQUALS(actual->mRenderInfo.size, expected->mRenderInfo.size, TEST_LOCATION);
    DALI_TEST_EQUALS(actual->mRenderInfo.marqueeWrapGap, expected->mRenderInfo.marqueeWrapGap, TEST_LOCATION);
    if(cutout)
    {
      SamePixels(actual->mRenderInfo.stylePixelData, expected->mRenderInfo.stylePixelData);
    }
  }
  END_TEST;
}

int UtcDaliTextRenderScaleEarlyCheckPreservesMeasurementsP(void)
{
  UiTestApplication application;
  for(auto request : {Ui::Integration::Text::Async::COMPUTE_NATURAL_SIZE,
                      Ui::Integration::Text::Async::COMPUTE_HEIGHT_FOR_WIDTH})
  {
    auto parameters        = Parameters(48.0f, 300.0f, 2.0f);
    parameters.requestType = request;
    parameters.isMultiLine = true;
    auto actual            = Run(parameters);
    parameters.renderScale = 1.0f;
    auto expected          = Run(parameters);
    DALI_TEST_EQUALS(actual->mParameters.renderScale, 2.0f, TEST_LOCATION);
    DALI_TEST_EQUALS(actual->mRenderInfo.size, expected->mRenderInfo.size, TEST_LOCATION);
    DALI_TEST_CHECK(!actual->mRenderInfo.textPixelData);
  }
  END_TEST;
}
