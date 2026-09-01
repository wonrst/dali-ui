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

#include <dali/integration-api/debug.h>
#include <dali/integration-api/trace.h>

#include <dali-ui-foundation/internal/text/glyph-metrics-helper.h>
#include <dali-ui-foundation/internal/text/rendering/text-reveal-fade-blur-processor.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

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
namespace
{
DALI_INIT_TRACE_FILTER(gTraceFilter, DALI_TRACE_TEXT_PERFORMANCE_MARKER, false);

constexpr uint32_t METADATA_PIXEL_SIZE       = 4u;
constexpr uint8_t  COVERAGE_TIMING_THRESHOLD = 2u;
constexpr float    TARGET_RADIUS_RATIO       = 0.125f;
constexpr float    AUTOMATIC_MIN_RADIUS      = 4.0f;
constexpr float    MIN_SUPPORTED_RADIUS      = 1.0f;
constexpr float    MAX_TARGET_RADIUS         = 8.0f;
constexpr uint32_t MAX_AUTO_PREBLUR_PIXELS   = 512u * 512u;

float ClampTargetRadius(float radius)
{
  return std::max(MIN_SUPPORTED_RADIUS, std::min(MAX_TARGET_RADIUS, radius));
}

uint32_t ResolveSupportRadius(float targetRadius, float scale)
{
  return std::max(1u, static_cast<uint32_t>(std::round(targetRadius * scale)));
}

uint32_t ClampCoordinate(int32_t coordinate, uint32_t length)
{
  return static_cast<uint32_t>(std::max(0, std::min(coordinate, static_cast<int32_t>(length) - 1)));
}

PixelBuffer BoxDownsample(const PixelBuffer& source, uint32_t width, uint32_t height)
{
  const uint32_t sourceWidth  = source.GetWidth();
  const uint32_t sourceHeight = source.GetHeight();
  const uint32_t channels     = Pixel::GetBytesPerPixel(source.GetPixelFormat());
  PixelBuffer    result       = PixelBuffer::New(width, height, source.GetPixelFormat());
  if(!result || channels == 0u)
  {
    return PixelBuffer();
  }

  const uint8_t* sourcePixels = source.GetBuffer();
  uint8_t*       outputPixels = result.GetBuffer();
  const uint32_t sourceStride = source.GetStrideBytes();
  const uint32_t outputStride = result.GetStrideBytes();

  for(uint32_t outputY = 0u; outputY < height; ++outputY)
  {
    const uint32_t sourceYBegin = static_cast<uint32_t>((static_cast<uint64_t>(outputY) * sourceHeight) / height);
    const uint32_t sourceYEnd   = std::max(sourceYBegin + 1u,
                                           static_cast<uint32_t>((static_cast<uint64_t>(outputY + 1u) * sourceHeight) / height));
    uint8_t*       outputRow    = outputPixels + static_cast<size_t>(outputY) * outputStride;
    for(uint32_t outputX = 0u; outputX < width; ++outputX)
    {
      const uint32_t sourceXBegin = static_cast<uint32_t>((static_cast<uint64_t>(outputX) * sourceWidth) / width);
      const uint32_t sourceXEnd   = std::max(sourceXBegin + 1u,
                                             static_cast<uint32_t>((static_cast<uint64_t>(outputX + 1u) * sourceWidth) / width));
      const uint32_t sampleCount  = (sourceXEnd - sourceXBegin) * (sourceYEnd - sourceYBegin);
      uint32_t       sums[4u]     = {0u, 0u, 0u, 0u};

      for(uint32_t sourceY = sourceYBegin; sourceY < sourceYEnd; ++sourceY)
      {
        const uint8_t* sourcePixel = sourcePixels + static_cast<size_t>(sourceY) * sourceStride +
                                     static_cast<size_t>(sourceXBegin) * channels;
        for(uint32_t sourceX = sourceXBegin; sourceX < sourceXEnd; ++sourceX)
        {
          for(uint32_t channel = 0u; channel < channels; ++channel)
          {
            sums[channel] += sourcePixel[channel];
          }
          sourcePixel += channels;
        }
      }

      uint8_t* outputPixel = outputRow + static_cast<size_t>(outputX) * channels;
      for(uint32_t channel = 0u; channel < channels; ++channel)
      {
        outputPixel[channel] = static_cast<uint8_t>((sums[channel] + sampleCount / 2u) / sampleCount);
      }
    }
  }
  return result;
}

void ApplyBoxBlurPass(PixelBuffer& buffer, PixelBuffer& temporary, uint32_t radius, bool discardNegligibleCoverage)
{
  if(radius == 0u)
  {
    return;
  }

  const uint32_t width       = buffer.GetWidth();
  const uint32_t height      = buffer.GetHeight();
  const uint32_t channels    = Pixel::GetBytesPerPixel(buffer.GetPixelFormat());
  const uint32_t windowSize  = radius * 2u + 1u;
  const uint32_t inputStride = buffer.GetStrideBytes();
  const uint32_t tempStride  = temporary.GetStrideBytes();
  const uint8_t* input       = buffer.GetBuffer();
  uint8_t*       temp        = temporary.GetBuffer();

  for(uint32_t y = 0u; y < height; ++y)
  {
    const uint8_t* sourceRow = input + static_cast<size_t>(y) * inputStride;
    uint8_t*       outputRow = temp + static_cast<size_t>(y) * tempStride;
    uint32_t       sums[4u]  = {0u, 0u, 0u, 0u};
    for(int32_t offset = -static_cast<int32_t>(radius); offset <= static_cast<int32_t>(radius); ++offset)
    {
      const uint8_t* pixel = sourceRow + static_cast<size_t>(ClampCoordinate(offset, width)) * channels;
      for(uint32_t channel = 0u; channel < channels; ++channel)
      {
        sums[channel] += pixel[channel];
      }
    }

    for(uint32_t x = 0u; x < width; ++x)
    {
      uint8_t* outputPixel = outputRow + static_cast<size_t>(x) * channels;
      for(uint32_t channel = 0u; channel < channels; ++channel)
      {
        outputPixel[channel] = static_cast<uint8_t>((sums[channel] + windowSize / 2u) / windowSize);
      }
      const uint8_t coverage = channels == 1u ? outputPixel[0u] : outputPixel[3u];
      if(discardNegligibleCoverage && coverage < COVERAGE_TIMING_THRESHOLD)
      {
        memset(outputPixel, 0u, channels);
      }

      const uint32_t removeX = ClampCoordinate(static_cast<int32_t>(x) - static_cast<int32_t>(radius), width);
      const uint32_t addX    = ClampCoordinate(static_cast<int32_t>(x) + static_cast<int32_t>(radius) + 1, width);
      const uint8_t* remove  = sourceRow + static_cast<size_t>(removeX) * channels;
      const uint8_t* add     = sourceRow + static_cast<size_t>(addX) * channels;
      for(uint32_t channel = 0u; channel < channels; ++channel)
      {
        sums[channel] += add[channel];
        sums[channel] -= remove[channel];
      }
    }
  }

  uint8_t* output = buffer.GetBuffer();
  for(uint32_t x = 0u; x < width; ++x)
  {
    uint32_t sums[4u] = {0u, 0u, 0u, 0u};
    for(int32_t offset = -static_cast<int32_t>(radius); offset <= static_cast<int32_t>(radius); ++offset)
    {
      const uint8_t* pixel = temp + static_cast<size_t>(ClampCoordinate(offset, height)) * tempStride +
                             static_cast<size_t>(x) * channels;
      for(uint32_t channel = 0u; channel < channels; ++channel)
      {
        sums[channel] += pixel[channel];
      }
    }

    for(uint32_t y = 0u; y < height; ++y)
    {
      uint8_t* outputPixel = output + static_cast<size_t>(y) * inputStride + static_cast<size_t>(x) * channels;
      for(uint32_t channel = 0u; channel < channels; ++channel)
      {
        outputPixel[channel] = static_cast<uint8_t>((sums[channel] + windowSize / 2u) / windowSize);
      }

      const uint32_t removeY = ClampCoordinate(static_cast<int32_t>(y) - static_cast<int32_t>(radius), height);
      const uint32_t addY    = ClampCoordinate(static_cast<int32_t>(y) + static_cast<int32_t>(radius) + 1, height);
      const uint8_t* remove  = temp + static_cast<size_t>(removeY) * tempStride + static_cast<size_t>(x) * channels;
      const uint8_t* add     = temp + static_cast<size_t>(addY) * tempStride + static_cast<size_t>(x) * channels;
      for(uint32_t channel = 0u; channel < channels; ++channel)
      {
        sums[channel] += add[channel];
        sums[channel] -= remove[channel];
      }
    }
  }
}

void SlidingMaximum(const std::vector<uint16_t>& input,
                    std::vector<uint16_t>&       output,
                    std::vector<uint32_t>&       queue,
                    uint32_t                     radius)
{
  const uint32_t length = static_cast<uint32_t>(input.size());
  if(length == 0u || radius == 0u)
  {
    output = input;
    return;
  }

  uint32_t head       = 0u;
  uint32_t tail       = 0u;
  uint32_t nextSource = 0u;
  for(uint32_t destination = 0u; destination < length; ++destination)
  {
    const uint32_t windowEnd = std::min(length - 1u, destination + radius);
    while(nextSource <= windowEnd)
    {
      while(head < tail && input[queue[tail - 1u]] <= input[nextSource])
      {
        --tail;
      }
      queue[tail++] = nextSource++;
    }

    const uint32_t windowBegin = destination > radius ? destination - radius : 0u;
    while(head < tail && queue[head] < windowBegin)
    {
      ++head;
    }
    output[destination] = head < tail ? input[queue[head]] : 0u;
  }
}

template<typename Read, typename Write>
void FilterLine(uint32_t               length,
                uint32_t               radius,
                Read&&                 read,
                Write&&                write,
                std::vector<uint16_t>& input,
                std::vector<uint16_t>& output,
                std::vector<uint32_t>& queue)
{
  input.resize(length);
  output.resize(length);
  queue.resize(length);
  for(uint32_t index = 0u; index < length; ++index)
  {
    input[index] = read(index);
  }
  SlidingMaximum(input, output, queue, radius);
  for(uint32_t index = 0u; index < length; ++index)
  {
    write(index, output[index]);
  }
}

void ApplyHorizontalMaximum(std::vector<uint16_t>& timing, uint32_t width, uint32_t height, uint32_t radius,
                            std::vector<uint16_t>& input, std::vector<uint16_t>& output, std::vector<uint32_t>& queue)
{
  for(uint32_t y = 0u; y < height; ++y)
  {
    const size_t row = static_cast<size_t>(y) * width;
    FilterLine(width, radius,
               [&](uint32_t x)
    { return timing[row + x]; },
               [&](uint32_t x, uint16_t value)
    { timing[row + x] = value; },
               input, output, queue);
  }
}

void ApplyVerticalMaximum(std::vector<uint16_t>& timing, uint32_t width, uint32_t height, uint32_t radius,
                          std::vector<uint16_t>& input, std::vector<uint16_t>& output, std::vector<uint32_t>& queue)
{
  for(uint32_t x = 0u; x < width; ++x)
  {
    FilterLine(height, radius,
               [&](uint32_t y)
    { return timing[static_cast<size_t>(y) * width + x]; },
               [&](uint32_t y, uint16_t value)
    { timing[static_cast<size_t>(y) * width + x] = value; },
               input, output, queue);
  }
}

void ApplyDiagonalMaximum(std::vector<uint16_t>& timing,
                          uint32_t               width,
                          uint32_t               height,
                          uint32_t               radius,
                          bool                   rising,
                          std::vector<uint16_t>& input,
                          std::vector<uint16_t>& output,
                          std::vector<uint32_t>& queue)
{
  const uint32_t lineCount = width + height - 1u;
  for(uint32_t line = 0u; line < lineCount; ++line)
  {
    const int32_t  startX = rising
                              ? std::max(0, static_cast<int32_t>(line) - static_cast<int32_t>(height) + 1)
                              : std::min(static_cast<int32_t>(width) - 1, static_cast<int32_t>(line));
    const int32_t  startY = rising
                              ? std::max(0, static_cast<int32_t>(height) - 1 - static_cast<int32_t>(line))
                              : std::max(0, static_cast<int32_t>(line) - static_cast<int32_t>(width) + 1);
    const uint32_t length = rising
                              ? std::min(width - static_cast<uint32_t>(startX), height - static_cast<uint32_t>(startY))
                              : std::min(static_cast<uint32_t>(startX) + 1u, height - static_cast<uint32_t>(startY));
    FilterLine(length, radius,
               [&](uint32_t index)
    {
      const int32_t x = rising ? startX + static_cast<int32_t>(index) : startX - static_cast<int32_t>(index);
      const int32_t y = startY + static_cast<int32_t>(index);
      return timing[static_cast<size_t>(y) * width + static_cast<uint32_t>(x)];
    },
               [&](uint32_t index, uint16_t value)
    {
      const int32_t x                                                   = rising ? startX + static_cast<int32_t>(index) : startX - static_cast<int32_t>(index);
      const int32_t y                                                   = startY + static_cast<int32_t>(index);
      timing[static_cast<size_t>(y) * width + static_cast<uint32_t>(x)] = value;
    },
               input, output, queue);
  }
}

uint16_t GetRevealOwnershipKey(const uint8_t* pixel)
{
  if(pixel[2u] == 0u)
  {
    return 0u;
  }
  const uint32_t encodedStart = static_cast<uint32_t>(pixel[0u]) * 256u + pixel[1u];
  return static_cast<uint16_t>(std::min(encodedStart, 65534u) + 1u);
}

uint8_t QuantizeOwnershipKey(uint16_t key)
{
  // Blur timing has one byte. Quantize toward a later start so precision loss
  // can delay a halo slightly, but can never expose a later unit too early.
  return key == 0u
           ? 0u
           : static_cast<uint8_t>((static_cast<uint32_t>(key - 1u) * 255u + 65533u) / 65534u);
}
} // unnamed namespace

float ResolveFadeBlurReferencePixelSize(const ModelInterface&       model,
                                        bool                        hasInlineReplacement,
                                        TextAbstraction::FontClient fontClient)
{
  return ResolveTextForegroundReferencePixelSize(model, hasInlineReplacement, fontClient);
}

FadeBlurParameters ResolveFadeBlurParameters(float    referencePixelSize,
                                             float    blurStrength,
                                             uint32_t rasterWidth,
                                             uint32_t rasterHeight)
{
  FadeBlurParameters parameters;
  parameters.referencePixelSize = std::max(1.0f, referencePixelSize);
  const float recommendedRadius = std::max(AUTOMATIC_MIN_RADIUS,
                                           ClampTargetRadius(std::round(
                                             parameters.referencePixelSize * TARGET_RADIUS_RATIO)));
  // Explicit strength is relative to the successful adaptive policy rather
  // than an absolute pixel-radius selector. The square-root curve keeps weak
  // authored values visibly blurred while preserving exact AUTO equivalence
  // at one. The curve itself is intentionally an implementation detail.
  parameters.targetRadius = blurStrength < 0.0f
                              ? recommendedRadius
                              : ClampTargetRadius(recommendedRadius * std::sqrt(
                                                                        std::max(0.0f, std::min(1.0f, blurStrength))));

  // Resolve preprocessing scale from the full recommended blur. Reducing an
  // explicit strength must never allocate a larger preblur than AUTO.
  if(ResolveSupportRadius(recommendedRadius, 0.25f) >= 2u)
  {
    parameters.scale = 0.25f;
  }
  else if(ResolveSupportRadius(recommendedRadius, 0.5f) >= 2u)
  {
    parameters.scale = 0.5f;
  }
  else
  {
    parameters.scale = 1.0f;
  }

  auto resolveLowResolutionSize = [&](float scale)
  {
    parameters.lowResolutionWidth  = rasterWidth == 0u
                                       ? 0u
                                       : std::max(1u, static_cast<uint32_t>(std::round(rasterWidth * scale)));
    parameters.lowResolutionHeight = rasterHeight == 0u
                                       ? 0u
                                       : std::max(1u, static_cast<uint32_t>(std::round(rasterHeight * scale)));
  };
  resolveLowResolutionSize(parameters.scale);

  // A small font does not imply a small control. Bound preserved-color
  // resources so an FHD Label cannot resolve to a full-size RGBA blur.
  if(rasterWidth > 0u && rasterHeight > 0u)
  {
    const auto exceedsBudget = [&]()
    {
      return static_cast<uint64_t>(parameters.lowResolutionWidth) * parameters.lowResolutionHeight >
             MAX_AUTO_PREBLUR_PIXELS;
    };
    if(parameters.scale > 0.5f && exceedsBudget())
    {
      parameters.scale = 0.5f;
      resolveLowResolutionSize(parameters.scale);
    }
    if(parameters.scale > 0.25f && exceedsBudget())
    {
      parameters.scale = 0.25f;
      resolveLowResolutionSize(parameters.scale);
    }
  }

  parameters.supportRadius = ResolveSupportRadius(parameters.targetRadius, parameters.scale);
  parameters.firstRadius   = parameters.supportRadius / 2u;
  parameters.secondRadius  = parameters.supportRadius - parameters.firstRadius;
  return parameters;
}

uint32_t GetFadeBlurGuardBand(const FadeBlurParameters& parameters)
{
  const float    scale           = std::max(0.25f, std::min(1.0f, parameters.scale));
  const uint32_t ownershipRadius = static_cast<uint32_t>(std::ceil(parameters.targetRadius + 1.0f / scale));
  const uint32_t axisRadius      = std::max(1u, static_cast<uint32_t>(std::floor(ownershipRadius * 0.41421356f)));
  const uint32_t diagonalRadius  = std::max(1u, (ownershipRadius - axisRadius + 1u) / 2u);
  const uint32_t ownershipExtent = axisRadius + diagonalRadius * 2u;
  const uint32_t blurExtent      = static_cast<uint32_t>(std::ceil(
    static_cast<float>(parameters.firstRadius + parameters.secondRadius + 1u) / scale));
  const uint32_t extent          = std::max(ownershipExtent, blurExtent);
  return (extent + 3u) & ~3u;
}

bool PrepareFadeBlurBuffer(PixelBuffer& buffer, float scale, float targetRadius)
{
  DALI_TRACE_SCOPE(gTraceFilter, "DALI_TEXT_REVEAL_FADE_BLUR_PREPARE");
  if(!buffer || (buffer.GetPixelFormat() != Pixel::L8 && buffer.GetPixelFormat() != Pixel::RGBA8888))
  {
    return false;
  }

  scale                     = std::max(0.25f, std::min(1.0f, scale));
  targetRadius              = ClampTargetRadius(targetRadius);
  const uint32_t blurWidth  = std::max(1u, static_cast<uint32_t>(std::round(buffer.GetWidth() * scale)));
  const uint32_t blurHeight = std::max(1u, static_cast<uint32_t>(std::round(buffer.GetHeight() * scale)));
  if(blurWidth > 65535u || blurHeight > 65535u)
  {
    return false;
  }

  if(blurWidth != buffer.GetWidth() || blurHeight != buffer.GetHeight())
  {
    DALI_TRACE_SCOPE(gTraceFilter, "DALI_TEXT_REVEAL_FADE_BLUR_DOWNSAMPLE");
    PixelBuffer downsampled = BoxDownsample(buffer, blurWidth, blurHeight);
    if(!downsampled)
    {
      return false;
    }
    buffer = downsampled;
  }

  const uint32_t supportRadius = ResolveSupportRadius(targetRadius, scale);
  const uint32_t firstRadius   = supportRadius / 2u;
  const uint32_t secondRadius  = supportRadius - firstRadius;
  PixelBuffer    temporary     = PixelBuffer::New(blurWidth, blurHeight, buffer.GetPixelFormat());
  if(!temporary)
  {
    return false;
  }
  {
    DALI_TRACE_SCOPE(gTraceFilter, "DALI_TEXT_REVEAL_FADE_BLUR_BOX_BLUR");
    ApplyBoxBlurPass(buffer, temporary, firstRadius, false);
    ApplyBoxBlurPass(buffer, temporary, secondRadius, true);
  }
  return true;
}

bool CropFadeBlurBuffer(PixelBuffer& buffer,
                        uint32_t     offsetX,
                        uint32_t     offsetY,
                        uint32_t     width,
                        uint32_t     height)
{
  if(!buffer || width == 0u || height == 0u ||
     offsetX > buffer.GetWidth() || width > buffer.GetWidth() - offsetX ||
     offsetY > buffer.GetHeight() || height > buffer.GetHeight() - offsetY)
  {
    return false;
  }
  if(offsetX == 0u && offsetY == 0u && width == buffer.GetWidth() && height == buffer.GetHeight())
  {
    return true;
  }

  const uint32_t channels = Pixel::GetBytesPerPixel(buffer.GetPixelFormat());
  PixelBuffer    cropped  = PixelBuffer::New(width, height, buffer.GetPixelFormat());
  if(!cropped || channels == 0u)
  {
    return false;
  }

  const uint8_t* source = buffer.GetBuffer() + static_cast<size_t>(offsetY) * buffer.GetStrideBytes() +
                          static_cast<size_t>(offsetX) * channels;
  uint8_t*     destination  = cropped.GetBuffer();
  const size_t bytesPerLine = static_cast<size_t>(width) * channels;
  for(uint32_t y = 0u; y < height; ++y)
  {
    memcpy(destination + static_cast<size_t>(y) * cropped.GetStrideBytes(),
           source + static_cast<size_t>(y) * buffer.GetStrideBytes(),
           bytesPerLine);
  }
  buffer = cropped;
  return true;
}

void WriteFadeBlurCoverage(const PixelBuffer& coverage, uint8_t* metadata, uint32_t width, uint32_t height)
{
  DALI_TRACE_SCOPE(gTraceFilter, "DALI_TEXT_REVEAL_FADE_BLUR_COVERAGE");
  DALI_ASSERT_ALWAYS(coverage && coverage.GetPixelFormat() == Pixel::L8 && metadata);

  const uint32_t sourceWidth  = coverage.GetWidth();
  const uint32_t sourceHeight = coverage.GetHeight();
  const uint8_t* source       = coverage.GetBuffer();
  const uint32_t sourceStride = coverage.GetStrideBytes();
  if(sourceWidth == width && sourceHeight == height)
  {
    for(uint32_t y = 0u; y < height; ++y)
    {
      const uint8_t* sourceRow = source + static_cast<size_t>(y) * sourceStride;
      uint8_t*       output    = metadata + static_cast<size_t>(y) * width * METADATA_PIXEL_SIZE + 3u;
      for(uint32_t x = 0u; x < width; ++x, output += METADATA_PIXEL_SIZE)
      {
        *output = sourceRow[x];
      }
    }
    return;
  }

  struct Sample
  {
    uint32_t first;
    uint32_t second;
    uint32_t weight;
  };
  std::vector<Sample> horizontal(width);
  std::vector<Sample> vertical(height);
  auto                buildSamples = [](std::vector<Sample>& samples, uint32_t sourceLength)
  {
    const uint32_t outputLength = static_cast<uint32_t>(samples.size());
    for(uint32_t output = 0u; output < outputLength; ++output)
    {
      const int64_t coordinate = (static_cast<int64_t>(output * 2u + 1u) * sourceLength * 128) /
                                   outputLength -
                                 128;
      const int32_t  first  = static_cast<int32_t>(coordinate >= 0
                                                     ? coordinate / 256
                                                     : -((-coordinate + 255) / 256));
      const uint32_t weight = static_cast<uint32_t>(coordinate - static_cast<int64_t>(first) * 256);
      samples[output]       = {ClampCoordinate(first, sourceLength),
                               ClampCoordinate(first + 1, sourceLength),
                               weight};
    }
  };
  buildSamples(horizontal, sourceWidth);
  buildSamples(vertical, sourceHeight);

  for(uint32_t y = 0u; y < height; ++y)
  {
    const Sample&  ySample   = vertical[y];
    const uint8_t* firstRow  = source + static_cast<size_t>(ySample.first) * sourceStride;
    const uint8_t* secondRow = source + static_cast<size_t>(ySample.second) * sourceStride;
    uint8_t*       output    = metadata + static_cast<size_t>(y) * width * METADATA_PIXEL_SIZE + 3u;
    for(uint32_t x = 0u; x < width; ++x, output += METADATA_PIXEL_SIZE)
    {
      const Sample&  xSample = horizontal[x];
      const uint32_t top     = firstRow[xSample.first] * (256u - xSample.weight) +
                           firstRow[xSample.second] * xSample.weight;
      const uint32_t bottom = secondRow[xSample.first] * (256u - xSample.weight) +
                              secondRow[xSample.second] * xSample.weight;
      *output = static_cast<uint8_t>((top * (256u - ySample.weight) + bottom * ySample.weight + 32768u) >> 16u);
    }
  }
}

void MaterializeFadeBlurTiming(uint8_t* metadata,
                               uint32_t width,
                               uint32_t height,
                               float    scale,
                               float    targetRadius,
                               bool     coverageAware)
{
  DALI_TRACE_SCOPE(gTraceFilter, "DALI_TEXT_REVEAL_FADE_BLUR_OWNERSHIP");
  if(!metadata || width == 0u || height == 0u)
  {
    return;
  }

  scale                           = std::max(0.25f, std::min(1.0f, scale));
  targetRadius                    = ClampTargetRadius(targetRadius);
  const float    resamplingMargin = 1.0f / scale;
  const uint32_t radius           = static_cast<uint32_t>(std::ceil(targetRadius + resamplingMargin));

  // Text normally occupies only a small part of its texture. Restrict the
  // morphology to the source ownership bounds plus its maximum footprint;
  // processing blank rows and columns dominated the original FHD setup cost.
  uint32_t sourceMinX = width;
  uint32_t sourceMinY = height;
  uint32_t sourceMaxX = 0u;
  uint32_t sourceMaxY = 0u;
  for(uint32_t y = 0u; y < height; ++y)
  {
    for(uint32_t x = 0u; x < width; ++x)
    {
      if(GetRevealOwnershipKey(metadata + (static_cast<size_t>(y) * width + x) * METADATA_PIXEL_SIZE) != 0u)
      {
        sourceMinX = std::min(sourceMinX, x);
        sourceMinY = std::min(sourceMinY, y);
        sourceMaxX = std::max(sourceMaxX, x);
        sourceMaxY = std::max(sourceMaxY, y);
      }
    }
  }
  if(sourceMinX == width)
  {
    return;
  }

  const uint32_t axisRadius      = std::max(1u, static_cast<uint32_t>(std::floor(radius * 0.41421356f)));
  const uint32_t diagonalRadius  = std::max(1u, (radius - axisRadius + 1u) / 2u);
  const uint32_t footprintExtent = axisRadius + diagonalRadius * 2u;

  const uint32_t timingMinX   = sourceMinX > footprintExtent ? sourceMinX - footprintExtent : 0u;
  const uint32_t timingMinY   = sourceMinY > footprintExtent ? sourceMinY - footprintExtent : 0u;
  const uint32_t timingMaxX   = std::min(width - 1u, sourceMaxX + footprintExtent);
  const uint32_t timingMaxY   = std::min(height - 1u, sourceMaxY + footprintExtent);
  const uint32_t timingWidth  = timingMaxX - timingMinX + 1u;
  const uint32_t timingHeight = timingMaxY - timingMinY + 1u;

  std::vector<uint16_t> timing(static_cast<size_t>(timingWidth) * timingHeight);
  for(uint32_t y = 0u; y < timingHeight; ++y)
  {
    const uint32_t sourceY = timingMinY + y;
    for(uint32_t x = 0u; x < timingWidth; ++x)
    {
      const uint32_t sourceX = timingMinX + x;
      timing[static_cast<size_t>(y) * timingWidth + x] =
        GetRevealOwnershipKey(metadata +
                              (static_cast<size_t>(sourceY) * width + sourceX) * METADATA_PIXEL_SIZE);
    }
  }

  std::vector<uint16_t> input;
  std::vector<uint16_t> output;
  std::vector<uint32_t> queue;
  input.reserve(std::max(timingWidth, timingHeight));
  output.reserve(std::max(timingWidth, timingHeight));
  queue.reserve(std::max(timingWidth, timingHeight));

  ApplyHorizontalMaximum(timing, timingWidth, timingHeight, axisRadius, input, output, queue);
  ApplyVerticalMaximum(timing, timingWidth, timingHeight, axisRadius, input, output, queue);
  ApplyDiagonalMaximum(timing, timingWidth, timingHeight, diagonalRadius, true, input, output, queue);
  ApplyDiagonalMaximum(timing, timingWidth, timingHeight, diagonalRadius, false, input, output, queue);

  for(uint32_t y = 0u; y < timingHeight; ++y)
  {
    const uint32_t destinationY = timingMinY + y;
    for(uint32_t x = 0u; x < timingWidth; ++x)
    {
      const uint32_t destinationX = timingMinX + x;
      uint8_t*       pixel        = metadata +
                       (static_cast<size_t>(destinationY) * width + destinationX) * METADATA_PIXEL_SIZE;
      pixel[2u] = coverageAware && pixel[3u] < COVERAGE_TIMING_THRESHOLD
                    ? 0u
                    : QuantizeOwnershipKey(timing[static_cast<size_t>(y) * timingWidth + x]);
    }
  }
}

} // namespace Reveal
} // namespace Internal
} // namespace Text
} // namespace Ui
} // namespace Dali
