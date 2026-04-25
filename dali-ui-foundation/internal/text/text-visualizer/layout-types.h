#ifndef DALI_UI_TEXT_VISUALIZER_LAYOUT_TYPES_H
#define DALI_UI_TEXT_VISUALIZER_LAYOUT_TYPES_H

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
#include <cstdint>

// INTERNAL INCLUDES
#include <dali/public-api/common/dali-vector.h>

namespace Dali::Ui::Internal::TextVisualizer
{
struct AvailableInterval
{
  float x{0.0f};
  float width{0.0f};
};

struct TextLineFragment
{
  uint32_t clusterStart{0u};
  uint32_t clusterEnd{0u};
  uint32_t glyphStart{0u};
  uint32_t glyphEnd{0u};
  float    x{0.0f};
  float    y{0.0f};
  float    width{0.0f};
};

struct TextLine
{
  float                          y{0.0f};
  float                          height{0.0f};
  Dali::Vector<TextLineFragment> fragments;

  void Clear()
  {
    y      = 0.0f;
    height = 0.0f;
    fragments.Clear();
  }

  bool Empty() const
  {
    return fragments.Empty();
  }
};

struct ClusterPlacement
{
  uint32_t clusterIndex{0u};
  float    x{0.0f};
  float    y{0.0f};
  float    width{0.0f};
};

struct GlyphPlacement
{
  uint32_t glyphIndex{0u};
  float    x{0.0f};
  float    y{0.0f};
  float    width{0.0f};
  float    height{0.0f};
  float    advance{0.0f};
};

struct LayoutResult
{
  Dali::Vector<TextLine>         lines;
  Dali::Vector<ClusterPlacement> clusterPlacements;
  Dali::Vector<GlyphPlacement>   glyphPlacements;
  float                          width{0.0f};
  float                          height{0.0f};

  void Clear()
  {
    lines.Clear();
    clusterPlacements.Clear();
    glyphPlacements.Clear();
    width  = 0.0f;
    height = 0.0f;
  }

  void Reserve(uint32_t lineCount, uint32_t glyphPlacementCount, uint32_t clusterPlacementCount)
  {
    lines.Reserve(lineCount);
    glyphPlacements.Reserve(glyphPlacementCount);
    clusterPlacements.Reserve(clusterPlacementCount);
  }

  bool Empty() const
  {
    return lines.Empty() && clusterPlacements.Empty() && glyphPlacements.Empty();
  }

  uint32_t GetLineCount() const
  {
    return lines.Count();
  }
};

} // namespace Dali::Ui::Internal::TextVisualizer

#endif // DALI_UI_TEXT_VISUALIZER_LAYOUT_TYPES_H
