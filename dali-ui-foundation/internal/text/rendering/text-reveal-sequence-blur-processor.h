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

#ifndef DALI_UI_TEXT_REVEAL_SEQUENCE_BLUR_PROCESSOR_H
#define DALI_UI_TEXT_REVEAL_SEQUENCE_BLUR_PROCESSOR_H

#include <dali/public-api/adaptor-framework/pixel-buffer.h>

namespace Dali
{
namespace Ui
{
namespace Text
{
namespace Internal
{
namespace Reveal
{
/**
 * @brief Stores the setup policy for the sequence-aware Blur V2 prototype.
 */
struct SequenceBlurParameters
{
  float targetRadius{1.0f};
  float preprocessingScale{1.0f};
  float mediumRadius{0.75f};
};

/**
 * @brief Maps normalized strength to a text-size-relative preprocessing policy.
 */
SequenceBlurParameters ResolveSequenceBlurParameters(float referencePixelSize,
                                                     float blurStrength,
                                                     bool  useMultiRadiusQualityScale = false);

/**
 * @brief Converts exact foreground coverage and ownership into Blur V2 metadata.
 *
 * RG keeps the exact 16-bit unit start. B becomes conservative quantized unit
 * timing over the blur footprint, A becomes preblur coverage, and the L8
 * sidecar becomes conservative sequence-start timing over the same footprint.
 * When requested, mediumBlurCoverage is generated from the original coverage
 * in the same preprocessing domain using approximately half the full radius.
 * A non-negative ownershipOracleProgress gates source coverage before blur and
 * is intended only for fixed-progress ownership-quality diagnostics.
 */
bool PrepareSequenceBlurMetadata(PixelBuffer&                  metadata,
                                 PixelBuffer&                  sequenceMetadata,
                                 const SequenceBlurParameters& parameters,
                                 PixelBuffer*                  mediumBlurCoverage      = nullptr,
                                 float                         ownershipOracleProgress = -1.0f);

} // namespace Reveal
} // namespace Internal
} // namespace Text
} // namespace Ui
} // namespace Dali

#endif // DALI_UI_TEXT_REVEAL_SEQUENCE_BLUR_PROCESSOR_H
