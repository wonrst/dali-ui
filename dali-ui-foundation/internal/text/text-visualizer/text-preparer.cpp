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
#include <dali-ui-foundation/internal/text/character-set-conversion.h>
#include <dali-ui-foundation/internal/text/logical-model-impl.h>
#include <dali-ui-foundation/internal/text/segmentation.h>
#include <dali-ui-foundation/internal/text/text-visualizer/text-preparer.h>

namespace Dali::Ui::Internal::TextVisualizer
{

PreparedText TextPreparer::Prepare(const Input& input)
{
  PreparedText                      preparedText;
  Dali::Vector<Text::Character>     characters;
  Dali::Vector<Text::LineBreakInfo> lineBreakInfo;
  Dali::Vector<Text::ParagraphRun>  paragraphInfo;

  preparedText.SetOriginalText(input.text);
  preparedText.SetFontFamily(input.fontFamily);
  preparedText.SetFontSize(input.fontSize);

  if(!input.text.Empty())
  {
    const auto* const utf8               = reinterpret_cast<const uint8_t*>(input.text.CStr());
    const uint32_t    utf8ByteCount      = input.text.Size();
    const uint32_t    numberOfCharacters = Text::GetNumberOfUtf8Characters(utf8, utf8ByteCount);

    characters.Resize(numberOfCharacters);
    const uint32_t convertedCharacterCount = Text::Utf8ToUtf32(utf8, utf8ByteCount, characters.Begin());
    characters.Resize(convertedCharacterCount);

    lineBreakInfo.Resize(convertedCharacterCount, TextAbstraction::LINE_NO_BREAK);
    TextAbstraction::Segmentation segmentation = TextAbstraction::Segmentation::Get();
    Text::SetLineBreakInfo(segmentation, characters, 0u, convertedCharacterCount, lineBreakInfo);

    Text::LogicalModelPtr logicalModel = Text::LogicalModel::New();
    logicalModel->mLineBreakInfo       = lineBreakInfo;
    logicalModel->CreateParagraphInfo(0u, convertedCharacterCount);
    paragraphInfo = logicalModel->mParagraphInfo;
  }

  preparedText.SetCharacters(characters);
  preparedText.SetLineBreakInfo(lineBreakInfo);
  preparedText.SetParagraphInfo(paragraphInfo);
  preparedText.SetClusterCount(preparedText.GetCharacterCount());
  preparedText.SetPrepared(true);

  return preparedText;
}

} // namespace Dali::Ui::Internal::TextVisualizer
