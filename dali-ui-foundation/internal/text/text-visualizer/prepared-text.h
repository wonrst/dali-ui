#ifndef DALI_UI_TEXT_VISUALIZER_PREPARED_TEXT_H
#define DALI_UI_TEXT_VISUALIZER_PREPARED_TEXT_H

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
#include <cstdint>

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/font-run.h>
#include <dali-ui-foundation/internal/text/paragraph-run.h>
#include <dali-ui-foundation/internal/text/script-run.h>
#include <dali-ui-foundation/internal/text/text-definitions.h>
#include <dali/public-api/common/dali-string.h>
#include <dali/public-api/common/dali-vector.h>

namespace Dali::Ui::Internal::TextVisualizer
{
/**
 * @brief Prepared text cache for TextVisualizer.
 *
 * This stores stable prepare-stage data up to script/font validation.
 * Actual shaping, glyph metrics and cluster mapping are connected later.
 */
class PreparedText
{
public:
  PreparedText();

  void                SetOriginalText(const Dali::String& text);
  const Dali::String& GetOriginalText() const;

  void                SetFontFamily(const Dali::String& fontFamily);
  const Dali::String& GetFontFamily() const;

  void  SetFontSize(float fontSize);
  float GetFontSize() const;

  void     SetClusterCount(uint32_t clusterCount);
  uint32_t GetClusterCount() const;

  void                                 SetCharacters(const Dali::Vector<Text::Character>& characters);
  const Dali::Vector<Text::Character>& GetCharacters() const;
  uint32_t                             GetCharacterCount() const;

  void                                     SetLineBreakInfo(const Dali::Vector<Text::LineBreakInfo>& lineBreakInfo);
  const Dali::Vector<Text::LineBreakInfo>& GetLineBreakInfo() const;
  uint32_t                                 GetLineBreakCount() const;

  void                                    SetParagraphInfo(const Dali::Vector<Text::ParagraphRun>& paragraphInfo);
  const Dali::Vector<Text::ParagraphRun>& GetParagraphInfo() const;
  uint32_t                                GetParagraphCount() const;

  void                                 SetScriptRuns(const Dali::Vector<Text::ScriptRun>& scriptRuns);
  const Dali::Vector<Text::ScriptRun>& GetScriptRuns() const;
  uint32_t                             GetScriptRunCount() const;

  void                               SetFontRuns(const Dali::Vector<Text::FontRun>& fontRuns);
  const Dali::Vector<Text::FontRun>& GetFontRuns() const;
  uint32_t                           GetFontRunCount() const;

  void SetPrepared(bool prepared);
  bool IsPrepared() const;

  bool Empty() const;
  void Clear();

private:
  Dali::String                      mOriginalText;
  Dali::String                      mFontFamily;
  float                             mFontSize;
  uint32_t                          mClusterCount;
  Dali::Vector<Text::Character>     mCharacters;
  Dali::Vector<Text::LineBreakInfo> mLineBreakInfo;
  Dali::Vector<Text::ParagraphRun>  mParagraphInfo;
  Dali::Vector<Text::ScriptRun>     mScriptRuns;
  Dali::Vector<Text::FontRun>       mFontRuns;
  bool                              mPrepared;
};

} // namespace Dali::Ui::Internal::TextVisualizer

#endif // DALI_UI_TEXT_VISUALIZER_PREPARED_TEXT_H
