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

// CLASS HEADER
#include <dali-ui-foundation/internal/text/text-visualizer/atlas-view-adapter.h>

// EXTERNAL INCLUDES
#include <algorithm>
#include <cmath>

namespace Dali::Ui::Internal::TextVisualizer
{
namespace
{
const Dali::Vector<Text::GlyphInfo>& EmptyGlyphs()
{
  static const Dali::Vector<Text::GlyphInfo> emptyGlyphs;
  return emptyGlyphs;
}

const Dali::Vector<GlyphPlacement>& EmptyGlyphPlacements()
{
  static const Dali::Vector<GlyphPlacement> emptyPlacements;
  return emptyPlacements;
}

const Dali::Vector<Text::GlyphIndex>& EmptyGlyphIndices()
{
  static const Dali::Vector<Text::GlyphIndex> emptyGlyphIndices;
  return emptyGlyphIndices;
}

const Dali::Vector<Text::CharacterIndex>& EmptyCharacterIndices()
{
  static const Dali::Vector<Text::CharacterIndex> emptyCharacterIndices;
  return emptyCharacterIndices;
}
} // unnamed namespace

AtlasViewAdapter::AtlasViewAdapter()
: mPreparedText(nullptr),
  mLayoutResult(nullptr),
  mControlSize(Vector2::ZERO),
  mLayoutSize(Vector2::ZERO),
  mTextColor(0.0f, 0.0f, 0.0f, 1.0f)
{
}

void AtlasViewAdapter::SetPreparedText(const PreparedText* preparedText)
{
  mPreparedText = preparedText;
}

void AtlasViewAdapter::SetLayoutResult(const LayoutResult* layoutResult)
{
  mLayoutResult = layoutResult;
  mLayoutSize   = (nullptr != layoutResult) ? Vector2(layoutResult->width, layoutResult->height) : Vector2::ZERO;
}

void AtlasViewAdapter::SetControlSize(const Vector2& controlSize)
{
  mControlSize = controlSize;
}

void AtlasViewAdapter::SetTextColor(const Vector4& textColor)
{
  mTextColor = textColor;
}

void AtlasViewAdapter::Clear()
{
  mPreparedText = nullptr;
  mLayoutResult = nullptr;
  mControlSize  = Vector2::ZERO;
  mLayoutSize   = Vector2::ZERO;
  mTextColor    = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
}

bool AtlasViewAdapter::GetGlyphPlacement(uint32_t index, GlyphPlacement& placement) const
{
  if((nullptr == mLayoutResult) || (index >= mLayoutResult->glyphPlacements.Count()))
  {
    return false;
  }

  placement = mLayoutResult->glyphPlacements[index];
  return true;
}

bool AtlasViewAdapter::GetGlyphInfo(uint32_t glyphIndex, Text::GlyphInfo& glyphInfo) const
{
  if((nullptr == mPreparedText) || (glyphIndex >= mPreparedText->GetGlyphCount()))
  {
    return false;
  }

  glyphInfo = mPreparedText->GetGlyphs()[glyphIndex];
  return true;
}

bool AtlasViewAdapter::GetRendererGlyphPosition(uint32_t index, Vector2& position) const
{
  GlyphPlacement  placement;
  Text::GlyphInfo glyphInfo;

  if(!GetGlyphPlacement(index, placement) || !GetGlyphInfo(placement.glyphIndex, glyphInfo))
  {
    return false;
  }

  // AtlasRenderer expects the glyph quad origin, not the pen position stored by LayoutResult.
  const float baselineOffset = GetLineBaselineOffset(placement.y);
  position.x                 = placement.x + glyphInfo.xBearing;
  position.y                 = placement.y + baselineOffset - glyphInfo.yBearing;

  return std::isfinite(position.x) && std::isfinite(position.y);
}

bool AtlasViewAdapter::HasRenderableGlyphs() const
{
  return (nullptr != mPreparedText) &&
         (nullptr != mLayoutResult) &&
         mPreparedText->HasGlyphData() &&
         mPreparedText->HasGlyphMetrics() &&
         !mLayoutResult->glyphPlacements.Empty() &&
         HasValidGlyphPlacementIndices();
}

bool AtlasViewAdapter::HasValidGlyphPlacementIndices() const
{
  if((nullptr == mPreparedText) || (nullptr == mLayoutResult))
  {
    return false;
  }

  const uint32_t glyphCount = mPreparedText->GetGlyphCount();
  for(Vector<GlyphPlacement>::ConstIterator it = mLayoutResult->glyphPlacements.Begin(), endIt = mLayoutResult->glyphPlacements.End();
      it != endIt; ++it)
  {
    if(it->glyphIndex >= glyphCount)
    {
      return false;
    }
  }

  return true;
}

uint32_t AtlasViewAdapter::GetGlyphCount() const
{
  return (nullptr != mPreparedText) ? mPreparedText->GetGlyphCount() : 0u;
}

uint32_t AtlasViewAdapter::GetGlyphPlacementCount() const
{
  return (nullptr != mLayoutResult) ? mLayoutResult->glyphPlacements.Count() : 0u;
}

uint32_t AtlasViewAdapter::GetRenderableGlyphCount() const
{
  return HasRenderableGlyphs() ? GetGlyphPlacementCount() : 0u;
}

const Vector2& AtlasViewAdapter::GetControlSize() const
{
  return (mControlSize != Vector2::ZERO) ? mControlSize : mLayoutSize;
}

const Vector2& AtlasViewAdapter::GetLayoutSize() const
{
  return mLayoutSize;
}

const Vector4& AtlasViewAdapter::GetTextColor() const
{
  return mTextColor;
}

const Dali::Vector<Text::GlyphInfo>& AtlasViewAdapter::GetGlyphs() const
{
  return (nullptr != mPreparedText) ? mPreparedText->GetGlyphs() : EmptyGlyphs();
}

const Dali::Vector<GlyphPlacement>& AtlasViewAdapter::GetGlyphPlacements() const
{
  return (nullptr != mLayoutResult) ? mLayoutResult->glyphPlacements : EmptyGlyphPlacements();
}

const Dali::Vector<Text::GlyphIndex>& AtlasViewAdapter::GetNewParagraphGlyphs() const
{
  return (nullptr != mPreparedText) ? mPreparedText->GetNewParagraphGlyphs() : EmptyGlyphIndices();
}

const Dali::Vector<Text::CharacterIndex>& AtlasViewAdapter::GetGlyphToCharacterMap() const
{
  return (nullptr != mPreparedText) ? mPreparedText->GetGlyphToCharacterMap() : EmptyCharacterIndices();
}

const Text::Character* AtlasViewAdapter::GetTextBuffer() const
{
  if((nullptr == mPreparedText) || mPreparedText->GetCharacters().Empty())
  {
    return nullptr;
  }

  return mPreparedText->GetCharacters().Begin();
}

float AtlasViewAdapter::GetLineBaselineOffset(float lineTop) const
{
  if((nullptr == mPreparedText) || (nullptr == mLayoutResult))
  {
    return 0.0f;
  }

  constexpr float LINE_TOLERANCE = 0.001f;

  float baselineOffset = 0.0f;
  bool  foundGlyph     = false;

  for(Vector<GlyphPlacement>::ConstIterator placementIt = mLayoutResult->glyphPlacements.Begin(),
                                            endIt       = mLayoutResult->glyphPlacements.End();
      placementIt != endIt; ++placementIt)
  {
    if(std::fabs(placementIt->y - lineTop) > LINE_TOLERANCE)
    {
      continue;
    }

    Text::GlyphInfo glyphInfo;
    if(!GetGlyphInfo(placementIt->glyphIndex, glyphInfo))
    {
      continue;
    }

    baselineOffset = std::max(baselineOffset, glyphInfo.yBearing);
    foundGlyph     = true;
  }

  return foundGlyph ? baselineOffset : 0.0f;
}

} // namespace Dali::Ui::Internal::TextVisualizer
