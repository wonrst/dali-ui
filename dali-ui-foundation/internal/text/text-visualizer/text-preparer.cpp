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
#include <dali-ui-foundation/internal/text/metrics.h>
#include <dali-ui-foundation/internal/text/multi-language-support.h>
#include <dali-ui-foundation/internal/text/segmentation.h>
#include <dali-ui-foundation/internal/text/shaper.h>
#include <dali-ui-foundation/internal/text/text-visualizer/text-preparer.h>
#include <dali-ui-foundation/internal/text/visual-model-impl.h>

namespace Dali::Ui::Internal::TextVisualizer
{
namespace
{
constexpr float FONT_SIZE_SCALE = 1.0f;

TextAbstraction::FontDescription CreateDefaultFontDescription(const Dali::String& fontFamily)
{
  TextAbstraction::FontDescription defaultFontDescription;

  if(!fontFamily.Empty())
  {
    defaultFontDescription.family = fontFamily.CStr();
  }

  return defaultFontDescription;
}

Text::PointSize26Dot6 GetDefaultPointSize(TextAbstraction::FontClient& fontClient, float fontSize)
{
  const float effectivePointSize = (fontSize > 0.0f) ? fontSize : TextAbstraction::FontClient::DEFAULT_POINT_SIZE;

  return static_cast<Text::PointSize26Dot6>(effectivePointSize * FONT_SIZE_SCALE *
                                            fontClient.GetNumberOfPointsPerOneUnitOfPointSize());
}
} // unnamed namespace

PreparedText TextPreparer::Prepare(const Input& input)
{
  PreparedText                       preparedText;
  Dali::Vector<Text::Character>      characters;
  Dali::Vector<Text::LineBreakInfo>  lineBreakInfo;
  Dali::Vector<Text::ParagraphRun>   paragraphInfo;
  Dali::Vector<Text::ScriptRun>      scriptRuns;
  Dali::Vector<Text::FontRun>        fontRuns;
  Dali::Vector<Text::GlyphInfo>      glyphs;
  Dali::Vector<Text::CharacterIndex> glyphToCharacterMap;
  Dali::Vector<Text::Length>         charactersPerGlyph;
  Dali::Vector<Text::GlyphIndex>     characterToGlyphTable;
  Dali::Vector<Text::Length>         glyphsPerCharacterTable;
  Dali::Vector<Text::GlyphIndex>     newParagraphGlyphs;

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

    Text::MultilanguageSupport multilanguageSupport = Text::MultilanguageSupport::Get();
    multilanguageSupport.SetScripts(characters, 0u, convertedCharacterCount, scriptRuns);

    TextAbstraction::FontClient                  fontClient = TextAbstraction::FontClient::Get();
    const Dali::Vector<Text::FontDescriptionRun> fontDescriptionRuns;
    const TextAbstraction::FontDescription       defaultFontDescription = CreateDefaultFontDescription(input.fontFamily);
    const Text::PointSize26Dot6                  defaultPointSize       = GetDefaultPointSize(fontClient, input.fontSize);

    multilanguageSupport.ValidateFonts(fontClient, characters, scriptRuns, fontDescriptionRuns,
                                       defaultFontDescription, defaultPointSize, FONT_SIZE_SCALE, 0u,
                                       convertedCharacterCount, fontRuns, nullptr);

    if(!scriptRuns.Empty() && !fontRuns.Empty())
    {
      TextAbstraction::Shaping shaping = TextAbstraction::Shaping::Get();
      Text::ShapeText(shaping, fontClient, characters, lineBreakInfo, scriptRuns, fontRuns, 0u, 0u,
                      convertedCharacterCount, glyphs, glyphToCharacterMap, charactersPerGlyph, newParagraphGlyphs);

      const Text::Length glyphCount = glyphs.Count();
      if(glyphCount > 0u)
      {
        Text::MetricsPtr metrics = Text::Metrics::New(fontClient);
        metrics->GetGlyphMetrics(glyphs.Begin(), glyphCount);

        for(Vector<Text::GlyphIndex>::ConstIterator it = newParagraphGlyphs.Begin(), endIt = newParagraphGlyphs.End();
            it != endIt; ++it)
        {
          const Text::GlyphIndex index = *it;
          if(index < glyphs.Count())
          {
            Text::GlyphInfo& glyph = glyphs[index];

            glyph.xBearing = 0.0f;
            glyph.width    = 0.0f;
            glyph.advance  = 0.0f;
          }
        }
      }

      Text::VisualModelPtr visualModel = Text::VisualModel::New();
      visualModel->mGlyphs             = glyphs;
      visualModel->mGlyphsToCharacters = glyphToCharacterMap;
      visualModel->mCharactersPerGlyph = charactersPerGlyph;
      visualModel->CreateGlyphsPerCharacterTable(0u, 0u, convertedCharacterCount);
      visualModel->CreateCharacterToGlyphTable(0u, 0u, convertedCharacterCount);

      glyphsPerCharacterTable = visualModel->mGlyphsPerCharacter;
      characterToGlyphTable   = visualModel->mCharactersToGlyph;
    }
  }

  preparedText.SetCharacters(characters);
  preparedText.SetLineBreakInfo(lineBreakInfo);
  preparedText.SetParagraphInfo(paragraphInfo);
  preparedText.SetScriptRuns(scriptRuns);
  preparedText.SetFontRuns(fontRuns);
  preparedText.SetGlyphs(glyphs);
  preparedText.SetGlyphToCharacterMap(glyphToCharacterMap);
  preparedText.SetCharactersPerGlyph(charactersPerGlyph);
  preparedText.SetCharacterToGlyphTable(characterToGlyphTable);
  preparedText.SetGlyphsPerCharacterTable(glyphsPerCharacterTable);
  preparedText.SetNewParagraphGlyphs(newParagraphGlyphs);
  preparedText.SetClusterCount(preparedText.GetCharacterCount());
  preparedText.SetPrepared(true);

  return preparedText;
}

} // namespace Dali::Ui::Internal::TextVisualizer
