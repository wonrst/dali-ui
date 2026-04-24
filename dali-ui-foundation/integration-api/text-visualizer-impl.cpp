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

// EXTERNAL INCLUDES
#include <dali/devel-api/object/type-registry-helper.h>

// INTERNAL INCLUDES
#include <dali-ui-foundation/integration-api/property-registration-helper.h>
#include <dali-ui-foundation/integration-api/text-visualizer-impl.h>
#include <dali-ui-foundation/integration-api/text-visualizer-property-handler.h>
#include <dali-ui-foundation/internal/text/text-visualizer/atlas-renderer-bridge.h>
#include <dali-ui-foundation/internal/text/text-visualizer/atlas-view-adapter.h>
#include <dali-ui-foundation/internal/text/text-visualizer/layout-engine.h>
#include <dali-ui-foundation/internal/text/text-visualizer/text-preparer.h>

namespace Dali
{

namespace Ui
{

namespace Integration
{

namespace
{

BaseHandle Create()
{
  return BaseHandle();
}

#define TEXT_VISUALIZER_PROPERTY_REGISTRATION(text, valueType, enumIndex) \
  DALI_PROPERTY_REGISTRATION_EXTERNAL(Ui::Text, TextVisualizerPropertyIndex, Ui::Integration, TextVisualizerImpl, text, valueType, enumIndex)

// Type Registration
DALI_TYPE_REGISTRATION_BEGIN(TextVisualizerImpl, ViewImpl, Create)
TEXT_VISUALIZER_PROPERTY_REGISTRATION("text", STRING, TEXT)
TEXT_VISUALIZER_PROPERTY_REGISTRATION("fontFamily", STRING, FONT_FAMILY)
TEXT_VISUALIZER_PROPERTY_REGISTRATION("fontSize", FLOAT, FONT_SIZE)
TEXT_VISUALIZER_PROPERTY_REGISTRATION("textColor", VECTOR4, TEXT_COLOR)
DALI_TYPE_REGISTRATION_END()

} // namespace

TextVisualizerImplPtr TextVisualizerImpl::New()
{
  return TextVisualizerImplPtr(new TextVisualizerImpl());
}

TextVisualizerImpl::TextVisualizerImpl()
: ViewImpl(),
  mText(),
  mFontFamily(),
  mFontSize(0.0f),
  mTextColor(Color::BLACK),
  mExclusionRegions(),
  mPreparedText(),
  mLayoutResult(),
  mAtlasViewAdapter(),
  mAtlasRendererBridge(),
  mLastLayoutSize(Vector2::ZERO),
  mPrepareDirty(true),
  mLayoutDirty(true),
  mRenderDirty(true)
{
}

TextVisualizerImpl::~TextVisualizerImpl()
{
}

void TextVisualizerImpl::SetText(const Dali::String& text)
{
  if(mText != text)
  {
    mText = text;
    MarkPrepareDirty();
    RelayoutRequest();
    InvalidateMeasure();
  }
}

Dali::String TextVisualizerImpl::GetText() const
{
  return mText;
}

void TextVisualizerImpl::SetFontFamily(const Dali::String& fontFamily)
{
  if(mFontFamily != fontFamily)
  {
    mFontFamily = fontFamily;
    MarkPrepareDirty();
    RelayoutRequest();
    InvalidateMeasure();
  }
}

Dali::String TextVisualizerImpl::GetFontFamily() const
{
  return mFontFamily;
}

void TextVisualizerImpl::SetFontSize(float fontSize)
{
  if(mFontSize != fontSize)
  {
    mFontSize = fontSize;
    MarkPrepareDirty();
    RelayoutRequest();
    InvalidateMeasure();
  }
}

float TextVisualizerImpl::GetFontSize() const
{
  return mFontSize;
}

void TextVisualizerImpl::SetTextColor(const UiColor& color)
{
  if(mTextColor.Resolve() != color.Resolve() || mTextColor.GetColorId() != color.GetColorId())
  {
    mTextColor = color;
    MarkRenderDirty();
    RelayoutRequest();
  }
}

UiColor TextVisualizerImpl::GetTextColor()
{
  return mTextColor;
}

void TextVisualizerImpl::Prepare()
{
  Internal::TextVisualizer::TextPreparer::Input input;
  input.text       = mText;
  input.fontFamily = mFontFamily;
  input.fontSize   = mFontSize;

  mPreparedText = Internal::TextVisualizer::TextPreparer::Prepare(input);
  mAtlasViewAdapter.Clear();
  mAtlasRendererBridge.Clear();
  ClearPrepareDirty();
  MarkLayoutDirty();
  MarkRenderDirty();
}

void TextVisualizerImpl::SetExclusionRegions(const Dali::Vector<Rect<float>>& regions)
{
  if(!AreExclusionRegionsEqual(regions))
  {
    mExclusionRegions = regions;
    MarkLayoutDirty();
    MarkRenderDirty();
    RelayoutRequest();
    InvalidateMeasure();
  }
}

Dali::Vector<Rect<float>> TextVisualizerImpl::GetExclusionRegions() const
{
  return mExclusionRegions;
}

void TextVisualizerImpl::ClearExclusionRegions()
{
  if(!mExclusionRegions.Empty())
  {
    mExclusionRegions.Clear();
    MarkLayoutDirty();
    MarkRenderDirty();
    RelayoutRequest();
    InvalidateMeasure();
  }
}

void TextVisualizerImpl::OnInitialize()
{
  ViewImpl::OnInitialize();
}

void TextVisualizerImpl::OnRelayout(const Vector2& size, RelayoutContainer& container)
{
  if(mPrepareDirty)
  {
    Prepare();
  }

  if(mLastLayoutSize != size)
  {
    MarkLayoutDirty();
  }

  if(mLayoutDirty)
  {
    UpdateLayout(size.x, mLayoutResult);
    mAtlasViewAdapter.SetPreparedText(&mPreparedText);
    mAtlasViewAdapter.SetLayoutResult(&mLayoutResult);
    mAtlasRendererBridge.SetAdapter(&mAtlasViewAdapter);
    ClearLayoutDirty();
  }

  if(mRenderDirty && mAtlasRendererBridge.HasRenderableGlyphs())
  {
    mAtlasRendererBridge.UpdateRenderData();
    // TODO: Clear render dirty after renderer geometry is updated.
  }

  mLastLayoutSize = size;
  ViewImpl::OnRelayout(size, container);
}

MeasuredSize TextVisualizerImpl::OnMeasure(float widthConstraint, float heightConstraint)
{
  (void)heightConstraint;

  if(mPrepareDirty)
  {
    Prepare();
  }

  if(mPreparedText.Empty() || mPreparedText.GetClusterCount() == 0u)
  {
    mLayoutResult.Clear();
    ClearLayoutDirty();
    return MeasuredSize(0.0f, 0.0f);
  }

  const float clusterAdvance    = Internal::TextVisualizer::LayoutEngine::GetPlaceholderClusterAdvance(mPreparedText);
  const float placeholderWidth  = static_cast<float>(mPreparedText.GetClusterCount()) * clusterAdvance;
  const float glyphNaturalWidth = (mPreparedText.HasGlyphData() && mPreparedText.HasGlyphMetrics()) ? mPreparedText.GetTotalGlyphAdvance() : 0.0f;
  const float naturalWidth      = glyphNaturalWidth > 0.0f ? glyphNaturalWidth : placeholderWidth;
  const float layoutWidth       = widthConstraint > 0.0f ? widthConstraint : naturalWidth;

  if(layoutWidth <= 0.0f)
  {
    return MeasuredSize(0.0f, 0.0f);
  }

  Internal::TextVisualizer::LayoutResult measuredLayoutResult;
  UpdateLayout(layoutWidth, measuredLayoutResult);

  if(measuredLayoutResult.Empty())
  {
    return MeasuredSize(0.0f, 0.0f);
  }

  const float measuredWidth = widthConstraint > 0.0f ? layoutWidth : measuredLayoutResult.width;
  return MeasuredSize(measuredWidth, measuredLayoutResult.height);
}

void TextVisualizerImpl::OnPropertySet(Dali::Property::Index index, const Dali::Property::Value& propertyValue)
{
  (void)propertyValue;

  switch(index)
  {
    case Text::TextVisualizerPropertyIndex::TEXT:
    case Text::TextVisualizerPropertyIndex::FONT_FAMILY:
    case Text::TextVisualizerPropertyIndex::FONT_SIZE:
    case Text::TextVisualizerPropertyIndex::TEXT_COLOR:
    {
      break;
    }
    default:
    {
      ViewImpl::OnPropertySet(index, propertyValue);
      break;
    }
  }
}

void TextVisualizerImpl::SetProperty(BaseObject* object, Dali::Property::Index index, const Dali::Property::Value& value)
{
  Ui::View view = Ui::View::DownCast(BaseHandle(object));
  if(view)
  {
    PropertyHandler::SetProperty(view, index, value);
  }
}

Dali::Property::Value TextVisualizerImpl::GetProperty(BaseObject* object, Dali::Property::Index index)
{
  Property::Value value;
  Ui::View        view = Ui::View::DownCast(BaseHandle(object));
  if(view)
  {
    value = PropertyHandler::GetProperty(view, index);
  }
  return value;
}

void TextVisualizerImpl::MarkPrepareDirty()
{
  mPreparedText.Clear();
  mLayoutResult.Clear();
  mAtlasViewAdapter.Clear();
  mAtlasRendererBridge.Clear();
  mPrepareDirty = true;
  mLayoutDirty  = true;
  mRenderDirty  = true;
}

void TextVisualizerImpl::MarkLayoutDirty()
{
  mLayoutDirty = true;
}

void TextVisualizerImpl::MarkRenderDirty()
{
  mRenderDirty = true;
}

void TextVisualizerImpl::ClearPrepareDirty()
{
  mPrepareDirty = false;
}

void TextVisualizerImpl::ClearLayoutDirty()
{
  mLayoutDirty = false;
}

bool TextVisualizerImpl::AreExclusionRegionsEqual(const Dali::Vector<Rect<float>>& regions) const
{
  if(mExclusionRegions.Count() != regions.Count())
  {
    return false;
  }

  const uint32_t count = regions.Count();
  for(uint32_t index = 0u; index < count; ++index)
  {
    if(mExclusionRegions[index] != regions[index])
    {
      return false;
    }
  }

  return true;
}

void TextVisualizerImpl::UpdateLayout(float layoutWidth, Internal::TextVisualizer::LayoutResult& result)
{
  result.Clear();

  if(mPreparedText.Empty() || mPreparedText.GetClusterCount() == 0u)
  {
    return;
  }

  if(mPreparedText.HasGlyphData() && mPreparedText.HasGlyphMetrics())
  {
    Internal::TextVisualizer::LayoutEngine::LayoutGlyphs(mPreparedText, layoutWidth, 0.0f, mExclusionRegions, result);
  }
  else
  {
    Internal::TextVisualizer::LayoutEngine::LayoutPlaceholder(mPreparedText, layoutWidth, 0.0f, mExclusionRegions, result);
  }
}

} // namespace Integration

} // namespace Ui

} // namespace Dali
