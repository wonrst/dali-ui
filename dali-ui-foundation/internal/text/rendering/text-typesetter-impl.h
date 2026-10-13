#ifndef DALI_UI_TEXT_TYPESETTER_IMPL_H
#define DALI_UI_TEXT_TYPESETTER_IMPL_H

/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
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
#include <dali/devel-api/text-abstraction/font-client.h>
#include <dali/devel-api/text-abstraction/text-abstraction-definitions.h>
#include <dali/public-api/adaptor-framework/pixel-buffer.h>
#include <dali/public-api/common/dali-vector.h>
#include <dali/public-api/common/intrusive-ptr.h>
#include <dali/public-api/images/pixel.h>
#include <dali/public-api/object/ref-object.h>
#include <memory> ///< for std::unique_ptr

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/rendering/text-raster-geometry.h>
#include <dali-ui-foundation/internal/text/rendering/text-typesetter.h>
#include <dali-ui-foundation/internal/text/strikethrough-glyph-run.h>
#include <dali-ui-foundation/internal/text/text-enumerations.h>
#include <dali-ui-foundation/internal/text/underlined-glyph-run.h>

namespace DALI_NAMESPACE
{
namespace Ui
{
namespace Text
{
class ModelInterface;
class ViewModel;
struct FinalElisionResult;
struct RevealRasterContext;

namespace Internal
{
namespace Reveal
{
/**
 * @brief Expands discrete Reveal ownership by one texel for LINEAR text filtering.
 *
 * Existing raster-owned texels remain unchanged. Empty destinations receive
 * ownership from their original 8-neighborhood only, keep zero coverage in
 * the alpha channel, and conservatively select the latest start timing when
 * different units meet.
 *
 * @param[in,out] metadata A tightly packed RGBA8888 Reveal metadata buffer.
 * @param[in] width The buffer width in pixels.
 * @param[in] height The buffer height in pixels.
 */
void ExpandMetadataOwnership(uint8_t* metadata, uint32_t width, uint32_t height);
} // namespace Reveal
} // namespace Internal

/** @brief A checked raster destination in local text paint coordinates. */
struct RasterTarget
{
  PixelBuffer    buffer;
  Raster::Bounds bounds;
  bool           valid{true};
};

/**
 * @brief This class is seperated logics for TypeSetter.
 * It will reduce the complexicy of typesetter logic.
 */
struct Typesetter::Impl
{
public:
  /**
   * @brief Create an initialized image buffer filled with transparent color.
   *
   * Creates the pixel data used to generate the final image with the given size.
   *
   * @param[in] bufferWidth The width of the image buffer.
   * @param[in] bufferHeight The height of the image buffer.
   * @param[in] pixelFormat The format of the pixel in the image that the text is rendered as (i.e. either
   * Pixel::BGRA8888 or Pixel::L8).
   *
   * @return An image buffer.
   */
  static PixelBuffer CreateTransparentImageBuffer(const uint32_t bufferWidth, const uint32_t bufferHeight,
                                                  const Pixel::Format pixelFormat);

public: // Constructor & Destructor
  /**
   * @brief Creates a Typesetter impl instance.
   */
  Impl(const ModelInterface* const model);

  ~Impl();

public:
  /**
   * @brief Retrieves the pointer to the view model.
   *
   * @return A pointer to the view model.
   */
  ViewModel* GetViewModel();

  /**
   * @brief Sets the model used by subsequent render calls.
   *
   * @param[in] model Pointer to the text's data model.
   */
  void SetModel(const ModelInterface* model);

  /**
   * @brief Sets the authoritative resolved glyph sequence.
   *
   * @param[in] result The replacement or non-replacement END final result.
   */
  void SetFinalElisionResult(const FinalElisionResult* result);

  /**
   * @brief Set the font client.
   *
   * Set the font client used in the update/render process of the text model.
   *
   * @param[in] fontClient The font client used by the Typesetter.
   */
  void SetFontClient(TextAbstraction::FontClient& fontClient);

  /**
   * @brief Get the font client.
   *
   * @return The font client used by the Typesetter.
   */
  TextAbstraction::FontClient& GetFontClient();

  void BeginRevealMetadata(uint32_t width, uint32_t height, const Internal::Reveal::Plan& plan);

  PixelData EndRevealMetadata();

public: // Image buffer creation
  void DrawGlyphsBackground(PixelBuffer& buffer, const uint32_t bufferWidth, const uint32_t bufferHeight,
                            const bool ignoreHorizontalAlignment, const int32_t horizontalOffset,
                            const int32_t verticalOffset, const Raster::SourceClip* sourceClip = nullptr);

  /**
   * @brief Create & draw the image buffer for the given range of the glyphs in the given style.
   *
   * Does the following operations:
   * - Retrieves the data buffers from the text model.
   * - Creates the pixel data used to generate the final image with the given size.
   * - Traverse the visible glyphs, retrieve their bitmaps and compose the final pixel data.
   *
   * @param[in] bufferWidth The width of the image buffer.
   * @param[in] bufferHeight The height of the image buffer.
   * @param[in] style The style of the text.
   * @param[in] ignoreHorizontalAlignment Whether to ignore the horizontal alignment, not ignored by default.
   * @param[in] pixelFormat The format of the pixel in the image that the text is rendered as (i.e. either
   * Pixel::BGRA8888 or Pixel::L8).
   * @param[in] horizontalOffset The horizontal offset to be added to the glyph's position.
   * @param[in] verticalOffset The vertical offset to be added to the glyph's position.
   * @param[in] fromGlyphIndex The index of the first glyph within the text to be drawn
   * @param[in] toGlyphIndex The index of the last glyph within the text to be drawn
   * @param[in] revealOffsetY Metadata storage origin in the original target, otherwise zero.
   * @param[in] revealFullHeight Original target height for metadata line traversal, or zero to use bufferHeight.
   * @param[out] recolorMask Optional paired coefficient plane for single-color RGBA8888 STYLE_NONE.
   *
   * @return An image buffer with the text.
   */
  PixelBuffer CreateImageBuffer(const uint32_t bufferWidth, const uint32_t bufferHeight,
                                const Typesetter::Style style, const bool ignoreHorizontalAlignment,
                                const Pixel::Format pixelFormat, const int32_t horizontalOffset,
                                const int32_t verticalOffset, const TextAbstraction::GlyphIndex fromGlyphIndex,
                                const TextAbstraction::GlyphIndex toGlyphIndex,
                                uint32_t revealOffsetY = 0u, uint32_t revealFullHeight = 0u, PixelBuffer* recolorMask = nullptr, const Raster::SourceClip* sourceClip = nullptr, Raster::BoundsAccumulator* sourceBounds = nullptr);

  /**
   * @brief Create & draw a L8 mask containing TextGradient target glyph coverage only.
   *
   * The mask includes default-color monochrome glyph fill. Explicit-color glyphs, renderable color glyphs and text
   * styles are excluded.
   *
   * @param[in] bufferWidth The width of the image buffer.
   * @param[in] bufferHeight The height of the image buffer.
   * @param[in] ignoreHorizontalAlignment Whether to ignore the horizontal alignment, not ignored by default.
   * @param[in] pixelFormat The format of the mask. The initial implementation uses Pixel::L8.
   * @param[in] horizontalOffset The horizontal offset to be added to the glyph's position.
   * @param[in] verticalOffset The vertical offset to be added to the glyph's position.
   * @param[in] fromGlyphIndex The index of the first glyph within the text to be drawn.
   * @param[in] toGlyphIndex The index of the last glyph within the text to be drawn.
   *
   * @param[in,out] target Optional tight destination; gradient coordinates retain the original buffer domain.
   * @param[in] sourceClip Optional pre-effect source planes.
   * @return An image buffer containing TextGradient target glyph coverage only.
   */
  PixelBuffer CreateTextGradientMaskImageBuffer(const uint32_t bufferWidth, const uint32_t bufferHeight,
                                                const bool                        ignoreHorizontalAlignment,
                                                const Pixel::Format               pixelFormat,
                                                const int32_t                     horizontalOffset,
                                                const int32_t                     verticalOffset,
                                                const TextAbstraction::GlyphIndex fromGlyphIndex,
                                                const TextAbstraction::GlyphIndex toGlyphIndex,
                                                RasterTarget* target = nullptr, const Raster::SourceClip* sourceClip = nullptr);

  /**
   * @brief Create & draw a RGBA buffer containing TextGradient non-target glyphs only.
   *
   * Explicit-color glyphs and renderable color glyphs are preserved. Default-color monochrome glyphs are excluded.
   *
   * @param[in] bufferWidth The width of the image buffer.
   * @param[in] bufferHeight The height of the image buffer.
   * @param[in] ignoreHorizontalAlignment Whether to ignore the horizontal alignment, not ignored by default.
   * @param[in] pixelFormat The format of the preserved buffer. The initial implementation uses Pixel::RGBA8888.
   * @param[in] horizontalOffset The horizontal offset to be added to the glyph's position.
   * @param[in] verticalOffset The vertical offset to be added to the glyph's position.
   * @param[in] fromGlyphIndex The index of the first glyph within the text to be drawn.
   * @param[in] toGlyphIndex The index of the last glyph within the text to be drawn.
   *
   * @param[in,out] target Optional tight destination; gradient coordinates retain the original buffer domain.
   * @param[in] sourceClip Optional pre-effect source planes.
   * @return An image buffer containing TextGradient non-target glyphs only.
   */
  PixelBuffer CreateTextGradientPreservedImageBuffer(const uint32_t bufferWidth, const uint32_t bufferHeight,
                                                     const bool                        ignoreHorizontalAlignment,
                                                     const Pixel::Format               pixelFormat,
                                                     const int32_t                     horizontalOffset,
                                                     const int32_t                     verticalOffset,
                                                     const TextAbstraction::GlyphIndex fromGlyphIndex,
                                                     const TextAbstraction::GlyphIndex toGlyphIndex,
                                                     RasterTarget* target = nullptr, const Raster::SourceClip* sourceClip = nullptr);

  /** @brief Request-local style runs shared by bounds and drawing passes. */
  struct StyleRuns
  {
    Vector<UnderlinedGlyphRun>    underline;
    Vector<StrikethroughGlyphRun> strikethrough;
  };

  /** @brief Composites a contained, tightly packed premultiplied RGBA ROI behind a plane. */
  static bool CompositeBehind(RasterTarget& destination, const RasterTarget& source);

  /** @brief Draws a style into a checked target without changing layout coordinates. */
  bool RasterizeStyle(uint32_t legacyWidth, uint32_t legacyHeight, Typesetter::Style style,
                      bool ignoreHorizontalAlignment, int32_t x, int32_t y,
                      TextAbstraction::GlyphIndex first, TextAbstraction::GlyphIndex last,
                      const StyleRuns& runs, RasterTarget& target, PixelBuffer* recolorMask = nullptr, const Raster::SourceClip* sourceClip = nullptr);

  bool RasterizeBackground(uint32_t legacyWidth, uint32_t legacyHeight, bool ignoreHorizontalAlignment,
                           int32_t x, int32_t y, RasterTarget& target, const Raster::SourceClip* sourceClip = nullptr);

  /** @brief Resolves one existing style pass without a destination allocation. */
  bool ResolveStyleBounds(uint32_t width, uint32_t height, Typesetter::Style style,
                          bool ignoreHorizontalAlignment, int32_t x, int32_t y,
                          TextAbstraction::GlyphIndex first, TextAbstraction::GlyphIndex last, const StyleRuns& runs, Raster::Bounds& result, const Raster::SourceClip* sourceClip = nullptr);

  /** @brief Resolves the existing range-background pass without drawing it. */
  bool ResolveBackgroundBounds(uint32_t width, uint32_t height, bool ignoreHorizontalAlignment,
                               int32_t x, int32_t y, Raster::Bounds& result, const Raster::SourceClip* sourceClip = nullptr);

private:
  template<bool MEASURE, bool TRANSLATE = false>
  PixelBuffer ProcessImage(uint32_t width, uint32_t height, Typesetter::Style style, bool ignoreHorizontalAlignment,
                           Pixel::Format format, int32_t x, int32_t y, TextAbstraction::GlyphIndex first,
                           TextAbstraction::GlyphIndex last, uint32_t revealOffsetY, uint32_t revealFullHeight,
                           Raster::BoundsAccumulator* bounds, const StyleRuns* runs, RasterTarget* target = nullptr, PixelBuffer* recolorMask = nullptr, const Raster::SourceClip* sourceClip = nullptr, Raster::BoundsAccumulator* sourceBounds = nullptr);

  template<bool MEASURE, bool TRANSLATE = false>
  void ProcessGlyphsBackground(PixelBuffer& buffer, uint32_t width, uint32_t height, bool ignoreHorizontalAlignment,
                               int32_t x, int32_t y, Raster::BoundsAccumulator* bounds, RasterTarget* target = nullptr, const Raster::SourceClip* sourceClip = nullptr);

  std::unique_ptr<ViewModel>           mModel;
  std::unique_ptr<RevealRasterContext> mRevealRasterContext;
  TextAbstraction::FontClient          mFontClient;
};

} // namespace Text

} // namespace Ui

} //namespace DALI_NAMESPACE

#endif // DALI_UI_TEXT_TYPESETTER_IMPL_H
