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
#include <algorithm>
#include <cmath>
#include <vector>

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/text-visualizer/layout-engine.h>

namespace Dali::Ui::Internal::TextVisualizer
{
namespace
{
struct BlockedInterval
{
  float start{0.0f};
  float end{0.0f};
};

struct SortedExclusionRegion
{
  Rect<float> rect;
  float       top{0.0f};
  float       bottom{0.0f};
};

using SortedExclusionRegions = std::vector<SortedExclusionRegion>;

bool HasVerticalOverlap(float lineTop, float lineBottom, const Rect<float>& region)
{
  return (region.y < lineBottom) && ((region.y + region.height) > lineTop);
}

bool IsInsideLayoutWidth(const BlockedInterval& interval)
{
  return interval.end > interval.start;
}

SortedExclusionRegions BuildSortedExclusionRegions(const Dali::Vector<Rect<float>>& exclusionRegions)
{
  SortedExclusionRegions sortedRegions;
  sortedRegions.reserve(exclusionRegions.Count());

  for(uint32_t index = 0u; index < exclusionRegions.Count(); ++index)
  {
    const Rect<float>& region = exclusionRegions[index];
    const float        bottom = region.y + region.height;

    SortedExclusionRegion sortedRegion;
    sortedRegion.rect   = region;
    sortedRegion.top    = std::min(region.y, bottom);
    sortedRegion.bottom = std::max(region.y, bottom);
    sortedRegions.push_back(sortedRegion);
  }

  std::sort(sortedRegions.begin(), sortedRegions.end(), [](const SortedExclusionRegion& lhs, const SortedExclusionRegion& rhs)
  {
    if(lhs.top == rhs.top)
    {
      return lhs.bottom < rhs.bottom;
    }
    return lhs.top < rhs.top;
  });

  return sortedRegions;
}

float GetEffectiveLineHeight(const PreparedText& preparedText, float lineHeight)
{
  if(lineHeight > 0.0f)
  {
    return lineHeight;
  }

  if(preparedText.HasLineMetrics() && (preparedText.GetLineMetrics().naturalLineHeight > 0.0f))
  {
    return preparedText.GetLineMetrics().naturalLineHeight;
  }

  return LayoutEngine::GetPlaceholderLineHeight(preparedText);
}

float GetGlyphPlacementWidth(const Text::GlyphInfo& glyph)
{
  if(glyph.width > 0.0f)
  {
    return glyph.width;
  }

  return glyph.advance > 0.0f ? glyph.advance : 0.0f;
}

float GetGlyphPlacementAdvance(const Text::GlyphInfo& glyph)
{
  if(glyph.advance > 0.0f)
  {
    return glyph.advance;
  }

  return glyph.width > 0.0f ? glyph.width : 0.0f;
}

uint32_t GetEstimatedLineGuard(float lineHeight, const Dali::Vector<Rect<float>>& exclusionRegions, uint32_t glyphCount)
{
  float maxBlockedBottom = 0.0f;

  for(uint32_t index = 0u; index < exclusionRegions.Count(); ++index)
  {
    const Rect<float>& region = exclusionRegions[index];
    maxBlockedBottom          = std::max(maxBlockedBottom, region.y + region.height);
  }

  const uint32_t blockedLineCount = lineHeight > 0.0f ? static_cast<uint32_t>(std::ceil(std::max(0.0f, maxBlockedBottom) / lineHeight)) : 0u;
  return std::max(1u, glyphCount + blockedLineCount + 2u);
}

uint32_t GetGlyphCharacterStart(const PreparedText& preparedText, uint32_t glyphIndex)
{
  const Dali::Vector<Text::CharacterIndex>& glyphToCharacterMap = preparedText.GetGlyphToCharacterMap();
  if(glyphIndex < glyphToCharacterMap.Count())
  {
    return glyphToCharacterMap[glyphIndex];
  }

  return preparedText.GetCharacterCount();
}

uint32_t GetGlyphCharacterEnd(const PreparedText& preparedText, uint32_t glyphIndex)
{
  const Dali::Vector<Text::CharacterIndex>& glyphToCharacterMap = preparedText.GetGlyphToCharacterMap();
  const Dali::Vector<Text::Length>&         charactersPerGlyph  = preparedText.GetCharactersPerGlyph();

  if(glyphIndex < glyphToCharacterMap.Count())
  {
    const uint32_t characterStart = glyphToCharacterMap[glyphIndex];
    const uint32_t characterSpan  = glyphIndex < charactersPerGlyph.Count() ? charactersPerGlyph[glyphIndex] : 0u;
    return std::min(preparedText.GetCharacterCount(), characterStart + characterSpan);
  }

  return preparedText.GetCharacterCount();
}

TextLineMetrics CalculateTextLineMetrics(const PreparedText& preparedText,
                                         const TextLine&     textLine)
{
  TextLineMetrics                        metrics;
  const Dali::Vector<Text::GlyphInfo>&   glyphs          = preparedText.GetGlyphs();
  const PreparedText::LineMetrics* const fallbackMetrics = preparedText.HasLineMetrics() ? &preparedText.GetLineMetrics() : nullptr;

  for(Vector<TextLineFragment>::ConstIterator fragmentIt = textLine.fragments.Begin(), fragmentEndIt = textLine.fragments.End();
      fragmentIt != fragmentEndIt; ++fragmentIt)
  {
    for(uint32_t glyphIndex = fragmentIt->glyphStart; glyphIndex < fragmentIt->glyphEnd && glyphIndex < glyphs.Count(); ++glyphIndex)
    {
      const Text::GlyphInfo& glyph = glyphs[glyphIndex];
      metrics.ascender             = std::max(metrics.ascender, glyph.yBearing);
      metrics.descender            = std::max(metrics.descender, std::max(0.0f, glyph.height - glyph.yBearing));
      metrics.valid                = true;
    }
  }

  if(metrics.valid)
  {
    const float lineGap       = (nullptr != fallbackMetrics) ? fallbackMetrics->lineGap : 0.0f;
    metrics.baselineOffset    = metrics.ascender;
    metrics.naturalLineHeight = metrics.ascender + metrics.descender + lineGap;
  }

  return metrics;
}

Dali::Vector<AvailableInterval> BuildAvailableIntervalsFromSorted(float                         layoutWidth,
                                                                  float                         lineY,
                                                                  float                         lineHeight,
                                                                  const SortedExclusionRegions& exclusionRegions)
{
  Dali::Vector<AvailableInterval> availableIntervals;

  if(layoutWidth <= 0.0f || lineHeight <= 0.0f)
  {
    return availableIntervals;
  }

  if(exclusionRegions.empty())
  {
    availableIntervals.PushBack({0.0f, layoutWidth});
    return availableIntervals;
  }

  const float lineTop    = lineY;
  const float lineBottom = lineY + lineHeight;

  std::vector<BlockedInterval> blockedIntervals;
  blockedIntervals.reserve(exclusionRegions.size());

  for(SortedExclusionRegions::const_iterator it = exclusionRegions.begin(), endIt = exclusionRegions.end(); it != endIt; ++it)
  {
    if(it->bottom <= lineTop)
    {
      continue;
    }

    if(it->top >= lineBottom)
    {
      break;
    }

    const Rect<float>& region = it->rect;
    if(!HasVerticalOverlap(lineTop, lineBottom, region))
    {
      continue;
    }

    BlockedInterval blockedInterval;
    blockedInterval.start = std::max(0.0f, region.x);
    blockedInterval.end   = std::min(layoutWidth, region.x + region.width);

    if(IsInsideLayoutWidth(blockedInterval))
    {
      blockedIntervals.push_back(blockedInterval);
    }
  }

  if(blockedIntervals.empty())
  {
    availableIntervals.PushBack({0.0f, layoutWidth});
    return availableIntervals;
  }

  std::sort(blockedIntervals.begin(), blockedIntervals.end(), [](const BlockedInterval& lhs, const BlockedInterval& rhs)
  {
    if(lhs.start == rhs.start)
    {
      return lhs.end < rhs.end;
    }
    return lhs.start < rhs.start;
  });

  std::vector<BlockedInterval> mergedIntervals;
  mergedIntervals.reserve(blockedIntervals.size());

  for(const BlockedInterval& blockedInterval : blockedIntervals)
  {
    if(mergedIntervals.empty() || blockedInterval.start > mergedIntervals.back().end)
    {
      mergedIntervals.push_back(blockedInterval);
      continue;
    }

    mergedIntervals.back().end = std::max(mergedIntervals.back().end, blockedInterval.end);
  }

  float currentX = 0.0f;
  for(const BlockedInterval& blockedInterval : mergedIntervals)
  {
    if(blockedInterval.start > currentX)
    {
      const float availableWidth = blockedInterval.start - currentX;
      if(availableWidth > 0.0f)
      {
        availableIntervals.PushBack({currentX, availableWidth});
      }
    }

    currentX = std::max(currentX, blockedInterval.end);
  }

  if(currentX < layoutWidth)
  {
    const float availableWidth = layoutWidth - currentX;
    if(availableWidth > 0.0f)
    {
      availableIntervals.PushBack({currentX, availableWidth});
    }
  }

  return availableIntervals;
}
} // namespace

Dali::Vector<AvailableInterval> LayoutEngine::BuildAvailableIntervals(float                            layoutWidth,
                                                                      float                            lineY,
                                                                      float                            lineHeight,
                                                                      const Dali::Vector<Rect<float>>& exclusionRegions)
{
  const SortedExclusionRegions sortedExclusionRegions = BuildSortedExclusionRegions(exclusionRegions);
  return BuildAvailableIntervalsFromSorted(layoutWidth, lineY, lineHeight, sortedExclusionRegions);
}

float LayoutEngine::GetPlaceholderClusterAdvance(const PreparedText& preparedText)
{
  const float fontSize = preparedText.GetFontSize();
  return fontSize > 0.0f ? fontSize * 0.5f : 10.0f;
}

float LayoutEngine::GetPlaceholderLineHeight(const PreparedText& preparedText)
{
  const float fontSize = preparedText.GetFontSize();
  return fontSize > 0.0f ? fontSize * 1.2f : 20.0f;
}

void LayoutEngine::LayoutPlaceholder(const PreparedText&              preparedText,
                                     float                            layoutWidth,
                                     float                            lineHeight,
                                     const Dali::Vector<Rect<float>>& exclusionRegions,
                                     LayoutResult&                    result)
{
  result.Clear();

  const uint32_t clusterCount = preparedText.GetClusterCount();
  if(preparedText.Empty() || layoutWidth <= 0.0f || clusterCount == 0u)
  {
    return;
  }

  const float clusterAdvance      = GetPlaceholderClusterAdvance(preparedText);
  const float effectiveLineHeight = GetEffectiveLineHeight(preparedText, lineHeight);
  if(clusterAdvance <= 0.0f || effectiveLineHeight <= 0.0f)
  {
    return;
  }

  uint32_t                     currentCluster         = 0u;
  float                        currentY               = 0.0f;
  const uint32_t               maxLineCount           = std::max(1u, clusterCount + static_cast<uint32_t>(exclusionRegions.Count()) + 1u);
  const uint32_t               estimatedLineCount     = std::min(maxLineCount, clusterCount);
  const SortedExclusionRegions sortedExclusionRegions = BuildSortedExclusionRegions(exclusionRegions);
  result.Reserve(estimatedLineCount, 0u, clusterCount);

  for(uint32_t lineIndex = 0u; lineIndex < maxLineCount && currentCluster < clusterCount; ++lineIndex)
  {
    const Dali::Vector<AvailableInterval> availableIntervals = BuildAvailableIntervalsFromSorted(layoutWidth, currentY, effectiveLineHeight, sortedExclusionRegions);

    TextLine textLine;
    textLine.y      = currentY;
    textLine.height = effectiveLineHeight;
    textLine.fragments.Reserve(availableIntervals.Count());

    bool lineHasPlacement = false;

    for(uint32_t intervalIndex = 0u; intervalIndex < availableIntervals.Count() && currentCluster < clusterCount; ++intervalIndex)
    {
      const AvailableInterval& availableInterval = availableIntervals[intervalIndex];
      const uint32_t           fitCount          = static_cast<uint32_t>(availableInterval.width / clusterAdvance);

      if(fitCount == 0u)
      {
        continue;
      }

      const uint32_t placeCount = std::min(fitCount, clusterCount - currentCluster);
      if(placeCount == 0u)
      {
        continue;
      }

      TextLineFragment fragment;
      fragment.clusterStart = currentCluster;
      fragment.clusterEnd   = currentCluster + placeCount;
      fragment.x            = availableInterval.x;
      fragment.y            = currentY;
      fragment.width        = static_cast<float>(placeCount) * clusterAdvance;
      textLine.fragments.PushBack(fragment);

      for(uint32_t placedIndex = 0u; placedIndex < placeCount; ++placedIndex)
      {
        ClusterPlacement placement;
        placement.clusterIndex = currentCluster + placedIndex;
        placement.x            = availableInterval.x + static_cast<float>(placedIndex) * clusterAdvance;
        placement.y            = currentY;
        placement.width        = clusterAdvance;
        result.clusterPlacements.PushBack(placement);
      }

      currentCluster += placeCount;
      lineHasPlacement = true;
      result.width     = std::max(result.width, fragment.x + fragment.width);
    }

    if(lineHasPlacement)
    {
      result.lines.PushBack(textLine);
      result.height = currentY + effectiveLineHeight;
    }

    currentY += effectiveLineHeight;
  }
}

void LayoutEngine::LayoutGlyphs(const PreparedText&              preparedText,
                                float                            layoutWidth,
                                float                            lineHeight,
                                const Dali::Vector<Rect<float>>& exclusionRegions,
                                LayoutResult&                    result)
{
  result.Clear();

  if(!preparedText.HasGlyphData() || layoutWidth <= 0.0f)
  {
    return;
  }

  const Dali::Vector<Text::GlyphInfo>& glyphs              = preparedText.GetGlyphs();
  const uint32_t                       glyphCount          = glyphs.Count();
  const float                          effectiveLineHeight = GetEffectiveLineHeight(preparedText, lineHeight);

  if(glyphCount == 0u || effectiveLineHeight <= 0.0f)
  {
    return;
  }

  uint32_t                     currentGlyph           = 0u;
  float                        currentY               = 0.0f;
  const uint32_t               maxLineCount           = GetEstimatedLineGuard(effectiveLineHeight, exclusionRegions, glyphCount);
  const uint32_t               estimatedLineCount     = std::min(maxLineCount, glyphCount);
  const SortedExclusionRegions sortedExclusionRegions = BuildSortedExclusionRegions(exclusionRegions);
  result.Reserve(estimatedLineCount, glyphCount, 0u);

  for(uint32_t lineIndex = 0u; lineIndex < maxLineCount && currentGlyph < glyphCount; ++lineIndex)
  {
    const Dali::Vector<AvailableInterval> availableIntervals = BuildAvailableIntervalsFromSorted(layoutWidth, currentY, effectiveLineHeight, sortedExclusionRegions);

    TextLine textLine;
    textLine.y      = currentY;
    textLine.height = effectiveLineHeight;
    textLine.fragments.Reserve(availableIntervals.Count());

    bool lineHasPlacement = false;

    for(uint32_t intervalIndex = 0u; intervalIndex < availableIntervals.Count() && currentGlyph < glyphCount; ++intervalIndex)
    {
      const AvailableInterval& availableInterval = availableIntervals[intervalIndex];
      const float              intervalEnd       = availableInterval.x + availableInterval.width;
      float                    cursorX           = availableInterval.x;
      bool                     fragmentOpen      = false;
      TextLineFragment         fragment;

      while(currentGlyph < glyphCount)
      {
        const Text::GlyphInfo& glyph          = glyphs[currentGlyph];
        const float            glyphAdvance   = GetGlyphPlacementAdvance(glyph);
        const float            glyphWidth     = GetGlyphPlacementWidth(glyph);
        const float            requiredWidth  = std::max(glyphAdvance, glyphWidth);
        const bool             intervalCanFit = (requiredWidth <= 0.0f) || ((cursorX + requiredWidth) <= intervalEnd);
        const bool             allowOversized = !lineHasPlacement && !fragmentOpen;

        if(!intervalCanFit && !allowOversized)
        {
          break;
        }

        GlyphPlacement placement;
        placement.glyphIndex = currentGlyph;
        placement.x          = cursorX;
        placement.y          = currentY;
        placement.width      = glyph.width;
        placement.height     = glyph.height;
        placement.advance    = glyphAdvance;
        result.glyphPlacements.PushBack(placement);

        if(!fragmentOpen)
        {
          fragment.clusterStart = GetGlyphCharacterStart(preparedText, currentGlyph);
          fragment.glyphStart   = currentGlyph;
          fragment.x            = cursorX;
          fragment.y            = currentY;
          fragment.width        = 0.0f;
          fragmentOpen          = true;
        }

        fragment.clusterEnd = GetGlyphCharacterEnd(preparedText, currentGlyph);
        fragment.glyphEnd   = currentGlyph + 1u;

        const float placementRight = std::max(cursorX + glyphWidth, cursorX + glyphAdvance);
        fragment.width             = std::max(fragment.width, placementRight - fragment.x);
        result.width               = std::max(result.width, fragment.x + fragment.width);

        cursorX += glyphAdvance;
        ++currentGlyph;
        lineHasPlacement = true;

        if(!intervalCanFit && allowOversized)
        {
          break;
        }
      }

      if(fragmentOpen)
      {
        textLine.fragments.PushBack(fragment);
      }
    }

    if(lineHasPlacement)
    {
      textLine.metrics = CalculateTextLineMetrics(preparedText, textLine);
      result.lines.PushBack(textLine);
      result.height = currentY + effectiveLineHeight;
    }

    currentY += effectiveLineHeight;
  }
}

} // namespace Dali::Ui::Internal::TextVisualizer
