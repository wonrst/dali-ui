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
  mCharacters(),
  mLineBreakInfo(),
  mParagraphInfo(),
  mScriptRuns(),
  mFontRuns(),
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

void PreparedText::SetCharacters(const Dali::Vector<Text::Character>& characters)
{
  mCharacters = characters;
}

const Dali::Vector<Text::Character>& PreparedText::GetCharacters() const
{
  return mCharacters;
}

uint32_t PreparedText::GetCharacterCount() const
{
  return static_cast<uint32_t>(mCharacters.Count());
}

void PreparedText::SetLineBreakInfo(const Dali::Vector<Text::LineBreakInfo>& lineBreakInfo)
{
  mLineBreakInfo = lineBreakInfo;
}

const Dali::Vector<Text::LineBreakInfo>& PreparedText::GetLineBreakInfo() const
{
  return mLineBreakInfo;
}

uint32_t PreparedText::GetLineBreakCount() const
{
  return static_cast<uint32_t>(mLineBreakInfo.Count());
}

void PreparedText::SetParagraphInfo(const Dali::Vector<Text::ParagraphRun>& paragraphInfo)
{
  mParagraphInfo = paragraphInfo;
}

const Dali::Vector<Text::ParagraphRun>& PreparedText::GetParagraphInfo() const
{
  return mParagraphInfo;
}

uint32_t PreparedText::GetParagraphCount() const
{
  return static_cast<uint32_t>(mParagraphInfo.Count());
}

void PreparedText::SetScriptRuns(const Dali::Vector<Text::ScriptRun>& scriptRuns)
{
  mScriptRuns = scriptRuns;
}

const Dali::Vector<Text::ScriptRun>& PreparedText::GetScriptRuns() const
{
  return mScriptRuns;
}

uint32_t PreparedText::GetScriptRunCount() const
{
  return static_cast<uint32_t>(mScriptRuns.Count());
}

void PreparedText::SetFontRuns(const Dali::Vector<Text::FontRun>& fontRuns)
{
  mFontRuns = fontRuns;
}

const Dali::Vector<Text::FontRun>& PreparedText::GetFontRuns() const
{
  return mFontRuns;
}

uint32_t PreparedText::GetFontRunCount() const
{
  return static_cast<uint32_t>(mFontRuns.Count());
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
  return mCharacters.Empty();
}

void PreparedText::Clear()
{
  mOriginalText.Clear();
  mFontFamily.Clear();
  mFontSize     = 0.0f;
  mClusterCount = 0u;
  mCharacters.Clear();
  mLineBreakInfo.Clear();
  mParagraphInfo.Clear();
  mScriptRuns.Clear();
  mFontRuns.Clear();
  mPrepared = false;
}

} // namespace Dali::Ui::Internal::TextVisualizer
