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
#include <dali-ui-foundation/internal/text/text-visualizer/prepared-text.h>

namespace Dali::Ui::Internal::TextVisualizer
{

PreparedText::PreparedText()
: mOriginalText(),
  mFontFamily(),
  mFontSize(0.0f),
  mClusterCount(0u),
  mPrepared(false)
{
}

void PreparedText::SetOriginalText(const Dali::String& text)
{
  mOriginalText = text;
}

const Dali::String& PreparedText::GetOriginalText() const
{
  return mOriginalText;
}

void PreparedText::SetFontFamily(const Dali::String& fontFamily)
{
  mFontFamily = fontFamily;
}

const Dali::String& PreparedText::GetFontFamily() const
{
  return mFontFamily;
}

void PreparedText::SetFontSize(float fontSize)
{
  mFontSize = fontSize;
}

float PreparedText::GetFontSize() const
{
  return mFontSize;
}

void PreparedText::SetClusterCount(uint32_t clusterCount)
{
  mClusterCount = clusterCount;
}

uint32_t PreparedText::GetClusterCount() const
{
  return mClusterCount;
}

void PreparedText::SetPrepared(bool prepared)
{
  mPrepared = prepared;
}

bool PreparedText::IsPrepared() const
{
  return mPrepared;
}

bool PreparedText::Empty() const
{
  return mOriginalText.Empty();
}

void PreparedText::Clear()
{
  mOriginalText.Clear();
  mFontFamily.Clear();
  mFontSize     = 0.0f;
  mClusterCount = 0u;
  mPrepared     = false;
}

} // namespace Dali::Ui::Internal::TextVisualizer
