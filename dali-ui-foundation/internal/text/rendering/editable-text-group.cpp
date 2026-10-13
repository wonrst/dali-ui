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
#include <dali-ui-foundation/internal/text/rendering/editable-text-group.h>

// EXTERNAL INCLUDES
#include <dali/devel-api/adaptor-framework/image-loading-devel.h>
#include <dali/integration-api/adaptor-framework/adaptor.h>
#include <dali/integration-api/adaptor-framework/scene-holder.h>
#include <dali/integration-api/string-utils.h>
#include <dali/public-api/actors/custom-actor-impl.h>
#include <dali/public-api/actors/custom-actor.h>
#include <dali/public-api/animation/constraints.h>
#include <dali/public-api/object/weak-handle.h>
#include <dali/public-api/rendering/frame-buffer.h>
#include <dali/public-api/rendering/geometry.h>
#include <dali/public-api/rendering/renderer.h>
#include <dali/public-api/rendering/texture-set.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

// INTERNAL INCLUDES
#include <dali-ui-foundation/integration-api/view-depth-index-ranges.h>
#include <dali-ui-foundation/internal/graphics/builtin-shader-extern-gen.h>
#include <dali-ui-foundation/internal/text/rendering/text-renderer.h>

namespace DALI_NAMESPACE::Ui::Text
{
namespace
{
class GroupSource : public CustomActorImpl
{
public:
  void SetTask(RenderTask task)
  {
    if(mTask.GetHandle())
    {
      UnregisterOffScreenRenderableType(OffScreenRenderable::Type::FORWARD);
    }
    mTask = WeakHandle<RenderTask>(task);
    if(task)
    {
      RegisterOffScreenRenderableType(OffScreenRenderable::Type::FORWARD);
    }
  }

  void GetOffScreenRenderTasks(Dali::Vector<RenderTask>& tasks, bool isForward) override
  {
    auto task = mTask.GetHandle();
    if(isForward && task)
    {
      tasks.PushBack(task);
    }
  }

  void OnSceneConnection(int32_t) override
  {
  }
  void OnSceneDisconnection() override
  {
  }
  void OnChildAdd(Actor&) override
  {
  }
  void OnChildRemove(Actor&) override
  {
  }
  void OnSizeSet(const Vector3&) override
  {
  }
  void OnSizeAnimation(Animation&, const Vector3&) override
  {
  }

private:
  WeakHandle<RenderTask> mTask;
};
} // namespace

bool EditableTextGroup::RequiresComposition(const Renderer& renderer, bool background, bool selection)
{
  return background || selection || renderer.RequiresGroupComposition();
}

EditableTextGroup::EditableTextGroup(Actor viewport)
: mViewport(viewport),
  mSource(CustomActor(*new GroupSource())),
  mOutput(Actor::New()),
  mSize(Vector2::ZERO)
{
  for(Actor actor : {mSource, mOutput})
  {
    actor.SetProperty(Actor::Property::PARENT_ORIGIN, ParentOrigin::TOP_LEFT);
    actor.SetProperty(Actor::Property::PIVOT, Pivot::TOP_LEFT);
    mViewport.Add(actor);
  }
  mSource.SetProperty(Actor::Property::COLOR_MODE, USE_OWN_MULTIPLY_PARENT_COLOR);
  mOutput.SetProperty(Actor::Property::VISIBLE, false);
  mOutput.SetProperty(Actor::Property::COLOR_MODE, USE_OWN_MULTIPLY_PARENT_COLOR);
  mViewport.SceneConnectedSignal().Connect(this, &EditableTextGroup::OnScene);
  mViewport.SceneDisconnectedSignal().Connect(this, &EditableTextGroup::OffScene);
}

EditableTextGroup::~EditableTextGroup()
{
  // Controls retained by focus/selection state may outlive adaptor shutdown.
  if(!Dali::Adaptor::IsAvailable())
  {
    return;
  }
  ReleaseTarget();
  if(mBackgroundPosition)
  {
    mBackgroundPosition.Remove();
  }
  while(mSource.GetChildCount())
  {
    Actor child = mSource.GetChildAt(0u);
    mViewport.Add(child);
    child.LowerBelow(mOutput);
  }
  mSource.Unparent();
  mOutput.Unparent();
}

void EditableTextGroup::Update(Actor text, Actor background, Actor replacements, Actor cursor)
{
  Actor highlight = mViewport.FindChildByName("HighlightActor");
  // Keep the intervening selection highlight between background and text paint.
  // Caret and handles are not part of the text composition.
  auto add = [this](Actor actor)
  {
    if(actor && actor.GetParent() != mSource)
    {
      mSource.Add(actor);
    }
  };
  if(background)
  {
    add(background);
    background.SetProperty(Actor::Property::POSITION, text.GetProperty<Vector3>(Actor::Property::POSITION));
    background.LowerToBottom();
  }
  if(highlight)
  {
    add(highlight);
    if(background)
    {
      highlight.RaiseAbove(background);
    }
    else
    {
      highlight.LowerToBottom();
    }
  }
  if(background != mBackground)
  {
    if(mBackgroundPosition)
    {
      mBackgroundPosition.Remove();
      mBackgroundPosition.Reset();
    }
    mBackground = background;
    if(background && text)
    {
      mBackgroundPosition = Constraint::New<Vector3>(background, Actor::Property::POSITION, EqualToConstraint());
      mBackgroundPosition.AddSource(Source(text, Actor::Property::POSITION));
      mBackgroundPosition.Apply();
    }
  }
  if(text)
  {
    add(text);
  }
  if(replacements)
  {
    add(replacements);
    if(text)
    {
      replacements.LowerBelow(text);
    }
  }
  mOutput.RaiseToTop();
  if(cursor)
  {
    cursor.RaiseToTop();
  }
  UpdateTarget();
}

void EditableTextGroup::OnScene(Actor)
{
  UpdateTarget();
}

void EditableTextGroup::OffScene(Actor)
{
  ReleaseTarget();
}

void EditableTextGroup::ReleaseTarget()
{
  static_cast<GroupSource&>(CustomActor::DownCast(mSource).GetImplementation()).SetTask(RenderTask());
  mSource.SetProperty(Actor::Property::COLOR_MODE, USE_OWN_MULTIPLY_PARENT_COLOR);
  mOutput.SetProperty(Actor::Property::VISIBLE, false);
  if(mTask)
  {
    mTasks.RemoveTask(mTask);
    mTask.Reset();
    mTasks.Reset();
  }
  if(mCamera)
  {
    mCamera.Unparent();
    mCamera.Reset();
  }
  while(mOutput.GetRendererCount())
  {
    mOutput.RemoveRenderer(0u);
  }
  mSize = Vector2::ZERO;
}

void EditableTextGroup::UpdateTarget()
{
  if(!mViewport.IsConnectedToScene())
  {
    return;
  }
  const Vector2 size = mViewport.GetProperty<Vector3>(Actor::Property::SIZE).GetVectorXY();
  if(!std::isfinite(size.x) || !std::isfinite(size.y) || size.x <= 0.0f || size.y <= 0.0f)
  {
    ReleaseTarget();
    return;
  }
  const double maximum     = std::min(static_cast<double>(Dali::GetMaxTextureSize()),
                                      static_cast<double>(std::numeric_limits<uint16_t>::max() - 1u));
  const double pixelWidth  = std::ceil(static_cast<double>(size.x));
  const double pixelHeight = std::ceil(static_cast<double>(size.y));
  if(maximum < 1.0 || pixelWidth > maximum || pixelHeight > maximum)
  {
    // Keep paint visible through the direct atlas path when capture is unsupported.
    ReleaseTarget();
    return;
  }
  if(mTask && size == mSize)
  {
    return;
  }
  if(mTask && pixelWidth == std::ceil(static_cast<double>(mSize.x)) &&
     pixelHeight == std::ceil(static_cast<double>(mSize.y)))
  {
    mSize = size;
    mSource.SetProperty(Actor::Property::SIZE, size);
    mOutput.SetProperty(Actor::Property::SIZE, size);
    mCamera.SetOrthographicProjection(size);
    const float cameraZ = mCamera.GetProperty<Vector3>(Actor::Property::POSITION).z;
    mCamera.SetProperty(Actor::Property::POSITION, Vector3(size.x * 0.5f, size.y * 0.5f, cameraZ));
    return;
  }
  ReleaseTarget();
  auto scene = Dali::Integration::SceneHolder::Get(mViewport);
  if(!scene)
  {
    return;
  }
  const auto  width   = static_cast<uint32_t>(pixelWidth);
  const auto  height  = static_cast<uint32_t>(pixelHeight);
  Texture     texture = Texture::New(TextureType::TEXTURE_2D, Pixel::RGBA8888, width, height);
  FrameBuffer target  = FrameBuffer::New(width, height, FrameBuffer::Attachment::NONE);
  if(!texture || !target)
  {
    return;
  }
  target.AttachColorTexture(texture);

  Property::Map format;
  format["aPosition"]      = Property::VECTOR2;
  VertexBuffer vertices    = VertexBuffer::New(format);
  Vector2      positions[] = {Vector2(-0.5f, -0.5f), Vector2(0.5f, -0.5f), Vector2(-0.5f, 0.5f), Vector2(0.5f, 0.5f)};
  vertices.SetData(positions, 4u);
  Geometry geometry = Geometry::New();
  geometry.AddVertexBuffer(vertices);
  geometry.SetType(Geometry::TRIANGLE_STRIP);
  Shader         shader   = Shader::New(Dali::Integration::ToDaliStringView(SHADER_TEXT_EDITABLE_GROUP_VERT),
                                        Dali::Integration::ToDaliStringView(SHADER_TEXT_EDITABLE_GROUP_FRAG),
                                        static_cast<Shader::Hint::Value>(Shader::Hint::FILE_CACHE_SUPPORT | Shader::Hint::INTERNAL),
                                        "TEXT_EDITABLE_GROUP");
  Dali::Renderer renderer = Dali::Renderer::New(geometry, shader);
  TextureSet     textures = TextureSet::New();
  textures.SetTexture(0u, texture);
  renderer.SetTextures(textures);
  renderer.SetProperty(Dali::Renderer::Property::BLEND_PRE_MULTIPLIED_ALPHA, true);
  renderer.SetProperty(Dali::Renderer::Property::BLEND_MODE, BlendMode::ON);
  renderer.SetProperty(Dali::Renderer::Property::DEPTH_INDEX, Ui::Integration::DepthIndex::CONTENT);
  // Publish the output before making its source exclusive.
  mOutput.AddRenderer(renderer);
  mSize = size;
  mSource.SetProperty(Actor::Property::SIZE, size);
  mOutput.SetProperty(Actor::Property::SIZE, size);
  mCamera = CameraActor::New(size);
  mCamera.SetOrthographicProjection(size);
  mCamera.SetProperty(CameraActor::Property::INVERT_Y_AXIS, true);
  mCamera.SetProperty(Actor::Property::PARENT_ORIGIN, ParentOrigin::TOP_LEFT);
  const float cameraZ = mCamera.GetProperty<Vector3>(Actor::Property::POSITION).z;
  mCamera.SetProperty(Actor::Property::POSITION, Vector3(size.x * 0.5f, size.y * 0.5f, cameraZ));
  mViewport.Add(mCamera);

  mTasks = scene.GetRenderTaskList();
  mTask  = mTasks.CreateTask();
  mTask.SetSourceActor(mSource);
  mTask.SetCameraActor(mCamera);
  mTask.SetInputEnabled(false);
  mTask.SetFrameBuffer(target);
  mTask.SetClearEnabled(true);
  mTask.SetClearColor(Color::TRANSPARENT);
  mTask.SetExclusive(true);
  static_cast<GroupSource&>(CustomActor::DownCast(mSource).GetImplementation()).SetTask(mTask);
  mSource.SetProperty(Actor::Property::COLOR_MODE, USE_OWN_COLOR);
  mOutput.SetProperty(Actor::Property::VISIBLE, true);
}
} // namespace DALI_NAMESPACE::Ui::Text
