#ifndef DALI_UI_TEXT_EFFECT_OVERFLOW_H
#define DALI_UI_TEXT_EFFECT_OVERFLOW_H

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
#include <dali/public-api/math/vector2.h>

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/text-model-interface.h>

namespace DALI_NAMESPACE
{
namespace Ui
{
namespace Text
{
/** @brief Shared admission for independent effect planes after final layout. */
inline bool IsEffectOverflowSupported(const ModelInterface& model, const Vector2& size, int maxTextureSize,
                                      bool marquee, bool reveal)
{
  return !marquee && !model.IsCutoutEnabled() && !model.IsBackgroundWithCutoutEnabled() &&
         !reveal &&
         maxTextureSize > 2 &&
         size.width < maxTextureSize && size.height < maxTextureSize;
}
} // namespace Text
} // namespace Ui
} // namespace DALI_NAMESPACE

#endif // DALI_UI_TEXT_EFFECT_OVERFLOW_H
