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

bool HasVerticalOverlap(float lineTop, float lineBottom, const Rect<float>& region)
{
  return (region.y < lineBottom) && ((region.y + region.height) > lineTop);
}

bool IsInsideLayoutWidth(const BlockedInterval& interval)
{
  return interval.end > interval.start;
}
} // namespace

Dali::Vector<AvailableInterval> LayoutEngine::BuildAvailableIntervals(float                            layoutWidth,
                                                                      float                            lineY,
                                                                      float                            lineHeight,
                                                                      const Dali::Vector<Rect<float>>& exclusionRegions)
{
  Dali::Vector<AvailableInterval> availableIntervals;

  if(layoutWidth <= 0.0f || lineHeight <= 0.0f)
  {
    return availableIntervals;
  }

  if(exclusionRegions.Empty())
  {
    availableIntervals.PushBack({0.0f, layoutWidth});
    return availableIntervals;
  }

  const float lineTop    = lineY;
  const float lineBottom = lineY + lineHeight;

  std::vector<BlockedInterval> blockedIntervals;
  blockedIntervals.reserve(exclusionRegions.Count());

  for(uint32_t index = 0u; index < exclusionRegions.Count(); ++index)
  {
    const Rect<float>& region = exclusionRegions[index];
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

} // namespace Dali::Ui::Internal::TextVisualizer
