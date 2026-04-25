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
#include <dali/public-api/rendering/vertex-buffer.h>

#include <cstdint>
#include <vector>

namespace Dali::Ui::Internal::TextVisualizer
{
/**
 * @brief Phase 2 skeleton for a TextVisualizer-only lightweight glyph renderer.
 *
 * This class intentionally only owns the output actor lifecycle for now. It does not
 * build glyph meshes, access atlas glyph cache, or connect to the active TextVisualizer
 * render path yet. The existing Text::AtlasRenderer path remains unchanged.
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

  void ClearMeshes();
  bool HasMeshRecords() const;
  uint32_t GetMeshRecordCount() const;

private:
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

  Actor mRenderHost;
  Actor mOutputActor;
  bool  mAttached;

  std::vector<MeshRecord> mMeshRecords;
};

} // namespace Dali::Ui::Internal::TextVisualizer

#endif // DALI_UI_TEXT_VISUALIZER_GLYPH_RENDERER_H
