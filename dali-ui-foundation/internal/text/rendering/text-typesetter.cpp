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
#include <dali/integration-api/debug.h>
#include <dali/integration-api/trace.h>
#include <dali/public-api/common/constants.h>
#include <dali/public-api/math/math-utils.h>
#include <memory.h>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/glyph-metrics-helper.h>
#include <dali-ui-foundation/internal/text/line-helper-functions.h>
#include <dali-ui-foundation/internal/text/marquee/marquee-start-geometry.h>
#include <dali-ui-foundation/internal/text/rendering/styles/character-spacing-helper-functions.h>
#include <dali-ui-foundation/internal/text/rendering/styles/strikethrough-helper-functions.h>
#include <dali-ui-foundation/internal/text/rendering/styles/underline-helper-functions.h>
#include <dali-ui-foundation/internal/text/rendering/text-raster-coordinate.h>
#include <dali-ui-foundation/internal/text/rendering/text-typesetter-impl.h>
#include <dali-ui-foundation/internal/text/rendering/text-typesetter.h>
#include <dali-ui-foundation/internal/text/rendering/view-model.h>
#include <dali-ui-foundation/internal/text/replacement/replacement-run-snapshot.h>

namespace DALI_NAMESPACE
{
namespace Ui
{
namespace Text
{
namespace
{
DALI_INIT_TRACE_FILTER(gTraceFilter, DALI_TRACE_TEXT_PERFORMANCE_MARKER, false);

// Validate once per render operation, before allocating or converting dimensions.
// Style/metadata passes use RGBA; plain text and masks may use L8.
bool ResolveRasterSize(const Vector2& size, uint32_t& width, uint32_t& height, uint32_t bytesPerPixel = 4u)
{
  int32_t x, y;
  if(!Raster::Convert(size.width, x) || !Raster::Convert(size.height, y) || x < 0 || y < 0 ||
     !Raster::BufferFits(static_cast<uint32_t>(x), static_cast<uint32_t>(y), bytesPerPixel))
  {
    return false;
  }
  width  = static_cast<uint32_t>(x);
  height = static_cast<uint32_t>(y);
  return true;
}

// Alignment is resolved in the legacy target, before any paint-origin change.
bool ResolveRasterPen(ViewModel& viewModel, const Vector2& size,
                      const Vector2& originSize, int32_t& penX, int32_t& penY)
{
  const Size& layoutSize = viewModel.GetLayoutSize();

  // Horizontal alignment is already part of the line layout.
  penX = 0;

  // Consider the case where pen y needs to be calculated based on the original size when rendering marquee texture.
  bool  isOriginHeight = originSize.height > 0.0f ? true : false;
  float controlHeight  = isOriginHeight ? viewModel.GetControlSize().height : size.height;
  float layoutHeight   = isOriginHeight ? originSize.height : layoutSize.height;

  // Set the offset for the vertical alignment.
  penY = 0;
  switch(viewModel.GetVerticalAlignment())
  {
    case Alignment::START:
    {
      // No offset to add.
      break;
    }
    case Alignment::CENTER:
    {
      if(!Raster::Convert(std::round(0.5f * (controlHeight - layoutHeight)), penY))
      {
        return false;
      }
      break;
    }
    case Alignment::END:
    {
      if(!Raster::Convert(controlHeight - layoutHeight, penY))
      {
        return false;
      }
      break;
    }
  }

  const bool isCutoutEnabled = viewModel.IsCutoutEnabled();
  if(isCutoutEnabled)
  {
    Vector2 offset = viewModel.GetOffsetWithCutout();
    if(!Raster::Convert(offset.x, penX) || !Raster::Convert(offset.y, penY))
    {
      return false;
    }
  }

  return true;
}

bool ResolveSourcePlanes(const Vector4* viewport, bool elide, const Raster::Bounds& raw,
                         Raster::SourceClip& clip)
{
  clip = {};
  if(!viewport || elide) return true;
  if(viewport->z < 0.f || viewport->w < 0.f ||
     !Raster::Convert(viewport->x, clip.viewport.left) ||
     !Raster::Convert(viewport->y, clip.viewport.top) ||
     !Raster::Convert(viewport->x + viewport->z, clip.viewport.right) ||
     !Raster::Convert(viewport->y + viewport->w, clip.viewport.bottom)) return false;
  if(!raw.Empty())
  {
    if(raw.left < clip.viewport.left) clip.planes |= Raster::SourceClip::LEFT;
    if(raw.right > clip.viewport.right) clip.planes |= Raster::SourceClip::RIGHT;
    if(raw.top < clip.viewport.top) clip.planes |= Raster::SourceClip::TOP;
    if(raw.bottom > clip.viewport.bottom) clip.planes |= Raster::SourceClip::BOTTOM;
  }
  return true;
}

// PixelBuffer's CPU Gaussian kernel spans [-ceil(radius), ceil(radius)].
// Keep its full support, including taps below the convolution threshold; bounds
// must not depend on color, 8-bit quantization or mirror-repeat target clipping.
// Adaptor owns the kernel implementation; this is its raster-support contract.
bool ExpandBlurSupport(Raster::Bounds& bounds, float radius)
{
  if(!std::isfinite(radius))
  {
    return false;
  }
  if(bounds.Empty() || radius <= Math::MACHINE_EPSILON_1)
  {
    return true;
  }
  int32_t support;
  if(!Raster::Convert(std::ceil(radius), support) || support < 0)
  {
    return false;
  }
  Raster::BoundsAccumulator expanded;
  expanded.Include(static_cast<int64_t>(bounds.left) - support, static_cast<int64_t>(bounds.top) - support,
                   static_cast<int64_t>(bounds.right) + support, static_cast<int64_t>(bounds.bottom) + support);
  if(!expanded.valid)
  {
    return false;
  }
  bounds = expanded.bounds;
  return true;
}

/**
 * @brief Fast multiply & divide by 255. It wiil be useful when we applying alpha value in color
 *
 * @param x The value between [0..255]
 * @param y The value between [0..255]
 * @return (x*y)/255
 */
inline uint8_t MultiplyAndNormalizeColor(const uint8_t x, const uint8_t y) noexcept
{
  const uint32_t xy = static_cast<const uint32_t>(x) * y;
  return ((xy << 15) + (xy << 7) + xy) >> 23;
}

/**
 * @brief Fast multiply & Summation & divide by 255.
 *
 * @param x1 The value between [0..255]
 * @param y1 The value between [0..255]
 * @param x2 The value between [0..255]
 * @param y2 The value between [0..255]
 * @return min(255, (x1*y1)/255 + (x2*y2)/255)
 */
inline uint8_t MultiplyAndSummationAndNormalizeColor(const uint8_t x1, const uint8_t y1, const uint8_t x2,
                                                     const uint8_t y2) noexcept
{
  const uint32_t xy1 = static_cast<const uint32_t>(x1) * y1;
  const uint32_t xy2 = static_cast<const uint32_t>(x2) * y2;
  const uint32_t res = std::min(65025u, xy1 + xy2); // 65025 is 255 * 255.
  return ((res + ((res + 257) >> 8)) >> 8);         // fast divide by 255.
}

/**
 * @brief Combine the two RGBA image buffers together.
 *
 * The top layer buffer will blend over the bottom layer buffer:
 * - If the pixel is not fully opaque from either buffer, it will be blended with
 *   the pixel from the other buffer and copied to the combined buffer.
 * - If the pixels from both buffers are fully opaque, the pixels from the top layer
 *   buffer will be copied to the combined buffer.
 *
 * Due to the performance issue, We need to re-use input'ed pixelBuffer memory.
 * We can determine which pixelBuffer's memory is destination
 *
 * @param[in, out] topPixelBuffer The top layer buffer.
 * @param[in, out] bottomPixelBuffer The bottom layer buffer.
 * @param[in] bufferWidth The width of the image buffer.
 * @param[in] bufferHeight The height of the image buffer.
 * @param[in] storeResultIntoTop True if we store the combined image buffer result into topPixelBuffer.
 * False if we store the combined image buffer result into bottomPixelBuffer.
 *
 */
void CombineImageBuffer(PixelBuffer& __restrict__ topPixelBuffer,
                        PixelBuffer& __restrict__ bottomPixelBuffer, const uint32_t bufferWidth,
                        const uint32_t bufferHeight, bool storeResultIntoTop)
{
  // Assume that we always combine two RGBA images
  // Jump with 4bytes for optimize runtime.
  uint32_t* topBuffer    = reinterpret_cast<uint32_t*>(topPixelBuffer.GetBuffer());
  uint32_t* bottomBuffer = reinterpret_cast<uint32_t*>(bottomPixelBuffer.GetBuffer());

  if(topBuffer == nullptr && bottomBuffer == nullptr)
  {
    // Nothing to do if both buffers are empty.
    return;
  }

  if(topBuffer == nullptr)
  {
    // Nothing to do if topBuffer is empty.
    // If we need to store the result into top, change topPixelBuffer as bottomPixelBuffer.
    if(storeResultIntoTop)
    {
      topPixelBuffer = bottomPixelBuffer;
    }
    return;
  }

  if(bottomBuffer == nullptr)
  {
    // Nothing to do if bottomBuffer is empty.
    // If we need to store the result into bottom, change bottomPixelBuffer as topPixelBuffer.
    if(!storeResultIntoTop)
    {
      bottomPixelBuffer = topPixelBuffer;
    }
    return;
  }

  const uint32_t bufferSizeInt = bufferWidth * bufferHeight;

  uint32_t* __restrict__ combinedBuffer       = storeResultIntoTop ? topBuffer : bottomBuffer;
  uint8_t* __restrict__ topAlphaBufferPointer = reinterpret_cast<uint8_t*>(topBuffer) + 3;

  for(uint32_t pixelIndex = 0; pixelIndex < bufferSizeInt; ++pixelIndex)
  {
    // If the alpha of the pixel in either buffer is not fully opaque, blend the two pixels.
    // Otherwise, copy pixel from topBuffer to combinedBuffer.
    // Note : Be careful when we read & write into combinedBuffer. It can be write into same pointer.

    uint8_t topAlpha = *topAlphaBufferPointer;

    if(topAlpha == 0)
    {
      // Copy the pixel from bottomBuffer to combinedBuffer
      if(storeResultIntoTop)
      {
        *(combinedBuffer) = *(bottomBuffer);
      }
    }
    else if(topAlpha == 255)
    {
      // Copy the pixel from topBuffer to combinedBuffer
      if(!storeResultIntoTop)
      {
        *(combinedBuffer) = *(topBuffer);
      }
    }
    else
    {
      // At least one pixel is not fully opaque
      // "Over" blend the the pixel from topBuffer with the pixel in bottomBuffer
      uint32_t blendedBottomBufferColor                    = *(bottomBuffer);
      uint8_t* __restrict__ blendedBottomBufferColorBuffer = reinterpret_cast<uint8_t*>(&blendedBottomBufferColor);

      blendedBottomBufferColorBuffer[0] = MultiplyAndNormalizeColor(blendedBottomBufferColorBuffer[0], 255 - topAlpha);
      blendedBottomBufferColorBuffer[1] = MultiplyAndNormalizeColor(blendedBottomBufferColorBuffer[1], 255 - topAlpha);
      blendedBottomBufferColorBuffer[2] = MultiplyAndNormalizeColor(blendedBottomBufferColorBuffer[2], 255 - topAlpha);
      blendedBottomBufferColorBuffer[3] = MultiplyAndNormalizeColor(blendedBottomBufferColorBuffer[3], 255 - topAlpha);

      *(combinedBuffer) = *(topBuffer) + blendedBottomBufferColor;
    }

    // Increase each buffer's pointer.
    ++combinedBuffer;
    ++topBuffer;
    ++bottomBuffer;
    topAlphaBufferPointer += sizeof(uint32_t) / sizeof(uint8_t);
  }
}

} // namespace

TypesetterPtr Typesetter::New(const ModelInterface* const model)
{
  return TypesetterPtr(new Typesetter(model));
}

ViewModel* Typesetter::GetViewModel()
{
  return mImpl->GetViewModel();
}

void Typesetter::SetModel(const ModelInterface* model)
{
  mImpl->SetModel(model);
}

void Typesetter::SetFinalElisionResult(const FinalElisionResult* result)
{
  mImpl->SetFinalElisionResult(result);
}

void Typesetter::SetFontClient(TextAbstraction::FontClient& fontClient)
{
  mImpl->SetFontClient(fontClient);
}

TextAbstraction::FontClient Typesetter::GetFontClient()
{
  return mImpl->GetFontClient();
}

PixelData Typesetter::Render(const Vector2& size, Direction textDirection,
                             RenderBehaviour behaviour, bool ignoreHorizontalAlignment, Pixel::Format pixelFormat,
                             const Vector2& originSize, PixelData* recolorMask, const Raster::SourceClip* sourceClip)
{
  PixelBuffer mask;
  PixelBuffer result =
    RenderWithPixelBuffer(size, textDirection, behaviour, ignoreHorizontalAlignment, pixelFormat, originSize, recolorMask ? &mask : nullptr, sourceClip);
  if(recolorMask)
  {
    *recolorMask = result && mask ? PixelBuffer::Convert(mask) : PixelData{};
  }
  PixelData pixelData = result ? PixelBuffer::Convert(result) : PixelData{};

  return pixelData;
}

MarqueeTextureAnchor Typesetter::ResolveMarqueeTextureAnchor(const MarqueeStartAnchor& anchor) const
{
  if(!anchor.valid)
  {
    return {};
  }

  // The caller resolves immediately after the primary marquee Render(). The
  // ViewModel therefore describes the glyphs and positions actually used to
  // generate that texture, not a source-layout estimate.
  const ViewModel&              viewModel          = *mImpl->GetViewModel();
  const Length                  numberOfGlyphs     = viewModel.GetNumberOfGlyphs();
  const Vector<CharacterIndex>& glyphsToCharacters = viewModel.GetGlyphsToCharacters();
  const GlyphInfo*              glyphs             = viewModel.GetGlyphs();
  const Vector2*                positions          = viewModel.GetLayout();
  if(numberOfGlyphs == 0u || glyphsToCharacters.Count() != numberOfGlyphs || !glyphs || !positions)
  {
    return {};
  }

  Length occurrence = 0u;
  for(GlyphIndex glyphIndex = 0u; glyphIndex < numberOfGlyphs; ++glyphIndex)
  {
    if(glyphsToCharacters[glyphIndex] != anchor.characterIndex)
    {
      continue;
    }

    if(occurrence++ != anchor.glyphOccurrence)
    {
      continue;
    }

    const GlyphInfo& glyph    = glyphs[glyphIndex];
    const float      textureX = positions[glyphIndex].x;
    if(IsSyntheticReplacementGlyph(glyph) ||
       glyph.width <= 0.01f || glyph.height <= 0.01f || !std::isfinite(textureX))
    {
      return {};
    }
    return {textureX, true};
  }

  return {};
}

PixelData Typesetter::RenderTextGradientMask(const Vector2& size, Direction,
                                             bool ignoreHorizontalAlignment, Pixel::Format pixelFormat,
                                             const Vector2& originSize, const Raster::SourceClip* sourceClip)
{
  uint32_t bufferWidth, bufferHeight;
  if(!ResolveRasterSize(size, bufferWidth, bufferHeight, Pixel::GetBytesPerPixel(pixelFormat)))
  {
    return {};
  }

  DALI_TRACE_SCOPE(gTraceFilter, "DALI_TEXT_RENDERING_TYPESETTER_TEXT_GRADIENT_MASK");

  auto& viewModel = *(mImpl->GetViewModel());

  viewModel.ElideGlyphs(mImpl->GetFontClient());

  const Size& layoutSize = viewModel.GetLayoutSize();
  int32_t     penX       = 0;

  const bool  isOriginHeight = originSize.height > 0.0f ? true : false;
  const float controlHeight  = isOriginHeight ? viewModel.GetControlSize().height : size.height;
  const float layoutHeight   = isOriginHeight ? originSize.height : layoutSize.height;

  int32_t penY = 0u;
  switch(viewModel.GetVerticalAlignment())
  {
    case Alignment::START:
    {
      break;
    }
    case Alignment::CENTER:
    {
      if(!Raster::Convert(std::round(0.5f * (controlHeight - layoutHeight)), penY))
      {
        return {};
      }
      break;
    }
    case Alignment::END:
    {
      if(!Raster::Convert(controlHeight - layoutHeight, penY))
      {
        return {};
      }
      break;
    }
  }

  if(viewModel.IsCutoutEnabled())
  {
    Vector2 offset = viewModel.GetOffsetWithCutout();
    if(!Raster::Convert(offset.x, penX) || !Raster::Convert(offset.y, penY))
    {
      return {};
    }
  }

  auto startIndexOfGlyphs = viewModel.GetStartIndexOfElidedGlyphs();
  auto endIndexOfGlyphs   = viewModel.GetEndIndexOfElidedGlyphs();

  PixelBuffer result =
    mImpl->CreateTextGradientMaskImageBuffer(bufferWidth, bufferHeight, ignoreHorizontalAlignment, pixelFormat, penX,
                                             penY, startIndexOfGlyphs, endIndexOfGlyphs, nullptr, sourceClip);

  return result ? PixelBuffer::Convert(result) : PixelData{};
}

PixelData Typesetter::RenderTextGradientPreserved(const Vector2& size, Direction,
                                                  bool ignoreHorizontalAlignment, Pixel::Format pixelFormat,
                                                  const Vector2& originSize, const Raster::SourceClip* sourceClip)
{
  uint32_t bufferWidth, bufferHeight;
  if(!ResolveRasterSize(size, bufferWidth, bufferHeight, Pixel::GetBytesPerPixel(pixelFormat)))
  {
    return {};
  }

  DALI_TRACE_SCOPE(gTraceFilter, "DALI_TEXT_RENDERING_TYPESETTER_TEXT_GRADIENT_PRESERVED");

  auto& viewModel = *(mImpl->GetViewModel());

  viewModel.ElideGlyphs(mImpl->GetFontClient());

  const Size& layoutSize = viewModel.GetLayoutSize();
  int32_t     penX       = 0;

  const bool  isOriginHeight = originSize.height > 0.0f ? true : false;
  const float controlHeight  = isOriginHeight ? viewModel.GetControlSize().height : size.height;
  const float layoutHeight   = isOriginHeight ? originSize.height : layoutSize.height;

  int32_t penY = 0u;
  switch(viewModel.GetVerticalAlignment())
  {
    case Alignment::START:
    {
      break;
    }
    case Alignment::CENTER:
    {
      if(!Raster::Convert(std::round(0.5f * (controlHeight - layoutHeight)), penY))
      {
        return {};
      }
      break;
    }
    case Alignment::END:
    {
      if(!Raster::Convert(controlHeight - layoutHeight, penY))
      {
        return {};
      }
      break;
    }
  }

  if(viewModel.IsCutoutEnabled())
  {
    Vector2 offset = viewModel.GetOffsetWithCutout();
    if(!Raster::Convert(offset.x, penX) || !Raster::Convert(offset.y, penY))
    {
      return {};
    }
  }

  auto startIndexOfGlyphs = viewModel.GetStartIndexOfElidedGlyphs();
  auto endIndexOfGlyphs   = viewModel.GetEndIndexOfElidedGlyphs();

  PixelBuffer result =
    mImpl->CreateTextGradientPreservedImageBuffer(bufferWidth, bufferHeight, ignoreHorizontalAlignment, pixelFormat,
                                                  penX, penY, startIndexOfGlyphs, endIndexOfGlyphs, nullptr, sourceClip);

  return result ? PixelBuffer::Convert(result) : PixelData{};
}

PixelData Typesetter::RenderWithCutout(const Vector2& size, Direction textDirection,
                                       PixelBuffer mask, RenderBehaviour behaviour,
                                       bool ignoreHorizontalAlignment, Pixel::Format pixelFormat, float originAlpha,
                                       const Vector2& originSize)
{
  PixelBuffer result =
    RenderWithPixelBuffer(size, textDirection, behaviour, ignoreHorizontalAlignment, pixelFormat, originSize);
  if(!result || !mask)
  {
    return {};
  }
  SetMaskForImageBuffer(mask, result, result.GetWidth(), result.GetHeight(), originAlpha);

  PixelData pixelData = result ? PixelBuffer::Convert(result) : PixelData{};

  return pixelData;
}

// Request-local geometry needed to rasterize effects without a common working
// canvas. Each effect ROI is resolved during the same geometry operation.
struct Typesetter::RasterPlan
{
  struct OverlayPass
  {
    Style          style;
    GlyphIndex     first;
    GlyphIndex     last;
    Raster::Bounds bounds;
  };
  std::vector<OverlayPass> overlayPasses;
  TextRasterGeometry       geometry;
  Impl::StyleRuns          runs;
  Raster::Bounds           outline;
  Raster::Bounds           shadow;
  Raster::Bounds           background;
  Raster::SourceClip       sourceClip;
};

bool Typesetter::ResolveSourceClip(const Vector2& size, const Vector4& viewport, Raster::SourceClip& sourceClip)
{
  sourceClip  = {};
  auto& model = *mImpl->GetViewModel();
  if(model.IsTextElideEnabled()) return true;
  uint32_t width, height;
  int32_t  x, y;
  if(!ResolveRasterSize(size, width, height) || !ResolveRasterPen(model, size, Size::ZERO, x, y)) return false;
  if(!ResolveSourcePlanes(&viewport, false, Raster::Bounds{}, sourceClip)) return false;
  model.ElideGlyphs(mImpl->GetFontClient());
  Impl::StyleRuns runs;
  Raster::Bounds  raw;
  if(!mImpl->ResolveStyleBounds(width, height, STYLE_NONE, false, x, y,
                                model.GetStartIndexOfElidedGlyphs(), model.GetEndIndexOfElidedGlyphs(), runs, raw)) return false;
  return ResolveSourcePlanes(&viewport, false, raw, sourceClip);
}

PixelData Typesetter::RenderFill(const Vector2& size, Direction direction, Pixel::Format format,
                                 const Vector4* viewport, Raster::SourceClip& clip, PixelData* recolorMask)
{
  clip = {};
  if(recolorMask) *recolorMask = {};
  auto& model = *mImpl->GetViewModel();
  if(!viewport || model.IsTextElideEnabled())
  {
    return Render(size, direction, RENDER_NO_STYLES, false, format, Size::ZERO, recolorMask);
  }
  if(!ResolveSourcePlanes(viewport, false, Raster::Bounds{}, clip)) return {};
  const bool storageInsideSource = viewport->x <= 0.f && viewport->y <= 0.f &&
                                   viewport->x + viewport->z >= size.x && viewport->y + viewport->w >= size.y;
  if(!storageInsideSource)
  {
    if(!ResolveSourceClip(size, *viewport, clip)) return {};
    return Render(size, direction, RENDER_NO_STYLES, false, format, Size::ZERO, recolorMask, clip.planes ? &clip : nullptr);
  }
  // Storage already clips fill. Collect its original bitmap support in the
  // same traversal to classify source planes before rendering shifted effects.
  Raster::BoundsAccumulator bounds;
  PixelBuffer               mask;
  auto                      fill = RenderWithPixelBuffer(size, direction, RENDER_NO_STYLES, false, format, Size::ZERO,
                                    recolorMask ? &mask : nullptr, nullptr, &bounds);
  if(!fill || !bounds.valid || !ResolveSourcePlanes(viewport, false, bounds.bounds, clip)) return {};
  if(recolorMask) *recolorMask = mask ? PixelBuffer::Convert(mask) : PixelData{};
  return PixelBuffer::Convert(fill);
}

bool Typesetter::ResolveRasterGeometry(const Vector2& size, Direction textDirection, TextRasterGeometry& result,
                                       bool ignoreHorizontalAlignment, const Vector2& originSize, const Vector4* sourceViewport)
{
  result = {};
  uint32_t width, height;
  if(!ResolveRasterSize(size, width, height))
  {
    return false;
  }
  if(width == 0u || height == 0u)
  {
    return true;
  }
  mImpl->GetViewModel()->ElideGlyphs(mImpl->GetFontClient());
  RasterPlan plan;
  if(!ResolveRasterPlan(size, textDirection, plan, ignoreHorizontalAlignment, originSize, sourceViewport))
  {
    return false;
  }
  result = plan.geometry;
  return true;
}

bool Typesetter::ResolveRasterPlan(const Vector2& size, Direction, RasterPlan& plan,
                                   bool ignoreHorizontalAlignment, const Vector2& originSize, const Vector4* sourceViewport)
{
  plan = {};
  uint32_t width, height;
  if(!ResolveRasterSize(size, width, height))
  {
    return false;
  }
  if(width == 0u || height == 0u)
  {
    return true;
  }

  auto& model = *mImpl->GetViewModel();
  // Both request entry points prepare the final ViewModel before this step.
  int32_t x, y;
  if(!ResolveRasterPen(model, size, originSize, x, y))
  {
    return false;
  }

  auto& runs = plan.runs;
  runs.underline.Resize(model.GetNumberOfUnderlineRuns());
  model.GetUnderlineRuns(runs.underline.Begin(), 0u, runs.underline.Count());
  runs.strikethrough.Resize(model.GetNumberOfStrikethroughRuns());
  model.GetStrikethroughRuns(runs.strikethrough.Begin(), 0u, runs.strikethrough.Count());

  auto&      geometry = plan.geometry;
  const auto first    = model.GetStartIndexOfElidedGlyphs();
  const auto last     = model.GetEndIndexOfElidedGlyphs();
  const auto resolve  = [&](Style style, GlyphIndex begin, GlyphIndex end, Raster::Bounds& bounds)
  {
    return mImpl->ResolveStyleBounds(width, height, style, ignoreHorizontalAlignment, x, y, begin, end, runs, bounds, plan.sourceClip.planes ? &plan.sourceClip : nullptr);
  };
  if(!resolve(STYLE_NONE, first, last, geometry.fill))
  {
    return false;
  }

  if(!ResolveSourcePlanes(sourceViewport, model.IsTextElideEnabled(), geometry.fill, plan.sourceClip)) return false;
  if(plan.sourceClip.planes && !resolve(STYLE_NONE, first, last, geometry.fill))
  {
    return false;
  }

  Raster::BoundsAccumulator underlay;
  for(Style style : {STYLE_OUTLINE, STYLE_SHADOW})
  {
    const bool  outline = style == STYLE_OUTLINE;
    const bool  enabled = outline ? model.IsOutlineEnabled() : model.IsShadowEnabled();
    const float alpha   = outline ? model.GetOutlineColor().a : model.GetShadowColor().a;
    if(enabled && fabsf(alpha) > Math::MACHINE_EPSILON_1)
    {
      Raster::Bounds bounds;
      if(!resolve(style, first, last, bounds) ||
         !ExpandBlurSupport(bounds, outline ? model.GetOutlineBlurRadius() : model.GetShadowBlurRadius()))
      {
        return false;
      }
      (outline ? plan.outline : plan.shadow) = bounds;
      underlay.Include(bounds);
    }
  }
  Raster::BoundsAccumulator background;
  if(model.IsBackgroundEnabled())
  {
    Raster::Bounds bounds;
    if(!resolve(STYLE_BACKGROUND, first, last, bounds))
    {
      return false;
    }
    background.Include(bounds);
    underlay.Include(bounds);
  }
  if(model.IsMarkupBackgroundColorSet())
  {
    Raster::Bounds bounds;
    if(!mImpl->ResolveBackgroundBounds(width, height, ignoreHorizontalAlignment, x, y, bounds, plan.sourceClip.planes ? &plan.sourceClip : nullptr))
    {
      return false;
    }
    background.Include(bounds);
    underlay.Include(bounds);
  }
  plan.background = background.bounds;
  if(model.IsBackgroundWithCutoutEnabled())
  {
    underlay.Include(0, 0, width, height);
  }

  Raster::BoundsAccumulator overlay;
  const auto                addOverlayPass = [&](Style style, GlyphIndex begin, GlyphIndex end)
  {
    Raster::Bounds bounds;
    if(!resolve(style, begin, end, bounds))
    {
      return false;
    }
    overlay.Include(bounds);
    if(!bounds.Empty())
    {
      plan.overlayPasses.push_back({style, begin, end, bounds});
    }
    return true;
  };
  for(Style style : {STYLE_UNDERLINE, STYLE_STRIKETHROUGH})
  {
    const bool underline = style == STYLE_UNDERLINE;
    if(underline ? model.IsUnderlineEnabled() : model.IsStrikethroughEnabled())
    {
      if(!addOverlayPass(style, underline ? first : 0u, last))
      {
        return false;
      }
    }
  }
  const auto resolveRun = [&](Style style, const GlyphRun& run)
  {
    const uint64_t end = static_cast<uint64_t>(run.glyphIndex) + run.numberOfGlyphs - 1u;
    return end <= std::numeric_limits<GlyphIndex>::max() &&
           addOverlayPass(style, run.glyphIndex, static_cast<GlyphIndex>(end));
  };
  // Range passes use the same source-domain ranges as their existing drawing
  // passes. The ViewModel traversal resolves them into final visible glyphs.
  if(model.IsMarkupUnderlineSet())
  {
    for(const auto& run : runs.underline)
    {
      if(run.glyphRun.numberOfGlyphs == 0u)
      {
        continue;
      }
      if(!resolveRun(STYLE_UNDERLINE, run.glyphRun))
      {
        return false;
      }
    }
  }
  if(model.IsMarkupStrikethroughSet())
  {
    for(const auto& run : runs.strikethrough)
    {
      if(run.glyphRun.numberOfGlyphs == 0u)
      {
        continue;
      }
      if(!resolveRun(STYLE_STRIKETHROUGH, run.glyphRun))
      {
        return false;
      }
    }
  }
  if(!underlay.valid || !overlay.valid)
  {
    return false;
  }
  geometry.underlay = underlay.bounds;
  geometry.overlay  = overlay.bounds;
  Raster::BoundsAccumulator composition;
  composition.Include(geometry.fill);
  composition.Include(geometry.underlay);
  composition.Include(geometry.overlay);
  if(!composition.valid)
  {
    return false;
  }
  geometry.composition = composition.bounds;
  return true;
}

namespace
{
bool PlaneSize(const Raster::Bounds& bounds, Pixel::Format format, uint32_t limit, uint32_t& width, uint32_t& height)
{
  if(bounds.Empty())
  {
    width = height = 0u;
    return true;
  }
  const int64_t w = static_cast<int64_t>(bounds.right) - bounds.left;
  const int64_t h = static_cast<int64_t>(bounds.bottom) - bounds.top;
  if(w > limit || h > limit || w > std::numeric_limits<int32_t>::max() || h > std::numeric_limits<int32_t>::max())
  {
    return false;
  }
  width  = static_cast<uint32_t>(w);
  height = static_cast<uint32_t>(h);
  return Raster::BufferFits(width, height, Pixel::GetBytesPerPixel(format));
}

bool AllocatePlane(const Raster::Bounds& bounds, Pixel::Format format, uint32_t limit, RasterTarget& target)
{
  uint32_t width, height;
  if(!PlaneSize(bounds, format, limit, width, height))
  {
    return false;
  }
  target.bounds = bounds;
  if(bounds.Empty())
  {
    return true;
  }
  target.buffer = PixelBuffer::New(width, height, format);
  if(!target.buffer || !target.buffer.GetBuffer())
  {
    return false;
  }
  memset(target.buffer.GetBuffer(), 0, static_cast<size_t>(width) * height * Pixel::GetBytesPerPixel(format));
  return true;
}

} // namespace

bool Typesetter::RenderRasterPlanes(const Vector2& size, Direction textDirection, Pixel::Format fillFormat,
                                    uint32_t maxDimension, TextRasterResult& result, bool colorGlyphMask, const Vector4* sourceViewport, bool textGradientMixed, bool embossMask, bool ignoreHorizontalAlignment, const Vector2& originSize)
{
  result = {};
  if(maxDimension == 0u || maxDimension > static_cast<uint32_t>(std::numeric_limits<int32_t>::max() / 2) ||
     (fillFormat != Pixel::L8 && fillFormat != Pixel::RGBA8888) || (colorGlyphMask && fillFormat != Pixel::RGBA8888))
  {
    return false;
  }
  uint32_t width, height;
  if(!ResolveRasterSize(size, width, height))
  {
    return false;
  }
  auto& model = *mImpl->GetViewModel();
  if(model.IsCutoutEnabled() || model.IsBackgroundWithCutoutEnabled())
  {
    return false;
  }
  // The existing Gaussian implementation uses int32 radius*radius and mirrored
  // int32 indexing. Validate before bitmap creation or any blur allocation.
  for(Style style : {STYLE_OUTLINE, STYLE_SHADOW})
  {
    const bool  outline = style == STYLE_OUTLINE;
    const bool  enabled = outline ? model.IsOutlineEnabled() : model.IsShadowEnabled();
    const float alpha   = outline ? model.GetOutlineColor().a : model.GetShadowColor().a;
    if(!enabled || fabsf(alpha) <= Math::MACHINE_EPSILON_1)
    {
      continue;
    }
    const float radius = outline ? model.GetOutlineBlurRadius() : model.GetShadowBlurRadius();
    const auto& offset = outline ? model.GetOutlineOffset() : model.GetShadowOffset();
    if(!std::isfinite(radius) || radius < 0.f || std::ceil(radius) > 46340.f || std::ceil(radius) * 2.0 > maxDimension ||
       !std::isfinite(offset.x) || !std::isfinite(offset.y) ||
       std::abs(static_cast<double>(offset.x)) > maxDimension || std::abs(static_cast<double>(offset.y)) > maxDimension)
    {
      return false;
    }
  }
  const uint32_t outline = model.IsOutlineEnabled() ? model.GetOutlineWidth() : 0u;
  if(static_cast<uint64_t>(outline) * 2u >= maxDimension)
  {
    return false;
  }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  try
#endif
  {
    // Prepare once, after scalar admission and before inspecting final glyphs.
    model.ElideGlyphs(mImpl->GetFontClient());
    for(GlyphIndex i = 0; i < model.GetNumberOfGlyphs(); ++i)
    {
      const auto& glyph = model.GetGlyphs()[i];
      if(IsSyntheticReplacementGlyph(glyph))
      {
        continue;
      }
      const double w = static_cast<double>(glyph.width) + 2.0 * outline;
      const double h = static_cast<double>(glyph.height) + 2.0 * outline;
      if(!(w >= 0.0 && h >= 0.0 && w <= maxDimension && h <= maxDimension))
      {
        return false;
      }
    }
    // Reject malformed final coordinates before extent/dash traversal. Empty
    // text and layout-only replacement glyphs remain valid inputs.
    for(GlyphIndex i = 0; i < model.GetNumberOfGlyphs(); ++i)
    {
      if(IsSyntheticReplacementGlyph(model.GetGlyphs()[i]))
      {
        continue;
      }
      int32_t coordinate;
      if(!Raster::Convert(model.GetLayout()[i].x, coordinate) || !Raster::Convert(model.GetLayout()[i].y, coordinate) ||
         std::abs(static_cast<double>(model.GetLayout()[i].x)) > maxDimension ||
         std::abs(static_cast<double>(model.GetLayout()[i].y)) > maxDimension)
      {
        return false;
      }
    }
    RasterPlan plan;
    if(!ResolveRasterPlan(size, textDirection, plan, ignoreHorizontalAlignment, originSize, sourceViewport))
    {
      return false;
    }
    uint32_t w, h;
    if(!PlaneSize(plan.geometry.fill, fillFormat, maxDimension, w, h))
    {
      return false;
    }
    for(const auto& bounds : {plan.geometry.underlay, plan.geometry.overlay, plan.shadow, plan.outline, plan.background})
    {
      if(!PlaneSize(bounds, Pixel::RGBA8888, maxDimension, w, h))
      {
        return false;
      }
    }
    int32_t penX, penY;
    if(!ResolveRasterPen(model, size, originSize, penX, penY))
    {
      return false;
    }
    const auto first = model.GetStartIndexOfElidedGlyphs();
    const auto last  = model.GetEndIndexOfElidedGlyphs();
    const auto draw  = [&](RasterTarget& target, Style style, GlyphIndex begin, GlyphIndex end)
    {
      return target.bounds.Empty() || mImpl->RasterizeStyle(width, height, style, ignoreHorizontalAlignment, penX, penY, begin, end, plan.runs, target, nullptr, plan.sourceClip.planes ? &plan.sourceClip : nullptr);
    };
    TextRasterResult pending;
    pending.geometry         = plan.geometry;
    pending.layoutSize       = model.GetLayoutSize();
    pending.legacyRasterSize = size;
    RasterTarget fill;
    PixelBuffer  mask;
    const auto*  sourceClip = plan.sourceClip.planes ? &plan.sourceClip : nullptr;
    if(textGradientMixed && (fillFormat != Pixel::RGBA8888 || colorGlyphMask || embossMask)) return false;
    if(embossMask && (fillFormat != Pixel::RGBA8888 || colorGlyphMask)) return false;
    if(!AllocatePlane(plan.geometry.fill, fillFormat, maxDimension, fill)) return false;
    if(!fill.bounds.Empty())
    {
      if(textGradientMixed)
      {
        RasterTarget gradientMask;
        if(!AllocatePlane(plan.geometry.fill, Pixel::L8, maxDimension, gradientMask) ||
           !mImpl->CreateTextGradientPreservedImageBuffer(width, height, ignoreHorizontalAlignment, fillFormat, penX, penY, first, last, &fill, sourceClip) ||
           !mImpl->CreateTextGradientMaskImageBuffer(width, height, ignoreHorizontalAlignment, Pixel::L8, penX, penY, first, last, &gradientMask, sourceClip))
        {
          return false;
        }
        pending.textGradientMask = PixelBuffer::Convert(gradientMask.buffer);
      }
      else if(!mImpl->RasterizeStyle(width, height, STYLE_NONE, ignoreHorizontalAlignment, penX, penY, first, last,
                                     plan.runs, fill, colorGlyphMask ? &mask : nullptr, sourceClip))
      {
        return false;
      }
    }
    if(embossMask && !fill.bounds.Empty())
    {
      RasterTarget rawMask;
      if(!AllocatePlane(plan.geometry.fill, Pixel::L8, maxDimension, rawMask) ||
         !mImpl->RasterizeStyle(width, height, STYLE_MASK, ignoreHorizontalAlignment, penX, penY, first, last,
                                plan.runs, rawMask, nullptr, sourceClip))
      {
        return false;
      }
      pending.embossMask = PixelBuffer::Convert(rawMask.buffer);
    }
    if(fill.buffer) pending.fill = PixelBuffer::Convert(fill.buffer);
    if(mask) pending.colorGlyphMask = PixelBuffer::Convert(mask);

    RasterTarget underlay;
    if(!AllocatePlane(plan.geometry.underlay, Pixel::RGBA8888, maxDimension, underlay))
    {
      return false;
    }
    // Preserve outline, then shadow, then background, each behind the prior
    // result. An effect temporary and its blur scratch die before the next ROI.
    for(Style style : {STYLE_OUTLINE, STYLE_SHADOW})
    {
      const bool  isOutline = style == STYLE_OUTLINE;
      const auto& bounds    = isOutline ? plan.outline : plan.shadow;
      if(bounds.Empty())
      {
        continue;
      }
      RasterTarget effect;
      if(!AllocatePlane(bounds, Pixel::RGBA8888, maxDimension, effect) || !draw(effect, style, first, last))
      {
        return false;
      }
      const float radius = isOutline ? model.GetOutlineBlurRadius() : model.GetShadowBlurRadius();
      if(radius > Math::MACHINE_EPSILON_1 && !effect.buffer.ApplyGaussianBlur(radius))
      {
        return false;
      }
      if(!mImpl->CompositeBehind(underlay, effect))
      {
        return false;
      }
    }
    if(!plan.background.Empty())
    {
      RasterTarget background;
      if(!AllocatePlane(plan.background, Pixel::RGBA8888, maxDimension, background))
      {
        return false;
      }
      if(model.IsBackgroundEnabled() && !draw(background, STYLE_BACKGROUND, first, last))
      {
        return false;
      }
      if(model.IsMarkupBackgroundColorSet() && !mImpl->RasterizeBackground(width, height, ignoreHorizontalAlignment, penX, penY, background, plan.sourceClip.planes ? &plan.sourceClip : nullptr))
      {
        return false;
      }
      if(!mImpl->CompositeBehind(underlay, background))
      {
        return false;
      }
    }
    if(underlay.buffer)
    {
      pending.underlay = PixelBuffer::Convert(underlay.buffer);
    }

    RasterTarget overlay;
    if(!AllocatePlane(plan.geometry.overlay, Pixel::RGBA8888, maxDimension, overlay))
    {
      return false;
    }
    // Reuse the per-pass bounds already measured by the geometry plan. Small
    // spans need only their own ROI, even when the final overlay is very wide.
    for(const auto& pass : plan.overlayPasses)
    {
      RasterTarget effect;
      if(!AllocatePlane(pass.bounds, Pixel::RGBA8888, maxDimension, effect) ||
         !draw(effect, pass.style, pass.first, pass.last) ||
         !mImpl->CompositeBehind(overlay, effect))
      {
        return false;
      }
    }
    if(overlay.buffer)
    {
      pending.overlay = PixelBuffer::Convert(overlay.buffer);
    }
    result = std::move(pending);
    return true;
  }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  // Recover from the existing allocating APIs when exceptions are enabled.
  // Backing-buffer failures use explicit return values in either build mode.
  catch(const std::bad_alloc&)
  {
    return false;
  }
#endif
}

bool Typesetter::RenderRevealPlanes(const Vector2& size, Direction direction, Pixel::Format format,
                                    uint32_t maxDimension, TextRasterResult& result, bool colorGlyphMask,
                                    bool textGradientMixed, bool embossMask)
{
  result = {};
  uint32_t width, height;
  auto&    model = *mImpl->GetViewModel();
  if(!ResolveRasterSize(size, width, height, 4u) || width >= maxDimension || height >= maxDimension ||
     model.IsCutoutEnabled() || model.IsBackgroundWithCutoutEnabled() ||
     (format != Pixel::L8 && format != Pixel::RGBA8888) ||
     ((colorGlyphMask || textGradientMixed || embossMask) && format != Pixel::RGBA8888) ||
     (static_cast<unsigned>(colorGlyphMask) + textGradientMixed + embossMask > 1u))
  {
    return false;
  }

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
  try
#endif
  {
    TextRasterResult pending;
    pending.legacyRasterSize = size;
    pending.layoutSize       = model.GetLayoutSize();
    const Raster::Bounds bounded{0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height)};
    pending.geometry.fill = pending.geometry.composition = bounded;
    if(textGradientMixed)
    {
      pending.fill             = RenderTextGradientPreserved(size, direction, false, Pixel::RGBA8888);
      pending.textGradientMask = RenderTextGradientMask(size, direction);
      if(!pending.textGradientMask) return false;
    }
    else
    {
      pending.fill = Render(size, direction, RENDER_NO_STYLES, false, format, Size::ZERO,
                            colorGlyphMask ? &pending.colorGlyphMask : nullptr);
      if(colorGlyphMask && !pending.colorGlyphMask) return false;
    }
    if(!pending.fill) return false;
    if(embossMask)
    {
      pending.embossMask = Render(size, direction, RENDER_MASK, false, Pixel::L8);
      if(!pending.embossMask) return false;
    }
    if(model.IsShadowEnabled() || model.IsOutlineEnabled() || model.IsBackgroundEnabled() || model.IsMarkupBackgroundColorSet())
    {
      pending.underlay = Render(size, direction, RENDER_NO_TEXT, false, Pixel::RGBA8888);
      if(!pending.underlay) return false;
      pending.geometry.underlay = bounded;
    }
    if(model.IsUnderlineEnabled() || model.IsMarkupUnderlineSet() || model.IsStrikethroughEnabled() || model.IsMarkupStrikethroughSet())
    {
      pending.overlay = Render(size, direction, RENDER_OVERLAY_STYLE, false, Pixel::RGBA8888);
      if(!pending.overlay) return false;
      pending.geometry.overlay = bounded;
    }
    result = std::move(pending);
    return true;
  }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
  catch(const std::bad_alloc&)
  {
    return false;
  }
#endif
}

PixelBuffer Typesetter::RenderWithPixelBuffer(const Vector2& size,
                                              Direction,
                                              RenderBehaviour behaviour, bool ignoreHorizontalAlignment,
                                              Pixel::Format pixelFormat, const Vector2& originSize, PixelBuffer* recolorMask, const Raster::SourceClip* sourceClip, Raster::BoundsAccumulator* sourceBounds)
{
  if(recolorMask)
  {
    recolorMask->Reset();
    if(behaviour != RENDER_NO_STYLES || pixelFormat != Pixel::RGBA8888)
    {
      return {};
    }
  }
  uint32_t       bufferWidth, bufferHeight;
  const uint32_t bytesPerPixel = (behaviour == RENDER_NO_STYLES || behaviour == RENDER_MASK)
                                   ? Pixel::GetBytesPerPixel(pixelFormat)
                                   : 4u;
  if(!ResolveRasterSize(size, bufferWidth, bufferHeight, bytesPerPixel))
  {
    return {};
  }

  DALI_TRACE_SCOPE(gTraceFilter, "DALI_TEXT_RENDERING_TYPESETTER");
  // @todo. This initial implementation for a TextLabel has only one visible page.

  // Use l-value to make ensure it is not nullptr, so compiler happy.
  auto& viewModel = *(mImpl->GetViewModel());

  // Elides the text if needed.
  viewModel.ElideGlyphs(mImpl->GetFontClient());

  int32_t penX, penY;
  if(!ResolveRasterPen(viewModel, size, originSize, penX, penY))
  {
    return {};
  }

  // Generate the image buffers of the text for each different style first,
  // then combine all of them together as one final image buffer. We try to
  // do all of these in CPU only, so that once the final texture is generated,
  // no calculation is needed in GPU during each frame.

  const uint32_t bufferSizeInt  = bufferWidth * bufferHeight;
  const size_t   bufferSizeChar = sizeof(uint32_t) * static_cast<std::size_t>(bufferSizeInt);

  // PixelBuffer keeps the requested empty geometry, but has no backing bytes.
  // Avoid passing its null buffer to memset/memcpy in the style passes.
  if(bufferSizeInt == 0u)
  {
    return PixelBuffer::New(bufferWidth, bufferHeight,
                            (behaviour == RENDER_NO_TEXT || behaviour == RENDER_OVERLAY_STYLE) ? Pixel::RGBA8888 : pixelFormat);
  }

  // Elided text in ellipsis at START could start on index greater than 0
  auto startIndexOfGlyphs = viewModel.GetStartIndexOfElidedGlyphs();
  auto endIndexOfGlyphs   = viewModel.GetEndIndexOfElidedGlyphs();

  PixelBuffer imageBuffer;

  if(RENDER_MASK == behaviour)
  {
    // Generate the image buffer as an alpha mask for color glyphs.
    imageBuffer = mImpl->CreateImageBuffer(bufferWidth, bufferHeight, Typesetter::STYLE_MASK, ignoreHorizontalAlignment,
                                           pixelFormat, penX, penY, startIndexOfGlyphs, endIndexOfGlyphs, 0u, 0u, nullptr, sourceClip);
  }
  else if(RENDER_NO_TEXT == behaviour || RENDER_OVERLAY_STYLE == behaviour)
  {
    // Generate an empty image buffer so that it can been combined with the image buffers for styles
    imageBuffer = PixelBuffer::New(bufferWidth, bufferHeight, Pixel::RGBA8888);
    memset(imageBuffer.GetBuffer(), 0u, bufferSizeChar);
  }
  else
  {
    // Generate the image buffer for the text with no style.
    imageBuffer = mImpl->CreateImageBuffer(bufferWidth, bufferHeight, Typesetter::STYLE_NONE, ignoreHorizontalAlignment,
                                           pixelFormat, penX, penY, startIndexOfGlyphs, endIndexOfGlyphs, 0u, 0u, recolorMask, sourceClip, sourceBounds);
  }

  if((RENDER_NO_STYLES != behaviour) && (RENDER_MASK != behaviour))
  {
    // Generate the outline if enabled
    const float outlineAlpha = viewModel.GetOutlineColor().a;
    if(viewModel.IsOutlineEnabled() && fabsf(outlineAlpha) > Math::MACHINE_EPSILON_1 && RENDER_OVERLAY_STYLE != behaviour)
    {
      // Create the image buffer for outline
      PixelBuffer outlineImageBuffer =
        mImpl->CreateImageBuffer(bufferWidth, bufferHeight, Typesetter::STYLE_OUTLINE, ignoreHorizontalAlignment,
                                 pixelFormat, penX, penY, startIndexOfGlyphs, endIndexOfGlyphs, 0u, 0u, nullptr, sourceClip);

      const float& blurRadius = viewModel.GetOutlineBlurRadius();

      if(blurRadius > Math::MACHINE_EPSILON_1)
      {
        outlineImageBuffer.ApplyGaussianBlur(blurRadius);
      }

      // Combine the two buffers
      CombineImageBuffer(imageBuffer, outlineImageBuffer, bufferWidth, bufferHeight, true);
    }

    // @todo. Support shadow for partial text later on.

    // Generate the shadow if enabled
    const float shadowAlpha = viewModel.GetShadowColor().a;
    if(RENDER_OVERLAY_STYLE != behaviour && viewModel.IsShadowEnabled() &&
       fabsf(shadowAlpha) > Math::MACHINE_EPSILON_1)
    {
      // Create the image buffer for shadow
      PixelBuffer shadowImageBuffer =
        mImpl->CreateImageBuffer(bufferWidth, bufferHeight, Typesetter::STYLE_SHADOW, ignoreHorizontalAlignment,
                                 pixelFormat, penX, penY, startIndexOfGlyphs, endIndexOfGlyphs, 0u, 0u, nullptr, sourceClip);

      // Check whether it will be a soft shadow
      const float& blurRadius = viewModel.GetShadowBlurRadius();

      if(blurRadius > Math::MACHINE_EPSILON_1)
      {
        shadowImageBuffer.ApplyGaussianBlur(blurRadius);
      }

      // Combine the two buffers
      CombineImageBuffer(imageBuffer, shadowImageBuffer, bufferWidth, bufferHeight, true);
    }

    // Generate the background if enabled
    const bool backgroundEnabled   = viewModel.IsBackgroundEnabled();
    const bool backgroundMarkupSet = viewModel.IsMarkupBackgroundColorSet();
    if((backgroundEnabled || backgroundMarkupSet) && RENDER_OVERLAY_STYLE != behaviour)
    {
      PixelBuffer backgroundImageBuffer;

      if(backgroundEnabled)
      {
        backgroundImageBuffer =
          mImpl->CreateImageBuffer(bufferWidth, bufferHeight, Typesetter::STYLE_BACKGROUND, ignoreHorizontalAlignment,
                                   pixelFormat, penX, penY, startIndexOfGlyphs, endIndexOfGlyphs, 0u, 0u, nullptr, sourceClip);
      }
      else
      {
        backgroundImageBuffer = Impl::CreateTransparentImageBuffer(bufferWidth, bufferHeight, pixelFormat);
      }

      if(backgroundMarkupSet)
      {
        mImpl->DrawGlyphsBackground(backgroundImageBuffer, bufferWidth, bufferHeight, ignoreHorizontalAlignment, penX,
                                    penY, sourceClip);
      }

      // Combine the two buffers
      CombineImageBuffer(imageBuffer, backgroundImageBuffer, bufferWidth, bufferHeight, true);
    }

    // Generate the background_with_mask if enabled
    const bool backgroundWithCutoutEnabled = viewModel.IsBackgroundWithCutoutEnabled();
    if((backgroundWithCutoutEnabled) && RENDER_OVERLAY_STYLE != behaviour)
    {
      PixelBuffer backgroundImageBuffer;

      backgroundImageBuffer =
        CreateFullBackgroundBuffer(bufferWidth, bufferHeight, viewModel.GetBackgroundColorWithCutout());

      // Combine the two buffers
      CombineImageBuffer(imageBuffer, backgroundImageBuffer, bufferWidth, bufferHeight, true);
    }

    if(RENDER_OVERLAY_STYLE == behaviour)
    {
      if(viewModel.IsUnderlineEnabled())
      {
        // Create the image buffer for underline
        PixelBuffer underlineImageBuffer =
          mImpl->CreateImageBuffer(bufferWidth, bufferHeight, Typesetter::STYLE_UNDERLINE, ignoreHorizontalAlignment,
                                   pixelFormat, penX, penY, startIndexOfGlyphs, endIndexOfGlyphs, 0u, 0u, nullptr, sourceClip);

        // Combine the two buffers
        CombineImageBuffer(imageBuffer, underlineImageBuffer, bufferWidth, bufferHeight, true);
      }

      if(viewModel.IsStrikethroughEnabled())
      {
        // Create the image buffer for strikethrough
        PixelBuffer strikethroughImageBuffer =
          mImpl->CreateImageBuffer(bufferWidth, bufferHeight, Typesetter::STYLE_STRIKETHROUGH,
                                   ignoreHorizontalAlignment, pixelFormat, penX, penY, 0u, endIndexOfGlyphs, 0u, 0u, nullptr, sourceClip);

        // Combine the two buffers
        CombineImageBuffer(imageBuffer, strikethroughImageBuffer, bufferWidth, bufferHeight, true);
      }

      // Range underline/strikethrough runs from markup or StyledText spans.
      // The helper names keep the legacy markup terminology.
      if(viewModel.IsMarkupUnderlineSet())
      {
        imageBuffer = ApplyUnderlineMarkupImageBuffer(imageBuffer, bufferWidth, bufferHeight,
                                                      ignoreHorizontalAlignment, pixelFormat, penX, penY, sourceClip);
      }

      if(viewModel.IsMarkupStrikethroughSet())
      {
        imageBuffer = ApplyStrikethroughMarkupImageBuffer(imageBuffer, bufferWidth, bufferHeight,
                                                          ignoreHorizontalAlignment, pixelFormat, penX, penY, sourceClip);
      }
    }
  }

  return imageBuffer;
}

Internal::Reveal::Plan Typesetter::CreateFinalRevealPlan(const Internal::Reveal::Plan& sourcePlan,
                                                         Internal::Reveal::Unit        unit,
                                                         Internal::Reveal::Sequence    sequence,
                                                         float                         sequenceStaggerRatio)
{
  auto& viewModel = *mImpl->GetViewModel();
  viewModel.EnableFinalGlyphMapping();
  viewModel.ElideGlyphs(mImpl->GetFontClient());
  Internal::Reveal::Plan finalPlan = Internal::Reveal::ProjectToFinalGlyphs(sourcePlan,
                                                                            viewModel.GetNumberOfGlyphs(),
                                                                            viewModel.GetFinalToSourceGlyphIndices(),
                                                                            viewModel.GetEllipsisFinalGlyphIndex(),
                                                                            unit);
  if(unit == Internal::Reveal::Unit::LINE)
  {
    if(Internal::Reveal::ApplyLineUnitSchedule(finalPlan,
                                               viewModel.GetLines(),
                                               viewModel.GetNumberOfLines()) &&
       sequence == Internal::Reveal::Sequence::PER_LINE)
    {
      Internal::Reveal::ApplyPerLineSequenceSchedule(finalPlan,
                                                     viewModel.GetLines(),
                                                     viewModel.GetNumberOfLines(),
                                                     sequenceStaggerRatio);
    }
  }
  else if(unit == Internal::Reveal::Unit::PIXEL)
  {
    TextAbstraction::FontClient fontClient;
    if(finalPlan.fadeDurationRatio == Text::Reveal::AUTO_FADE_DURATION_RATIO)
    {
      fontClient = mImpl->GetFontClient();
    }
    if(!Internal::Reveal::ApplyPixelSpatialSchedule(finalPlan,
                                                    viewModel,
                                                    fontClient,
                                                    viewModel.GetFinalToSourceGlyphIndices(),
                                                    viewModel.GetEllipsisFinalGlyphIndex(),
                                                    sequence,
                                                    sequenceStaggerRatio))
    {
      // The scheduler commits only complete PIXEL timing, so a failure leaves
      // this projected CHARACTER-compatible plan intact.
    }
  }
  else if(sequence == Internal::Reveal::Sequence::PER_LINE)
  {
    Internal::Reveal::ApplyPerLineSequenceSchedule(finalPlan,
                                                   viewModel.GetLines(),
                                                   viewModel.GetNumberOfLines(),
                                                   sequenceStaggerRatio);
  }
  return finalPlan;
}

bool Typesetter::ExtractReplacementRevealTimings(const Internal::Reveal::Plan&       finalPlan,
                                                 const ReplacementSourceSnapshot&    source,
                                                 const Vector<ReplacementPlacement>& placements,
                                                 Vector<ReplacementRevealTiming>&    timings)
{
  timings.Clear();
  auto&             viewModel          = *mImpl->GetViewModel();
  const Length      finalGlyphCount    = viewModel.GetNumberOfGlyphs();
  const GlyphInfo*  finalGlyphs        = viewModel.GetGlyphs();
  const GlyphIndex* finalToSourceGlyph = viewModel.GetFinalToSourceGlyphIndices();
  if(finalPlan.glyphToUnit.size() != finalGlyphCount || !finalGlyphs)
  {
    return false;
  }

  std::unordered_map<GlyphIndex, GlyphIndex> finalGlyphBySourceSyntheticGlyph;
  finalGlyphBySourceSyntheticGlyph.reserve(placements.Count());
  for(GlyphIndex finalGlyph = 0u; finalGlyph < finalGlyphCount; ++finalGlyph)
  {
    if(IsSyntheticReplacementGlyph(finalGlyphs[finalGlyph]))
    {
      const GlyphIndex sourceGlyph = finalToSourceGlyph ? finalToSourceGlyph[finalGlyph] : finalGlyph;
      finalGlyphBySourceSyntheticGlyph.emplace(sourceGlyph, finalGlyph);
    }
  }

  uint32_t expectedTimingCount = 0u;
  for(const ReplacementPlacement& placement : placements)
  {
    if(!placement.visible || placement.elided || placement.sourceRunIndex >= source.runs.Count())
    {
      continue;
    }
    const ReplacementRunSnapshot& run = source.runs[placement.sourceRunIndex];
    if(run.type != ReplacementType::IMAGE || run.image.source.empty() ||
       run.occurrenceIdentity != placement.occurrenceIdentity)
    {
      continue;
    }
    ++expectedTimingCount;

    const auto matchingFinalGlyph = finalGlyphBySourceSyntheticGlyph.find(placement.syntheticGlyphIndex);
    if(matchingFinalGlyph == finalGlyphBySourceSyntheticGlyph.end())
    {
      timings.Clear();
      return false;
    }

    const uint32_t unit = finalPlan.glyphToUnit[matchingFinalGlyph->second];
    if(unit == Internal::Reveal::NO_UNIT || unit >= finalPlan.unitStart.size() ||
       unit >= finalPlan.imageReplacementUnitMask.size() || finalPlan.imageReplacementUnitMask[unit] == 0u)
    {
      timings.Clear();
      return false;
    }

    float start           = finalPlan.unitStart[unit];
    float progressionSpan = 0.0f;
    bool  rightToLeft     = false;
    if(unit < finalPlan.pixelUnitTiming.size())
    {
      progressionSpan = finalPlan.pixelUnitTiming[unit].progressionSpan;
      rightToLeft     = finalPlan.pixelUnitTiming[unit].rightToLeft;
      start += 0.5f * progressionSpan;
    }
    timings.PushBack({placement.occurrenceIdentity,
                      start,
                      finalPlan.fadeDuration,
                      progressionSpan,
                      rightToLeft});
  }
  return timings.Count() == expectedTimingCount;
}

PixelData Typesetter::RenderTextRevealMetadata(
  const Vector2& tileSize,
  Direction,
  const Internal::Reveal::Plan& plan,
  float&                        fadeDuration,
  uint32_t                      tileOffsetY,
  const Vector2&                fullSize,
  bool                          ignoreHorizontalAlignment,
  const Vector2&                originSize)
{
  uint32_t width, height;
  if(!ResolveRasterSize(tileSize, width, height))
  {
    return {};
  }

  // Reuse the normal glyph traversal, but offset it into a tile-local target.
  // The caller projects one canonical final plan before visiting any tile.
  auto& viewModel = *mImpl->GetViewModel();

  const Size& layoutSize = viewModel.GetLayoutSize();
  int32_t     penX       = 0;

  const Vector2 renderSize    = fullSize.height > 0.0f ? fullSize : tileSize;
  const bool    useOriginSize = originSize.height > 0.0f;
  const float   controlHeight = useOriginSize ? viewModel.GetControlSize().height : renderSize.height;
  const float   layoutHeight  = useOriginSize ? originSize.height : layoutSize.height;
  int32_t       penY          = 0;
  switch(viewModel.GetVerticalAlignment())
  {
    case Alignment::START:
      break;
    case Alignment::CENTER:
      if(!Raster::Convert(std::round(0.5f * (controlHeight - layoutHeight)), penY))
      {
        return {};
      }
      break;
    case Alignment::END:
      if(!Raster::Convert(controlHeight - layoutHeight, penY))
      {
        return {};
      }
      break;
  }
  if(!Raster::Add(penY, -static_cast<int64_t>(tileOffsetY), penY))
  {
    return {};
  }

  const auto startGlyph = viewModel.GetStartIndexOfElidedGlyphs();
  const auto endGlyph   = viewModel.GetEndIndexOfElidedGlyphs();
  fadeDuration          = plan.fadeDuration;

  mImpl->BeginRevealMetadata(width, height, plan);
  mImpl->CreateImageBuffer(width, height, Typesetter::STYLE_NONE,
                           ignoreHorizontalAlignment, Pixel::RGBA8888,
                           penX, penY, startGlyph, endGlyph);
  return mImpl->EndRevealMetadata();
}

Rect<uint32_t> Typesetter::GetRuntimeBlurLineRasterBounds(const Vector2& size, LineIndex lineIndex, const int32_t* lineBaseline)
{
  const auto&          model  = *mImpl->GetViewModel();
  const auto           width  = static_cast<uint32_t>(size.width);
  const auto           height = static_cast<uint32_t>(size.height);
  const Rect<uint32_t> full(0u, 0u, width, height);
  if(lineIndex >= model.GetNumberOfLines() || model.GetGradientSpanModelData() ||
     model.GetHyphensCount() != 0u || model.GetEllipsisPosition() != Text::EllipsisPosition::END)
  {
    return full;
  }
  const auto& line = model.GetLines()[lineIndex];
  if(line.isSplitToTwoHalves || line.glyphRun.numberOfGlyphs == 0u)
  {
    return full;
  }

  int32_t baseline = lineBaseline ? *lineBaseline : 0;
  if(!lineBaseline)
  {
    if(model.GetVerticalAlignment() == Alignment::CENTER)
    {
      baseline = static_cast<int32_t>(std::round(0.5f * (size.height - model.GetLayoutSize().height)));
    }
    else if(model.GetVerticalAlignment() == Alignment::END)
    {
      baseline = static_cast<int32_t>(size.height - model.GetLayoutSize().height);
    }
    // Keep the separate per-line truncations used by CreateImageBufferForEachLine.
    // Summing fractional metrics first would move later lines by whole pixels.
    for(LineIndex index = 0u; index <= lineIndex; ++index)
    {
      const auto& current = model.GetLines()[index];
      baseline += static_cast<int32_t>(current.ascender + GetPreOffsetVerticalLineAlignment(current, model.GetVerticalLineAlignment()));
      if(index != lineIndex)
      {
        baseline += static_cast<int32_t>(-current.descender + GetPostOffsetVerticalLineAlignment(current, model.GetVerticalLineAlignment()));
      }
    }
  }

  const auto start  = model.GetStartIndexOfElidedGlyphs();
  const auto first  = std::max(line.glyphRun.glyphIndex, start);
  const auto last   = std::min(line.glyphRun.glyphIndex + line.glyphRun.numberOfGlyphs - 1u, model.GetEndIndexOfElidedGlyphs());
  int64_t    top    = height;
  int64_t    bottom = 0;
  for(GlyphIndex index = first; index <= last; ++index)
  {
    const auto& glyph = model.GetGlyphs()[index - start];
    if(IsSyntheticReplacementGlyph(glyph) || glyph.width < Math::MACHINE_EPSILON_1000 || glyph.height < Math::MACHINE_EPSILON_1000)
    {
      continue;
    }
    TextAbstraction::GlyphBufferData bitmap;
    bitmap.width  = static_cast<uint32_t>(glyph.width);
    bitmap.height = static_cast<uint32_t>(glyph.height);
    mImpl->GetFontClient().CreateBitmap(glyph.fontId, glyph.index, glyph.isItalicRequired, glyph.isBoldRequired, bitmap, 0);
    if(bitmap.buffer && bitmap.width > 0u && bitmap.height > 0u)
    {
      const int64_t y = static_cast<int64_t>(baseline) + static_cast<int32_t>(model.GetLayout()[index - start].y);
      top             = std::min(top, y);
      bottom          = std::max(bottom, y + bitmap.height);
    }
    // GlyphBufferData releases owned buffers; cached font bitmaps stay borrowed.
  }
  // Match the final source crop's filtering guard; Gaussian support is added
  // later to the GPU target and does not require a full-height CPU plane.
  top    = std::max<int64_t>(0, top - 2);
  bottom = std::min<int64_t>(height, bottom + 2);
  return bottom > top ? Rect<uint32_t>(0u, static_cast<uint32_t>(top), width, static_cast<uint32_t>(bottom - top)) : full;
}

PixelData Typesetter::RenderRuntimeBlurLine(const Vector2& size, LineIndex lineIndex,
                                            RuntimeBlurPlane plane, Pixel::Format format,
                                            const Internal::Reveal::Plan& plan,
                                            uint32_t rasterOffsetY, uint32_t rasterHeight,
                                            uint32_t rasterOffsetX, uint32_t rasterWidth, PixelData* recolorMask)
{
  if(recolorMask)
  {
    *recolorMask = {};
    if(plane != RuntimeBlurPlane::TEXT || format != Pixel::RGBA8888)
    {
      return {};
    }
  }
  const auto& model = *mImpl->GetViewModel();
  if(lineIndex >= model.GetNumberOfLines())
  {
    return {};
  }
  const auto& line    = model.GetLines()[lineIndex];
  const auto& lastRun = line.isSplitToTwoHalves ? line.glyphRunSecondHalf : line.glyphRun;
  if(lastRun.numberOfGlyphs == 0u)
  {
    return {};
  }
  const GlyphIndex first = line.glyphRun.glyphIndex;
  const GlyphIndex last  = lastRun.glyphIndex + lastRun.numberOfGlyphs - 1u;
  int32_t          penY  = 0;
  if(model.GetVerticalAlignment() == Alignment::CENTER)
  {
    penY = static_cast<int32_t>(std::round(0.5f * (size.height - model.GetLayoutSize().height)));
  }
  else if(model.GetVerticalAlignment() == Alignment::END)
  {
    penY = static_cast<int32_t>(size.height - model.GetLayoutSize().height);
  }
  const auto width  = static_cast<uint32_t>(size.width);
  const auto height = static_cast<uint32_t>(size.height);
  if(rasterHeight == 0u)
  {
    rasterHeight = height;
  }
  if(rasterWidth == 0u)
  {
    rasterWidth = width;
  }
  if(rasterOffsetY > height || rasterHeight > height - rasterOffsetY ||
     rasterOffsetX > width || rasterWidth > width - rasterOffsetX ||
     (plane != RuntimeBlurPlane::REVEAL_METADATA && (rasterOffsetX != 0u || rasterWidth != width)) ||
     (plane != RuntimeBlurPlane::REVEAL_METADATA && model.GetGradientSpanModelData() &&
      (rasterOffsetY != 0u || rasterHeight != height)))
  {
    return {};
  }
  penY -= static_cast<int32_t>(rasterOffsetY);
  PixelBuffer buffer;
  PixelBuffer mask;
  switch(plane)
  {
    case RuntimeBlurPlane::GRADIENT_PRESERVED:
      buffer = mImpl->CreateTextGradientPreservedImageBuffer(width, rasterHeight, false, format, 0, penY, first, last);
      break;
    case RuntimeBlurPlane::GRADIENT_MASK:
      buffer = mImpl->CreateTextGradientMaskImageBuffer(width, rasterHeight, false, format, 0, penY, first, last);
      break;
    case RuntimeBlurPlane::REVEAL_METADATA:
      mImpl->BeginRevealMetadata(rasterWidth, rasterHeight, plan);
      mImpl->CreateImageBuffer(rasterWidth, rasterHeight, STYLE_NONE, false, Pixel::RGBA8888,
                               -static_cast<int32_t>(rasterOffsetX), penY, first, last,
                               rasterOffsetY, height);
      return mImpl->EndRevealMetadata();
    default:
      buffer = mImpl->CreateImageBuffer(width, rasterHeight,
                                        plane == RuntimeBlurPlane::COLOR_MASK ? STYLE_MASK : STYLE_NONE,
                                        false, format, 0, penY, first, last, 0u, 0u, recolorMask ? &mask : nullptr);
      break;
  }
  if(recolorMask && buffer && mask)
  {
    *recolorMask = PixelBuffer::Convert(mask);
  }
  return buffer ? PixelBuffer::Convert(buffer) : PixelData{};
}

PixelBuffer Typesetter::CreateFullBackgroundBuffer(const uint32_t bufferWidth, const uint32_t bufferHeight,
                                                   const Vector4& backgroundColor)
{
  if(!Raster::BufferFits(bufferWidth, bufferHeight, 4u))
  {
    return {};
  }

  const uint32_t bufferSizeInt        = bufferWidth * bufferHeight;
  uint8_t        backgroundColorAlpha = static_cast<uint8_t>(backgroundColor.a * 255.f);

  PixelBuffer buffer = PixelBuffer::New(bufferWidth, bufferHeight, Pixel::RGBA8888);
  if(bufferSizeInt == 0u)
  {
    return buffer;
  }

  uint32_t* bitmapBuffer = reinterpret_cast<uint32_t*>(buffer.GetBuffer());

  uint32_t packedBackgroundColor       = 0u;
  uint8_t* packedBackgroundColorBuffer = reinterpret_cast<uint8_t*>(&packedBackgroundColor);

  // Write the color to the pixel buffer
  *(packedBackgroundColorBuffer + 3u) = backgroundColorAlpha;
  *(packedBackgroundColorBuffer + 2u) = static_cast<uint8_t>(backgroundColor.b * backgroundColorAlpha);
  *(packedBackgroundColorBuffer + 1u) = static_cast<uint8_t>(backgroundColor.g * backgroundColorAlpha);
  *(packedBackgroundColorBuffer)      = static_cast<uint8_t>(backgroundColor.r * backgroundColorAlpha);

  std::fill(bitmapBuffer, bitmapBuffer + bufferSizeInt, packedBackgroundColor);

  return buffer;
}

PixelBuffer Typesetter::ApplyUnderlineMarkupImageBuffer(PixelBuffer    topPixelBuffer,
                                                        const uint32_t bufferWidth, const uint32_t bufferHeight,
                                                        const bool          ignoreHorizontalAlignment,
                                                        const Pixel::Format pixelFormat,
                                                        const int32_t       horizontalOffset,
                                                        const int32_t verticalOffset, const Raster::SourceClip* sourceClip)
{
  // Use l-value to make ensure it is not nullptr, so compiler happy.
  auto& viewModel = *(mImpl->GetViewModel());

  // Underline-tags (this is for Markup case)
  // Get the underline runs.
  const Length               numberOfUnderlineRuns = viewModel.GetNumberOfUnderlineRuns();
  Vector<UnderlinedGlyphRun> underlineRuns;
  underlineRuns.Resize(numberOfUnderlineRuns);
  viewModel.GetUnderlineRuns(underlineRuns.Begin(), 0u, numberOfUnderlineRuns);

  // Iterate on the consecutive underlined glyph run and connect them into one chunk of underlined characters.
  Vector<UnderlinedGlyphRun>::ConstIterator itGlyphRun    = underlineRuns.Begin();
  Vector<UnderlinedGlyphRun>::ConstIterator endItGlyphRun = underlineRuns.End();
  GlyphIndex                                startGlyphIndex, endGlyphIndex;

  // The outer loop to iterate on the separated chunks of underlined glyph runs
  while(itGlyphRun != endItGlyphRun)
  {
    startGlyphIndex = itGlyphRun->glyphRun.glyphIndex;
    endGlyphIndex   = startGlyphIndex + itGlyphRun->glyphRun.numberOfGlyphs - 1;

    // Create the image buffer for underline
    PixelBuffer underlineImageBuffer =
      mImpl->CreateImageBuffer(bufferWidth, bufferHeight, Typesetter::STYLE_UNDERLINE, ignoreHorizontalAlignment,
                               pixelFormat, horizontalOffset, verticalOffset, startGlyphIndex, endGlyphIndex, 0u, 0u, nullptr, sourceClip);
    // Combine the two buffers
    // Result pixel buffer will be stored into topPixelBuffer.
    CombineImageBuffer(underlineImageBuffer, topPixelBuffer, bufferWidth, bufferHeight, false);

    itGlyphRun++;
  }

  return topPixelBuffer;
}

PixelBuffer Typesetter::ApplyStrikethroughMarkupImageBuffer(
  PixelBuffer topPixelBuffer, const uint32_t bufferWidth, const uint32_t bufferHeight,
  const bool ignoreHorizontalAlignment, const Pixel::Format pixelFormat, const int32_t horizontalOffset,
  const int32_t verticalOffset, const Raster::SourceClip* sourceClip)
{
  // Use l-value to make ensure it is not nullptr, so compiler happy.
  auto& viewModel = *(mImpl->GetViewModel());

  // strikethrough-tags (this is for Markup case)
  // Get the strikethrough runs.
  const Length                  numberOfStrikethroughRuns = viewModel.GetNumberOfStrikethroughRuns();
  Vector<StrikethroughGlyphRun> strikethroughRuns;
  strikethroughRuns.Resize(numberOfStrikethroughRuns);
  viewModel.GetStrikethroughRuns(strikethroughRuns.Begin(), 0u, numberOfStrikethroughRuns);

  // Iterate on the consecutive strikethrough glyph run and connect them into one chunk of strikethrough characters.
  Vector<StrikethroughGlyphRun>::ConstIterator itGlyphRun    = strikethroughRuns.Begin();
  Vector<StrikethroughGlyphRun>::ConstIterator endItGlyphRun = strikethroughRuns.End();
  GlyphIndex                                   startGlyphIndex, endGlyphIndex;

  // The outer loop to iterate on the separated chunks of strikethrough glyph runs
  while(itGlyphRun != endItGlyphRun)
  {
    startGlyphIndex = itGlyphRun->glyphRun.glyphIndex;
    endGlyphIndex   = startGlyphIndex + itGlyphRun->glyphRun.numberOfGlyphs - 1;

    // Create the image buffer for strikethrough
    PixelBuffer strikethroughImageBuffer =
      mImpl->CreateImageBuffer(bufferWidth, bufferHeight, Typesetter::STYLE_STRIKETHROUGH, ignoreHorizontalAlignment,
                               pixelFormat, horizontalOffset, verticalOffset, startGlyphIndex, endGlyphIndex, 0u, 0u, nullptr, sourceClip);
    // Combine the two buffers
    // Result pixel buffer will be stored into topPixelBuffer.
    CombineImageBuffer(strikethroughImageBuffer, topPixelBuffer, bufferWidth, bufferHeight, false);

    itGlyphRun++;
  }

  return topPixelBuffer;
}

void Typesetter::SetMaskForImageBuffer(PixelBuffer& __restrict__ topPixelBuffer,
                                       PixelBuffer& __restrict__ bottomPixelBuffer, const uint32_t bufferWidth,
                                       const uint32_t bufferHeight, float originAlpha)
{
  // Assume that we always combine two RGBA images
  // Jump with 4bytes for optimize runtime.
  uint32_t* topBuffer    = reinterpret_cast<uint32_t*>(topPixelBuffer.GetBuffer());
  uint32_t* bottomBuffer = reinterpret_cast<uint32_t*>(bottomPixelBuffer.GetBuffer());

  if(topBuffer == nullptr || bottomBuffer == nullptr)
  {
    // Nothing to do if one of both buffers are empty.
    return;
  }

  const uint32_t bufferSizeInt = bufferWidth * bufferHeight;

  for(uint32_t pixelIndex = 0; pixelIndex < bufferSizeInt; ++pixelIndex)
  {
    uint32_t topBufferColor                       = *(topBuffer);
    uint32_t bottomBufferColor                    = *(bottomBuffer);
    uint8_t* __restrict__ topBufferColorBuffer    = reinterpret_cast<uint8_t*>(&topBufferColor);
    uint8_t* __restrict__ bottomBufferColorBuffer = reinterpret_cast<uint8_t*>(&bottomBufferColor);

    // Return the transparency of the text to original.
    uint8_t originAlphaInt = static_cast<uint8_t>(originAlpha * 255);

    uint8_t topAlpha    = topBufferColorBuffer[3];
    uint8_t bottomAlpha = 255 - topAlpha;

    // Manual blending.
    bottomBufferColorBuffer[0] = MultiplyAndSummationAndNormalizeColor(topBufferColorBuffer[0], originAlphaInt,
                                                                       bottomBufferColorBuffer[0], bottomAlpha);
    bottomBufferColorBuffer[1] = MultiplyAndSummationAndNormalizeColor(topBufferColorBuffer[1], originAlphaInt,
                                                                       bottomBufferColorBuffer[1], bottomAlpha);
    bottomBufferColorBuffer[2] = MultiplyAndSummationAndNormalizeColor(topBufferColorBuffer[2], originAlphaInt,
                                                                       bottomBufferColorBuffer[2], bottomAlpha);
    bottomBufferColorBuffer[3] = MultiplyAndSummationAndNormalizeColor(topBufferColorBuffer[3], originAlphaInt,
                                                                       bottomBufferColorBuffer[3], bottomAlpha);

    *(bottomBuffer) = bottomBufferColor;

    // Increase each buffer's pointer.
    ++topBuffer;
    ++bottomBuffer;
  }
}

Typesetter::Typesetter(const ModelInterface* const model)
: mImpl{std::make_unique<Impl>(model)}
{
}

Typesetter::~Typesetter() = default;

} // namespace Text

} // namespace Ui

} //namespace DALI_NAMESPACE
