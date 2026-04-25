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
#include <dali-ui-foundation/internal/text/text-visualizer/rendering/text-visualizer-glyph-renderer.h>

namespace Dali::Ui::Internal::TextVisualizer
{

TextVisualizerGlyphRenderer::TextVisualizerGlyphRenderer()
: mRenderHost(),
  mOutputActor(),
  mAttached(false)
{
}

TextVisualizerGlyphRenderer::~TextVisualizerGlyphRenderer()
{
  Clear();
}

void TextVisualizerGlyphRenderer::Clear()
{
  DetachOutputFromHost();
  mOutputActor.Reset();
  mRenderHost.Reset();
  mAttached = false;
}

bool TextVisualizerGlyphRenderer::HasOutputActor() const
{
  return static_cast<bool>(mOutputActor);
}

Actor TextVisualizerGlyphRenderer::GetOutputActor() const
{
  return mOutputActor;
}

bool TextVisualizerGlyphRenderer::IsAttached() const
{
  return mAttached;
}

void TextVisualizerGlyphRenderer::SetRenderHost(Actor renderHost)
{
  if(!renderHost)
  {
    DetachOutputFromHost();
  }

  mRenderHost = renderHost;
}

Actor TextVisualizerGlyphRenderer::GetRenderHost() const
{
  return mRenderHost;
}

bool TextVisualizerGlyphRenderer::HasRenderHost() const
{
  return static_cast<bool>(mRenderHost);
}

void TextVisualizerGlyphRenderer::EnsureOutputActor()
{
  if(mOutputActor)
  {
    return;
  }

  mOutputActor = Actor::New();
  mOutputActor.SetProperty(Actor::Property::PARENT_ORIGIN, ParentOrigin::TOP_LEFT);
  mOutputActor.SetProperty(Actor::Property::PIVOT, Pivot::TOP_LEFT);
  mOutputActor.SetProperty(Actor::Property::POSITION, Vector3::ZERO);
  mOutputActor.SetProperty(Actor::Property::VISIBLE, true);
}

bool TextVisualizerGlyphRenderer::AttachOutputToHost()
{
  if(!mRenderHost)
  {
    return false;
  }

  EnsureOutputActor();
  if(!mOutputActor)
  {
    return false;
  }

  Actor outputParent = mOutputActor.GetParent();
  if(outputParent && outputParent != mRenderHost)
  {
    return false;
  }

  if(!outputParent)
  {
    mRenderHost.Add(mOutputActor);
  }

  mAttached = true;
  return true;
}

void TextVisualizerGlyphRenderer::DetachOutputFromHost()
{
  if(mOutputActor)
  {
    Actor parent = mOutputActor.GetParent();
    if(parent)
    {
      mOutputActor.Unparent();
    }
  }

  mAttached = false;
}

} // namespace Dali::Ui::Internal::TextVisualizer
