#ifndef DALI_UI_TEXT_VISUALIZER_ATLAS_RENDERER_BRIDGE_H
#define DALI_UI_TEXT_VISUALIZER_ATLAS_RENDERER_BRIDGE_H

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

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/rendering/text-renderer.h>
#include <dali/public-api/actors/actor.h>

namespace Dali::Ui::Internal::TextVisualizer
{
class AtlasViewAdapter;

/**
 * @brief Bridge object that owns a renderer instance and references atlas-ready adapter data.
 *
 * This class intentionally stops short of scene attachment and geometry upload. It only tracks
 * whether AtlasRenderer creation is possible and keeps a non-owning adapter pointer for future
 * render integration.
 */
class AtlasRendererBridge
{
public:
  AtlasRendererBridge();
  ~AtlasRendererBridge();

  void SetAdapter(const AtlasViewAdapter* adapter);
  void Clear();

  bool HasRenderableGlyphs() const;
  bool IsRendererCreated() const;
  bool HasRenderHost() const;
  bool IsRendererAttached() const;

  void  EnsureRenderer();
  void  ResetRenderer();
  bool  UpdateRenderData();
  void  SetRenderHost(Actor renderHost);
  Actor GetRenderHost() const;
  bool  AttachRendererToHost();
  void  DetachRendererFromHost();

private:
  struct Impl;

  const AtlasViewAdapter* mAdapter;
  Text::RendererPtr       mRenderer;
  Actor                   mRenderHost;
  bool                    mRendererAttached;
  Impl*                   mImpl;
};

} // namespace Dali::Ui::Internal::TextVisualizer

#endif // DALI_UI_TEXT_VISUALIZER_ATLAS_RENDERER_BRIDGE_H
