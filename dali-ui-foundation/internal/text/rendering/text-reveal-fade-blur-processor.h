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

#ifndef DALI_UI_TEXT_REVEAL_FADE_BLUR_PROCESSOR_H
#define DALI_UI_TEXT_REVEAL_FADE_BLUR_PROCESSOR_H

#include <dali/public-api/adaptor-framework/pixel-buffer.h>

#include <cstdint>

namespace Dali::TextAbstraction
{
class FontClient;
}

namespace Dali
{
namespace Ui
{
namespace Text
{
class ModelInterface;

namespace Internal
{
namespace Reveal
{
/**
 * @brief Stores the resolved Fade+Blur preprocessing policy.
 *
 * The target radius is expressed in final text pixels. Kernel radii are
 * expressed in the selected preprocessing resolution.
 */
struct FadeBlurParameters
{
  float    referencePixelSize{1.0f};
  float    targetRadius{1.0f};
  float    scale{1.0f};
  uint32_t supportRadius{1u};
  uint32_t firstRadius{0u};
  uint32_t secondRadius{1u};
  uint32_t lowResolutionWidth{0u};
  uint32_t lowResolutionHeight{0u};
};

/**
 * @brief Resolves a cheap pixel-space text-size reference from final layout.
 *
 * Models without inline replacements reuse their existing line metrics. A
 * replacement model takes a setup-time rare path over the committed line glyph
 * ranges and ignores synthetic replacement glyphs. No glyph bitmap is read.
 * Final fitting and UI scale therefore affect the result, while line spacing
 * and inline replacement geometry are deliberately excluded.
 *
 * @param[in] model The final render model
 * @param[in] hasInlineReplacement Whether the model contains projected replacements
 * @param[in] fontClient The caller-owned font client used by the current text pipeline
 * @return The representative text height in final pixels, or zero when a
 *         replacement model has no ordinary text glyph
 */
float ResolveFadeBlurReferencePixelSize(const ModelInterface&       model,
                                        bool                        hasInlineReplacement,
                                        TextAbstraction::FontClient fontClient);

/**
 * @brief Resolves the authored strength into radius, scale, and box kernels.
 *
 * A negative strength selects the automatic perceptual policy. Non-zero
 * normalized strengths progressively weaken that same adaptive result; one
 * resolves exactly like automatic strength. Raster dimensions bound
 * preserved-color preblur resources.
 *
 * @param[in] referencePixelSize Final laid-out text height in pixels
 * @param[in] blurStrength Authored automatic sentinel or normalized strength
 * @param[in] rasterWidth Final text raster width, or zero when unavailable
 * @param[in] rasterHeight Final text raster height, or zero when unavailable
 * @return The resolved preprocessing parameters
 */
FadeBlurParameters ResolveFadeBlurParameters(float    referencePixelSize,
                                             float    blurStrength,
                                             uint32_t rasterWidth  = 0u,
                                             uint32_t rasterHeight = 0u);

/**
 * @brief Returns a tile guard band sufficient for blur coverage and ownership.
 *
 * The result is aligned to every supported preprocessing scale so adjacent
 * height tiles use the same downsample grid.
 */
uint32_t GetFadeBlurGuardBand(const FadeBlurParameters& parameters);

/**
 * @brief Downsamples and blurs a Fade+Blur setup resource.
 *
 * The target radius is expressed in final text pixels and is independent of
 * the preprocessing scale. The operation uses direct box downsampling and a
 * two-box separable blur so setup cost remains linear in the resource size.
 */
bool PrepareFadeBlurBuffer(PixelBuffer& buffer, float scale, float targetRadius);

/**
 * @brief Crops a processed buffer to the requested rectangle.
 */
bool CropFadeBlurBuffer(PixelBuffer& buffer,
                        uint32_t     offsetX,
                        uint32_t     offsetY,
                        uint32_t     width,
                        uint32_t     height);

/**
 * @brief Writes an L8 preblur into the alpha channel of full-size metadata.
 *
 * Low-resolution coverage is bilinearly projected directly into metadata,
 * avoiding both a generic full-size resize and a full-size blur temporary.
 */
void WriteFadeBlurCoverage(const PixelBuffer& coverage,
                           uint8_t*           metadata,
                           uint32_t           width,
                           uint32_t           height);

/**
 * @brief Materializes conservative later-start-wins blur timing in metadata B.
 *
 * The octagonal footprint omits negligible corners of the separable blur that
 * caused rectangular timing cut-outs while retaining conservative ownership
 * across the meaningful halo. Coverage-aware output is used only when no
 * separate preserved-color blur resource depends on the timing field.
 */
void MaterializeFadeBlurTiming(uint8_t* metadata,
                               uint32_t width,
                               uint32_t height,
                               float    scale,
                               float    targetRadius,
                               bool     coverageAware);

} // namespace Reveal
} // namespace Internal
} // namespace Text
} // namespace Ui
} // namespace Dali

#endif // DALI_UI_TEXT_REVEAL_FADE_BLUR_PROCESSOR_H
