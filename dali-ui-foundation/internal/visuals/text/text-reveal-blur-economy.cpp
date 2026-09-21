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

#include "text-reveal-blur-economy.h"

#include <dali/public-api/math/math-utils.h>

namespace DALI_NAMESPACE::Ui::Internal::RevealBlurEconomy
{
namespace
{
// Keep ECONOMY's scaled kernel local so GaussianBlurAlgorithm remains unchanged.
constexpr float    MAXIMUM_BELL_CURVE_WIDTH            = 64.062302f;
constexpr int32_t  MAXIMUM_BELL_CURVE_LOOP_TRIAL_COUNT = 20;
constexpr uint32_t MAXIMUM_KERNEL_RADIUS               = 200u;

float CalculateGaussianWeight(float localOffset, float sigma)
{
  return (1.0f / (sigma * sqrt(2.0f * Dali::Math::PI))) * exp(-0.5f * (localOffset / sigma * localOffset / sigma));
}

float CalculateBellCurveWidth(uint32_t numSamples)
{
  const float epsilon     = 1e-2f / static_cast<float>(numSamples * 2u);
  const float localOffset = static_cast<float>((numSamples * 2u) - 1u);

  float lowerBoundBellCurveWidth = Dali::Math::MACHINE_EPSILON_10000;
  float upperBoundBellCurveWidth = MAXIMUM_BELL_CURVE_WIDTH;
  float bellCurveWidth           = -1.0f;

  int trialCount = 0;
  while(trialCount++ < MAXIMUM_BELL_CURVE_LOOP_TRIAL_COUNT &&
        upperBoundBellCurveWidth - lowerBoundBellCurveWidth > Dali::Math::MACHINE_EPSILON_10000)
  {
    bellCurveWidth = (lowerBoundBellCurveWidth + upperBoundBellCurveWidth) * 0.5f;
    if(CalculateGaussianWeight(localOffset, bellCurveWidth) < epsilon)
    {
      lowerBoundBellCurveWidth = bellCurveWidth;
    }
    else
    {
      upperBoundBellCurveWidth = bellCurveWidth;
    }
  }
  return bellCurveWidth;
}
} // namespace

bool CalculateVerticalKernel(uint32_t blurRadius, float sourceToReducedScale, std::vector<float>& weights, std::vector<float>& offsets)
{
  weights.clear();
  offsets.clear();
  if(blurRadius < 2u || blurRadius > MAXIMUM_KERNEL_RADIUS || (blurRadius & 1u) != 0u ||
     !std::isfinite(sourceToReducedScale) || sourceToReducedScale <= 1.0f || sourceToReducedScale > 4.0f)
  {
    return false;
  }

  const float        sigma   = CalculateBellCurveWidth(blurRadius >> 1u) / sourceToReducedScale;
  const auto         support = static_cast<uint32_t>(std::floor(static_cast<float>(blurRadius - 1u) / sourceToReducedScale));
  const auto         pairs   = std::max(2u, (support + 2u) / 2u);
  std::vector<float> half(pairs * 2u, 0.0f);
  half[0]     = CalculateGaussianWeight(0.0f, sigma);
  float total = half[0];
  for(uint32_t i = 1u; i <= support; ++i)
  {
    half[i] = CalculateGaussianWeight(static_cast<float>(i), sigma);
    total += 2.0f * half[i];
  }
  for(auto& weight : half)
  {
    weight /= total;
  }
  half[0] *= 0.5f;
  weights.resize(pairs);
  offsets.resize(pairs);
  for(uint32_t i = 0u; i < pairs; ++i)
  {
    weights[i] = half[2u * i] + half[2u * i + 1u];
    offsets[i] = weights[i] > 0.0f ? 2.0f * static_cast<float>(i) + half[2u * i + 1u] / weights[i]
                                   : (i > 0u ? offsets[i - 1u] : 0.0f);
  }
  return true;
}
} // namespace DALI_NAMESPACE::Ui::Internal::RevealBlurEconomy
