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

#ifndef DALI_UI_TEXT_MARQUEE_PAINT_H
#define DALI_UI_TEXT_MARQUEE_PAINT_H

// EXTERNAL INCLUDES
#include <dali/public-api/math/vector2.h>
#include <algorithm>
#include <cmath>

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/rendering/text-raster-geometry.h>

namespace DALI_NAMESPACE
{
namespace Ui
{
namespace Text
{
constexpr unsigned MARQUEE_PAINT_COPIES = 8u;

/** @brief Resolves conservative periodic support, including filtering and Emboss. */
inline bool ResolveMarqueePaintSupport(const Raster::Bounds& composition, const Raster::Bounds& fill,
                                       const Vector2& rasterSize, bool horizontal, bool emboss,
                                       const Vector2& direction, float strength, Vector2& support)
{
  const double period = std::trunc(horizontal ? rasterSize.x : rasterSize.y);
  if(!std::isfinite(period) || period <= 0.0 || composition.Empty()) return false;
  double first = horizontal ? composition.left : composition.top;
  double last  = horizontal ? composition.right : composition.bottom;
  if(emboss)
  {
    const double length = std::hypot(static_cast<double>(direction.x), static_cast<double>(direction.y));
    if(!std::isfinite(length) || length == 0.0 || !std::isfinite(strength) || strength < 0.f) return false;
    const double displacement = std::ceil(std::abs((horizontal ? direction.x : direction.y) / length * strength));
    first                     = std::min(first, (horizontal ? fill.left : fill.top) - displacement);
    last                      = std::max(last, (horizontal ? fill.right : fill.bottom) + displacement);
  }
  first = (first - 1.0) / period;
  last  = (last + 1.0) / period;
  if(!std::isfinite(first) || !std::isfinite(last) || std::ceil(last - first) + 1.0 > MARQUEE_PAINT_COPIES) return false;
  support = Vector2(first, last);
  return true;
}

} // namespace Text
} // namespace Ui
} // namespace DALI_NAMESPACE

#endif // DALI_UI_TEXT_MARQUEE_PAINT_H
