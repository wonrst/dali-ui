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
  }
}

UiColor TextVisualizerImpl::GetTextColor()
{
  return mTextColor;
}

void TextVisualizerImpl::Prepare()
{
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
  }
}

void TextVisualizerImpl::OnInitialize()
{
  ViewImpl::OnInitialize();
}

void TextVisualizerImpl::OnRelayout(const Vector2& size, RelayoutContainer& container)
{
  ViewImpl::OnRelayout(size, container);
}

MeasuredSize TextVisualizerImpl::OnMeasure(float widthConstraint, float heightConstraint)
{
  return ViewImpl::OnMeasure(widthConstraint, heightConstraint);
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

} // namespace Integration

} // namespace Ui

} // namespace Dali
