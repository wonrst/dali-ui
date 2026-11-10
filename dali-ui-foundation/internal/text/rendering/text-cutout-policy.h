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

#ifndef DALI_UI_TEXT_CUTOUT_POLICY_H
#define DALI_UI_TEXT_CUTOUT_POLICY_H

#include <dali-ui-foundation/internal/text/styled-text/gradient-span-data.h>

namespace DALI_NAMESPACE
{
namespace Ui
{
namespace Text
{
/**
 * @brief Checks the supported shader cutout combinations without changing the request.
 * @param[in] spans Canonical winning span paints, or nullptr.
 * @param[in] marquee Whether scrolling is active.
 * @param[in] textGradient Whether the global foreground gradient is renderable.
 * @param[in] overlayGradient Whether the global overlay gradient is renderable.
 * @param[in] reveal Whether text reveal is enabled.
 * @param[in] replacement Whether valid inline replacements are present.
 * @param[in] emboss Whether the bevel effect is enabled.
 * @param[in] nonColorBackground Whether the owner has a non-COLOR background visual.
 * @return True if the request can use cropped shader cutout planes.
 */
inline bool IsShaderCutoutSupported(const Internal::GradientSpanModelData* spans,
                                    bool marquee, bool textGradient, bool overlayGradient,
                                    bool reveal, bool replacement, bool emboss, bool nonColorBackground = false)
{
  return !(marquee || textGradient || overlayGradient || reveal || replacement || emboss || nonColorBackground ||
           (spans && !spans->characterRuns.Empty()));
}
} // namespace Text
} // namespace Ui
} // namespace DALI_NAMESPACE
#endif // DALI_UI_TEXT_CUTOUT_POLICY_H
