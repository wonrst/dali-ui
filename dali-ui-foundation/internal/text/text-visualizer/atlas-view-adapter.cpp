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
  mLayoutResult(nullptr)
{
}

void AtlasViewAdapter::SetPreparedText(const PreparedText* preparedText)
{
  mPreparedText = preparedText;
}

void AtlasViewAdapter::SetLayoutResult(const LayoutResult* layoutResult)
{
  mLayoutResult = layoutResult;
}

void AtlasViewAdapter::Clear()
{
  mPreparedText = nullptr;
  mLayoutResult = nullptr;
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

} // namespace Dali::Ui::Internal::TextVisualizer
