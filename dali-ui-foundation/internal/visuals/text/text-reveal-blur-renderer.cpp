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

#include "text-reveal-blur-renderer.h"

#include <dali-ui-foundation/internal/graphics/builtin-shader-extern-gen.h>
#include <dali-ui-foundation/internal/render-effects/gaussian-blur-algorithm.h>
#include <dali/integration-api/adaptor-framework/adaptor.h>
#include <dali/integration-api/debug.h>
#include <dali/integration-api/string-utils.h>
#include <dali/public-api/math/vector2.h>
#include <dali/public-api/math/vector4.h>
#include <dali/public-api/rendering/vertex-buffer.h>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace DALI_NAMESPACE::Ui::Internal::TextRevealBlurRenderer
{
namespace
{
// Match the existing Gaussian shader range: up to 200 pixels / 100 samples.
constexpr uint32_t MAXIMUM_NUMBER_OF_SAMPLES = 100u;

// Diagnostic only: identical sequence-start gate and line-local center mapping
// to text-reveal-blur.frag, without Gaussian offset/weight evaluation. Keeping
// the shared sample block declaration/factory preserves kernel setup/ownership.
constexpr std::string_view ONE_TAP_DIAGNOSTIC_FRAGMENT = R"SHADER(
//@version 100
precision highp float;
INPUT highp vec2 vTexCoord;
UNIFORM sampler2D sTexture;
UNIFORM_BLOCK FragBlock
{
  UNIFORM highp float uOpacity;
  UNIFORM highp float uTextRevealBlurProgress;
#ifndef TEXT_REVEAL_DRAW_BATCH
  UNIFORM highp float uRevealSequenceStart;
  UNIFORM highp vec4 uRevealBatchRect;
#endif
  UNIFORM highp vec2 uRevealBatchInvSize;
};
UNIFORM_BLOCK GaussianBlurSampleBlock
{
  UNIFORM highp float uSampleOffsets[NUM_SAMPLES];
  UNIFORM highp float uSampleWeights[NUM_SAMPLES];
};
#ifdef TEXT_REVEAL_DRAW_BATCH
INPUT highp vec4 vRevealRectangle;
INPUT highp vec2 vRevealBlurState;
#define uRevealBatchRect vRevealRectangle
#define uRevealSequenceStart vRevealBlurState.y
#endif
void main()
{
  // Preserve the existing transparent-before-start behavior; this path makes
  // no texture read until its sequence is eligible to render.
  if(uTextRevealBlurProgress <= 0.0 ||
     uTextRevealBlurProgress < uRevealSequenceStart - 1.0 / 65535.0)
  {
    gl_FragColor = vec4(0.0);
    return;
  }
  highp vec2 point = uRevealBatchRect.xy + vTexCoord * uRevealBatchRect.zw;
  highp vec2 low = uRevealBatchRect.xy + uRevealBatchInvSize * 0.5;
  highp vec2 high = uRevealBatchRect.xy + uRevealBatchRect.zw - uRevealBatchInvSize * 0.5;
  gl_FragColor = TEXTURE(sTexture, clamp(point, low, high)) * uOpacity;
}
)SHADER";

/**
 * @brief Gets the cached quad for non-batched Reveal blur passes.
 */
inline static Dali::Geometry& GetCachedGeometry()
{
  thread_local static Dali::Geometry gPredefinedGeometry;
  if(!gPredefinedGeometry)
  {
    gPredefinedGeometry = Dali::Geometry::New();

    struct VertexPosition
    {
      Dali::Vector2 position;
    };

    VertexPosition positionArray[]  = {{Dali::Vector2(-0.5f, -0.5f)},
                                       {Dali::Vector2(0.5f, -0.5f)},
                                       {Dali::Vector2(-0.5f, 0.5f)},
                                       {Dali::Vector2(0.5f, 0.5f)}};
    uint32_t       numberOfVertices = sizeof(positionArray) / sizeof(VertexPosition);

    Dali::Property::Map positionVertexFormat;
    positionVertexFormat["aPosition"]   = Dali::Property::VECTOR2;
    Dali::VertexBuffer positionVertices = Dali::VertexBuffer::New(positionVertexFormat);
    positionVertices.SetData(positionArray, numberOfVertices);
    gPredefinedGeometry.AddVertexBuffer(positionVertices);

    const uint16_t indices[] = {0, 3, 1, 0, 2, 3};
    gPredefinedGeometry.SetIndexBuffer(&indices[0], sizeof(indices) / sizeof(indices[0]));
  }
  return gPredefinedGeometry;
}

Shader& GetShader(uint32_t blurRadius, bool batch, bool oneTapDiagnostic)
{
  if(!GaussianBlurAlgorithm::IsSupportedRadius(blurRadius))
  {
    thread_local Shader invalid;
    invalid.Reset();
    return invalid;
  }
  // Effect shaders and all Reveal-specific bindings belong to separate caches.
  // Only the immutable Gaussian sample block is shared with ordinary effects.
  thread_local Shader shaders[2u][MAXIMUM_NUMBER_OF_SAMPLES + 1u];
  thread_local Shader oneTapShaders[2u][MAXIMUM_NUMBER_OF_SAMPLES + 1u];
  const uint32_t      numSamples = blurRadius >> 1;
  DALI_ASSERT_DEBUG(numSamples <= MAXIMUM_NUMBER_OF_SAMPLES && "numSamples too big!");
  auto& shader = oneTapDiagnostic ? oneTapShaders[batch ? 1u : 0u][numSamples]
                                  : shaders[batch ? 1u : 0u][numSamples];
  if(!shader)
  {
    std::ostringstream shaderName;
    shaderName.imbue(std::locale::classic());
    shaderName << "GaussianBlurShader_" << numSamples << "_TextReveal";
    if(batch)
    {
      shaderName << "_Batch";
    }
    if(oneTapDiagnostic)
    {
      shaderName << "_OneTapDiagnostic";
    }
    std::ostringstream fragment;
    fragment.imbue(std::locale::classic());
    if(batch)
    {
      fragment << "#define TEXT_REVEAL_DRAW_BATCH\n";
    }
    fragment << (oneTapDiagnostic ? ONE_TAP_DIAGNOSTIC_FRAGMENT : std::string_view(SHADER_TEXT_REVEAL_BLUR_FRAG));
    const std::string batchVertex = batch ? "#define NUM_REVEAL_LINES " + std::to_string(MAX_LINES_PER_DRAW) + "\n" + std::string(SHADER_TEXT_REVEAL_BLUR_VERT)
                                          : std::string();
    shader                        = GaussianBlurAlgorithm::CreateShader(
      blurRadius,
      batch ? Dali::Integration::ToDaliStringView(batchVertex) : Dali::Integration::ToDaliStringView(SHADER_CONTROL_RENDERERS_VERT),
      Dali::Integration::ToDaliStringView(fragment.str()), Shader::Hint::FILE_CACHE_SUPPORT,
      Dali::Integration::ToDaliStringView(shaderName.str()));
    shader.ReserveCustomProperties(1);
    shader.RegisterUniqueProperty("viewEffectiveScale", 1.0f);
  }
  return shader;
}

Renderer CreateRenderer(uint32_t blurRadius, Geometry geometry, bool batch, bool oneTapDiagnostic = false)
{
  if(!Dali::Adaptor::IsAvailable())
  {
    return {};
  }
  auto shader = GetShader(blurRadius, batch, oneTapDiagnostic);
  if(!Dali::Adaptor::IsAvailable() || !shader)
  {
    return {};
  }
  Renderer renderer = Renderer::New(geometry, shader);
  if(!Dali::Adaptor::IsAvailable())
  {
    return {};
  }
  // Initialize scalar passes too; shared shaders must not retain another
  // renderer's atlas binding. Texture/sampler ownership stays with the caller.
  if(!batch)
  {
    renderer.RegisterProperty("uRevealBatchRect", Vector4(0.0f, 0.0f, 1.0f, 1.0f));
  }
  renderer.RegisterProperty("uRevealBatchInvSize", Vector2::ZERO);
  renderer.SetProperty(Renderer::Property::BLEND_PRE_MULTIPLIED_ALPHA, true);
  return renderer;
}
} // unnamed namespace

Renderer Create(uint32_t blurRadius)
{
  if(!Dali::Adaptor::IsAvailable())
  {
    return {};
  }
  return CreateRenderer(blurRadius, GetCachedGeometry(), false);
}

Renderer CreateBatch(uint32_t blurRadius, Geometry geometry)
{
  return CreateRenderer(blurRadius, geometry, true);
}

Renderer CreateOneTapDiagnostic(uint32_t blurRadius, Geometry geometry)
{
  if(!Dali::Adaptor::IsAvailable())
  {
    return {};
  }
  static const bool logged = []
  {
    DALI_LOG_RELEASE_INFO("[TEXT-REVEAL-ONE-TAP-POC] PERFORMANCE H/V use center 1-tap; topology unchanged\n");
    return true;
  }();
  (void)logged;
  const bool batch = static_cast<bool>(geometry);
  return CreateRenderer(blurRadius, batch ? geometry : GetCachedGeometry(), batch, true);
}
} //namespace DALI_NAMESPACE::Ui::Internal::TextRevealBlurRenderer
