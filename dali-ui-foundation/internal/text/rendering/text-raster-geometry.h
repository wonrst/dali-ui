#ifndef DALI_UI_TEXT_RASTER_GEOMETRY_H
#define DALI_UI_TEXT_RASTER_GEOMETRY_H

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
#include <dali/public-api/images/pixel-data.h>
#include <dali/public-api/math/vector2.h>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace DALI_NAMESPACE
{
namespace Ui
{
namespace Text
{
namespace Raster
{
/**
 * @brief Signed, half-open paint bounds [left, top, right, bottom).
 *
 * Coordinates are quantized local text paint pixels, including legacy alignment
 * and effect offsets, before clipping or raster-origin translation. Empty bounds
 * denote an inactive plane. These are raster support bounds, not alpha-tight ink
 * bounds: transparent bitmap padding and blur kernel support are included.
 * Widen before deriving sizes or translating the origin.
 */
struct Bounds
{
  int32_t left{0};
  int32_t top{0};
  int32_t right{0};
  int32_t bottom{0};

  bool Empty() const
  {
    return left >= right || top >= bottom;
  }
};

/** @brief Request-local hard planes in pre-effect text paint coordinates. */
struct SourceClip
{
  enum Plane : uint8_t
  {
    LEFT   = 1u,
    RIGHT  = 2u,
    TOP    = 4u,
    BOTTOM = 8u
  };
  Bounds  viewport;
  uint8_t planes{0u};

  void Clip(int64_t& left, int64_t& top, int64_t& right, int64_t& bottom) const
  {
    if(planes & LEFT) left = std::max<int64_t>(left, viewport.left);
    if(planes & RIGHT) right = std::min<int64_t>(right, viewport.right);
    if(planes & TOP) top = std::max<int64_t>(top, viewport.top);
    if(planes & BOTTOM) bottom = std::min<int64_t>(bottom, viewport.bottom);
  }
};

/** @brief Accumulates checked paint rectangles without allocating storage. */
struct BoundsAccumulator
{
  Bounds bounds;
  bool   valid{true};

  void Include(int64_t left, int64_t top, int64_t right, int64_t bottom)
  {
    if(left >= right || top >= bottom)
    {
      return;
    }
    if(left < std::numeric_limits<int32_t>::min() || top < std::numeric_limits<int32_t>::min() ||
       right > std::numeric_limits<int32_t>::max() || bottom > std::numeric_limits<int32_t>::max())
    {
      valid = false;
      return;
    }
    if(!bounds.Empty())
    {
      left   = std::min<int64_t>(left, bounds.left);
      top    = std::min<int64_t>(top, bounds.top);
      right  = std::max<int64_t>(right, bounds.right);
      bottom = std::max<int64_t>(bottom, bounds.bottom);
    }
    bounds = {static_cast<int32_t>(left), static_cast<int32_t>(top), static_cast<int32_t>(right), static_cast<int32_t>(bottom)};
  }

  void Include(const Bounds& other)
  {
    Include(other.left, other.top, other.right, other.bottom);
  }
};
} // namespace Raster

/**
 * @brief Raster support of the final laid-out text and its existing effect planes.
 *
 * Layout coordinates remain owned by the final ViewModel. Local text paint
 * coordinates add the legacy alignment/bitmap/effect offsets and quantize using
 * the drawing rules. A plane raster coordinate is paint minus that plane's
 * top-left; a composition coordinate is paint minus composition's top-left.
 * Origin translation is integer-only and must follow quantization.
 *
 * Bounds do not change layout, TextFit, clipping policy or scene geometry.
 * Composition is the union of active planes, without the legacy target rectangle.
 */
struct TextRasterGeometry
{
  Raster::Bounds fill;
  Raster::Bounds underlay;
  Raster::Bounds overlay;
  Raster::Bounds composition;
};

/**
 * @brief CPU planes and their geometry, produced as one complete result.
 *
 * Plane bounds follow the presentation policy: tight paint support for static
 * overflow/Marquee, or the original bounded raster domain for Reveal. There is
 * no filtering gutter in this CPU payload. Empty planes have no PixelData.
 * Layout and original target sizes remain separate from plane storage.
 */
struct TextRasterResult
{
  TextRasterGeometry geometry;
  PixelData          textGradientMask; ///< Target coverage paired with preserved RGBA fill for Global Gradient.
  PixelData          fill;
  PixelData          underlay;
  PixelData          overlay;
  PixelData          embossMask;     ///< Raw monochrome coverage for the legacy Emboss color branch.
  PixelData          colorGlyphMask; ///< Optional monochrome coverage in the fill domain, for animatable mixed text.
  Vector2            layoutSize;
  Vector2            legacyRasterSize; ///< Alignment target in raster pixels, including async supersampling.
};

} // namespace Text
} // namespace Ui
} // namespace DALI_NAMESPACE

#endif // DALI_UI_TEXT_RASTER_GEOMETRY_H
