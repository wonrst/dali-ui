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

#include <dali-ui-foundation/integration-api/label-impl.h>
#include <dali-ui-foundation/integration-api/text/text-reveal-blur-prototype.h>

namespace Dali
{
namespace Ui
{
namespace Integration
{
void SetTextRevealBlurForPrototype(Label                           label,
                                   float                           strength,
                                   float                           durationRatio,
                                   TextRevealBlurCurve             curve,
                                   TextRevealBlurDebugView         debugView,
                                   TextRevealBlurDebugTiming       debugTiming,
                                   TextRevealBlurSpatialMode       spatialMode,
                                   TextRevealBlurPreprocessingMode preprocessingMode,
                                   TextRevealBlurStageSplit        stageSplit,
                                   float                           ownershipOracleProgress)
{
  DALI_ASSERT_ALWAYS(label);
  auto&       implementation = static_cast<LabelImpl&>(label.GetImplementation());
  const float curveValue     = curve == TextRevealBlurCurve::EASE_IN_QUADRATIC
                                 ? 1.0f
                                 : (curve == TextRevealBlurCurve::SOFT ? 0.5f : 0.0f);
  implementation.SetTextRevealBlurForPrototype(strength,
                                               durationRatio,
                                               curveValue,
                                               static_cast<float>(debugView),
                                               static_cast<float>(debugTiming),
                                               static_cast<float>(spatialMode),
                                               static_cast<float>(preprocessingMode),
                                               stageSplit == TextRevealBlurStageSplit::EARLY ? 0.4f : 0.5f,
                                               ownershipOracleProgress);
}

} // namespace Integration
} // namespace Ui
} // namespace Dali
