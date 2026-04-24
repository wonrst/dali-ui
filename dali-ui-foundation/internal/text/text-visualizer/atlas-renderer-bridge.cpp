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
namespace
{
uint32_t CountDescendants(const Actor& actor)
{
  if(!actor)
  {
    return 0u;
  }

  uint32_t count = actor.GetChildCount();
  for(uint32_t index = 0u; index < actor.GetChildCount(); ++index)
  {
    count += CountDescendants(actor.GetChildAt(index));
  }
  return count;
}

uint32_t CountRenderersRecursive(const Actor& actor)
{
  if(!actor)
  {
    return 0u;
  }

  uint32_t count = actor.GetRendererCount();
  for(uint32_t index = 0u; index < actor.GetChildCount(); ++index)
  {
    count += CountRenderersRecursive(actor.GetChildAt(index));
  }
  return count;
}
} // unnamed namespace

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
  mTextControlActor(),
  mRendererOutput(),
  mRendererAttached(false),
  mRenderCallCount(0u),
  mAttachCallCount(0u),
  mAnimatablePropertyIndex(Property::INVALID_INDEX),
  mAlignmentOffset(0.0f),
  mDepth(0),
  mImpl(new Impl())
{
}

AtlasRendererBridge::~AtlasRendererBridge()
{
  delete mImpl;
}

void AtlasRendererBridge::SetAdapter(const AtlasViewAdapter* adapter)
{
  if(nullptr == adapter)
  {
    DetachRendererFromHost();
    mImpl->Clear();
    mAdapter = nullptr;
    mViewInterface.Clear();
    return;
  }

  mAdapter = adapter;
  mViewInterface.SetAdapter(adapter);
}

void AtlasRendererBridge::Clear()
{
  DetachRendererFromHost();
  mAdapter = nullptr;
  mViewInterface.Clear();
  mImpl->Clear();
  mRenderHost.Reset();
  mTextControlActor.Reset();
  mRenderCallCount = 0u;
  mAttachCallCount = 0u;
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

bool AtlasRendererBridge::HasTextControlActor() const
{
  return static_cast<bool>(mTextControlActor);
}

bool AtlasRendererBridge::HasRendererOutput() const
{
  return static_cast<bool>(mRendererOutput);
}

bool AtlasRendererBridge::IsRendererAttached() const
{
  return mRendererAttached;
}

bool AtlasRendererBridge::IsRenderReady() const
{
  return IsRendererCreated() &&
         HasRenderHost() &&
         HasRendererOutput() &&
         IsRendererAttached() &&
         HasViewInterfaceAdapter() &&
         HasRenderableGlyphs();
}

bool AtlasRendererBridge::HasViewInterfaceAdapter() const
{
  return mViewInterface.HasAdapter();
}

Actor AtlasRendererBridge::GetRendererOutput() const
{
  return mRendererOutput;
}

uint32_t AtlasRendererBridge::GetRendererOutputDescendantCount() const
{
  return CountDescendants(mRendererOutput);
}

uint32_t AtlasRendererBridge::GetRendererOutputTotalRendererCount() const
{
  return CountRenderersRecursive(mRendererOutput);
}

bool AtlasRendererBridge::HasRendererOutputRenderableDescendant() const
{
  return GetRendererOutputTotalRendererCount() > 0u;
}

Vector3 AtlasRendererBridge::GetFirstRendererOutputChildSize() const
{
  if(mRendererOutput && mRendererOutput.GetChildCount() > 0u)
  {
    return mRendererOutput.GetChildAt(0u).GetProperty<Vector3>(Actor::Property::SIZE);
  }

  return Vector3::ZERO;
}

bool AtlasRendererBridge::IsFirstRendererOutputChildVisible() const
{
  return mRendererOutput &&
         (mRendererOutput.GetChildCount() > 0u) &&
         mRendererOutput.GetChildAt(0u).GetProperty<bool>(Actor::Property::VISIBLE);
}

uint32_t AtlasRendererBridge::GetViewInterfaceGetGlyphsCallCount() const
{
  return mViewInterface.GetDiagnostics().getGlyphsCallCount;
}

uint32_t AtlasRendererBridge::GetLastRequestedGlyphCount() const
{
  return mViewInterface.GetDiagnostics().lastRequestedGlyphCount;
}

uint32_t AtlasRendererBridge::GetLastReturnedGlyphCount() const
{
  return mViewInterface.GetDiagnostics().lastReturnedGlyphCount;
}

uint32_t AtlasRendererBridge::GetLastGlyphStartIndex() const
{
  return mViewInterface.GetDiagnostics().lastGlyphStartIndex;
}

uint32_t AtlasRendererBridge::GetRenderCallCount() const
{
  return mRenderCallCount;
}

uint32_t AtlasRendererBridge::GetAttachCallCount() const
{
  return mAttachCallCount;
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
  DetachRendererFromHost();
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
  if(!renderHost)
  {
    DetachRendererFromHost();
  }

  mRenderHost = renderHost;
}

Actor AtlasRendererBridge::GetRenderHost() const
{
  return mRenderHost;
}

void AtlasRendererBridge::SetTextControlActor(Actor textControlActor)
{
  mTextControlActor = textControlActor;
}

Actor AtlasRendererBridge::GetTextControlActor() const
{
  return mTextControlActor;
}

bool AtlasRendererBridge::AttachRendererToHost()
{
  if(!HasRenderHost() || !HasRenderableGlyphs() || !HasViewInterfaceAdapter())
  {
    return false;
  }

  EnsureRenderer();
  if(!IsRendererCreated())
  {
    return false;
  }

  mAlignmentOffset = 0.0f;
  mViewInterface.ResetDiagnostics();
  ++mRenderCallCount;

  Actor output = mRenderer->Render(mViewInterface,
                                   HasTextControlActor() ? mTextControlActor : mRenderHost,
                                   mAnimatablePropertyIndex,
                                   mAlignmentOffset,
                                   mDepth);
  if(!output)
  {
    return false;
  }

  Actor outputParent = output.GetParent();
  if(outputParent && outputParent != mRenderHost)
  {
    return false;
  }

  if(mRendererOutput && mRendererOutput != output)
  {
    Actor previousParent = mRendererOutput.GetParent();
    if(previousParent == mRenderHost)
    {
      mRendererOutput.Unparent();
    }
  }

  if(!output.GetParent())
  {
    mRenderHost.Add(output);
    ++mAttachCallCount;
  }

  mRendererOutput   = output;
  mRendererAttached = true;
  return true;
}

void AtlasRendererBridge::DetachRendererFromHost()
{
  if(mRendererOutput)
  {
    Actor parent = mRendererOutput.GetParent();
    if(parent)
    {
      mRendererOutput.Unparent();
    }
    mRendererOutput.Reset();
  }

  mRendererAttached = false;
}

} // namespace Dali::Ui::Internal::TextVisualizer
