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

#include <dali-ui-foundation/internal/text/rendering/text-reveal-sequence-blur-processor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
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
constexpr uint32_t METADATA_PIXEL_SIZE       = 4u;
constexpr uint8_t  COVERAGE_TIMING_THRESHOLD = 2u;
constexpr uint32_t MAX_PREPROCESS_PIXELS     = 512u * 512u;

struct ResampleCoordinate
{
  uint32_t first;
  uint32_t second;
  uint32_t weight;
};

uint8_t QuantizeUnitStart(uint16_t key);

uint32_t ClampCoordinate(int32_t coordinate, uint32_t length)
{
  return static_cast<uint32_t>(std::max(0, std::min(coordinate, static_cast<int32_t>(length) - 1)));
}

PixelBuffer BoxDownsample(const PixelBuffer& source, uint32_t width, uint32_t height)
{
  const uint32_t sourceWidth  = source.GetWidth();
  const uint32_t sourceHeight = source.GetHeight();
  PixelBuffer    result       = PixelBuffer::New(width, height, Pixel::L8);
  if(!result)
  {
    return {};
  }

  const uint8_t* sourcePixels = source.GetBuffer();
  uint8_t*       outputPixels = result.GetBuffer();
  const uint32_t sourceStride = source.GetStrideBytes();
  const uint32_t outputStride = result.GetStrideBytes();
  for(uint32_t outputY = 0u; outputY < height; ++outputY)
  {
    const uint32_t sourceYBegin = static_cast<uint32_t>(static_cast<uint64_t>(outputY) * sourceHeight / height);
    const uint32_t sourceYEnd   = std::max(sourceYBegin + 1u,
                                           static_cast<uint32_t>(static_cast<uint64_t>(outputY + 1u) * sourceHeight / height));
    for(uint32_t outputX = 0u; outputX < width; ++outputX)
    {
      const uint32_t sourceXBegin = static_cast<uint32_t>(static_cast<uint64_t>(outputX) * sourceWidth / width);
      const uint32_t sourceXEnd   = std::max(sourceXBegin + 1u,
                                             static_cast<uint32_t>(static_cast<uint64_t>(outputX + 1u) * sourceWidth / width));
      uint32_t       sum          = 0u;
      for(uint32_t sourceY = sourceYBegin; sourceY < sourceYEnd; ++sourceY)
      {
        const uint8_t* row = sourcePixels + static_cast<size_t>(sourceY) * sourceStride;
        for(uint32_t sourceX = sourceXBegin; sourceX < sourceXEnd; ++sourceX)
        {
          sum += row[sourceX];
        }
      }
      const uint32_t count = (sourceXEnd - sourceXBegin) * (sourceYEnd - sourceYBegin);
      outputPixels[static_cast<size_t>(outputY) * outputStride + outputX] =
        static_cast<uint8_t>((sum + count / 2u) / count);
    }
  }
  return result;
}

PixelBuffer CopyCoverage(const PixelBuffer& source)
{
  PixelBuffer result = PixelBuffer::New(source.GetWidth(), source.GetHeight(), Pixel::L8);
  if(!result)
  {
    return {};
  }

  for(uint32_t y = 0u; y < source.GetHeight(); ++y)
  {
    std::memcpy(result.GetBuffer() + static_cast<size_t>(y) * result.GetStrideBytes(),
                source.GetBuffer() + static_cast<size_t>(y) * source.GetStrideBytes(),
                source.GetWidth());
  }
  return result;
}

void ApplyBoxBlurPass(PixelBuffer& buffer, PixelBuffer& temporary, uint32_t radius)
{
  if(radius == 0u)
  {
    return;
  }

  const uint32_t width       = buffer.GetWidth();
  const uint32_t height      = buffer.GetHeight();
  const uint32_t windowSize  = radius * 2u + 1u;
  const uint32_t inputStride = buffer.GetStrideBytes();
  const uint32_t tempStride  = temporary.GetStrideBytes();
  const uint8_t* input       = buffer.GetBuffer();
  uint8_t*       temp        = temporary.GetBuffer();

  for(uint32_t y = 0u; y < height; ++y)
  {
    const uint8_t* sourceRow = input + static_cast<size_t>(y) * inputStride;
    uint8_t*       outputRow = temp + static_cast<size_t>(y) * tempStride;
    uint32_t       sum       = 0u;
    for(int32_t offset = -static_cast<int32_t>(radius); offset <= static_cast<int32_t>(radius); ++offset)
    {
      sum += sourceRow[ClampCoordinate(offset, width)];
    }
    for(uint32_t x = 0u; x < width; ++x)
    {
      outputRow[x] = static_cast<uint8_t>((sum + windowSize / 2u) / windowSize);
      sum += sourceRow[ClampCoordinate(static_cast<int32_t>(x + radius + 1u), width)];
      sum -= sourceRow[ClampCoordinate(static_cast<int32_t>(x) - static_cast<int32_t>(radius), width)];
    }
  }

  uint8_t* output = buffer.GetBuffer();
  for(uint32_t x = 0u; x < width; ++x)
  {
    uint32_t sum = 0u;
    for(int32_t offset = -static_cast<int32_t>(radius); offset <= static_cast<int32_t>(radius); ++offset)
    {
      sum += temp[static_cast<size_t>(ClampCoordinate(offset, height)) * tempStride + x];
    }
    for(uint32_t y = 0u; y < height; ++y)
    {
      output[static_cast<size_t>(y) * inputStride + x] = static_cast<uint8_t>((sum + windowSize / 2u) / windowSize);
      sum += temp[static_cast<size_t>(ClampCoordinate(static_cast<int32_t>(y + radius + 1u), height)) * tempStride + x];
      sum -= temp[static_cast<size_t>(ClampCoordinate(static_cast<int32_t>(y) - static_cast<int32_t>(radius), height)) * tempStride + x];
    }
  }
}

void BuildResampleCoordinates(std::vector<ResampleCoordinate>& coordinates, uint32_t sourceLength)
{
  const uint32_t outputLength = static_cast<uint32_t>(coordinates.size());
  for(uint32_t output = 0u; output < outputLength; ++output)
  {
    const int64_t coordinate = (static_cast<int64_t>(output * 2u + 1u) * sourceLength * 128) /
                                 outputLength -
                               128;
    const int32_t first = static_cast<int32_t>(coordinate >= 0
                                                 ? coordinate / 256
                                                 : -((-coordinate + 255) / 256));
    coordinates[output] = {ClampCoordinate(first, sourceLength),
                           ClampCoordinate(first + 1, sourceLength),
                           static_cast<uint32_t>(coordinate - static_cast<int64_t>(first) * 256)};
  }
}

uint8_t SampleCoverage(const PixelBuffer&        coverage,
                       const ResampleCoordinate& xSample,
                       const ResampleCoordinate& ySample)
{
  const uint8_t* firstRow  = coverage.GetBuffer() + static_cast<size_t>(ySample.first) * coverage.GetStrideBytes();
  const uint8_t* secondRow = coverage.GetBuffer() + static_cast<size_t>(ySample.second) * coverage.GetStrideBytes();
  const uint32_t top       = firstRow[xSample.first] * (256u - xSample.weight) +
                       firstRow[xSample.second] * xSample.weight;
  const uint32_t bottom = secondRow[xSample.first] * (256u - xSample.weight) +
                          secondRow[xSample.second] * xSample.weight;
  return static_cast<uint8_t>((top * (256u - ySample.weight) +
                               bottom * ySample.weight + 32768u) >>
                              16u);
}

void WriteBlurMetadata(const PixelBuffer&           coverage,
                       const PixelBuffer*           mediumCoverage,
                       const std::vector<uint16_t>& unitOwnership,
                       const std::vector<uint16_t>& sequenceOwnership,
                       PixelBuffer&                 metadata,
                       PixelBuffer&                 sequenceMetadata,
                       PixelBuffer*                 mediumBlurCoverage)
{
  const uint32_t                  width        = metadata.GetWidth();
  const uint32_t                  height       = metadata.GetHeight();
  const uint32_t                  sourceWidth  = coverage.GetWidth();
  const uint32_t                  sourceHeight = coverage.GetHeight();
  std::vector<ResampleCoordinate> horizontal(width);
  std::vector<ResampleCoordinate> vertical(height);
  BuildResampleCoordinates(horizontal, sourceWidth);
  BuildResampleCoordinates(vertical, sourceHeight);
  for(uint32_t y = 0u; y < height; ++y)
  {
    const ResampleCoordinate& ySample        = vertical[y];
    uint8_t*                  metadataRow    = metadata.GetBuffer() + static_cast<size_t>(y) * metadata.GetStrideBytes();
    uint8_t*                  sequenceRow    = sequenceMetadata.GetBuffer() + static_cast<size_t>(y) * sequenceMetadata.GetStrideBytes();
    uint8_t*                  mediumRow      = mediumBlurCoverage
                                                 ? mediumBlurCoverage->GetBuffer() + static_cast<size_t>(y) * mediumBlurCoverage->GetStrideBytes()
                                                 : nullptr;
    const size_t              firstRowIndex  = static_cast<size_t>(ySample.first) * sourceWidth;
    const size_t              secondRowIndex = static_cast<size_t>(ySample.second) * sourceWidth;
    for(uint32_t x = 0u; x < width; ++x)
    {
      const ResampleCoordinate& xSample = horizontal[x];
      uint8_t*                  pixel   = metadataRow + static_cast<size_t>(x) * METADATA_PIXEL_SIZE;
      pixel[3u]                         = SampleCoverage(coverage, xSample, ySample);
      if(mediumRow)
      {
        mediumRow[x] = SampleCoverage(*mediumCoverage, xSample, ySample);
      }
      if(pixel[3u] < COVERAGE_TIMING_THRESHOLD)
      {
        pixel[2u]      = 0u;
        sequenceRow[x] = 0u;
        continue;
      }

      const uint16_t unitKey     = std::max({unitOwnership[firstRowIndex + xSample.first],
                                             unitOwnership[firstRowIndex + xSample.second],
                                             unitOwnership[secondRowIndex + xSample.first],
                                             unitOwnership[secondRowIndex + xSample.second]});
      const uint16_t sequenceKey = std::max({sequenceOwnership[firstRowIndex + xSample.first],
                                             sequenceOwnership[firstRowIndex + xSample.second],
                                             sequenceOwnership[secondRowIndex + xSample.first],
                                             sequenceOwnership[secondRowIndex + xSample.second]});
      pixel[2u]                  = QuantizeUnitStart(unitKey);
      sequenceRow[x]             = static_cast<uint8_t>(sequenceKey);
    }
  }
}

void SlidingMaximum(const std::vector<uint16_t>& input,
                    std::vector<uint16_t>&       output,
                    std::vector<uint32_t>&       queue,
                    uint32_t                     radius)
{
  const uint32_t length = static_cast<uint32_t>(input.size());
  uint32_t       head = 0u, tail = 0u, next = 0u;
  for(uint32_t destination = 0u; destination < length; ++destination)
  {
    const uint32_t end = std::min(length - 1u, destination + radius);
    while(next <= end)
    {
      while(head < tail && input[queue[tail - 1u]] <= input[next])
      {
        --tail;
      }
      queue[tail++] = next++;
    }
    const uint32_t begin = destination > radius ? destination - radius : 0u;
    while(head < tail && queue[head] < begin)
    {
      ++head;
    }
    output[destination] = input[queue[head]];
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

void DilateOwnership(std::vector<uint16_t>& ownership, uint32_t width, uint32_t height, uint32_t radius)
{
  std::vector<uint16_t> input;
  std::vector<uint16_t> output;
  std::vector<uint32_t> queue;
  const uint32_t        maximumLineLength = std::max(width, height);
  input.reserve(maximumLineLength);
  output.reserve(maximumLineLength);
  queue.reserve(maximumLineLength);
  for(uint32_t y = 0u; y < height; ++y)
  {
    const size_t row = static_cast<size_t>(y) * width;
    FilterLine(width, radius,
               [&](uint32_t x)
    { return ownership[row + x]; },
               [&](uint32_t x, uint16_t value)
    { ownership[row + x] = value; },
               input, output, queue);
  }
  for(uint32_t x = 0u; x < width; ++x)
  {
    FilterLine(height, radius,
               [&](uint32_t y)
    { return ownership[static_cast<size_t>(y) * width + x]; },
               [&](uint32_t y, uint16_t value)
    { ownership[static_cast<size_t>(y) * width + x] = value; },
               input, output, queue);
  }
}

uint8_t QuantizeUnitStart(uint16_t key)
{
  return key == 0u
           ? 0u
           : static_cast<uint8_t>(1u + (static_cast<uint32_t>(key - 1u) * 254u + 65533u) / 65534u);
}
} // unnamed namespace

SequenceBlurParameters ResolveSequenceBlurParameters(float referencePixelSize,
                                                     float blurStrength,
                                                     bool  useMultiRadiusQualityScale)
{
  SequenceBlurParameters parameters;
  referencePixelSize      = std::max(1.0f, referencePixelSize);
  blurStrength            = std::max(0.0f, std::min(1.0f, blurStrength));
  const float fullRadius  = std::max(2.0f, std::min(12.0f, referencePixelSize * 0.125f));
  parameters.targetRadius = std::max(0.75f, fullRadius * std::sqrt(blurStrength));
  parameters.mediumRadius = std::max(0.75f, parameters.targetRadius * 0.5f);
  if(parameters.targetRadius * 0.25f >= 2.0f)
  {
    parameters.preprocessingScale = 0.25f;
  }
  else if(parameters.targetRadius * 0.5f >= 2.0f)
  {
    parameters.preprocessingScale = 0.5f;
  }
  if(useMultiRadiusQualityScale)
  {
    // Preserve the existing discrete preprocessing scales, but promote the
    // Multi-Radius path until the intermediate radius has at least two source
    // pixels of support when the authored radius makes that possible. The
    // allocation-area guard in PrepareSequenceBlurMetadata remains final.
    while(parameters.preprocessingScale < 1.0f &&
          parameters.mediumRadius * parameters.preprocessingScale < 2.0f)
    {
      parameters.preprocessingScale *= 2.0f;
    }
  }
  return parameters;
}

bool PrepareSequenceBlurMetadata(PixelBuffer&                  metadata,
                                 PixelBuffer&                  sequenceMetadata,
                                 const SequenceBlurParameters& parameters,
                                 PixelBuffer*                  mediumBlurCoverage,
                                 float                         ownershipOracleProgress)
{
  if(!metadata || metadata.GetPixelFormat() != Pixel::RGBA8888 ||
     !sequenceMetadata || sequenceMetadata.GetPixelFormat() != Pixel::L8 ||
     metadata.GetWidth() != sequenceMetadata.GetWidth() ||
     metadata.GetHeight() != sequenceMetadata.GetHeight())
  {
    return false;
  }

  const uint32_t width    = metadata.GetWidth();
  const uint32_t height   = metadata.GetHeight();
  PixelBuffer    coverage = PixelBuffer::New(width, height, Pixel::L8);
  if(!coverage)
  {
    return false;
  }

  const bool           ownershipOracleEnabled = ownershipOracleProgress >= 0.0f;
  std::vector<uint8_t> visibleSource;
  std::vector<uint8_t> revealTargetSource;
  if(ownershipOracleEnabled)
  {
    ownershipOracleProgress = std::max(0.0f, std::min(1.0f, ownershipOracleProgress));
    visibleSource.resize(static_cast<size_t>(width) * height, 0u);
    revealTargetSource.resize(static_cast<size_t>(width) * height, 0u);
  }

  for(uint32_t y = 0u; y < height; ++y)
  {
    const uint8_t* metadataRow = metadata.GetBuffer() + static_cast<size_t>(y) * metadata.GetStrideBytes();
    uint8_t*       coverageRow = coverage.GetBuffer() + static_cast<size_t>(y) * coverage.GetStrideBytes();
    for(uint32_t x = 0u; x < width; ++x)
    {
      const uint8_t* pixel   = metadataRow + static_cast<size_t>(x) * METADATA_PIXEL_SIZE;
      bool           visible = true;
      if(ownershipOracleEnabled)
      {
        const uint32_t encodedStart = (static_cast<uint32_t>(pixel[0u]) << 8u) | pixel[1u];
        visible                     = pixel[2u] != 0u && ownershipOracleProgress > 0.0f &&
                  static_cast<float>(encodedStart) / 65535.0f <= ownershipOracleProgress;
        const size_t sourceIndex        = static_cast<size_t>(y) * width + x;
        visibleSource[sourceIndex]      = visible ? 1u : 0u;
        revealTargetSource[sourceIndex] = pixel[2u];
      }
      coverageRow[x] = visible ? pixel[3u] : 0u;
    }
  }

  float scale           = std::max(0.25f, std::min(1.0f, parameters.preprocessingScale));
  auto  resolveBlurSize = [&](uint32_t& blurWidth, uint32_t& blurHeight)
  {
    blurWidth  = std::max(1u, static_cast<uint32_t>(std::round(width * scale)));
    blurHeight = std::max(1u, static_cast<uint32_t>(std::round(height * scale)));
  };
  uint32_t blurWidth;
  uint32_t blurHeight;
  resolveBlurSize(blurWidth, blurHeight);
  while(static_cast<uint64_t>(blurWidth) * blurHeight > MAX_PREPROCESS_PIXELS &&
        (blurWidth > 1u || blurHeight > 1u))
  {
    scale *= 0.5f;
    resolveBlurSize(blurWidth, blurHeight);
  }
  if(blurWidth != width || blurHeight != height)
  {
    coverage = BoxDownsample(coverage, blurWidth, blurHeight);
    if(!coverage)
    {
      return false;
    }
  }

  PixelBuffer mediumCoverage;
  if(mediumBlurCoverage)
  {
    mediumCoverage = CopyCoverage(coverage);
    if(!mediumCoverage)
    {
      return false;
    }
  }

  const uint32_t supportRadius       = std::max(1u, static_cast<uint32_t>(std::round(parameters.targetRadius * scale)));
  const uint32_t mediumSupportRadius = std::max(1u, static_cast<uint32_t>(std::round(parameters.mediumRadius * scale)));
  PixelBuffer    temporary           = PixelBuffer::New(blurWidth, blurHeight, Pixel::L8);
  if(!temporary)
  {
    return false;
  }
  ApplyBoxBlurPass(coverage, temporary, supportRadius / 2u);
  ApplyBoxBlurPass(coverage, temporary, supportRadius - supportRadius / 2u);
  if(mediumCoverage)
  {
    ApplyBoxBlurPass(mediumCoverage, temporary, mediumSupportRadius / 2u);
    ApplyBoxBlurPass(mediumCoverage, temporary, mediumSupportRadius - mediumSupportRadius / 2u);
    *mediumBlurCoverage = PixelBuffer::New(width, height, Pixel::L8);
    if(!*mediumBlurCoverage)
    {
      return false;
    }
  }

  std::vector<uint16_t> unitOwnership(static_cast<size_t>(blurWidth) * blurHeight, 0u);
  std::vector<uint16_t> sequenceOwnership(static_cast<size_t>(blurWidth) * blurHeight, 0u);
  for(uint32_t y = 0u; y < height; ++y)
  {
    const uint8_t* metadataRow = metadata.GetBuffer() + static_cast<size_t>(y) * metadata.GetStrideBytes();
    const uint8_t* sequenceRow = sequenceMetadata.GetBuffer() + static_cast<size_t>(y) * sequenceMetadata.GetStrideBytes();
    const uint32_t ownershipY  = std::min(blurHeight - 1u,
                                          static_cast<uint32_t>(static_cast<uint64_t>(y) * blurHeight / height));
    for(uint32_t x = 0u; x < width; ++x)
    {
      if(ownershipOracleEnabled && visibleSource[static_cast<size_t>(y) * width + x] == 0u)
      {
        continue;
      }
      const uint8_t* pixel      = metadataRow + static_cast<size_t>(x) * METADATA_PIXEL_SIZE;
      const uint32_t ownershipX = std::min(blurWidth - 1u,
                                           static_cast<uint32_t>(static_cast<uint64_t>(x) * blurWidth / width));
      const size_t   index      = static_cast<size_t>(ownershipY) * blurWidth + ownershipX;
      if(pixel[2u] != 0u)
      {
        const uint32_t encoded = (static_cast<uint32_t>(pixel[0u]) << 8u) | pixel[1u];
        unitOwnership[index]   = std::max(unitOwnership[index],
                                          static_cast<uint16_t>(std::min(encoded, 65534u) + 1u));
      }
      sequenceOwnership[index] = std::max(sequenceOwnership[index],
                                          static_cast<uint16_t>(sequenceRow[x]));
    }
  }

  // The two-pass box filter has a square support. Expand ownership in the
  // same low-resolution domain and sample the maximum of every bilinear
  // contributor when projecting back, so a blur halo can never borrow an
  // earlier unit or sequence start.
  const uint32_t ownershipRadius = supportRadius + 1u;
  DilateOwnership(unitOwnership, blurWidth, blurHeight, ownershipRadius);
  DilateOwnership(sequenceOwnership, blurWidth, blurHeight, ownershipRadius);
  WriteBlurMetadata(coverage,
                    mediumBlurCoverage ? &mediumCoverage : nullptr,
                    unitOwnership,
                    sequenceOwnership,
                    metadata,
                    sequenceMetadata,
                    mediumBlurCoverage);
  if(ownershipOracleEnabled)
  {
    // Blur preprocessing temporarily reuses the target byte for blur timing.
    // Preserve the original target marker where gated coverage is empty so
    // the sharp foreground still obeys its original Reveal schedule.
    for(uint32_t y = 0u; y < height; ++y)
    {
      uint8_t* metadataRow = metadata.GetBuffer() + static_cast<size_t>(y) * metadata.GetStrideBytes();
      for(uint32_t x = 0u; x < width; ++x)
      {
        uint8_t* pixel = metadataRow + static_cast<size_t>(x) * METADATA_PIXEL_SIZE;
        if(pixel[2u] == 0u)
        {
          pixel[2u] = revealTargetSource[static_cast<size_t>(y) * width + x];
        }
      }
    }
  }
  return true;
}

} // namespace Reveal
} // namespace Internal
} // namespace Text
} // namespace Ui
} // namespace Dali
