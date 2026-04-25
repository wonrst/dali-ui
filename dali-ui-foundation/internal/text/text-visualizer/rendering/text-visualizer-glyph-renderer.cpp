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

// EXTERNAL INCLUDES
#include <dali/integration-api/string-utils.h>
#include <dali/public-api/object/property-map.h>
#include <dali/public-api/rendering/texture-set.h>

// INTERNAL INCLUDES
#include <dali-ui-foundation/devel-api/view-depth-index-ranges.h>
#include <dali-ui-foundation/internal/graphics/builtin-shader-extern-gen.h>
#include <dali-ui-foundation/internal/text/rendering/atlas/atlas-glyph-manager.h>
#include <dali-ui-foundation/internal/text/rendering/atlas/atlas-manager.h>
#include <dali-ui-foundation/internal/text/rendering/atlas/atlas-mesh-factory.h>
#include <dali-ui-foundation/internal/text/text-visualizer/atlas-view-adapter.h>
#include <dali-ui-foundation/internal/text/text-visualizer/layout-types.h>
#include <dali-ui-foundation/internal/text/text-visualizer/prepared-text.h>

namespace Dali::Ui::Internal::TextVisualizer
{
namespace
{
struct PendingMesh
{
  uint32_t             atlasId{0u};
  Dali::Ui::AtlasManager::Mesh2D mesh;
};

Property::Map CreateQuadVertexFormat()
{
  Property::Map format;
  format["aPosition"] = Property::VECTOR2;
  format["aTexCoord"] = Property::VECTOR2;
  format["aColor"]    = Property::VECTOR4;
  return format;
}

Vector2 GetMeshActorSize(const LayoutResult& layoutResult, const AtlasViewAdapter& adapter)
{
  Vector2 actorSize = adapter.GetControlSize();
  if(actorSize.x <= 0.0f)
  {
    actorSize.x = layoutResult.width;
  }
  if(actorSize.y <= 0.0f)
  {
    actorSize.y = layoutResult.height;
  }
  return actorSize;
}

PendingMesh* FindPendingMesh(std::vector<PendingMesh>& pendingMeshes, uint32_t atlasId)
{
  for(PendingMesh& pendingMesh : pendingMeshes)
  {
    if(pendingMesh.atlasId == atlasId)
    {
      return &pendingMesh;
    }
  }

  return nullptr;
}
} // unnamed namespace

TextVisualizerGlyphRenderer::TextVisualizerGlyphRenderer()
: mRenderHost(),
  mOutputActor(),
  mAttached(false),
  mMeshRecords(),
  mShaderL8(),
  mShaderRgba()
{
}

TextVisualizerGlyphRenderer::~TextVisualizerGlyphRenderer()
{
  Clear();
}

void TextVisualizerGlyphRenderer::Clear()
{
  ClearMeshes();
  DetachOutputFromHost();
  mOutputActor.Reset();
  mRenderHost.Reset();
  mAttached = false;
  mShaderL8.Reset();
  mShaderRgba.Reset();
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

void TextVisualizerGlyphRenderer::ClearMeshes()
{
  for(MeshRecord& record : mMeshRecords)
  {
    if(record.actor)
    {
      Actor parent = record.actor.GetParent();
      if(parent)
      {
        record.actor.Unparent();
      }
    }

    record.actor.Reset();
    record.renderer.Reset();
    record.geometry.Reset();
    record.vertexBuffer.Reset();
    record.vertexCount = 0u;
    record.indexCount  = 0u;
    record.atlasId     = 0u;
  }

  mMeshRecords.clear();
}

bool TextVisualizerGlyphRenderer::HasMeshRecords() const
{
  return !mMeshRecords.empty();
}

uint32_t TextVisualizerGlyphRenderer::GetMeshRecordCount() const
{
  return static_cast<uint32_t>(mMeshRecords.size());
}

bool TextVisualizerGlyphRenderer::Render(const PreparedText& preparedText, const LayoutResult& layoutResult, const AtlasViewAdapter& adapter, const Vector4& textColor)
{
  ClearMeshes();

  if(preparedText.Empty() ||
     !preparedText.HasGlyphData() ||
     !preparedText.HasGlyphMetrics() ||
     layoutResult.glyphPlacements.Empty() ||
     !adapter.HasRenderableGlyphs() ||
     (0u == adapter.GetRendererGlyphPositionCacheCount()))
  {
    return false;
  }

  Dali::Ui::AtlasGlyphManager glyphManager = Dali::Ui::AtlasGlyphManager::Get();
  if(!glyphManager)
  {
    return false;
  }

  std::vector<PendingMesh> pendingMeshes;
  pendingMeshes.reserve(adapter.GetRenderableGlyphCount());

  for(uint32_t placementIndex = 0u, placementCount = adapter.GetGlyphPlacementCount(); placementIndex < placementCount; ++placementIndex)
  {
    GlyphPlacement  placement;
    Text::GlyphInfo glyphInfo;
    Vector2         position;

    if(!adapter.GetGlyphPlacement(placementIndex, placement) ||
       !adapter.GetGlyphInfo(placement.glyphIndex, glyphInfo) ||
       !adapter.GetRendererGlyphPosition(placementIndex, position))
    {
      return false;
    }

    Dali::Ui::AtlasGlyphManager::GlyphStyle style;
    style.outline  = 0u;
    style.isItalic = glyphInfo.isItalicRequired;
    style.isBold   = glyphInfo.isBoldRequired;

    Dali::Ui::AtlasManager::AtlasSlot slot;
    if(!glyphManager.IsCached(glyphInfo.fontId, glyphInfo.index, style, slot) ||
       (0u == slot.mImageId) ||
       (0u == slot.mAtlasId))
    {
      return false;
    }

    Dali::Ui::AtlasManager::Mesh2D glyphMesh;
    glyphManager.GenerateMeshData(slot.mImageId, position, glyphMesh);
    if(glyphMesh.mVertices.Empty() || glyphMesh.mIndices.Empty())
    {
      return false;
    }

    for(Vector<Dali::Ui::AtlasManager::Vertex2D>::Iterator it = glyphMesh.mVertices.Begin(), endIt = glyphMesh.mVertices.End(); it != endIt; ++it)
    {
      it->mColor = textColor;
    }

    PendingMesh* pendingMesh = FindPendingMesh(pendingMeshes, slot.mAtlasId);
    if(nullptr != pendingMesh)
    {
      Internal::AtlasMeshFactory::AppendMesh(pendingMesh->mesh, glyphMesh);
    }
    else
    {
      PendingMesh newPendingMesh;
      newPendingMesh.atlasId = slot.mAtlasId;
      newPendingMesh.mesh    = glyphMesh;
      pendingMeshes.push_back(newPendingMesh);
    }
  }

  if(pendingMeshes.empty())
  {
    return false;
  }

  EnsureOutputActor();
  if(!mOutputActor)
  {
    return false;
  }

  const Vector2 actorSize    = GetMeshActorSize(layoutResult, adapter);
  Property::Map vertexFormat = CreateQuadVertexFormat();

  for(const PendingMesh& pendingMesh : pendingMeshes)
  {
    if(pendingMesh.mesh.mVertices.Empty() || pendingMesh.mesh.mIndices.Empty())
    {
      ClearMeshes();
      return false;
    }

    VertexBuffer vertexBuffer = VertexBuffer::New(vertexFormat);
    vertexBuffer.SetData(const_cast<Dali::Ui::AtlasManager::Vertex2D*>(pendingMesh.mesh.mVertices.Begin()), pendingMesh.mesh.mVertices.Size());

    Geometry geometry = Geometry::New();
    geometry.AddVertexBuffer(vertexBuffer);
    geometry.SetIndexBuffer(pendingMesh.mesh.mIndices.Begin(), pendingMesh.mesh.mIndices.Size());

    TextureSet textureSet = glyphManager.GetTextures(pendingMesh.atlasId);
    if(!textureSet)
    {
      ClearMeshes();
      return false;
    }

    const bool isColorShader = Pixel::BGRA8888 == glyphManager.GetPixelFormat(pendingMesh.atlasId);
    Shader     shader;
    if(isColorShader)
    {
      if(!mShaderRgba)
      {
        mShaderRgba = Shader::New(Integration::ToDaliStringView(SHADER_TEXT_ATLAS_SHADER_VERT),
                                  Integration::ToDaliStringView(SHADER_TEXT_ATLAS_RGBA_SHADER_FRAG),
                                  static_cast<Shader::Hint::Value>(Shader::Hint::FILE_CACHE_SUPPORT | Shader::Hint::INTERNAL),
                                  "TEXT_VISUALIZER_GLYPH_RENDERER_RGBA");
      }
      shader = mShaderRgba;
    }
    else
    {
      if(!mShaderL8)
      {
        mShaderL8 = Shader::New(Integration::ToDaliStringView(SHADER_TEXT_ATLAS_SHADER_VERT),
                                Integration::ToDaliStringView(SHADER_TEXT_ATLAS_L8_SHADER_FRAG),
                                static_cast<Shader::Hint::Value>(Shader::Hint::FILE_CACHE_SUPPORT | Shader::Hint::INTERNAL),
                                "TEXT_VISUALIZER_GLYPH_RENDERER_L8");
      }
      shader = mShaderL8;
    }

    shader.RegisterProperty("textColorAnimatable", Vector4(1.0f, 1.0f, 1.0f, 1.0f));

    Renderer renderer = Renderer::New(geometry, shader);
    renderer.SetTextures(textureSet);
    renderer.SetProperty(Renderer::Property::BLEND_MODE, BlendMode::ON);
    renderer.SetProperty(Renderer::Property::DEPTH_INDEX, DepthIndex::CONTENT);

    Actor meshActor = Actor::New();
    meshActor.AddRenderer(renderer);
    meshActor.SetProperty(Actor::Property::PARENT_ORIGIN, ParentOrigin::TOP_LEFT);
    meshActor.SetProperty(Actor::Property::PIVOT, Pivot::TOP_LEFT);
    meshActor.SetProperty(Actor::Property::SIZE, Vector3(actorSize.x, actorSize.y, 0.0f));
    meshActor.SetProperty(Actor::Property::COLOR_MODE, USE_OWN_MULTIPLY_PARENT_COLOR);

    mOutputActor.Add(meshActor);

    MeshRecord meshRecord;
    meshRecord.atlasId      = pendingMesh.atlasId;
    meshRecord.actor        = meshActor;
    meshRecord.renderer     = renderer;
    meshRecord.geometry     = geometry;
    meshRecord.vertexBuffer = vertexBuffer;
    meshRecord.vertexCount  = pendingMesh.mesh.mVertices.Size();
    meshRecord.indexCount   = pendingMesh.mesh.mIndices.Size();
    mMeshRecords.push_back(meshRecord);
  }

  return HasMeshRecords();
}

} // namespace Dali::Ui::Internal::TextVisualizer
