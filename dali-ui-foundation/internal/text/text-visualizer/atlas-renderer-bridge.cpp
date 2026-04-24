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

// CLASS HEADER
#include <dali-ui-foundation/internal/text/text-visualizer/atlas-renderer-bridge.h>

// EXTERNAL INCLUDES
#include <vector>

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/rendering/atlas/text-atlas-renderer.h>
#include <dali-ui-foundation/internal/text/text-visualizer/atlas-view-adapter.h>

namespace Dali::Ui::Internal::TextVisualizer
{
struct AtlasRendererBridge::Impl
{
  struct AtlasGlyphRenderData
  {
    Text::GlyphInfo glyph;
    GlyphPlacement  placement;
    float           x{0.0f};
    float           y{0.0f};
    float           width{0.0f};
    float           height{0.0f};
  };

  void Clear()
  {
    mRenderData.clear();
  }

  std::vector<AtlasGlyphRenderData> mRenderData;
};

AtlasRendererBridge::AtlasRendererBridge()
: mAdapter(nullptr),
  mRenderer(),
  mRenderHost(),
  mImpl(new Impl())
{
}

AtlasRendererBridge::~AtlasRendererBridge()
{
  delete mImpl;
}

void AtlasRendererBridge::SetAdapter(const AtlasViewAdapter* adapter)
{
  mAdapter = adapter;
}

void AtlasRendererBridge::Clear()
{
  mAdapter = nullptr;
  mImpl->Clear();
  mRenderHost.Reset();
  ResetRenderer();
}

bool AtlasRendererBridge::HasRenderableGlyphs() const
{
  return (nullptr != mAdapter) && mAdapter->HasRenderableGlyphs();
}

bool AtlasRendererBridge::IsRendererCreated() const
{
  return static_cast<bool>(mRenderer);
}

bool AtlasRendererBridge::HasRenderHost() const
{
  return static_cast<bool>(mRenderHost);
}

void AtlasRendererBridge::EnsureRenderer()
{
  if(!mRenderer && HasRenderableGlyphs())
  {
    mRenderer = Text::AtlasRenderer::New();
  }
}

void AtlasRendererBridge::ResetRenderer()
{
  mRenderer.Reset();
}

bool AtlasRendererBridge::UpdateRenderData()
{
  if(!HasRenderableGlyphs())
  {
    return false;
  }

  EnsureRenderer();
  if(!IsRendererCreated())
  {
    return false;
  }

  mImpl->Clear();

  const uint32_t renderableGlyphCount = mAdapter->GetRenderableGlyphCount();
  for(uint32_t index = 0u; index < renderableGlyphCount; ++index)
  {
    GlyphPlacement  placement;
    Text::GlyphInfo glyph;

    if(!mAdapter->GetGlyphPlacement(index, placement) ||
       !mAdapter->GetGlyphInfo(placement.glyphIndex, glyph))
    {
      mImpl->Clear();
      return false;
    }

    Impl::AtlasGlyphRenderData renderData;
    renderData.glyph     = glyph;
    renderData.placement = placement;
    renderData.x         = placement.x;
    renderData.y         = placement.y;
    renderData.width     = placement.width;
    renderData.height    = (placement.height > 0.0f) ? placement.height : glyph.height;
    // TODO: Apply baseline, bearing, glyph offset, and text color during actual AtlasRenderer integration.

    mImpl->mRenderData.push_back(renderData);
  }

  return !mImpl->mRenderData.empty();
}

void AtlasRendererBridge::SetRenderHost(Actor renderHost)
{
  mRenderHost = renderHost;
}

Actor AtlasRendererBridge::GetRenderHost() const
{
  return mRenderHost;
}

} // namespace Dali::Ui::Internal::TextVisualizer
