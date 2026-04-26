#ifndef DALI_UI_TEXT_VISUALIZER_GLYPH_RENDERER_H
#define DALI_UI_TEXT_VISUALIZER_GLYPH_RENDERER_H

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

// EXTERNAL INCLUDES
#include <dali/public-api/actors/actor.h>
#include <dali/public-api/rendering/geometry.h>
#include <dali/public-api/rendering/renderer.h>
#include <dali/public-api/rendering/shader.h>
#include <dali/public-api/rendering/vertex-buffer.h>

#include <cstdint>
#include <vector>

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/rendering/atlas/atlas-glyph-manager.h>

namespace Dali::Ui::Internal::TextVisualizer
{
class AtlasViewAdapter;
class LayoutResult;
class PreparedText;

/**
 * @brief Phase 2 prototype for a TextVisualizer-only lightweight glyph renderer.
 *
 * This class is intentionally not connected to the active TextVisualizer render path yet.
 * The current mesh MVP only uses already-cached atlas glyphs and returns false on cache miss.
 * The existing Text::AtlasRenderer path remains unchanged.
 */
class TextVisualizerGlyphRenderer
{
public:
  TextVisualizerGlyphRenderer();
  ~TextVisualizerGlyphRenderer();

  void Clear();

  bool  HasOutputActor() const;
  Actor GetOutputActor() const;

  bool IsAttached() const;

  void  SetRenderHost(Actor renderHost);
  Actor GetRenderHost() const;
  bool  HasRenderHost() const;

  void EnsureOutputActor();
  bool AttachOutputToHost();
  void DetachOutputFromHost();

  void     ClearMeshes();
  bool     HasMeshRecords() const;
  uint32_t GetMeshRecordCount() const;

  void     ClearGlyphCache();
  bool     HasGlyphCacheEntries() const;
  uint32_t GetGlyphCacheEntryCount() const;
  bool     HasGlyphCacheSignature() const;
  uint64_t GetGlyphCacheSignature() const;

  bool Render(const PreparedText& preparedText, const LayoutResult& layoutResult, const AtlasViewAdapter& adapter, const Vector4& textColor);

private:
  struct GlyphCacheEntry
  {
    Text::FontId                            fontId{0u};
    Text::GlyphIndex                        glyphIndex{0u};
    Dali::Ui::AtlasGlyphManager::GlyphStyle style;
    uint32_t                                atlasId{0u};
    uint32_t                                imageId{0u};
  };

  struct MeshRecord
  {
    uint32_t     atlasId{0u};
    Actor        actor;
    Renderer     renderer;
    Geometry     geometry;
    VertexBuffer vertexBuffer;
    uint32_t     vertexCount{0u};
    uint32_t     indexCount{0u};
  };

  bool CalculateGlyphCacheSignature(const AtlasViewAdapter& adapter, uint64_t& signature) const;
  bool CanReuseGlyphCache(uint64_t signature) const;
  bool AcquireGlyphReferences(const AtlasViewAdapter& adapter, std::vector<GlyphCacheEntry>& entries) const;
  void ReleaseGlyphReferences(std::vector<GlyphCacheEntry>& entries) const;

  Actor mRenderHost;
  Actor mOutputActor;
  bool  mAttached;

  std::vector<GlyphCacheEntry> mGlyphCacheEntries;
  uint64_t                     mGlyphCacheSignature;
  bool                         mHasGlyphCacheSignature;

  std::vector<MeshRecord> mMeshRecords;
  Shader                  mShaderL8;
  Shader                  mShaderRgba;
};

} // namespace Dali::Ui::Internal::TextVisualizer

#endif // DALI_UI_TEXT_VISUALIZER_GLYPH_RENDERER_H
