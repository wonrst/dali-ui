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
#include <dali-ui-foundation/internal/text/async-text/text-loading-task.h>

// EXTERNAL INCLUDES
#include <dali/devel-api/adaptor-framework/thread-settings.h>
#include <dali/integration-api/adaptor-framework/adaptor.h>
#include <dali/integration-api/debug.h>
#include <dali/public-api/signals/callback.h>

#include <limits>
#include <utility>

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/async-text/async-text-manager-impl.h> ///< To call AsyncTextManager::ReleaseLoader
#include <dali-ui-foundation/internal/text/async-text/async-text-manager.h>
#include <dali-ui-foundation/internal/text/text-trace.h>

namespace DALI_NAMESPACE
{
namespace Ui
{
namespace Internal
{
namespace
{
DALI_INIT_TRACE_FILTER(gTraceFilter, DALI_TRACE_TEXT_ASYNC_PERFORMANCE_MARKER, false);

} // namespace

TextLoadingTask::TextLoadingTask(const uint32_t id, Text::AsyncTextParameters&& parameters, CallbackBase* callback)
: AsyncTask(callback),
  mId(id),
  mParameters(std::move(parameters)),
  mRenderInfo(),
  mIsReady(false),
  mMutex()
{
}

TextLoadingTask::~TextLoadingTask()
{
  if(DALI_LIKELY(Dali::Adaptor::IsAvailable()))
  {
    // To avoid loader leaking. Never ever happend, but for safety.
    if(DALI_UNLIKELY(mLoader))
    {
      DALI_LOG_ERROR("Need to release loader!!");
      Text::Internal::AsyncTextManager::ReleaseLoaderToManager(nullptr, mLoader);
    }
  }
}

uint32_t TextLoadingTask::GetId()
{
  return mId;
}

void TextLoadingTask::SetLoader(Text::AsyncTextLoader& loader)
{
  mLoader = loader;

  if(DALI_LIKELY(!mIsReady && mLoader))
  {
    {
      Dali::Mutex::ScopedLock lock(mMutex);
      mIsReady = true;
    }
    NotifyToReady();
  }
}

void TextLoadingTask::Process()
{
  if(DALI_LIKELY(mId != 0u))
  {
    // Exclude scheduling, loader release and completion callbacks.
    DALI_TEXT_TRACE_SCOPE(gTraceFilter, "DALI_TEXT_ASYNC_PROCESS");
    if(mParameters.renderScale > 1.0f && mParameters.maxTextureSize > 0 &&
       mParameters.requestType <= Ui::Integration::Text::Async::RENDER_CONSTRAINT)
    {
      // Request dimensions are not RenderScale-scaled yet. Ignore unspecified
      // axes, including MAX_FLOAT; actual raster growth is checked by the loader.
      const double scale = static_cast<double>(mParameters.renderScale);
      const double limit = static_cast<double>(mParameters.maxTextureSize);
      if((mParameters.textWidth > 0.0f && mParameters.textWidth < std::numeric_limits<float>::max() && static_cast<double>(mParameters.textWidth) * scale > limit) ||
         (mParameters.textHeight > 0.0f && mParameters.textHeight < std::numeric_limits<float>::max() && static_cast<double>(mParameters.textHeight) * scale > limit))
      {
        mParameters.renderScale = 1.0f;
      }
    }
    if(mParameters.renderScale > 1.0f)
    {
      // Keep only the scalar inputs which layout may change. No text/style
      // snapshots are copied on the normal RenderScale path.
      const float width        = mParameters.textWidth;
      const float height       = mParameters.textHeight;
      const float fontSize     = mParameters.fontSize;
      const float minLineSize  = mParameters.minLineSize;
      const float originWidth  = mParameters.originWidth;
      const float originHeight = mParameters.originHeight;
      const int   marqueeGap   = mParameters.marqueeGap;
      const bool  marquee      = mParameters.isMarqueeEnabled;
      const bool  exceeded     = mParameters.isMarqueeMaxTextureExceeded;
      const bool  ellipsis     = mParameters.ellipsis;
      Load();
      // Render requests fallback before rasterization by changing the effective
      // scale and returning no pixels. Retry once, before publishing any result.
      if(mParameters.renderScale == 1.0f && !mRenderInfo.textPixelData)
      {
        mParameters.textWidth                   = width;
        mParameters.textHeight                  = height;
        mParameters.fontSize                    = fontSize;
        mParameters.minLineSize                 = minLineSize;
        mParameters.originWidth                 = originWidth;
        mParameters.originHeight                = originHeight;
        mParameters.marqueeGap                  = marqueeGap;
        mParameters.isMarqueeEnabled            = marquee;
        mParameters.isMarqueeMaxTextureExceeded = exceeded;
        mParameters.ellipsis                    = ellipsis;
        mParameters.renderScaleWidth            = 0.0f;
        mParameters.renderScaleHeight           = 0.0f;
        Load();
      }
    }
    else
    {
      Load();
    }
  }
  ReleaseLoader();
}

bool TextLoadingTask::IsReady()
{
  Dali::Mutex::ScopedLock lock(mMutex);
  return mIsReady;
}

void TextLoadingTask::Load()
{
  switch(mParameters.requestType)
  {
    case Ui::Integration::Text::Async::RENDER_FIXED_SIZE:
    case Ui::Integration::Text::Async::RENDER_FIXED_WIDTH:
    case Ui::Integration::Text::Async::RENDER_FIXED_HEIGHT:
    case Ui::Integration::Text::Async::RENDER_CONSTRAINT:
    {
      // To avoid duplicate calculation, we can skip Initialize and Update.
      Size naturalSize       = Size::ZERO;
      bool cachedNaturalSize = false;

      if(mParameters.renderScale > 1.0f)
      {
        naturalSize = mLoader.SetupRenderScale(mParameters, cachedNaturalSize);
      }

      if(!mParameters.suppressAutoMarquee && mParameters.marqueeTriggerPolicy == Text::MarqueeTriggerPolicy::ON_OVERFLOW)
      {
        if(mParameters.marqueeOrientation == Text::MarqueeOrientation::HORIZONTAL)
        {
          if(mParameters.isMultiLine)
          {
            DALI_LOG_DEBUG_INFO("Marquee Horizontal: valid only for single-line text\n");
            mRenderInfo = mLoader.RenderText(mParameters, cachedNaturalSize, naturalSize);
          }
          else
          {
            if(!cachedNaturalSize)
            {
              naturalSize       = mLoader.ComputeNaturalSize(mParameters);
              cachedNaturalSize = true;
            }
            if(mParameters.textWidth < naturalSize.width)
            {
              mParameters.isMarqueeEnabled = true;
              mRenderInfo                  = mLoader.RenderMarquee(mParameters, cachedNaturalSize, naturalSize);
            }
            else
            {
              mRenderInfo = mLoader.RenderText(mParameters, cachedNaturalSize, naturalSize);
            }
          }
        }
        else // MarqueeOrientation::VERTICAL
        {
          if(!mParameters.isMultiLine)
          {
            DALI_LOG_DEBUG_INFO("Marquee Vertical: valid only for multi-line text\n");
            mRenderInfo = mLoader.RenderText(mParameters, cachedNaturalSize, naturalSize);
          }
          else
          {
            const float textHeight = mLoader.ComputeHeightForWidth(mParameters, mParameters.textWidth, cachedNaturalSize);
            if(mParameters.textHeight < textHeight)
            {
              mParameters.isMarqueeEnabled = true;
              mRenderInfo                  = mLoader.RenderMarquee(mParameters, true, naturalSize);
            }
            else
            {
              mRenderInfo = mLoader.RenderText(mParameters, cachedNaturalSize, naturalSize);
            }
          }
        }
      }
      else if(mParameters.isMarqueeEnabled &&
              ((!mParameters.isMultiLine && mParameters.marqueeOrientation == Text::MarqueeOrientation::HORIZONTAL) ||
               (mParameters.isMultiLine && mParameters.marqueeOrientation == Text::MarqueeOrientation::VERTICAL)))
      {
        mRenderInfo = mLoader.RenderMarquee(mParameters, cachedNaturalSize, naturalSize);
      }
      else if(mParameters.isTextFitEnabled || mParameters.isTextFitCandidatesEnabled)
      {
        mRenderInfo = mLoader.RenderTextFit(mParameters, cachedNaturalSize, naturalSize);
      }
      else
      {
        mRenderInfo = mLoader.RenderText(mParameters, cachedNaturalSize, naturalSize);
      }
      break;
    }
    case Ui::Integration::Text::Async::COMPUTE_NATURAL_SIZE:
    {
      mRenderInfo = mLoader.GetNaturalSize(mParameters);
      break;
    }
    case Ui::Integration::Text::Async::COMPUTE_HEIGHT_FOR_WIDTH:
    {
      mRenderInfo = mLoader.GetHeightForWidth(mParameters);
      break;
    }
    default:
    {
      DALI_LOG_ERROR("Unexpected request type recieved : %d\n", mParameters.requestType);
      break;
    }
  }
}

void TextLoadingTask::ReleaseLoader()
{
  // Release all local varaibles before execute callback.
  if(DALI_LIKELY(mLoader))
  {
    auto loader = std::move(mLoader);
    Text::Internal::AsyncTextManager::ReleaseLoaderToManager(this, loader);
  }
}

} // namespace Internal

} // namespace Ui

} //namespace DALI_NAMESPACE
