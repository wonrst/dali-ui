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
#include <algorithm>

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

bool IsNewParagraphGlyph(Text::GlyphIndex glyphIndex, const Dali::Vector<Text::GlyphIndex>& newParagraphGlyphs)
{
  for(Vector<Text::GlyphIndex>::ConstIterator it = newParagraphGlyphs.Begin(), endIt = newParagraphGlyphs.End(); it != endIt; ++it)
  {
    if(*it == glyphIndex)
    {
      return true;
    }
  }

  return false;
}

PreparedText::LineMetrics CalculateLineMetrics(const Dali::Vector<Text::GlyphInfo>&  glyphs,
                                               const Dali::Vector<Text::GlyphIndex>& newParagraphGlyphs,
                                               float                                 fontSize)
{
  PreparedText::LineMetrics lineMetrics;

  bool hasMeasuredGlyph = false;
  for(uint32_t glyphIndex = 0u; glyphIndex < glyphs.Count(); ++glyphIndex)
  {
    if(IsNewParagraphGlyph(glyphIndex, newParagraphGlyphs))
    {
      continue;
    }

    const Text::GlyphInfo& glyph = glyphs[glyphIndex];
    if((glyph.advance <= 0.0f) && (glyph.width <= 0.0f) && (glyph.height <= 0.0f))
    {
      continue;
    }

    lineMetrics.ascender  = std::max(lineMetrics.ascender, glyph.yBearing);
    lineMetrics.descender = std::max(lineMetrics.descender, std::max(0.0f, glyph.height - glyph.yBearing));
    hasMeasuredGlyph      = true;
  }

  const float fallbackLineGap    = std::max(fontSize * 0.2f, 0.0f);
  const float fallbackLineHeight = (fontSize > 0.0f) ? (fontSize * 1.2f) : 0.0f;

  lineMetrics.lineGap = fallbackLineGap;

  if(hasMeasuredGlyph)
  {
    lineMetrics.naturalLineHeight = lineMetrics.ascender + lineMetrics.descender + lineMetrics.lineGap;
  }
  else if(fontSize > 0.0f)
  {
    lineMetrics.ascender          = fontSize;
    lineMetrics.descender         = 0.0f;
    lineMetrics.naturalLineHeight = fallbackLineHeight;
  }

  if((lineMetrics.naturalLineHeight <= 0.0f) && (fallbackLineHeight > 0.0f))
  {
    lineMetrics.naturalLineHeight = fallbackLineHeight;
  }

  lineMetrics.baselineOffset = std::max(lineMetrics.ascender, 0.0f);

  if(lineMetrics.naturalLineHeight < (lineMetrics.baselineOffset + lineMetrics.descender))
  {
    lineMetrics.naturalLineHeight = lineMetrics.baselineOffset + lineMetrics.descender + lineMetrics.lineGap;
  }

  return lineMetrics;
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

  if(!glyphs.Empty())
  {
    preparedText.SetLineMetrics(CalculateLineMetrics(glyphs, newParagraphGlyphs, input.fontSize));
  }

  preparedText.SetClusterCount(preparedText.GetCharacterCount());
  preparedText.SetPrepared(true);

  return preparedText;
}

} // namespace Dali::Ui::Internal::TextVisualizer
