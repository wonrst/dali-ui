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

#ifndef DALI_UI_INTERNAL_TEXT_REVEAL_BLUR_ECONOMY_H
#define DALI_UI_INTERNAL_TEXT_REVEAL_BLUR_ECONOMY_H

#include <dali/public-api/math/vector2.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace DALI_NAMESPACE::Ui::Internal::RevealBlurEconomy
{
/**
 * @brief Packs ECONOMY's vertical Gaussian in the reduced input pixel domain.
 *
 * Uses the validated even kernel radius and the actual source/input height
 * ratio, including ceil-rounded quarter-height targets. Invalid inputs clear
 * both vectors and return false. The reduction ratio must be in (1, 4].
 * Padding to two pairs avoids single-element uniform arrays.
 */
bool CalculateVerticalKernel(uint32_t blurRadius, float sourceToReducedScale, std::vector<float>& weights, std::vector<float>& offsets);

inline float SmoothStep(float start, float end, float value)
{
  const float t = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

/**
 * @brief Keeps the reduced-resolution filter above its useful detail range.
 *
 * The floor saturates at 12 display pixels and never exceeds the authored
 * radius. Small radii remain supported without increasing their maximum blur.
 */
inline float Radius(float authoredRadius, float amount)
{
  if(!std::isfinite(authoredRadius) || authoredRadius <= 0.0f)
  {
    return 0.0f;
  }
  const float floor = std::min(authoredRadius, std::clamp(authoredRadius * 0.5f, 10.0f, 12.0f));
  return floor + (authoredRadius - floor) * SmoothStep(0.35f, 1.0f, amount);
}

/**
 * @brief Returns sharp/blur premultiplied composition weights.
 *
 * Weights are calculated on the update side, once per sequence. Their sum
 * never exceeds one; reduced-resolution detail is replaced by current Source
 * rather than amplified. The input is the native sequence blur amount.
 */
inline Vector2 Composition(float amount)
{
  const float sharp = 1.0f - SmoothStep(0.0f, 0.60f, amount);
  const float blur  = std::min(1.0f - sharp, 0.90f * SmoothStep(0.0f, 0.65f, amount));
  return Vector2(sharp, blur);
}
} // namespace DALI_NAMESPACE::Ui::Internal::RevealBlurEconomy

#endif // DALI_UI_INTERNAL_TEXT_REVEAL_BLUR_ECONOMY_H
