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

#ifndef DALI_UI_TEXT_REVEAL_BLUR_PROTOTYPE_H
#define DALI_UI_TEXT_REVEAL_BLUR_PROTOTYPE_H

#include <dali-ui-foundation/public-api/dali-ui-common.h>
#include <dali-ui-foundation/public-api/views/text-controls/label.h>

namespace Dali
{
namespace Ui
{
namespace Integration
{
/**
 * @brief Selects the temporary sequence blur sharpness curve.
 */
enum class TextRevealBlurCurve
{
  LINEAR,
  SOFT,
  EASE_IN_QUADRATIC
};

/**
 * @brief Selects a temporary diagnostic view of the Blur V2 inputs.
 */
enum class TextRevealBlurDebugView
{
  NORMAL,
  BLUR_ONLY,
  SHARP_ONLY,
  MEDIUM_BLUR_ONLY
};

/**
 * @brief Selects temporary diagnostic ownership timing for blurred coverage.
 */
enum class TextRevealBlurDebugTiming
{
  CONSERVATIVE,
  COMMON,                 ///< Diagnostic only; may expose blur from future Reveal units.
  VISIBLE_COVERAGE_ORACLE ///< Fixed-progress diagnostic that gates source coverage before blur.
};

/**
 * @brief Selects the temporary spatial-radius quality prototype.
 */
enum class TextRevealBlurSpatialMode
{
  TWO_STATE,
  MULTI_RADIUS
};

/**
 * @brief Selects the temporary Multi-Radius preprocessing quality policy.
 */
enum class TextRevealBlurPreprocessingMode
{
  CURRENT_SCALE,
  MULTI_RADIUS_QUALITY_SCALE
};

/**
 * @brief Selects the temporary Full-to-Medium and Medium-to-Sharp stage split.
 */
enum class TextRevealBlurStageSplit
{
  HALF,
  EARLY
};

/**
 * @brief Sets the internal sequence-aware Reveal blur prototype parameters.
 *
 * This integration hook intentionally keeps the experiment out of the public
 * Text::Reveal API while strength, sequence-local duration, sharpness curve,
 * and temporary rendering diagnostics are evaluated by the sample application.
 */
DALI_UI_API void SetTextRevealBlurForPrototype(Label                           label,
                                               float                           strength,
                                               float                           durationRatio,
                                               TextRevealBlurCurve             curve                   = TextRevealBlurCurve::LINEAR,
                                               TextRevealBlurDebugView         debugView               = TextRevealBlurDebugView::NORMAL,
                                               TextRevealBlurDebugTiming       debugTiming             = TextRevealBlurDebugTiming::CONSERVATIVE,
                                               TextRevealBlurSpatialMode       spatialMode             = TextRevealBlurSpatialMode::TWO_STATE,
                                               TextRevealBlurPreprocessingMode preprocessingMode       = TextRevealBlurPreprocessingMode::CURRENT_SCALE,
                                               TextRevealBlurStageSplit        stageSplit              = TextRevealBlurStageSplit::HALF,
                                               float                           ownershipOracleProgress = -1.0f);

} // namespace Integration
} // namespace Ui
} // namespace Dali

#endif // DALI_UI_TEXT_REVEAL_BLUR_PROTOTYPE_H
